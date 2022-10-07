#pragma once
#include <array>
#include <memory/hsk_managedubo.hpp>
#include <stages/hsk_raytracingstage.hpp>

// number of samples to store in a single reservoir
#define RESERVOIR_SIZE 4

namespace hsk {
    class RestirStage : public RaytracingStage
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
        virtual void Init(const VkContext* context, Scene* scene, ManagedImage* envmap, ManagedImage* noiseSource);
        virtual void CreateRaytraycingPipeline() override;
        virtual void OnShadersRecompiled(ShaderCompiler* shaderCompiler) override;

        virtual void Destroy() override;
        virtual void DestroyShaders() override;

        struct RtStageShader
        {
            std::string  Path = "";
            ShaderModule Module;

            void Create(const VkContext* context);
            void Destroy();
        };

      protected:
        RtStageShader mRaygen{"shaders/raygen.rgen.spv"};
        RtStageShader mDefault_AnyHit{"shaders/ray-default/anyhit.rahit.spv"};
        RtStageShader mDefault_ClosestHit{"shaders/ray-default/closesthit.rchit.spv"};
        RtStageShader mDefault_Miss{"shaders/ray-default/miss.rmiss.spv"};

        std::array<hsk::ManagedBuffer, 2>                  mRestirStorageBuffers;
        std::array<std::vector<VkDescriptorBufferInfo>, 2> mBufferInfos_StorageBufferRead;
        std::array<std::vector<VkDescriptorBufferInfo>, 2> mBufferInfos_StorageBufferWrite;
        hsk::ManagedUbo<RestirConfiguration>               mRestirConfigurationUbo;
        std::vector<VkDescriptorBufferInfo>                mRestirConfigurationBufferInfos;

        std::shared_ptr<DescriptorSetHelper::DescriptorInfo> MakeDescriptorInfos_RestirConfigurationUbo(VkShaderStageFlags shaderStage);
        std::shared_ptr<DescriptorSetHelper::DescriptorInfo> MakeDescriptorInfos_StorageBufferReadSource(VkShaderStageFlags shaderStage);
        std::shared_ptr<DescriptorSetHelper::DescriptorInfo> MakeDescriptorInfos_StorageBufferWriteTarget(VkShaderStageFlags shaderStage);
    };
}  // namespace hsk
