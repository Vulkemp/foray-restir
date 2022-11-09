#include "restirstage.hpp"
#include "restir_app.hpp"
#include <core/foray_shadermanager.hpp>
#include <scene/globalcomponents/foray_cameramanager.hpp>
#include <scene/globalcomponents/foray_materialbuffer.hpp>
#include <scene/globalcomponents/foray_texturestore.hpp>
#include <scene/globalcomponents/foray_tlasmanager.hpp>

// only testwise
#include <as/foray_geometrymetabuffer.hpp>
#include <as/foray_tlas.hpp>
#include <scene/globalcomponents/foray_geometrystore.hpp>

namespace foray {
    void RestirStage::Init(foray::core::Context*              context,
                           foray::scene::Scene*               scene,
                           foray::core::CombinedImageSampler* envmap,
                           foray::core::CombinedImageSampler* noiseSource,
                           foray::stages::GBufferStage*       gbufferStage,
                           RestirProject*                     restirApp)
    {
        mRestirApp    = restirApp;
        mContext      = context;
        mScene        = scene;
        mGBufferStage = gbufferStage;
        if(envmap != nullptr)
        {
            mEnvMap = envmap;
        }
        if(noiseSource != nullptr)
        {
            mNoiseSource = noiseSource;
        }
        CreateGBufferSampler();
        PrepareAttachments();

        // init restir buffers

        mRestirConfigurationUbo.Create(mContext, "RestirConfigurationUbo");

        RestirConfiguration& restirConfig    = mRestirConfigurationUbo.GetData();
        restirConfig.ReservoirSize           = RESERVOIR_SIZE;
        restirConfig.InitialLightSampleCount = 32;  // number of samples to initally sample?
        restirConfig.ScreenSize              = glm::uvec2(mContext->GetSwapchainSize().width, mContext->GetSwapchainSize().height);
        restirConfig.NumTriLights            = 12;  // TODO: get from collect emissive triangles

        mBufferInfos_PrevFrameBuffers.resize(4);

        RaytracingStage::Init();
    }

    void RestirStage::CreateRaytraycingPipeline()
    {
        // default shaders
        mRaygen.Create(mContext);
        mDefault_AnyHit.Create(mContext);
        mDefault_ClosestHit.Create(mContext);
        mDefault_Miss.Create(mContext);

        mPipeline.GetRaygenSbt().SetGroup(0, &(mRaygen.Module));
        mPipeline.GetMissSbt().SetGroup(0, &(mDefault_Miss.Module));
        mPipeline.GetHitSbt().SetGroup(0, &(mDefault_ClosestHit.Module), &(mDefault_AnyHit.Module), nullptr);

        // visibility test
        mRtShader_VisibilityTestHit.Create(mContext);
        mRtShader_VisibilityTestMiss.Create(mContext);
        mPipeline.GetMissSbt().SetGroup(1, &(mRtShader_VisibilityTestMiss.Module));
        mPipeline.GetHitSbt().SetGroup(1, &(mRtShader_VisibilityTestHit.Module), &(mDefault_AnyHit.Module), nullptr);
        RaytracingStage::CreateRaytraycingPipeline();
    }

    void RestirStage::OnShadersRecompiled()
    {
        bool rebuild = foray::core::ShaderManager::Instance().HasShaderBeenRecompiled(mRaygen.Path)
                       || foray::core::ShaderManager::Instance().HasShaderBeenRecompiled(mDefault_AnyHit.Path)
                       || foray::core::ShaderManager::Instance().HasShaderBeenRecompiled(mDefault_ClosestHit.Path)
                       || foray::core::ShaderManager::Instance().HasShaderBeenRecompiled(mDefault_Miss.Path)
                       || foray::core::ShaderManager::Instance().HasShaderBeenRecompiled(mRtShader_VisibilityTestHit.Path)
                       || foray::core::ShaderManager::Instance().HasShaderBeenRecompiled(mRtShader_VisibilityTestMiss.Path);
        if(rebuild)
        {
            ReloadShaders();
        }
    }

    void RestirStage::OnResized(const VkExtent2D& extent)
    {
        // update ubo
        RestirConfiguration restirConfig = mRestirConfigurationUbo.GetData();
        restirConfig.ScreenSize          = glm::uvec2(mContext->GetSwapchainSize().width, mContext->GetSwapchainSize().height);


        for(auto& frameBuffer : mPrevFrameBuffers)
        {
            frameBuffer.Resize({extent.width, extent.height, 1});
        }

        RaytracingStage::OnResized(extent);
        UpdateDescriptors();
    }

    void RestirStage::CreatePipelineLayout()
    {
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {mDescriptorSet.GetDescriptorSetLayout(), mDescriptorSetsReservoirSwap[0].GetDescriptorSetLayout()};
        mPipelineLayout.AddDescriptorSetLayouts(descriptorSetLayouts);
        mPipelineLayout.AddPushConstantRange<RaytracingStage::PushConstant>(RTSTAGEFLAGS);
        //mPipelineLayout.AddPushConstantRange<PushConstantRestir>(RTSTAGEFLAGS, sizeof(RaytracingStage::PushConstant));
        mPipelineLayout.Build(mContext);
    }

    void RestirStage::RecordFrame_Prepare(VkCommandBuffer commandBuffer, base::FrameRenderInfo& renderInfo)
    {


        std::vector<core::ManagedImage*> colorImages = {
            // prev frame images
            &mPrevFrameBuffers[static_cast<uint32_t>(PreviousFrame::Albedo)],
            &mPrevFrameBuffers[static_cast<uint32_t>(PreviousFrame::Normal)],
            &mPrevFrameBuffers[static_cast<uint32_t>(PreviousFrame::WorldPos)],
            // gbuffer images
            mGBufferStage->GetImageOutput(mGBufferStage->AlbedoOutputName),
            mGBufferStage->GetImageOutput(mGBufferStage->NormalOutputName),
            mGBufferStage->GetImageOutput(mGBufferStage->PositionOutputName),
            mGBufferStage->GetImageOutput(mGBufferStage->MotionOutputName),
        };

        std::vector<core::ManagedImage*> depthImages = {&mPrevFrameBuffers[static_cast<uint32_t>(PreviousFrame::Depth)],
                                                        mGBufferStage->GetImageOutput(mGBufferStage->DepthOutputName)};


        std::vector<VkImageMemoryBarrier> imageMemoryBarriers;
        imageMemoryBarriers.reserve(colorImages.size() + depthImages.size());

        for(core::ManagedImage* image : colorImages)
        {
            core::ImageLayoutCache::Barrier barrier;
            barrier.NewLayout                   = VK_IMAGE_LAYOUT_GENERAL;
            barrier.SrcAccessMask               = VK_ACCESS_MEMORY_WRITE_BIT;
            barrier.DstAccessMask               = VK_ACCESS_SHADER_READ_BIT;
            barrier.SubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageMemoryBarriers.push_back(renderInfo.GetImageLayoutCache().Set(image, barrier));
        }

        for(core::ManagedImage* image : depthImages)
        {
            core::ImageLayoutCache::Barrier barrier;
            barrier.NewLayout                   = VK_IMAGE_LAYOUT_GENERAL;
            barrier.SrcAccessMask               = VK_ACCESS_MEMORY_WRITE_BIT;
            barrier.DstAccessMask               = VK_ACCESS_SHADER_READ_BIT;
            barrier.SubresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            imageMemoryBarriers.push_back(renderInfo.GetImageLayoutCache().Set(image, barrier));
        }

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr,
                             imageMemoryBarriers.size(), imageMemoryBarriers.data());

        uint32_t             frameNumber           = renderInfo.GetFrameNumber();
        RestirConfiguration& restirConfig          = mRestirConfigurationUbo.GetData();
        restirConfig.Frame                         = frameNumber;
        restirConfig.PrevFrameProjectionViewMatrix = mRestirApp->mScene->GetComponent<scene::gcomp::CameraManager>()->GetUbo().GetData().PreviousProjectionViewMatrix;
        mRestirConfigurationUbo.UpdateTo(frameNumber);
        mRestirConfigurationUbo.CmdCopyToDevice(frameNumber, commandBuffer);
    }

    void RestirStage::RecordFrame(VkCommandBuffer commandBuffer, base::FrameRenderInfo& renderInfo)
    {
        // wait for frameb
        uint32_t frameNumber = renderInfo.GetFrameNumber();

        RecordFrame_Prepare(commandBuffer, renderInfo);

        //vkCmdPushConstants(commandBuffer, mPipelineLayout, RTSTAGEFLAGS, sizeof(PushConstant), sizeof(PushConstantRestir), &mPushConstantRestir);
        mPushConstantRestir.DiscardPrevFrameReservoir = false;

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, mPipelineLayout, 1, 1, &mDescriptorSetsReservoirSwap[frameNumber % 2].GetDescriptorSet(), 0,
                                nullptr);

        RaytracingStage::RecordFrame(commandBuffer, renderInfo);


        // copy gbuffer to prev frame
        CopyGBufferToPrevFrameBuffers(commandBuffer, renderInfo);
    }


    void RestirStage::CreateFixedSizeComponents()
    {
        RaytracingStage::CreateFixedSizeComponents();
    }

    void RestirStage::DestroyFixedComponents()
    {
        mRestirConfigurationUbo.Destroy();
        vkDestroySampler(mContext->Device(), mGBufferSampler, nullptr);
        RaytracingStage::DestroyFixedComponents();
    }

    void RestirStage::CreateResolutionDependentComponents()
    {
        RaytracingStage::CreateResolutionDependentComponents();
    }

    void RestirStage::DestroyResolutionDependentComponents()
    {
        RaytracingStage::DestroyResolutionDependentComponents();
        for(core::ManagedImage& image : mPrevFrameBuffers)
        {
            image.Destroy();
        }
        for(size_t i = 0; i < 2; i++)
        {
            mReservoirBuffers[i].Destroy();
        }
    }

    // TODO: cleanup setup/update descriptors .. common interface
    void RestirStage::SetupDescriptors()
    {
        // setup base class descriptors
        RaytracingStage::SetupDescriptors();

        // bind variable descriptors
        SetResolutionDependentDescriptors();

        mDescriptorSet.SetDescriptorAt(11, &mRestirConfigurationUbo.GetUboBuffer().GetDeviceBuffer(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
        mDescriptorSet.SetDescriptorAt(16, mRestirApp->mTriangleLightsBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    }

    void RestirStage::CreateDescriptorSets()
    {
        // create base descriptor sets
        RaytracingStage::CreateDescriptorSets();

        // create reservoir swap descriptor sets
        for(size_t i = 0; i < 2; i++)
        {
            mDescriptorSetsReservoirSwap[i].Create(mContext, "DescriptorSet_ReservoirBufferSwap" + std::to_string(i));
        }
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

    void RestirStage::SetResolutionDependentDescriptors()
    {
        // swap set 0
        mDescriptorSetsReservoirSwap[0].SetDescriptorAt(0, mReservoirBuffers[0], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
        mDescriptorSetsReservoirSwap[0].SetDescriptorAt(1, mReservoirBuffers[1], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR);

        // swap set 1
        mDescriptorSetsReservoirSwap[1].SetDescriptorAt(0, mReservoirBuffers[1], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
        mDescriptorSetsReservoirSwap[1].SetDescriptorAt(1, mReservoirBuffers[0], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR);

        // =======================================================================================
        // Binding GBuffer images
        {
            std::vector<core::ManagedImage*>   gbufferImages = {mGBufferStage->GetImageOutput(mGBufferStage->AlbedoOutputName),
                                                                mGBufferStage->GetImageOutput(mGBufferStage->NormalOutputName),
                                                                mGBufferStage->GetImageOutput(mGBufferStage->PositionOutputName),
                                                                mGBufferStage->GetImageOutput(mGBufferStage->MotionOutputName),
                                                                mGBufferStage->GetImageOutput(mGBufferStage->DepthOutputName)};
            std::vector<VkDescriptorImageInfo> imageInfos(gbufferImages.size());
            for(size_t i = 0; i < gbufferImages.size(); i++)
            {
                imageInfos[i].imageView   = gbufferImages[i]->GetImageView();
                imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                imageInfos[i].sampler     = mGBufferSampler;
            }
            mDescriptorSet.SetDescriptorAt(14, imageInfos, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
        }


        // =======================================================================================
        // Binding previous frame gbuffer data
        std::vector<const core::ManagedImage*> prevFrameBuffers = {};
        for(uint32_t i = 0; i < mNumPreviousFrameBuffers; i++)
        {
            prevFrameBuffers.push_back(&mPrevFrameBuffers[i]);
        }
        mDescriptorSet.SetDescriptorAt(15, prevFrameBuffers, VK_IMAGE_LAYOUT_GENERAL, mGBufferSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    }

    void RestirStage::UpdateDescriptors()
    {
        SetResolutionDependentDescriptors();
        RaytracingStage::UpdateDescriptors();
    }

    void RestirStage::PrepareAttachments()
    {
        foray::stages::RaytracingStage::CreateOutputImage();

        static const VkFormat colorFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
        static const VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

        static const VkImageUsageFlags imageUsageFlags =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        static const VkImageUsageFlags depthUsageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        VkExtent2D extent = mContext->GetSwapchainSize();

        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;


        core::ManagedImage::CreateInfo createInfo = mGBufferStage->GetImageOutput(mGBufferStage->DepthOutputName)->GetCreateInfo();
        createInfo.Name                           = "PrevFrameDepth";
        createInfo.ImageCI.usage                  = createInfo.ImageCI.usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        mPrevFrameBuffers[static_cast<int32_t>(PreviousFrame::Depth)].Create(mContext, createInfo);

        createInfo      = mGBufferStage->GetImageOutput(mGBufferStage->PositionOutputName)->GetCreateInfo();
        createInfo.Name = "PrevFrameWorldPos";
        mPrevFrameBuffers[static_cast<int32_t>(PreviousFrame::WorldPos)].Create(mContext, createInfo);

        createInfo      = mGBufferStage->GetImageOutput(mGBufferStage->NormalOutputName)->GetCreateInfo();
        createInfo.Name = "PrevFrameNormal";
        mPrevFrameBuffers[static_cast<int32_t>(PreviousFrame::Normal)].Create(mContext, createInfo);

        createInfo      = mGBufferStage->GetImageOutput(mGBufferStage->AlbedoOutputName)->GetCreateInfo();
        createInfo.Name = "PrevFrameAlbedo";
        mPrevFrameBuffers[static_cast<int32_t>(PreviousFrame::Albedo)].Create(mContext, createInfo);

        RestirConfiguration& restirConfig = mRestirConfigurationUbo.GetData();

        VkExtent2D   windowSize    = mContext->GetSwapchainSize();
        VkDeviceSize reservoirSize = sizeof(Reservoir);
        VkDeviceSize bufferSize    = windowSize.width * windowSize.height * reservoirSize * restirConfig.ReservoirSize;
        for(size_t i = 0; i < mReservoirBuffers.size(); i++)
        {
            if(mReservoirBuffers[i].Exists())
            {
                mReservoirBuffers[i].Destroy();
            }

            mReservoirBuffers[i].Create(mContext, VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, bufferSize, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, 0,
                                        std::string("RestirStorageBuffer#") + std::to_string(i));
        }
    }

    void RestirStage::RtStageShader::Create(foray::core::Context* context)
    {
        Module.LoadFromSource(context, Path);
    }

    void RestirStage::RtStageShader::Destroy()
    {
        Module.Destroy();
    }

    void RestirStage::CopyGBufferToPrevFrameBuffers(VkCommandBuffer commandBuffer, base::FrameRenderInfo& renderInfo)
    {
        struct CopyInfo
        {
            core::ManagedImage* GBufferImage;
            PreviousFrame       PrevFrameId;
            VkImageAspectFlags  AspectFlags;
        };
        std::vector<CopyInfo> copyInfos = {
            {mGBufferStage->GetImageOutput(mGBufferStage->AlbedoOutputName), PreviousFrame::Albedo, VK_IMAGE_ASPECT_COLOR_BIT},
            {mGBufferStage->GetImageOutput(mGBufferStage->NormalOutputName), PreviousFrame::Normal, VK_IMAGE_ASPECT_COLOR_BIT},
            {mGBufferStage->GetImageOutput(mGBufferStage->PositionOutputName), PreviousFrame::WorldPos, VK_IMAGE_ASPECT_COLOR_BIT},
            {mGBufferStage->GetImageOutput(mGBufferStage->DepthOutputName), PreviousFrame::Depth, VK_IMAGE_ASPECT_DEPTH_BIT},
        };

        for(auto& copyInfo : copyInfos)
        {
            core::ManagedImage* prevFrameImage = &mPrevFrameBuffers[static_cast<uint32_t>(copyInfo.PrevFrameId)];
            {
                std::vector<VkImageMemoryBarrier> imageMemoryBarriers;
                imageMemoryBarriers.reserve(2);
                // transition gbuffer image to TRANSFER SRC OPTIMAL
                core::ImageLayoutCache::Barrier barrier;
                barrier.NewLayout                   = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.SrcAccessMask               = VK_ACCESS_MEMORY_WRITE_BIT;
                barrier.DstAccessMask               = VK_ACCESS_TRANSFER_READ_BIT;
                barrier.SubresourceRange.aspectMask = copyInfo.AspectFlags;
                imageMemoryBarriers.push_back(renderInfo.GetImageLayoutCache().Set(copyInfo.GBufferImage, barrier));

                barrier.NewLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.SrcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
                barrier.DstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imageMemoryBarriers.push_back(renderInfo.GetImageLayoutCache().Set(prevFrameImage, barrier));

                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, imageMemoryBarriers.size(),
                                     imageMemoryBarriers.data());
            }

            // copy image
            {
                VkImageCopy region{};
                region.extent                        = {mContext->GetSwapchainSize().width, mContext->GetSwapchainSize().height, 1};
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
                vkCmdCopyImage(commandBuffer, copyInfo.GBufferImage->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, prevFrameImage->GetImage(),
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
            }

            {
                core::ImageLayoutCache::Barrier barrier;
                // transition prev img layout from TRANSFER DST OPTIMAL to SHADER READ OPTIMAL
                barrier.NewLayout                   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.SrcAccessMask               = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.DstAccessMask               = 0;
                barrier.SubresourceRange.aspectMask = copyInfo.AspectFlags;
                renderInfo.GetImageLayoutCache().CmdBarrier(commandBuffer, prevFrameImage, barrier, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
            }
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
            AssertVkResult(vkCreateSampler(mContext->Device(), &samplerCi, nullptr, &mGBufferSampler));
        }
    }
}  // namespace foray
