#pragma once
#include <foray_api.hpp>
#include <scene/globalcomponents/foray_lightmanager.hpp>

namespace complex_raytracer {

    inline const std::string RAYGEN_FILE      = "shaders/raygen.rgen";
    inline const std::string CLOSESTHIT_FILE  = "shaders/default/closesthit.rchit";
    inline const std::string ANYHIT_FILE      = "shaders/default/anyhit.rahit";
    inline const std::string MISS_FILE        = "shaders/default/miss.rmiss";
    inline const std::string VISI_MISS_FILE   = "shaders/visibilitytest/miss.rmiss";
    inline const std::string VISI_ANYHIT_FILE = "shaders/visibilitytest/anyhit.rahit";

    inline const std::string SCENE_FILE = DATA_DIR "/gltf/testbox/scene.gltf";
    /// @brief If true, will invert the viewport when blitting. Will invert the scene while loading to -Y up if false
    inline constexpr bool INVERT_BLIT_INSTEAD = true;

    class ComplexRaytracingStage : public foray::stages::DefaultRaytracingStageBase
    {
      public:
        virtual void Init(foray::core::Context* context, foray::scene::Scene* scene);

      protected:
        virtual void ApiCreateRtPipeline() override;
        virtual void ApiDestroyRtPipeline() override;

        virtual void CreateOrUpdateDescriptors() override;

        foray::core::ShaderModule mRaygen;
        foray::core::ShaderModule mClosestHit;
        foray::core::ShaderModule mAnyHit;
        foray::core::ShaderModule mMiss;
        foray::core::ShaderModule mVisiMiss;
        foray::core::ShaderModule mVisiAnyHit;

        foray::scene::gcomp::LightManager* mLightManager;
    };

    class ComplexRaytracerApp : public foray::base::DefaultAppBase
    {
      protected:
        virtual void ApiBeforeInit() override;
        virtual void ApiInit() override;
        virtual void ApiOnEvent(const foray::osi::Event* event) override;

        virtual void ApiOnResized(VkExtent2D size) override;

        virtual void ApiRender(foray::base::FrameRenderInfo& renderInfo) override;
        virtual void ApiDestroy() override;

        ComplexRaytracingStage               mRtStage;
        foray::stages::ImageToSwapchainStage mSwapCopyStage;
        std::unique_ptr<foray::scene::Scene> mScene;
    };

}  // namespace complex_raytracer
