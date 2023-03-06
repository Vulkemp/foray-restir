#include "foray_complexrtapp.hpp"

namespace complex_raytracer {
    void ComplexRaytracingStage::Init(foray::core::Context* context, foray::scene::Scene* scene)
    {
        mLightManager = scene->GetComponent<foray::scene::gcomp::LightManager>();
        foray::stages::DefaultRaytracingStageBase::Init(context, scene);
    }

    void ComplexRaytracingStage::Destroy()
    {
        mLights.Destroy();
    }

    void ComplexRaytracingStage::ApiCreateRtPipeline()
    {
        foray::core::ShaderCompilerConfig options{.IncludeDirs = {FORAY_SHADER_DIR}};

        mShaderKeys.push_back(mRaygen.CompileFromSource(mContext, RAYGEN_FILE, options));
        mShaderKeys.push_back(mClosestHit.CompileFromSource(mContext, CLOSESTHIT_FILE, options));
        mShaderKeys.push_back(mAnyHit.CompileFromSource(mContext, ANYHIT_FILE, options));
        mShaderKeys.push_back(mMiss.CompileFromSource(mContext, MISS_FILE, options));
        mShaderKeys.push_back(mVisiMiss.CompileFromSource(mContext, VISI_MISS_FILE, options));
        mShaderKeys.push_back(mVisiAnyHit.CompileFromSource(mContext, VISI_ANYHIT_FILE, options));

        mPipeline.GetRaygenSbt().SetGroup(0, &mRaygen);
        mPipeline.GetHitSbt().SetGroup(0, &mClosestHit, &mAnyHit, nullptr);
        mPipeline.GetHitSbt().SetGroup(1, nullptr, &mVisiAnyHit, nullptr);
        mPipeline.GetMissSbt().SetGroup(0, &mMiss);
        mPipeline.GetMissSbt().SetGroup(1, &mVisiMiss);
        mPipeline.Build(mContext, mPipelineLayout);
    }

    void ComplexRaytracingStage::ApiDestroyRtPipeline()
    {
        mPipeline.Destroy();
        mRaygen.Destroy();
        mClosestHit.Destroy();
        mAnyHit.Destroy();
        mMiss.Destroy();
        mVisiMiss.Destroy();
        mVisiAnyHit.Destroy();
    }

    void ComplexRaytracingStage::CreateOrUpdateDescriptors()
    {
        InitLights();
        const uint32_t bindpoint_lights = 11;

        mDescriptorSet.SetDescriptorAt(bindpoint_lights, mLights.GetVkDescriptorBufferInfo(), VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, foray::stages::RTSTAGEFLAGS);


        foray::stages::DefaultRaytracingStageBase::CreateOrUpdateDescriptors();
    }

    void ComplexRaytracingStage::InitLights()
    {
        struct Light
        {
            glm::vec4 LightPosAndRadius;
        };

        // clang-format off
        Light lights[5] = {
            glm::vec4(-5.0f, -7.8113f, 6.5781f,  2.0f),
			glm::vec4(-5.0f, -7.8113f, 2.0045f,  1.55f),
			glm::vec4(-5.0f, -7.8113f, -1.4012f, 1.2f),
            glm::vec4(-5.0f, -7.8113f, -4.2897f, 0.9f),
			glm::vec4(-5.0f, -7.8113f, -6.6926f, 0.6f),
        };
        // clang-format on

        mLights.Create(mContext, VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(lights),
                       VMA_MEMORY_USAGE_AUTO);
        mLights.WriteDataDeviceLocal(lights, sizeof(lights));
    }


    std::vector<std::string> g_ShaderPrintfLog;
    VkBool32                 myDebugCallback(VkDebugReportFlagsEXT      flags,
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

    void ComplexRaytracerApp::ApiBeforeInit()
    {
        mInstance.SetEnableDebugReport(true);
        mInstance.SetDebugReportFunc(&myDebugCallback);
    }

    void ComplexRaytracerApp::ApiInit()
    {
        mWindowSwapchain.GetWindow().DisplayMode(foray::osi::EDisplayMode::WindowedResizable);

        mScene = std::make_unique<foray::scene::Scene>(&mContext);

        foray::gltf::ModelConverter converter(mScene.get());

        foray::gltf::ModelConverterOptions options{.FlipY = !INVERT_BLIT_INSTEAD};

        converter.LoadGltfModel(SCENE_FILE, nullptr, options);

        mScene->UpdateTlasManager();
        mScene->UseDefaultCamera(INVERT_BLIT_INSTEAD);
        mScene->UpdateLightManager();

        mRtStage.Init(&mContext, mScene.get());
        mSwapCopyStage.Init(&mContext, mRtStage.GetRtOutput());

        if constexpr(INVERT_BLIT_INSTEAD)
        {
            mSwapCopyStage.SetFlipY(true);
        }

        RegisterRenderStage(&mRtStage);
        RegisterRenderStage(&mSwapCopyStage);
    }

    void ComplexRaytracerApp::ApiOnEvent(const foray::osi::Event* event)
    {
        mScene->InvokeOnEvent(event);
    }

    void ComplexRaytracerApp::ApiOnResized(VkExtent2D size)
    {
        mScene->InvokeOnResized(size);
    }

    void ComplexRaytracerApp::ApiRender(foray::base::FrameRenderInfo& renderInfo)
    {
        foray::core::DeviceSyncCommandBuffer& cmdBuffer = renderInfo.GetPrimaryCommandBuffer();
        cmdBuffer.Begin();
        renderInfo.GetInFlightFrame()->ClearSwapchainImage(cmdBuffer, renderInfo.GetImageLayoutCache());
        mScene->Update(renderInfo, cmdBuffer);
        mRtStage.RecordFrame(cmdBuffer, renderInfo);
        mSwapCopyStage.RecordFrame(cmdBuffer, renderInfo);
        renderInfo.GetInFlightFrame()->PrepareSwapchainImageForPresent(cmdBuffer, renderInfo.GetImageLayoutCache());
        cmdBuffer.Submit();
    }

    void ComplexRaytracerApp::ApiDestroy()
    {
        mRtStage.Destroy();
        mSwapCopyStage.Destroy();
        mScene = nullptr;
    }

}  // namespace complex_raytracer
