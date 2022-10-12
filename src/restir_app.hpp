#pragma once

#include "configurepath.cmakegenerated.hpp"
#include <foray_exception.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <foray_glm.hpp>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "restirstage.hpp"
#include "stages/foray_flipimage.hpp"
#include "stages/foray_gbuffer.hpp"
#include "stages/foray_imagetoswapchain.hpp"
#include "stages/foray_imguistage.hpp"
#include "stages/foray_raytracingstage.hpp"
#include <foray_env.hpp>
#include <foray_rtrpf.hpp>
#include <scenegraph/foray_scenegraph.hpp>
#include <stdint.h>
#include <utility/foray_noisesource.hpp>

/*
*
* first version: use gbuffer stage first hit, generate rays, do monte carlo, use restir for sample reuse.
* What do we need:
* - an image sized storage buffer for restir storage, one for reading one for writing,
* - uniform buffer with restir configuration
*
* Restir requirements
* - knowledge where light samples are -> this is optional, can use monte carlo instead
* - buffer for the reservoirs
* - the restir algorithm
* - a shading pass?
*
* restir ablauf:
* gbuffer returns first hit. from there, we use monte carlo to sample a light (hitting the environment map, hitting an emissive triangle or
* a light source) and weight the sample.
*
*  point lights: sample directly by the point lights
*  area lights: sample a random point on a triangle.
* Then we use the sample for the reservoir and the temporal and spatial reservoir resampling
*
* then, the final sample is used to shade
*
* what we need: cpu side:
* the gbuffer stage
* uniform buffer/push constant, dictating restir configuration
* a reservoir buffer (or two, for read/write access?) - actally two, that can be swapped.
* our light sources? a monte carlo sampler?
* 
* refactor API - currently its very confusing which classes are needed/intended for me to interact with and which classes are framework internal
* There should be an API - something that uses intellisense and shows me most/all of my possibilities.
* Maybe use namespaces? Core for core functionality and in foray are usage classes?
* Detect use cases.
* Imo OnEvent should be from minimal app base - default app base should have an OnKeyEvent / OnMouseEvent, which are most used cases.
* then there should be an overview somewhere of the design, having the structure of DefaultAppBase, MinimalAppBase, what they're doing, etc.
*
*/

class RestirProject : public foray::DefaultAppBase
{
  public:
    RestirProject()  = default;
    ~RestirProject() = default;

  protected:
    virtual void Init() override;
    virtual void OnEvent(const foray::Event* event) override;
    virtual void Update(float delta) override;

    virtual void RecordCommandBuffer(foray::FrameRenderInfo& renderInfo) override;
    virtual void QueryResultsAvailable(uint64_t frameIndex) override;
    virtual void OnResized(VkExtent2D size) override;
    virtual void Destroy() override;
    virtual void OnShadersRecompiled(foray::ShaderCompiler* shaderCompiler) override;


    void PrepareImguiWindow();

    std::unique_ptr<foray::Scene> mScene;

    void loadScene();
    void LoadEnvironmentMap();
    void GenerateNoiseSource();

    /// @brief generates a GBuffer (Albedo, Positions, Normal, Motion Vectors, Mesh Instance Id as output images)
    foray::GBufferStage mGbufferStage;
    /// @brief Renders immediate mode GUI
    foray::ImguiStage mImguiStage;
    /// @brief Copies the intermediate rendertarget to the swapchain image
    foray::ImageToSwapchainStage mImageToSwapchainStage;
    /// @brief Generates a raytraced image
    foray::RestirStage mRestirStage;

    foray::ManagedImage mSphericalEnvMap{};

    foray::NoiseSource mNoiseSource;

    void ConfigureStages();

    std::unordered_map<std::string_view, foray::ManagedImage*> mOutputs;
    std::string_view                                         mCurrentOutput = "";
    bool                                                     mOutputChanged = false;

#ifdef ENABLE_GBUFFER_BENCH
    foray::BenchmarkLog mDisplayedLog;
#endif  // ENABLE_GBUFFER_BENCH

    void UpdateOutputs();
    void ApplyOutput();
};

int main(int argv, char** args)
{
    foray::OverrideCurrentWorkingDirectory(CWD_OVERRIDE_PATH);
    RestirProject project;
    return project.Run();
}