#include "restirstage.hpp"
#include "restir_app.hpp"
#include <core/foray_shadermanager.hpp>
#include <foray_api.hpp>
#include <scene/globalcomponents/foray_cameramanager.hpp>

// only testwise
#include <as/foray_geometrymetabuffer.hpp>
#include <as/foray_tlas.hpp>

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
        stages::ExtRaytracingStage::Init(context, scene, envmap, noiseSource);
        mRngSeedPushCOffset = ~0U;
    }

    void RestirStage::CustomObjectsCreate()
    {
        mRestirConfigurationUbo.Create(mContext, "RestirConfigurationUbo");

        RestirConfiguration& restirConfig    = mRestirConfigurationUbo.GetData();
        restirConfig.ReservoirSize           = RESERVOIR_SIZE;
        restirConfig.InitialLightSampleCount = 32;  // number of samples to initally sample?
        restirConfig.ScreenSize              = glm::uvec2(mContext->GetSwapchainSize().width, mContext->GetSwapchainSize().height);
        restirConfig.NumTriLights            = 12;  // TODO: get from collect emissive triangles
    }

    void RestirStage::GetGBufferImages()
    {
        mGBufferImages = {mGBufferStage->GetImageEOutput(stages::GBufferStage::EOutput::Albedo), mGBufferStage->GetImageEOutput(stages::GBufferStage::EOutput::Normal),
                          mGBufferStage->GetImageEOutput(stages::GBufferStage::EOutput::Position), mGBufferStage->GetImageEOutput(stages::GBufferStage::EOutput::Motion)};
    }

    void RestirStage::CreateOutputImages()
    {
        foray::stages::ExtRaytracingStage::CreateOutputImages();

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

    void RestirStage::CreateRtPipeline()
    {
        // default shaders
        mRaygen.Create(mContext);
        mDefault_AnyHit.Create(mContext);
        mRtShader_VisibilityTestHit.Create(mContext);
        mRtShader_VisibilityTestMiss.Create(mContext);

        // visibility test
        mPipeline.GetRaygenSbt().SetGroup(0, &(mRaygen.Module));
        mPipeline.GetMissSbt().SetGroup(0, &(mRtShader_VisibilityTestMiss.Module));
        mPipeline.GetHitSbt().SetGroup(0, &(mRtShader_VisibilityTestHit.Module), &(mDefault_AnyHit.Module), nullptr);
        mPipeline.Build(mContext, mPipelineLayout);

        mShaderSourcePaths.insert(mShaderSourcePaths.begin(), {mRaygen.Path, mDefault_AnyHit.Path, mRtShader_VisibilityTestHit.Path, mRtShader_VisibilityTestHit.Path});
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
        ExtRaytracingStage::CreateOrUpdateDescriptors();
    }

    void RestirStage::Resize(const VkExtent2D& extent)
    {
        RestirConfiguration& restirConfig = mRestirConfigurationUbo.GetData();
        restirConfig.ScreenSize           = glm::uvec2(extent.width, extent.height);
        DestroyOutputImages();
        CreateOutputImages();
        CreateOrUpdateDescriptors();
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

        ExtRaytracingStage::RecordFramePrepare(commandBuffer, renderInfo);
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

        stages::ExtRaytracingStage::RecordFrameTraceRays(commandBuffer, renderInfo);

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

    void RestirStage::DestroyRtPipeline()
    {
        mPipeline.Destroy();
        mRaygen.Destroy();
        mDefault_AnyHit.Destroy();
        mRtShader_VisibilityTestHit.Destroy();
        mRtShader_VisibilityTestMiss.Destroy();
    }

    void RestirStage::DestroyDescriptors()
    {
        stages::ExtRaytracingStage::DestroyDescriptors();
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

    void RestirStage::CustomObjectsDestroy()
    {
        mRestirConfigurationUbo.Destroy();
    }


#pragma endregion
#pragma region RtStageShader

    void RestirStage::RtStageShader::Create(foray::core::Context* context)
    {
        Module.LoadFromSource(context, Path);
    }

    void RestirStage::RtStageShader::Destroy()
    {
        Module.Destroy();
    }

#pragma endregion
}  // namespace foray
