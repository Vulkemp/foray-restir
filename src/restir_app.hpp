#pragma once

#include "configurepath.cmakegenerated.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <foray_glm.hpp>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "structs.hpp"
#include <stdint.h>

#include <foray_api.hpp>
#include <util/foray_noisesource.hpp>

#include "restirstage.hpp"

class RestirProject : public foray::base::DefaultAppBase
{
    friend foray::RestirStage;

  public:
    RestirProject() = default;
    ~RestirProject(){};

  protected:
    virtual void ApiBeforeInit() override;

    virtual void ApiInit() override;
    virtual void ApiOnEvent(const foray::osi::Event* event) override;

    virtual void ApiRender(foray::base::FrameRenderInfo& renderInfo) override;
    virtual void ApiOnResized(VkExtent2D size) override;
    virtual void ApiDestroy() override;
    virtual void ApiOnShadersRecompiled() override;


    void PrepareImguiWindow();

    std::unique_ptr<foray::scene::Scene> mScene;

    void loadScene();
    void LoadEnvironmentMap();
    void GenerateNoiseSource();

    void                       CollectEmissiveTriangles();
    void                       UploadLightsToGpu();
    foray::core::ManagedBuffer mTriangleLightsBuffer;

    std::vector<shader::TriLight> mTriangleLights;

    /// @brief generates a GBuffer (Albedo, Positions, Normal, Motion Vectors, Mesh Instance Id as output images)
    foray::stages::GBufferStage mGbufferStage;
    /// @brief Renders immediate mode GUI
    foray::stages::ImguiStage mImguiStage;
    /// @brief Copies the intermediate rendertarget to the swapchain image
    foray::stages::ImageToSwapchainStage mImageToSwapchainStage;
    /// @brief Generates a raytraced image
    foray::RestirStage mRestirStage;

    foray::core::ManagedImage         mSphericalEnvMap{};
    foray::core::CombinedImageSampler mSphericalEnvMapSampler{};


    foray::util::NoiseSource mNoiseSource;

    void ConfigureStages();

    std::unordered_map<std::string_view, foray::core::ManagedImage*> mOutputs;
    std::string_view                                                 mCurrentOutput = "";
    bool                                                             mOutputChanged = false;

    void UpdateOutputs();
    void ApplyOutput();
};