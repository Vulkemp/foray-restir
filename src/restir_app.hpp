#pragma once

#include "configurepath.cmakegenerated.hpp"
#include <hsk_exception.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <hsk_glm.hpp>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "restirstage.hpp"
#include "stages/hsk_flipimage.hpp"
#include "stages/hsk_gbuffer.hpp"
#include "stages/hsk_imagetoswapchain.hpp"
#include "stages/hsk_imguistage.hpp"
#include "stages/hsk_raytracingstage.hpp"
#include <hsk_env.hpp>
#include <hsk_rtrpf.hpp>
#include <scenegraph/hsk_scenegraph.hpp>
#include <stdint.h>
#include <utility/hsk_noisesource.hpp>

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

class RestirProject : public hsk::DefaultAppBase
{
  public:
    RestirProject()  = default;
    ~RestirProject() = default;

  protected:
    virtual void Init() override;
    virtual void OnEvent(const hsk::Event* event) override;
    virtual void Update(float delta) override;

    virtual void RecordCommandBuffer(hsk::FrameRenderInfo& renderInfo) override;
    virtual void QueryResultsAvailable(uint64_t frameIndex) override;
    virtual void OnResized(VkExtent2D size) override;
    virtual void Destroy() override;
    virtual void OnShadersRecompiled(hsk::ShaderCompiler* shaderCompiler) override;


    void PrepareImguiWindow();

    std::unique_ptr<hsk::Scene> mScene;

    void loadScene();
    void LoadEnvironmentMap();
    void GenerateNoiseSource();

    /// @brief generates a GBuffer (Albedo, Positions, Normal, Motion Vectors, Mesh Instance Id as output images)
    hsk::GBufferStage mGbufferStage;
    /// @brief Renders immediate mode GUI
    hsk::ImguiStage mImguiStage;
    /// @brief Copies the intermediate rendertarget to the swapchain image
    hsk::ImageToSwapchainStage mImageToSwapchainStage;
    /// @brief Generates a raytraced image
    hsk::RestirStage mRestirStage;

    hsk::ManagedImage mSphericalEnvMap{};

    hsk::NoiseSource mNoiseSource;

    void ConfigureStages();

    std::unordered_map<std::string_view, hsk::ManagedImage*> mOutputs;
    std::string_view                                         mCurrentOutput = "";
    bool                                                     mOutputChanged = false;

#ifdef ENABLE_GBUFFER_BENCH
    hsk::BenchmarkLog mDisplayedLog;
#endif  // ENABLE_GBUFFER_BENCH

    void UpdateOutputs();
    void ApplyOutput();
};

int main(int argv, char** args)
{
    hsk::OverrideCurrentWorkingDirectory(CWD_OVERRIDE_PATH);
    RestirProject project;
    return project.Run();
}