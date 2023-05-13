#include "restirstage.hpp"
#include "restir_app.hpp"
#include <core/foray_shadermanager.hpp>
#include <foray_api.hpp>
#include <scene/globalcomponents/foray_cameramanager.hpp>

// only testwise
#include <as/foray_geometrymetabuffer.hpp>
#include <as/foray_tlas.hpp>

#define RTSTAGEFLAGS VkShaderStageFlagBits::VK_SHADER_STAGE_RAYGEN_BIT_KHR | VkShaderStageFlagBits::VK_SHADER_STAGE_MISS_BIT_KHR | VkShaderStageFlagBits::VK_SHADER_STAGE_ANY_HIT_BIT_KHR

namespace foray {
#pragma region Init
    void RestirStage::Init(foray::core::Context*              context,
                           foray::scene::Scene*               scene,
                           foray::core::CombinedImageSampler* envmap,
                           foray::core::ManagedImage*         noiseSource,
                           foray::stages::GBufferStage*       gbufferStage,
                           RestirProject*                     restirApp)
    {
        mRestirApp    = restirApp;
        mGBufferStage = gbufferStage;
        GetGBufferImages();
        stages::DefaultRaytracingStageBase::Init(context, scene, envmap, noiseSource);
        mRngSeedPushCOffset = ~0U;
    }

    void RestirStage::ApiCustomObjectsCreate()
    {
        mRestirConfigurationUbo.Create(mContext, "RestirConfigurationUbo");

        RestirConfiguration& restirConfig    = mRestirConfigurationUbo.GetData();
        restirConfig.ReservoirSize           = RESERVOIR_SIZE;
        restirConfig.InitialLightSampleCount = 32;  // number of samples to initally sample?
        restirConfig.ScreenSize              = glm::uvec2(mContext->GetSwapchainSize().width, mContext->GetSwapchainSize().height);
    }

    void RestirStage::GetGBufferImages()
    {
        mGBufferImages = {mGBufferStage->GetImageEOutput(stages::GBufferStage::EOutput::Albedo), mGBufferStage->GetImageEOutput(stages::GBufferStage::EOutput::Normal),
                          mGBufferStage->GetImageEOutput(stages::GBufferStage::EOutput::Position), mGBufferStage->GetImageEOutput(stages::GBufferStage::EOutput::Motion),
                          mGBufferStage->GetImageEOutput(stages::GBufferStage::EOutput::MaterialIdx)};
    }

    void RestirStage::CreateOutputImages()
    {
        foray::stages::DefaultRaytracingStageBase::CreateOutputImages();

        mHistoryImages[PreviousFrame::Albedo].Create(mContext, mGBufferImages[UsedGBufferImages::GBUFFER_ALBEDO]);
        mHistoryImages[PreviousFrame::Normal].Create(mContext, mGBufferImages[UsedGBufferImages::GBUFFER_NORMAL]);
        mHistoryImages[PreviousFrame::WorldPos].Create(mContext, mGBufferImages[UsedGBufferImages::GBUFFER_POS]);

        RestirConfiguration& restirConfig = mRestirConfigurationUbo.GetData();

        VkExtent2D   windowSize    = mContext->GetSwapchainSize();
        VkDeviceSize reservoirSize = sizeof(Reservoir);
        VkDeviceSize bufferSize    = windowSize.width * windowSize.height * reservoirSize;
        for(size_t i = 0; i < mReservoirBuffers.size(); i++)
        {
            if(mReservoirBuffers[i].Exists())
            {
                mReservoirBuffers[i].Destroy();
            }

            mReservoirBuffers[i].Create(mContext, VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, bufferSize, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0,
                                        std::string("RestirStorageBuffer#") + std::to_string(i));
        }
    }

    void RestirStage::ApiCreateRtPipeline()
    {
        // default shaders
		foray::core::ShaderCompilerConfig options{.IncludeDirs = {FORAY_SHADER_DIR}};

        mShaderKeys.push_back(mRaygen.CompileFromSource(mContext, RAYGEN_FILE, options));
        mShaderKeys.push_back(mAnyHit.CompileFromSource(mContext, ANYHIT_FILE, options));
        mShaderKeys.push_back(mVisiMiss.CompileFromSource(mContext, VISI_MISS_FILE, options));
        mShaderKeys.push_back(mVisiAnyHit.CompileFromSource(mContext, VISI_ANYHIT_FILE, options));

        // visibility test
        mPipeline.GetRaygenSbt().SetGroup(0, &mRaygen);
        mPipeline.GetMissSbt().SetGroup(0, &mVisiMiss);
        mPipeline.GetHitSbt().SetGroup(0, &mVisiAnyHit, &mAnyHit, nullptr);

        mPipeline.Build(mContext, mPipelineLayout);

        //mShaderSourcePaths.insert(mShaderSourcePaths.begin(), {mRaygen.Path, mDefault_AnyHit.Path, mRtShader_VisibilityTestHit.Path, mRtShader_VisibilityTestHit.Path});
    }

    void RestirStage::CreatePipelineLayout()
    {
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {mDescriptorSet.GetDescriptorSetLayout(), mDescriptorSetsReservoirSwap[0].GetDescriptorSetLayout()};
        mPipelineLayout.AddDescriptorSetLayouts(descriptorSetLayouts);
        mPipelineLayout.AddPushConstantRange<PushConstantRestir>(RTSTAGEFLAGS);
        mPipelineLayout.Build(mContext);
    }

    void RestirStage::CreateOrUpdateDescriptors()
    {
        for(int32_t i = 0; i < mGBufferImages.size(); i++)
        {
            if(mGBufferImagesSampled[i].GetSampler() == nullptr)
            {
                mGBufferImagesSampled[i].Init(mContext, mGBufferImages[i], mSamplerCi);
            }
        }
        for(int32_t i = 0; i < mHistoryImages.size(); i++)
        {
            if(mHistoryImagesSampled[i].GetSampler() == nullptr)
            {
                mHistoryImagesSampled[i].Init(mContext, &mHistoryImages[i].GetHistoryImage(), mSamplerCi);
            }
        }

        // swap set 0
        mDescriptorSetsReservoirSwap[0].SetDescriptorAt(0, mReservoirBuffers[0], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
        mDescriptorSetsReservoirSwap[0].SetDescriptorAt(1, mReservoirBuffers[1], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR);

        // swap set 1
        mDescriptorSetsReservoirSwap[1].SetDescriptorAt(0, mReservoirBuffers[1], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
        mDescriptorSetsReservoirSwap[1].SetDescriptorAt(1, mReservoirBuffers[0], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR);

        // create reservoir swap descriptor sets
        for(size_t i = 0; i < 2; i++)
        {
            if(mDescriptorSetsReservoirSwap[i].Exists())
            {
                mDescriptorSetsReservoirSwap[i].Update();
            }
            else
            {
                mDescriptorSetsReservoirSwap[i].Create(mContext, "DescriptorSet_ReservoirBufferSwap" + std::to_string(i));
            }
        }

        // =======================================================================================
        // Binding GBuffer images
        {
            std::vector<const core::CombinedImageSampler*> sampledImages;
            sampledImages.reserve(5);
            for(core::CombinedImageSampler& image : mGBufferImagesSampled)
            {
                sampledImages.push_back(&image);
            }
            mDescriptorSet.SetDescriptorAt(14, sampledImages, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                           VK_SHADER_STAGE_RAYGEN_BIT_KHR);

        }


        // =======================================================================================
        // Binding previous frame gbuffer data
        std::vector<const core::CombinedImageSampler*> historyImagesSampled = {};
        for(uint32_t i = 0; i < mHistoryImagesSampled.size(); i++)
        {
            historyImagesSampled.push_back(&mHistoryImagesSampled[i]);
        }
        mDescriptorSet.SetDescriptorAt(15, historyImagesSampled, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                       VK_SHADER_STAGE_RAYGEN_BIT_KHR);

        // create base descriptor sets
        mDescriptorSet.SetDescriptorAt(11, &mRestirConfigurationUbo.GetUboBuffer().GetDeviceBuffer(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
        mDescriptorSet.SetDescriptorAt(16, mRestirApp->mTriangleLightsBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
        stages::DefaultRaytracingStageBase::CreateOrUpdateDescriptors();
    }

    void RestirStage::Resize(const VkExtent2D& extent)
    {
        RestirConfiguration& restirConfig = mRestirConfigurationUbo.GetData();
        restirConfig.ScreenSize           = glm::uvec2(extent.width, extent.height);
        DestroyOutputImages();
        CreateOutputImages();
        CreateOrUpdateDescriptors();
    }

    void RestirStage::SetNumberOfTriangleLights(uint32_t numTriangleLights)
    {
        mRestirConfigurationUbo.GetData().NumTriLights = numTriangleLights;
    }

#pragma endregion
#pragma region RecordFrame

    void RestirStage::RecordFramePrepare(VkCommandBuffer commandBuffer, base::FrameRenderInfo& renderInfo)
    {
        for(util::HistoryImage& image : mHistoryImages)
        {
            image.ApplyToLayoutCache(renderInfo.GetImageLayoutCache());
        }


        std::vector<core::ManagedImage*> colorImages = {
            // prev frame images
            &mHistoryImages[static_cast<uint32_t>(PreviousFrame::Albedo)].GetHistoryImage(),
            &mHistoryImages[static_cast<uint32_t>(PreviousFrame::Normal)].GetHistoryImage(),
            &mHistoryImages[static_cast<uint32_t>(PreviousFrame::WorldPos)].GetHistoryImage(),
            // gbuffer images
            mGBufferImages[UsedGBufferImages::GBUFFER_ALBEDO],
            mGBufferImages[UsedGBufferImages::GBUFFER_NORMAL],
            mGBufferImages[UsedGBufferImages::GBUFFER_POS],
            mGBufferImages[UsedGBufferImages::GBUFFER_MOTION],

        };

        std::vector<VkImageMemoryBarrier> imageMemoryBarriers;
        imageMemoryBarriers.reserve(colorImages.size());

        for(core::ManagedImage* image : colorImages)
        {
            core::ImageLayoutCache::Barrier barrier;
            barrier.NewLayout                   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.SrcAccessMask               = VK_ACCESS_MEMORY_WRITE_BIT;
            barrier.DstAccessMask               = VK_ACCESS_SHADER_READ_BIT;
            barrier.SubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageMemoryBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(image, barrier));
        }

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr,
                             imageMemoryBarriers.size(), imageMemoryBarriers.data());

        uint32_t             frameNumber           = renderInfo.GetFrameNumber();
        RestirConfiguration& restirConfig          = mRestirConfigurationUbo.GetData();
        restirConfig.Frame                         = frameNumber;
        restirConfig.PrevFrameProjectionViewMatrix = mRestirApp->mScene->GetComponent<scene::gcomp::CameraManager>()->GetUbo().GetData().PreviousProjectionViewMatrix;
        mRestirConfigurationUbo.UpdateTo(frameNumber);
        mRestirConfigurationUbo.CmdCopyToDevice(frameNumber, commandBuffer);
        mRestirConfigurationUbo.CmdPrepareForRead(commandBuffer, VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR, VK_ACCESS_SHADER_READ_BIT);

        DefaultRaytracingStageBase::RecordFramePrepare(commandBuffer, renderInfo);
    }

    void RestirStage::RecordFrameBind(VkCommandBuffer commandBuffer, base::FrameRenderInfo& renderInfo)
    {
        mPipeline.CmdBindPipeline(commandBuffer);

        // wait for frameb
        uint32_t frameNumber = renderInfo.GetFrameNumber();

        VkDescriptorSet descriptorSets[] = {mDescriptorSet.GetDescriptorSet(), mDescriptorSetsReservoirSwap[frameNumber % 2].GetDescriptorSet()};

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, mPipelineLayout, 0, 2U, descriptorSets, 0, nullptr);
    }

    void RestirStage::RecordFrameTraceRays(VkCommandBuffer commandBuffer, base::FrameRenderInfo& renderInfo)
    {
        mPushConstantRestir.RngSeed                   = renderInfo.GetFrameNumber();
        mPushConstantRestir.DiscardPrevFrameReservoir = false;

        vkCmdPushConstants(commandBuffer, mPipelineLayout, RTSTAGEFLAGS, 0U, sizeof(mPushConstantRestir), &mPushConstantRestir);

        stages::DefaultRaytracingStageBase::RecordFrameTraceRays(commandBuffer, renderInfo);

        // copy gbuffer to prev frame

        std::vector<util::HistoryImage*> historyImages;
        historyImages.reserve(mHistoryImages.size());

        for(int32_t i = 0; i < mHistoryImages.size(); i++)
        {
            historyImages.push_back(&mHistoryImages[i]);
        }
        util::HistoryImage::sMultiCopySourceToHistory(historyImages, commandBuffer, renderInfo);
    }

#pragma endregion
#pragma region Destroy

    void RestirStage::ApiDestroyRtPipeline()
    {
        mPipeline.Destroy();
        mRaygen.Destroy();
		mAnyHit.Destroy();
		mVisiAnyHit.Destroy();
		mVisiMiss.Destroy();
    }

    void RestirStage::DestroyDescriptors()
    {
        stages::DefaultRaytracingStageBase::DestroyDescriptors();
        mDescriptorSetsReservoirSwap[0].Destroy();
        mDescriptorSetsReservoirSwap[1].Destroy();

        for(core::CombinedImageSampler& sampler : mHistoryImagesSampled)
        {
            sampler.Destroy();
        }
        for(core::CombinedImageSampler& sampler : mGBufferImagesSampled)
        {
            sampler.Destroy();
        }
    }
    void RestirStage::DestroyOutputImages()
    {
        RenderStage::DestroyOutputImages();

        for(util::HistoryImage& image : mHistoryImages)
        {
            image.Destroy();
        }

        for(core::ManagedBuffer& buffer : mReservoirBuffers)
        {
            buffer.Destroy();
        }
    }

    void RestirStage::ApiCustomObjectsDestroy()
    {
        mRestirConfigurationUbo.Destroy();
    }


#pragma endregion
}  // namespace foray
