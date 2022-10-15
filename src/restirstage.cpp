#include "restirstage.hpp"
#include <core/foray_shadermanager.hpp>

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
        restirConfig.ReservoirSize       = RESERVOIR_SIZE;
        restirConfig.ScreenSize          = glm::uvec2(mContext->Swapchain.extent.width, mContext->Swapchain.extent.height);

        mRestirConfigurationBufferInfos.resize(1);

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
        restirConfig.ScreenSize = glm::uvec2(mContext->Swapchain.extent.width, mContext->Swapchain.extent.height);

        RaytracingStage::OnResized(extent);
        UpdateDescriptors();
    }

    void RestirStage::RecordFrame(base::FrameRenderInfo& renderInfo) {
        uint32_t frameNumber                    = renderInfo.GetFrameNumber();
        mRestirConfigurationUbo.GetData().Frame = frameNumber;
        mRestirConfigurationUbo.UpdateTo(frameNumber);
        mRestirConfigurationUbo.CmdCopyToDevice(frameNumber, renderInfo.GetCommandBuffer());
        RaytracingStage::RecordFrame(renderInfo);
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
        mDescriptorSet.SetDescriptorInfoAt(15, MakeDescriptorInfos_PrevFrameDepthBufferRead(VkShaderStageFlagBits::VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        mDescriptorSet.SetDescriptorInfoAt(16, MakeDescriptorInfos_PrevFrameDepthBufferWrite(VkShaderStageFlagBits::VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        RaytracingStage::UpdateDescriptors();
    }

    void RestirStage::PrepareAttachments()
    {
        foray::stages::RaytracingStage::PrepareAttachments();

        RestirConfiguration restirConfig = mRestirConfigurationUbo.GetData();

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

        for(size_t i = 0; i < mPrevFrameDepthImages.size(); i++)
        {
            if(mPrevFrameDepthImages[i].Exists())
            {
                mPrevFrameDepthImages[i].Destroy();
            }

            VkExtent3D               extent                = {mContext->Swapchain.extent.width, mContext->Swapchain.extent.height, 1};
            VmaMemoryUsage           memoryUsage           = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            VmaAllocationCreateFlags allocationCreateFlags = 0;

            mImageInfos_PrevFrameDepthBufferRead[i].resize(1);
            mImageInfos_PrevFrameDepthBufferWrite[i].resize(1);

            // TODO: use depth buffer precision for size
            mPrevFrameDepthImages[i].Create(mContext, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, extent.width * extent.height * 4, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, 0,
                                            std::string("PrevFrameDepthBuffer#") + std::to_string(i));
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

    std::shared_ptr<foray::core::DescriptorSetHelper::DescriptorInfo> RestirStage::MakeDescriptorInfos_PrevFrameDepthBufferRead(VkShaderStageFlags shaderStage)
    {
        auto descriptorInfo = std::make_shared<foray::core::DescriptorSetHelper::DescriptorInfo>();
        descriptorInfo->Init(VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shaderStage);

        uint32_t firstDescriptorSetIndex  = 0;
        uint32_t secondDescriptorSetIndex = 1;
        uint32_t depthBuffer1           = 0;
        uint32_t depthBuffer2             = 1;
        mRestirStorageBuffers[depthBuffer1].FillVkDescriptorBufferInfo(&mImageInfos_PrevFrameDepthBufferRead[firstDescriptorSetIndex][0]);
        mRestirStorageBuffers[depthBuffer2].FillVkDescriptorBufferInfo(&mImageInfos_PrevFrameDepthBufferRead[secondDescriptorSetIndex][0]);

        descriptorInfo->AddDescriptorSet(&mImageInfos_PrevFrameDepthBufferRead[firstDescriptorSetIndex]);
        descriptorInfo->AddDescriptorSet(&mImageInfos_PrevFrameDepthBufferRead[secondDescriptorSetIndex]);
        return descriptorInfo;
    }

    std::shared_ptr<foray::core::DescriptorSetHelper::DescriptorInfo> RestirStage::MakeDescriptorInfos_PrevFrameBuffers(VkShaderStageFlags shaderStage)
    {
        auto descriptorInfo = std::make_shared<foray::core::DescriptorSetHelper::DescriptorInfo>();
        descriptorInfo->Init(VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shaderStage);

        for(uint32_t i = 0; i < mNumPreviousFrameBuffer; i++)
        {
            mPrevFrameBuffers[i].FillVkDescriptorBufferInfo(&mBufferInfos_PrevFrameBuffers[i]);
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
