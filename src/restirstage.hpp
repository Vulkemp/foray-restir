#pragma once
#include <array>
#include <core/foray_descriptorset.hpp>
#include <stages/foray_gbuffer.hpp>
#include <stages/foray_raytracingstage.hpp>
#include <util/foray_managedubo.hpp>

class RestirProject;

// number of samples to store in a single reservoir
#define RESERVOIR_SIZE 4

namespace foray {
    class RestirStage : public foray::stages::RaytracingStage
    {
      protected:
        struct RestirConfiguration
        {
            glm::mat4  PrevFrameProjectionViewMatrix;
            glm::vec4  CameraPos;
            glm::uvec2 ScreenSize;
            uint32_t   ReservoirSize = 1;
            uint32_t   Frame;
            uint32_t   InitialLightSampleCount;
            uint32_t   TemporalSampleCountMultiplier;
            float      SpatialPosThreshold;
            float      SpatialNormalThreshold;
            uint32_t   SpatialNeighbors;
            float      SpatialRadius;
            int32_t    Flags;
            uint32_t   NumTriLights;
        };

        struct Reservoir
        {
            glm::vec4 Samples[RESERVOIR_SIZE];
            uint32_t  NumSamples;
        };

        struct PushConstantRestir
        {
            uint32_t DiscardPrevFrameReservoir{true};
        } mPushConstantRestir;

      public:
        virtual void Init(foray::core::Context*        context,
                          foray::scene::Scene*         scene,
                          foray::core::CombinedImageSampler* envmap,
                          foray::core::CombinedImageSampler* noiseSource,
                          foray::stages::GBufferStage* gbufferStage,
                          RestirProject*               restirApp);
        virtual void CreateRaytraycingPipeline() override;
        virtual void OnShadersRecompiled() override;
        virtual void OnResized(const VkExtent2D& extent) override;

        virtual void CreatePipelineLayout() override;

        virtual void RecordFrame(VkCommandBuffer commandBuffer, base::FrameRenderInfo& renderInfo) override;
        void         RecordFrame_Prepare(VkCommandBuffer commandBuffer, base::FrameRenderInfo& renderInfo);

        virtual void SetupDescriptors() override;
        virtual void CreateDescriptorSets() override;

        virtual void CreateFixedSizeComponents() override;
        virtual void DestroyFixedComponents() override;
        virtual void CreateResolutionDependentComponents() override;
        virtual void DestroyResolutionDependentComponents() override;


        virtual void Destroy() override;
        virtual void DestroyShaders() override;

        struct RtStageShader
        {
            std::string               Path = "";
            foray::core::ShaderModule Module;

            void Create(foray::core::Context* context);
            void Destroy();
        };

      protected:
        RestirProject* mRestirApp{};
        void           SetResolutionDependentDescriptors();
        virtual void   UpdateDescriptors() override;
        void           PrepareAttachments();

        void CopyGBufferToPrevFrameBuffers(VkCommandBuffer commandBuffer, base::FrameRenderInfo& renderInfo);

        void                               CreateGBufferSampler();
        VkSampler                          mGBufferSampler{};
        std::vector<VkDescriptorImageInfo> mGBufferImageInfos;
        foray::stages::GBufferStage*       mGBufferStage{};

        foray::core::ManagedImage                 mPreviousFrameDepthBuffer_Read;
        foray::core::ManagedImage                 mPreviousFrameDepthBuffer_Write;
        std::array<foray::core::ManagedBuffer, 2> mPrevFrameDepthImages;


        RtStageShader mRaygen{"shaders/raygen.rgen"};
        RtStageShader mDefault_AnyHit{"shaders/ray-default/anyhit.rahit"};
        RtStageShader mDefault_ClosestHit{"shaders/ray-default/closesthit.rchit"};
        RtStageShader mDefault_Miss{"shaders/ray-default/miss.rmiss"};

        RtStageShader mRtShader_VisibilityTestMiss{"shaders/restir/hwVisibilityTest.rmiss"};
        RtStageShader mRtShader_VisibilityTestHit{"shaders/restir/hwVisibilityTest.rchit"};

        // previous frame infos
        enum class PreviousFrame
        {
            Albedo,
            Normal,
            WorldPos,
            Depth,
        };
        const uint32_t                           mNumPreviousFrameBuffers{4};
        std::array<foray::core::ManagedImage, 4> mPrevFrameBuffers;
        std::vector<VkDescriptorImageInfo>       mBufferInfos_PrevFrameBuffers;

        std::array<foray::core::ManagedBuffer, 2> mReservoirBuffers;
        std::array<foray::core::DescriptorSet, 2> mDescriptorSetsReservoirSwap;

        foray::util::ManagedUbo<RestirConfiguration> mRestirConfigurationUbo;
    };
}  // namespace foray
