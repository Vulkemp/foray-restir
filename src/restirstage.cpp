#include "restirstage.hpp"
#include <utility/foray_shadermanager.hpp>

namespace foray {
    void RestirStage::Init(const VkContext* context, Scene* scene, ManagedImage* envmap, ManagedImage* noiseSource)
    {
        mContext = context;
        mScene   = scene;
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

        // init restir buffers
        mRestirConfigurationUbo.Create(mContext, "RestirConfigurationUbo");
        RestirConfiguration restirConfig = mRestirConfigurationUbo.GetData();
        restirConfig.ReservoirSize       = RESERVOIR_SIZE;

        mRestirConfigurationBufferInfos.resize(1);

        Extent2D     windowSize    = mContext->ContextSwapchain.Window.Size();
        VkDeviceSize reservoirSize = sizeof(Reservoir);
        VkDeviceSize bufferSize    = windowSize.Width * windowSize.Height * reservoirSize * restirConfig.ReservoirSize;
        for(size_t i = 0; i < mRestirStorageBuffers.size(); i++)
        {
            mBufferInfos_StorageBufferRead[i].resize(1);
            mBufferInfos_StorageBufferWrite[i].resize(1);
            mRestirStorageBuffers[i].Create(mContext, VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, bufferSize, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, 0,
                                            std::string("RestirStorageBuffer#") + std::to_string(i));
        }

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

    void RestirStage::OnShadersRecompiled(ShaderCompiler* shaderCompiler)
    {
        bool rebuild = ShaderManager::Instance().HasShaderBeenRecompiled(mRaygen.Path) || ShaderManager::Instance().HasShaderBeenRecompiled(mDefault_AnyHit.Path)
                       || ShaderManager::Instance().HasShaderBeenRecompiled(mDefault_ClosestHit.Path) || ShaderManager::Instance().HasShaderBeenRecompiled(mDefault_Miss.Path);
        if(rebuild)
        {
            ReloadShaders();
        }
    }

    void RestirStage::SetupDescriptors() {
        RaytracingStage::SetupDescriptors();
        mDescriptorSet.SetDescriptorInfoAt(11, MakeDescriptorInfos_RestirConfigurationUbo(VkShaderStageFlagBits::VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        mDescriptorSet.SetDescriptorInfoAt(12, MakeDescriptorInfos_StorageBufferReadSource(VkShaderStageFlagBits::VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        mDescriptorSet.SetDescriptorInfoAt(13, MakeDescriptorInfos_StorageBufferWriteTarget(VkShaderStageFlagBits::VK_SHADER_STAGE_RAYGEN_BIT_KHR));
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

    void RestirStage::RtStageShader::Create(const VkContext* context)
    {
        Module.LoadFromSource(context, Path);
    }
    void RestirStage::RtStageShader::Destroy()
    {
        Module.Destroy();
    }

    std::shared_ptr<DescriptorSetHelper::DescriptorInfo> RestirStage::MakeDescriptorInfos_RestirConfigurationUbo(VkShaderStageFlags shaderStage)
    {
        mRestirConfigurationUbo.GetUboBuffer().GetDeviceBuffer().FillVkDescriptorBufferInfo(&mRestirConfigurationBufferInfos[0]);
        auto descriptorInfo = std::make_shared<DescriptorSetHelper::DescriptorInfo>();
        descriptorInfo->Init(VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shaderStage);
        descriptorInfo->AddDescriptorSet(&mRestirConfigurationBufferInfos);
        return descriptorInfo;
    }

    std::shared_ptr<DescriptorSetHelper::DescriptorInfo> RestirStage::MakeDescriptorInfos_StorageBufferReadSource(VkShaderStageFlags shaderStage)
    {
        auto descriptorInfo = std::make_shared<DescriptorSetHelper::DescriptorInfo>();
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

    std::shared_ptr<DescriptorSetHelper::DescriptorInfo> RestirStage::MakeDescriptorInfos_StorageBufferWriteTarget(VkShaderStageFlags shaderStage)
    {
        auto descriptorInfo = std::make_shared<DescriptorSetHelper::DescriptorInfo>();
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
}  // namespace foray
