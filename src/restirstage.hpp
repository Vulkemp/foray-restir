#pragma once
#include <array>
#include <foray_api.hpp>
#include <util/foray_historyimage.hpp>
#include <util/foray_managedubo.hpp>

class RestirProject;

// number of samples to store in a single reservoir
#define RESERVOIR_SIZE 4

namespace foray {
    class RestirStage : public foray::stages::DefaultRaytracingStageBase
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
            uint32_t   EnableTemporal;
            uint32_t   EnableSpatial;
        };

        struct alignas(16) LightSample
        {
            glm::vec4 position_emissionLum;
            glm::vec4 normal;
            uint32_t  lightIndex;
            float     pHat;
            float     sumWeights;
            float     w;
        };

        struct alignas(16) Reservoir
        {
            LightSample Samples[RESERVOIR_SIZE];
            uint32_t    NumSamples;
        };

        struct PushConstantRestir
        {
            uint32_t RngSeed                   = 0U;
            VkBool32 DiscardPrevFrameReservoir = VK_TRUE;
        } mPushConstantRestir;

      public:
        virtual void Init(foray::core::Context*              context,
                          foray::scene::Scene*               scene,
                          foray::core::CombinedImageSampler* envmap,
                          foray::core::ManagedImage*         noiseSource,
                          foray::stages::GBufferStage*       gbufferStage,
                          foray::stages::ImguiStage*         imguiStage,
                          RestirProject*                     restirApp);

        virtual void Resize(const VkExtent2D& extent) override;

        void SetNumberOfTriangleLights(uint32_t numTriangleLights);

        void PrepareImguiWindow();

      protected:
        RestirProject* mRestirApp{};

        virtual void CreateOutputImages() override;
        virtual void DestroyOutputImages() override;

        virtual void ApiCreateRtPipeline() override;
        virtual void ApiDestroyRtPipeline() override;

        virtual void CreateOrUpdateDescriptors() override;
        virtual void DestroyDescriptors() override;

        virtual void ApiCustomObjectsCreate() override;
        virtual void ApiCustomObjectsDestroy() override;

        virtual void CreatePipelineLayout() override;

        virtual void RecordFramePrepare(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) override;
        virtual void RecordFrameBind(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) override;
        virtual void RecordFrameTraceRays(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) override;

        void GetGBufferImages();

        enum UsedGBufferImages
        {
            GBUFFER_ALBEDO = 0,
            GBUFFER_NORMAL = 1,
            GBUFFER_POS    = 2,
            GBUFFER_MOTION = 3,
			GBUFFER_MATERIAL_INDEX = 4
        };

        foray::stages::GBufferStage*              mGBufferStage{};
        std::array<core::ManagedImage*, 5>        mGBufferImages;
        std::array<core::CombinedImageSampler, 5> mGBufferImagesSampled;

		static inline const std::string RAYGEN_FILE = "shaders/raygen.rgen";
        static inline const std::string ANYHIT_FILE  = "shaders/ray-default/anyhit.rahit";

        static inline const std::string VISI_MISS_FILE   = "shaders/restir/visibilityTest.rmiss";
        static inline const std::string VISI_ANYHIT_FILE = "shaders/restir/visibilityTest.rchit";

		foray::core::ShaderModule mRaygen;
        foray::core::ShaderModule mAnyHit;

        foray::core::ShaderModule mVisiMiss;
        foray::core::ShaderModule mVisiAnyHit;

        // access to imgui stage
        foray::stages::ImguiStage* mImguiStageRef{};

        // previous frame infos
        enum PreviousFrame
        {
            Albedo,
            Normal,
            WorldPos,
        };
        std::array<foray::util::HistoryImage, 3>  mHistoryImages;
        std::array<core::CombinedImageSampler, 3> mHistoryImagesSampled;

        std::array<foray::core::ManagedBuffer, 2> mReservoirBuffers;
        std::array<foray::core::DescriptorSet, 2> mDescriptorSetsReservoirSwap;

        foray::util::ManagedUbo<RestirConfiguration> mRestirConfigurationUbo;

        static constexpr VkSamplerCreateInfo mSamplerCi = VkSamplerCreateInfo{.sType                   = VkStructureType::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                                                              .magFilter               = VkFilter::VK_FILTER_NEAREST,
                                                                              .minFilter               = VkFilter::VK_FILTER_NEAREST,
                                                                              .addressModeU            = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                                                              .addressModeV            = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                                                              .addressModeW            = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                                                              .anisotropyEnable        = VK_FALSE,
                                                                              .compareEnable           = VK_FALSE,
                                                                              .minLod                  = 0,
                                                                              .maxLod                  = 0,
                                                                              .borderColor             = VkBorderColor::VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                                                                              .unnormalizedCoordinates = VK_FALSE};
    };
}  // namespace foray
