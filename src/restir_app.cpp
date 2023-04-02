#include "restir_app.hpp"
#include <imgui/imgui.h>

#include <util/foray_imageloader.hpp>
#include <util/foray_pipelinebuilder.hpp>
#include <scene/foray_geo.hpp>

#include <scene/components/foray_node_components.hpp>
#include <scene/foray_mesh.hpp>
#include <scene/globalcomponents/foray_materialmanager.hpp>

#include "structs.hpp"


//#define USE_PRINTF

std::vector<std::string> g_ShaderPrintfLog;

// And this is the callback that the validator will call
VkBool32 myDebugCallback(VkDebugReportFlagsEXT      flags,
                         VkDebugReportObjectTypeEXT objectType,
                         uint64_t                   object,
                         size_t                     location,
                         int32_t                    messageCode,
                         const char*                pLayerPrefix,
                         const char*                pMessage,
                         void*                      pUserData)
{

    printf("debugPrintfEXT: %s", pMessage);
    g_ShaderPrintfLog.push_back(pMessage);
    printf("num %d", (int)g_ShaderPrintfLog.size());
    return false;
}

void RestirProject::ApiBeforeInit()
{
#ifdef USE_PRINTF
    mInstance.SetEnableDebugReport(true);
    mInstance.SetDebugReportFunc(&myDebugCallback);
#else

    mInstance.SetEnableDebugReport(false);
#endif

	// allow drawing mesh in polygon line mode
	mDevice.GetPhysicalDeviceFeatures().fillModeNonSolid = true;
}

void RestirProject::ApiInit()
{
    //mRenderLoop.GetFrameTiming().DisableFpsLimit();
    foray::logger()->set_level(spdlog::level::debug);
    LoadEnvironmentMap();
    GenerateNoiseSource();
    loadScene();
    CollectEmissiveTriangles();
    UploadLightsToGpu();
    ConfigureStages();
}

void RestirProject::ApiOnEvent(const foray::osi::Event* event)
{
    mScene->InvokeOnEvent(event);

    // process events for imgui
    mImguiStage.ProcessSdlEvent(&(event->RawSdlEventData));
}

void RestirProject::loadScene()
{
    struct ModelLoad
    {
        std::string                        ModelPath;
        foray::gltf::ModelConverterOptions ModelConverterOptions;
    };

    // clang-format off
    //"../data/scenes/sponza/glTF/Sponza.gltf",
    std::vector<ModelLoad> modelLoads({
        // Bistro exterior
        {
            //.ModelPath = "E:/gltf/BistroExterior_out/BistroExterior.gltf",
            .ModelPath = "E:\\Programming\\foray_restir\\data\\gltf\\testbox\\scene_emissive2.gltf",
            //.ModelPath = "../data/scenes/sponza/glTF/Sponza.gltf",
            .ModelConverterOptions = {
                .FlipY = false,
            },
        },
        // Light cube
        {
            .ModelPath = "../data/scenes/cube/cube2.gltf",
        }
    });
    // clang-format on

    mScene = std::make_unique<foray::scene::Scene>(&mContext);
    foray::gltf::ModelConverter converter(mScene.get());
    for(const auto& modelLoad : modelLoads)
    {
        converter.LoadGltfModel(foray::osi::MakeRelativePath(modelLoad.ModelPath), &mContext, modelLoad.ModelConverterOptions);
    }

    mScene->UpdateTlasManager();
    mScene->UseDefaultCamera(true);

    for(int32_t i = 0; i < modelLoads.size(); i++)
    {
        const auto& path = modelLoads[i].ModelPath;
        const auto& log  = converter.GetBenchmark().GetLogs()[i];
        foray::logger()->info("Model Load \"{}\":\n{}", path, log.PrintPretty());
    }
}

void RestirProject::LoadEnvironmentMap()
{

    constexpr VkFormat                    hdrVkFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    foray::util::ImageLoader<hdrVkFormat> imageLoader;
    // env maps at https://polyhaven.com/a/alps_field
    std::string pathToEnvMap = std::string(foray::osi::CurrentWorkingDirectory()) + "/../data/textures/envmap.exr";
    if(!imageLoader.Init(pathToEnvMap))
    {
        foray::logger()->warn("Loading env map failed \"{}\"", pathToEnvMap);
        return;
    }
    if(!imageLoader.Load())
    {
        foray::logger()->warn("Loading env map failed #2 \"{}\"", pathToEnvMap);
        return;
    }

    foray::core::ManagedImage::CreateInfo ci(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, hdrVkFormat,
                                             imageLoader.GetInfo().Extent, "Environment map");

    imageLoader.InitManagedImage(&mContext, &mSphericalEnvMap, ci);
    imageLoader.Destroy();

    VkSamplerCreateInfo samplerCi{.sType                   = VkStructureType::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                  .magFilter               = VkFilter::VK_FILTER_LINEAR,
                                  .minFilter               = VkFilter::VK_FILTER_LINEAR,
                                  .addressModeU            = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                  .addressModeV            = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                  .addressModeW            = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                  .anisotropyEnable        = VK_FALSE,
                                  .compareEnable           = VK_FALSE,
                                  .minLod                  = 0,
                                  .maxLod                  = 0,
                                  .unnormalizedCoordinates = VK_FALSE};

    mSphericalEnvMapSampler.Init(&mContext, &mSphericalEnvMap, samplerCi);
}

void RestirProject::GenerateNoiseSource()
{
    foray::bench::HostBenchmark bench;
    bench.Begin();
    mNoiseSource.Create(&mContext);
    bench.End();
    foray::logger()->info("Create Noise Tex \n{}", bench.GetLogs().front().PrintPretty());
}

void RestirProject::CollectEmissiveTriangles()
{
    // find cube mesh vertices and indices
    std::vector<foray::scene::Node*> nodesWithMeshInstances{};
    mScene->FindNodesWithComponent<foray::scene::ncomp::MeshInstance>(nodesWithMeshInstances);
    std::vector<foray::scene::Vertex>* vertices;
    std::vector<uint32_t>*             indices;
    int32_t                            materialIndex;

    foray::scene::gcomp::MaterialManager* materialManager = mScene->GetComponent<foray::scene::gcomp::MaterialManager>();
    std::vector<foray::scene::Material>&  materials       = materialManager->GetVector();

    for(foray::scene::Node* node : nodesWithMeshInstances)
    {
        foray::scene::ncomp::MeshInstance* meshInstance = node->GetComponent<foray::scene::ncomp::MeshInstance>();
        foray::scene::Mesh*                mesh         = meshInstance->GetMesh();
        auto                               primitives   = mesh->GetPrimitives();

        for(auto& primitve : primitives)
        {

            materialIndex = primitve.MaterialIndex;
            if(materialIndex < 0)
            {
                // negative material index indicates fallback material
                continue;
            }

            foray::scene::Material& material = materials[materialIndex];

			// if all components of emissive factor are 0, we skip primitive
            if(glm::all(glm::equal(material.EmissiveFactor, glm::vec3(0))))
            {
                foray::logger()->info("Discarded because emissive factor was 0");
                continue;
            }

            vertices = &(primitve.Vertices);
            indices  = &(primitve.Indices);

            // get geometry world transform
            foray::scene::ncomp::Transform* transform    = node->GetTransform();
            glm::mat4                       transformMat = transform->GetGlobalMatrix();

            // create triangles from vertices & indices
            uint32_t baseCount = mTriangleLights.size();
            uint32_t numTriangles = (indices->size() / 3);
            mTriangleLights.resize(baseCount + numTriangles);
            for(size_t i = 0; i < indices->size(); i += 3)
            {
                // collect vertices
                glm::vec3 p1_vec3 = vertices->at(indices->at(i)).Pos;
                glm::vec3 p2_vec3 = vertices->at(indices->at(i + 1)).Pos;
                glm::vec3 p3_vec3 = vertices->at(indices->at(i + 2)).Pos;

                // TODO: world matrix of primitive has to be considered for triangle position.
                shader::TriLight& triLight = mTriangleLights[baseCount + (i / 3)];
                triLight.p1                = transformMat * glm::vec4(p1_vec3, 1.0);
                triLight.p2                = transformMat * glm::vec4(p2_vec3, 1.0);
                triLight.p3                = transformMat * glm::vec4(p3_vec3, 1.0);

                // material index for shader lookup
                triLight.materialIndex = materialIndex;

                // compute triangle area
                glm::vec3 normal            = vertices->at(indices->at(i)).Normal + vertices->at(indices->at(i + 1)).Normal + vertices->at(indices->at(i + 2)).Normal;
                glm::vec3 normal_normalized = glm::normalize(normal);
                triLight.normal             = glm::vec4(normal_normalized.x, normal_normalized.y, normal_normalized.z, normal.length());
                //foray::logger()->debug("TriLightNormal: {},{},{},{}", triLight.normal.x, triLight.normal.y, triLight.normal.z, triLight.normal.w);
            }
        }
    }
}

void RestirProject::UploadLightsToGpu()
{
    VkBufferUsageFlags       bufferUsage    = VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkDeviceSize             bufferSize     = mTriangleLights.size() * sizeof(shader::TriLight);
    VmaMemoryUsage           bufferMemUsage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    VmaAllocationCreateFlags allocFlags     = 0;
    mTriangleLightsBuffer.Create(&mContext, bufferUsage, bufferSize, bufferMemUsage, allocFlags, "TriangleLightsBuffer");
    mTriangleLightsBuffer.WriteDataDeviceLocal(mTriangleLights.data(), bufferSize);
}

void RestirProject::ApiDestroy()
{
    mNoiseSource.Destroy();
    mScene->Destroy();
    mScene = nullptr;
    mGbufferStage.Destroy();
    mImguiStage.Destroy();
    mRestirStage.Destroy();
    mSphericalEnvMap.Destroy();
    mTriangleLightsBuffer.Destroy();
}

void RestirProject::ApiOnShadersRecompiled(std::unordered_set<uint64_t>& recompiledShaderKeys)
{
    //mGbufferStage.OnShadersRecompiled(recompiledShaderKeys);
    //mRestirStage.OnShadersRecompiled(recompiledShaderKeys);
}

void RestirProject::PrepareImguiWindow()
{
    mImguiStage.AddWindowDraw([this]() {
        ImGui::Begin("window");

        foray::base::RenderLoop::FrameTimeAnalysis analysis = this->GetRenderLoop().AnalyseFrameTimes();
        if(analysis.Count > 0)
        {
            ImGui::Text("FPS: avg: %f min: %f", 1.f / analysis.AvgFrameTime, 1.f / analysis.MaxFrameTime);
        }

        const char* current = mCurrentOutput.data();
        if(ImGui::BeginCombo("Output", current))
        {
            std::string_view newOutput = mCurrentOutput;
            for(auto output : mOutputs)
            {
                bool selected = output.first == mCurrentOutput;
                if(ImGui::Selectable(output.first.data(), selected))
                {
                    newOutput = output.first;
                }
            }

            if(newOutput != mCurrentOutput)
            {
                mCurrentOutput = newOutput;
                mOutputChanged = true;
            }

            ImGui::EndCombo();
        }

        ImGui::End();

        ImGui::Begin("printf trace");
        ImGui::Text("%d", (int)g_ShaderPrintfLog.size());
        ImGui::BeginChild("Scrolling");
        for(int n = 0; n < g_ShaderPrintfLog.size(); n++)
        {
            ImGui::Text("%s", g_ShaderPrintfLog[n].c_str());
        }
        ImGui::EndChild();
        ImGui::End();
    });
}

void RestirProject::ConfigureStages()
{
    mGbufferStage.Init(&mContext, mScene.get());
    mRestirStage.Init(&mContext, mScene.get(), &mSphericalEnvMapSampler, &mNoiseSource.GetImage(), &mGbufferStage, this);
    mRestirStage.SetNumberOfTriangleLights(mTriangleLights.size());
    
	auto depthImage = mGbufferStage.GetImageOutput(mGbufferStage.DepthOutputName);
    auto colorImage = mGbufferStage.GetImageOutput(mGbufferStage.AlbedoOutputName);
    auto rtOutput = mRestirStage.GetImageOutput(mRestirStage.OutputName);
    mETMStage.Init(&mContext, &mTriangleLights, depthImage, rtOutput, mScene.get());
    UpdateOutputs();

    mImguiStage.InitForSwapchain(&mContext);
    PrepareImguiWindow();

    // Init copy stage
    mImageToSwapchainStage.Init(&mContext, mOutputs[mCurrentOutput]);
    mImageToSwapchainStage.SetFlipY(true);

	RegisterRenderStage(&mGbufferStage);
    RegisterRenderStage(&mRestirStage);
    RegisterRenderStage(&mETMStage);
    RegisterRenderStage(&mImguiStage);
    RegisterRenderStage(&mImageToSwapchainStage);
}

void RestirProject::ApiRender(foray::base::FrameRenderInfo& renderInfo)
{
    if(mOutputChanged)
    {
        ApplyOutput();
        mOutputChanged = false;
    }

    foray::core::DeviceSyncCommandBuffer& commandBuffer = renderInfo.GetPrimaryCommandBuffer();
    commandBuffer.Begin();

    mScene->Update(renderInfo, commandBuffer);
    mGbufferStage.RecordFrame(commandBuffer, renderInfo);

    // after gbuffer stage, transform depth from attachment optimal to read optimal
    {
        foray::core::ImageLayoutCache::Barrier barrier;
        barrier.SrcAccessMask               = 0;
        barrier.DstAccessMask               = 0;
        barrier.NewLayout                   = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
        barrier.SubresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        renderInfo.GetImageLayoutCache().CmdBarrier(commandBuffer, mGbufferStage.GetImageOutput(foray::stages::GBufferStage::DepthOutputName), barrier,
                                                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
    }

    mRestirStage.RecordFrame(commandBuffer, renderInfo);

	mETMStage.RecordFrame(commandBuffer, renderInfo);

    // copy final image to swapchain
    mImageToSwapchainStage.RecordFrame(commandBuffer, renderInfo);

    // draw imgui windows
    mImguiStage.RecordFrame(commandBuffer, renderInfo);

    renderInfo.GetInFlightFrame()->PrepareSwapchainImageForPresent(commandBuffer, renderInfo.GetImageLayoutCache());
    commandBuffer.Submit();
}

void RestirProject::ApiOnResized(VkExtent2D size)
{
    mScene->InvokeOnResized(size);
    mGbufferStage.Resize(size);
    mRestirStage.Resize(size);
    UpdateOutputs();
    mImguiStage.Resize(size);
    mImageToSwapchainStage.Resize(size);
}

void lUpdateOutput(std::unordered_map<std::string_view, foray::core::ManagedImage*>& map, foray::stages::RenderStage& stage, const std::string_view name)
{
    map[name] = stage.GetImageOutput(name);
}

void RestirProject::UpdateOutputs()
{
    mOutputs.clear();
    lUpdateOutput(mOutputs, mGbufferStage, foray::stages::GBufferStage::AlbedoOutputName);
    lUpdateOutput(mOutputs, mGbufferStage, foray::stages::GBufferStage::PositionOutputName);
    lUpdateOutput(mOutputs, mGbufferStage, foray::stages::GBufferStage::NormalOutputName);
    lUpdateOutput(mOutputs, mRestirStage, foray::stages::DefaultRaytracingStageBase::OutputName);

    if(mCurrentOutput.size() == 0 || !mOutputs.contains(mCurrentOutput))
    {
        if(mOutputs.size() == 0)
        {
            mCurrentOutput = "";
        }
        else
        {
            mCurrentOutput = mOutputs.begin()->first;
        }
    }
}

void RestirProject::ApplyOutput()
{
    vkDeviceWaitIdle(mContext.Device());
    auto output = mOutputs[mCurrentOutput];
    mImageToSwapchainStage.SetSrcImage(output);
}