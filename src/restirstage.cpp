#include "restirstage.hpp"
#include <core/foray_shadermanager.hpp>
#include <util/foray_createinfotemplates.hpp>

namespace foray {
    void RestirStage::Init(const foray::core::VkContext* context,
                           foray::scene::Scene*          scene,
                           foray::core::ManagedImage*    envmap,
                           foray::core::ManagedImage*    noiseSource,
                           foray::stages::GBufferStage*  gbufferStage)
    {
        mContext      = context;
        mScene        = scene;
        mGBufferStage = gbufferStage;
        if(envmap != nullptr)
        {
            mEnvMap.Create(context, envmap);
        }
        if(noiseSource != nullptr)
        {
            mNoiseSource.Create(context, noiseSource, false);
            VkSamplerCreateInfo samplerCi{.sType                   = VkStructureType::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                          .magFilter               = VkFilter::VK_FILTER_NEAREST,
                                          .minFilter               = VkFilter::VK_FILTER_NEAREST,
                                          .addressModeU            = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                          .addressModeV            = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                          .addressModeW            = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                          .anisotropyEnable        = VK_FALSE,
                                          .compareEnable           = VK_FALSE,
                                          .minLod                  = 0,
                                          .maxLod                  = 0,
                                          .unnormalizedCoordinates = VK_FALSE};
            AssertVkResult(vkCreateSampler(context->Device, &samplerCi, nullptr, &mNoiseSource.Sampler));
        }
        CreateGBufferSampler();

        // init restir buffers

        mRestirConfigurationUbo.Create(mContext, "RestirConfigurationUbo");

        RestirConfiguration& restirConfig = mRestirConfigurationUbo.GetData();
        restirConfig.ReservoirSize        = RESERVOIR_SIZE;
        restirConfig.ScreenSize           = glm::uvec2(mContext->Swapchain.extent.width, mContext->Swapchain.extent.height);

        mRestirConfigurationBufferInfos.resize(1);

        mBufferInfos_PrevFrameBuffers.resize(4);

        RaytracingStage::Init();
    }

    void RestirStage::CreateRaytraycingPipeline()
    {
        mRaygen.Create(mContext);
        mDefault_AnyHit.Create(mContext);
        mDefault_ClosestHit.Create(mContext);
        mDefault_Miss.Create(mContext);

        mPipeline.GetRaygenSbt().SetGroup(0, &(mRaygen.Module));
        mPipeline.GetMissSbt().SetGroup(0, &(mDefault_Miss.Module));
        mPipeline.GetHitSbt().SetGroup(0, &(mDefault_ClosestHit.Module), &(mDefault_AnyHit.Module), nullptr);
        RaytracingStage::CreateRaytraycingPipeline();
    }

    void RestirStage::OnShadersRecompiled()
    {
        bool rebuild = foray::core::ShaderManager::Instance().HasShaderBeenRecompiled(mRaygen.Path)
                       || foray::core::ShaderManager::Instance().HasShaderBeenRecompiled(mDefault_AnyHit.Path)
                       || foray::core::ShaderManager::Instance().HasShaderBeenRecompiled(mDefault_ClosestHit.Path)
                       || foray::core::ShaderManager::Instance().HasShaderBeenRecompiled(mDefault_Miss.Path);
        if(rebuild)
        {
            ReloadShaders();
        }
    }

    void RestirStage::OnResized(const VkExtent2D& extent)
    {
        // update ubo
        RestirConfiguration restirConfig = mRestirConfigurationUbo.GetData();
        restirConfig.ScreenSize          = glm::uvec2(mContext->Swapchain.extent.width, mContext->Swapchain.extent.height);

        RaytracingStage::OnResized(extent);
        UpdateDescriptors();
    }

    void RestirStage::RecordFrame(base::FrameRenderInfo& renderInfo)
    {
        uint32_t frameNumber                    = renderInfo.GetFrameNumber();
        mRestirConfigurationUbo.GetData().Frame = frameNumber;
        mRestirConfigurationUbo.UpdateTo(frameNumber);
        mRestirConfigurationUbo.CmdCopyToDevice(frameNumber, renderInfo.GetCommandBuffer());
        RaytracingStage::RecordFrame(renderInfo);

        // copy gbuffer to prev frame
        CopyGBufferToPrevFrameBuffers(renderInfo);
    }

    // TODO: cleanup setup/update descriptors .. common interface
    void RestirStage::SetupDescriptors()
    {
        RaytracingStage::SetupDescriptors();
        UpdateDescriptors();
    }

    void RestirStage::Destroy()
    {
        RaytracingStage::Destroy();
    }

    void RestirStage::DestroyShaders()
    {
        mRaygen.Destroy();
        mDefault_AnyHit.Destroy();
        mDefault_ClosestHit.Destroy();
        mDefault_Miss.Destroy();
    }

    void RestirStage::UpdateDescriptors()
    {
        // updating gbuffer image handles
        mDescriptorSet.SetDescriptorInfoAt(11, MakeDescriptorInfos_RestirConfigurationUbo(VkShaderStageFlagBits::VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        mDescriptorSet.SetDescriptorInfoAt(12, MakeDescriptorInfos_StorageBufferReadSource(VkShaderStageFlagBits::VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        mDescriptorSet.SetDescriptorInfoAt(13, MakeDescriptorInfos_StorageBufferWriteTarget(VkShaderStageFlagBits::VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        mDescriptorSet.SetDescriptorInfoAt(14, MakeDescriptorInfos_GBufferImages(VkShaderStageFlagBits::VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        mDescriptorSet.SetDescriptorInfoAt(16, MakeDescriptorInfos_PrevFrameBuffers(VkShaderStageFlagBits::VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        //mDescriptorSet.SetDescriptorInfoAt(16, MakeDescriptorInfos_PrevFrameDepthBufferWrite(VkShaderStageFlagBits::VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        RaytracingStage::UpdateDescriptors();
    }

    void RestirStage::PrepareAttachments()
    {
        foray::stages::RaytracingStage::PrepareAttachments();

        static const VkFormat colorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        static const VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

        static const VkImageUsageFlags imageUsageFlags =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        static const VkImageUsageFlags depthUsageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        VkExtent3D               extent                = {mContext->Swapchain.extent.width, mContext->Swapchain.extent.height, 1};
        VmaMemoryUsage           memoryUsage           = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        VmaAllocationCreateFlags allocationCreateFlags = 0;
        VkImageLayout            intialLayout          = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageAspectFlags       aspectMask            = VK_IMAGE_ASPECT_COLOR_BIT;

        core::ManagedImage::QuickTransition t;
        t.SrcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        t.DstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        t.SrcMask  = 0;
        t.DstMask  = 0;
        t.AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        t.NewImageLayout = VK_IMAGE_LAYOUT_GENERAL;

        mPrevFrameBuffers[static_cast<int32_t>(PreviousFrame::Depth)].Create(mContext, memoryUsage, allocationCreateFlags, extent, depthUsageFlags, depthFormat, intialLayout,
                                                                             VK_IMAGE_ASPECT_DEPTH_BIT, "PrevFrameDepth");
        mPrevFrameBuffers[static_cast<int32_t>(PreviousFrame::Depth)].TransitionLayout(t);
        mPrevFrameBuffers[static_cast<int32_t>(PreviousFrame::WorldPos)].Create(mContext, memoryUsage, allocationCreateFlags, extent, imageUsageFlags, colorFormat, intialLayout,
                                                                                aspectMask, "PrevFrameWorld");
        t.AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        mPrevFrameBuffers[static_cast<int32_t>(PreviousFrame::WorldPos)].TransitionLayout(t);
        mPrevFrameBuffers[static_cast<int32_t>(PreviousFrame::Normal)].Create(mContext, memoryUsage, allocationCreateFlags, extent, imageUsageFlags, colorFormat, intialLayout,
                                                                              aspectMask, "PrevFrameNormal");
        mPrevFrameBuffers[static_cast<int32_t>(PreviousFrame::Normal)].TransitionLayout(t);
        mPrevFrameBuffers[static_cast<int32_t>(PreviousFrame::Albedo)].Create(mContext, memoryUsage, allocationCreateFlags, extent, imageUsageFlags, colorFormat, intialLayout,
                                                                              aspectMask, "PrevFrameAlbedo");
        mPrevFrameBuffers[static_cast<int32_t>(PreviousFrame::Albedo)].TransitionLayout(t);

        RestirConfiguration& restirConfig = mRestirConfigurationUbo.GetData();

        Extent2D     windowSize    = mContext->ContextSwapchain.Window.Size();
        VkDeviceSize reservoirSize = sizeof(Reservoir);
        VkDeviceSize bufferSize    = windowSize.Width * windowSize.Height * reservoirSize * restirConfig.ReservoirSize;
        for(size_t i = 0; i < mRestirStorageBuffers.size(); i++)
        {
            if(mRestirStorageBuffers[i].Exists())
            {
                mRestirStorageBuffers[i].Destroy();
            }

            mBufferInfos_StorageBufferRead[i].resize(1);
            mBufferInfos_StorageBufferWrite[i].resize(1);
            mRestirStorageBuffers[i].Create(mContext, VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, bufferSize, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, 0,
                                            std::string("RestirStorageBuffer#") + std::to_string(i));
        }
    }

    void RestirStage::RtStageShader::Create(const foray::core::VkContext* context)
    {
        Module.LoadFromSource(context, Path);
    }
    void RestirStage::RtStageShader::Destroy()
    {
        Module.Destroy();
    }

    void RestirStage::CopyGBufferToPrevFrameBuffers(base::FrameRenderInfo& renderInfo)
    {
        VkCommandBuffer commandBuffer = renderInfo.GetCommandBuffer();

        struct CopyInfo
        {
            core::ManagedImage* GBufferImage;
            PreviousFrame       PrevFrameId;
            VkImageAspectFlags  AspectFlags;
        };
        std::vector<CopyInfo> copyInfos = {
            {mGBufferStage->GetColorAttachmentByName(mGBufferStage->Albedo), PreviousFrame::Albedo, VK_IMAGE_ASPECT_COLOR_BIT},
            {mGBufferStage->GetColorAttachmentByName(mGBufferStage->WorldspaceNormal), PreviousFrame::Normal, VK_IMAGE_ASPECT_COLOR_BIT},
            {mGBufferStage->GetColorAttachmentByName(mGBufferStage->WorldspacePosition), PreviousFrame::WorldPos, VK_IMAGE_ASPECT_COLOR_BIT},
            {mGBufferStage->GetDepthBuffer(), PreviousFrame::Depth, VK_IMAGE_ASPECT_DEPTH_BIT},
        };

        for(auto& copyInfo : copyInfos)
        {
            // determine image type

            // transition gbuffer image layout
            core::ManagedImage::QuickTransition t;
            t.NewImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            t.DstStage       = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            t.SrcStage       = VK_PIPELINE_STAGE_TRANSFER_BIT;
            t.DstMask        = 0;
            t.SrcMask        = VK_ACCESS_TRANSFER_READ_BIT;
            t.AspectMask     = copyInfo.AspectFlags;
            copyInfo.GBufferImage->TransitionLayout(t, commandBuffer);

            // transition prev img layout
            core::ManagedImage* prevFrameImage = &mPrevFrameBuffers[static_cast<uint32_t>(copyInfo.PrevFrameId)];
            t.NewImageLayout                   = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            t.SrcMask                          = VK_ACCESS_TRANSFER_WRITE_BIT;
            prevFrameImage->TransitionLayout(t, commandBuffer);

            // copy image
            {
                VkImageCopy region{};
                region.extent                        = {mContext->Swapchain.extent.width, mContext->Swapchain.extent.height, 1};
                region.dstOffset                     = {0, 0, 0};
                region.srcOffset                     = {0, 0, 0};
                region.srcSubresource.aspectMask     = copyInfo.AspectFlags;
                region.srcSubresource.baseArrayLayer = 0;
                region.srcSubresource.layerCount     = 1;
                region.srcSubresource.mipLevel       = 0;
                region.dstSubresource.aspectMask     = copyInfo.AspectFlags;
                region.dstSubresource.baseArrayLayer = 0;
                region.dstSubresource.layerCount     = 1;
                region.dstSubresource.mipLevel       = 0;
                vkCmdCopyImage(commandBuffer, copyInfo.GBufferImage->GetImage(), copyInfo.GBufferImage->GetImageLayout(), prevFrameImage->GetImage(),
                               prevFrameImage->GetImageLayout(), 1, &region);
            }

            // transition layouts back
            t.NewImageLayout = VK_IMAGE_LAYOUT_GENERAL;
            t.DstStage       = VK_PIPELINE_STAGE_TRANSFER_BIT;
            t.SrcStage       = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            t.DstMask        = VK_ACCESS_TRANSFER_READ_BIT;
            t.SrcMask        = 0;
            copyInfo.GBufferImage->TransitionLayout(t, commandBuffer);

            // transition prev img layout
            t.DstMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            prevFrameImage->TransitionLayout(t, commandBuffer);
        }
    }

    void RestirStage::CreateGBufferSampler()
    {
        if(mGBufferSampler == nullptr)
        {
            VkSamplerCreateInfo samplerCi{.sType                   = VkStructureType::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                          .magFilter               = VkFilter::VK_FILTER_NEAREST,
                                          .minFilter               = VkFilter::VK_FILTER_NEAREST,
                                          .addressModeU            = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                          .addressModeV            = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                          .addressModeW            = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                          .anisotropyEnable        = VK_FALSE,
                                          .compareEnable           = VK_FALSE,
                                          .minLod                  = 0,
                                          .maxLod                  = 0,
                                          .unnormalizedCoordinates = VK_FALSE};
            AssertVkResult(vkCreateSampler(mContext->Device, &samplerCi, nullptr, &mGBufferSampler));
        }
    }

    std::shared_ptr<foray::core::DescriptorSetHelper::DescriptorInfo> RestirStage::MakeDescriptorInfos_RestirConfigurationUbo(VkShaderStageFlags shaderStage)
    {
        mRestirConfigurationUbo.GetUboBuffer().GetDeviceBuffer().FillVkDescriptorBufferInfo(&mRestirConfigurationBufferInfos[0]);
        auto descriptorInfo = std::make_shared<foray::core::DescriptorSetHelper::DescriptorInfo>();
        descriptorInfo->Init(VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shaderStage);
        descriptorInfo->AddDescriptorSet(&mRestirConfigurationBufferInfos);
        return descriptorInfo;
    }

    std::shared_ptr<foray::core::DescriptorSetHelper::DescriptorInfo> RestirStage::MakeDescriptorInfos_PrevFrameBuffers(VkShaderStageFlags shaderStage)
    {
        auto descriptorInfo = std::make_shared<foray::core::DescriptorSetHelper::DescriptorInfo>();
        descriptorInfo->Init(VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, shaderStage);

        for(uint32_t i = 0; i < mNumPreviousFrameBuffers; i++)
        {
            mBufferInfos_PrevFrameBuffers[i].imageLayout = mPrevFrameBuffers[i].GetImageLayout();
            mBufferInfos_PrevFrameBuffers[i].imageView   = mPrevFrameBuffers[i].GetImageView();
            mBufferInfos_PrevFrameBuffers[i].sampler     = mGBufferSampler;
        }

        descriptorInfo->AddDescriptorSet(&mBufferInfos_PrevFrameBuffers);
        return descriptorInfo;
    }

    std::shared_ptr<foray::core::DescriptorSetHelper::DescriptorInfo> RestirStage::MakeDescriptorInfos_StorageBufferReadSource(VkShaderStageFlags shaderStage)
    {
        auto descriptorInfo = std::make_shared<foray::core::DescriptorSetHelper::DescriptorInfo>();
        descriptorInfo->Init(VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shaderStage);

        uint32_t firstDescriptorSetIndex  = 0;
        uint32_t secondDescriptorSetIndex = 1;
        uint32_t storageBuffer1           = 0;
        uint32_t storageBuffer2           = 1;
        mRestirStorageBuffers[storageBuffer1].FillVkDescriptorBufferInfo(&mBufferInfos_StorageBufferRead[firstDescriptorSetIndex][0]);
        mRestirStorageBuffers[storageBuffer2].FillVkDescriptorBufferInfo(&mBufferInfos_StorageBufferRead[secondDescriptorSetIndex][0]);

        descriptorInfo->AddDescriptorSet(&mBufferInfos_StorageBufferRead[firstDescriptorSetIndex]);
        descriptorInfo->AddDescriptorSet(&mBufferInfos_StorageBufferRead[secondDescriptorSetIndex]);
        return descriptorInfo;
    }

    std::shared_ptr<foray::core::DescriptorSetHelper::DescriptorInfo> RestirStage::MakeDescriptorInfos_StorageBufferWriteTarget(VkShaderStageFlags shaderStage)
    {
        auto descriptorInfo = std::make_shared<foray::core::DescriptorSetHelper::DescriptorInfo>();
        descriptorInfo->Init(VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shaderStage);

        uint32_t firstDescriptorSetIndex  = 0;
        uint32_t secondDescriptorSetIndex = 1;
        uint32_t storageBuffer1           = 0;
        uint32_t storageBuffer2           = 1;
        mRestirStorageBuffers[storageBuffer2].FillVkDescriptorBufferInfo(&mBufferInfos_StorageBufferWrite[firstDescriptorSetIndex][0]);
        mRestirStorageBuffers[storageBuffer1].FillVkDescriptorBufferInfo(&mBufferInfos_StorageBufferWrite[secondDescriptorSetIndex][0]);

        descriptorInfo->AddDescriptorSet(&mBufferInfos_StorageBufferWrite[firstDescriptorSetIndex]);
        descriptorInfo->AddDescriptorSet(&mBufferInfos_StorageBufferWrite[secondDescriptorSetIndex]);
        return descriptorInfo;
    }

    std::shared_ptr<foray::core::DescriptorSetHelper::DescriptorInfo> RestirStage::MakeDescriptorInfos_GBufferImages(VkShaderStageFlags shaderStage)
    {
        auto descriptorInfo = std::make_shared<foray::core::DescriptorSetHelper::DescriptorInfo>();
        descriptorInfo->Init(VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, shaderStage);

        const uint32_t gbufferImageCount = 5;
        mGBufferImageInfos.resize(gbufferImageCount);
        std::array<foray::core::ManagedImage*, gbufferImageCount> gbufferImages = {
            mGBufferStage->GetColorAttachmentByName(mGBufferStage->Albedo),
            mGBufferStage->GetColorAttachmentByName(mGBufferStage->WorldspaceNormal),
            mGBufferStage->GetColorAttachmentByName(mGBufferStage->WorldspacePosition),
            mGBufferStage->GetColorAttachmentByName(mGBufferStage->MotionVector),
            mGBufferStage->GetDepthBuffer(),
        };

        for(size_t i = 0; i < gbufferImageCount; i++)
        {
            mGBufferImageInfos[i].imageView   = gbufferImages[i]->GetImageView();
            mGBufferImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            mGBufferImageInfos[i].sampler     = mGBufferSampler;
        }
        mGBufferImageInfos[4].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        descriptorInfo->AddDescriptorSet(&mGBufferImageInfos);
        return descriptorInfo;
    }
}  // namespace foray
