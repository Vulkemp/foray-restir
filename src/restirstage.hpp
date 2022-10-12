#pragma once
#include <array>
#include <stages/foray_gbuffer.hpp>
#include <stages/foray_raytracingstage.hpp>
#include <util/foray_managedubo.hpp>

// number of samples to store in a single reservoir
#define RESERVOIR_SIZE 4

namespace foray {
    class RestirStage : public foray::stages::RaytracingStage
    {
      protected:
        struct RestirConfiguration
        {
            uint32_t   ReservoirSize = 1;
            glm::mat4  PrevFrameProjectionViewMatrix;
            glm::vec4  CameraPos;
            glm::uvec2 ScreenSize;
            uint32_t   Frame;
            uint32_t   InitialLightSampleCount;
            uint32_t   TemporalSampleCountMultiplier;
            float      SpatialPosThreshold;
            float      SpatialNormalThreshold;
            uint32_t   SpatialNeighbors;
            float      SpatialRadius;
            int32_t    Flags;
        };

        struct Reservoir
        {
            glm::vec4 Samples[RESERVOIR_SIZE];
            uint32_t  NumSamples;
        };

      public:
        virtual void Init(const foray::core::VkContext* context,
                          foray::scene::Scene*          scene,
                          foray::core::ManagedImage*    envmap,
                          foray::core::ManagedImage*    noiseSource,
                          foray::stages::GBufferStage*  gbufferStage);
        virtual void CreateRaytraycingPipeline() override;
        virtual void OnShadersRecompiled() override;
        virtual void OnResized(const VkExtent2D& extent) override;

        virtual void SetupDescriptors() override;

        virtual void Destroy() override;
        virtual void DestroyShaders() override;

        struct RtStageShader
        {
            std::string               Path = "";
            foray::core::ShaderModule Module;

            void Create(const foray::core::VkContext* context);
            void Destroy();
        };

      protected:
        void UpdateDescriptorRestir();

        void                                                              CreateGBufferSampler();
        VkSampler                                                         mGBufferSampler{};
        std::vector<VkDescriptorImageInfo>                                mGBufferImageInfos;
        foray::stages::GBufferStage*                                      mGBufferStage{};
        std::shared_ptr<foray::core::DescriptorSetHelper::DescriptorInfo> MakeDescriptorInfos_GBufferImages(VkShaderStageFlags shaderStage);

        RtStageShader mRaygen{"shaders/raygen.rgen"};
        RtStageShader mDefault_AnyHit{"shaders/ray-default/anyhit.rahit"};
        RtStageShader mDefault_ClosestHit{"shaders/ray-default/closesthit.rchit"};
        RtStageShader mDefault_Miss{"shaders/ray-default/miss.rmiss"};

        std::array<foray::core::ManagedBuffer, 2>                         mRestirStorageBuffers;
        std::array<std::vector<VkDescriptorBufferInfo>, 2>                mBufferInfos_StorageBufferRead;
        std::array<std::vector<VkDescriptorBufferInfo>, 2>                mBufferInfos_StorageBufferWrite;
        std::shared_ptr<foray::core::DescriptorSetHelper::DescriptorInfo> MakeDescriptorInfos_StorageBufferReadSource(VkShaderStageFlags shaderStage);
        std::shared_ptr<foray::core::DescriptorSetHelper::DescriptorInfo> MakeDescriptorInfos_StorageBufferWriteTarget(VkShaderStageFlags shaderStage);

        foray::util::ManagedUbo<RestirConfiguration>                      mRestirConfigurationUbo;
        std::vector<VkDescriptorBufferInfo>                               mRestirConfigurationBufferInfos;
        std::shared_ptr<foray::core::DescriptorSetHelper::DescriptorInfo> MakeDescriptorInfos_RestirConfigurationUbo(VkShaderStageFlags shaderStage);
    };
}  // namespace foray
