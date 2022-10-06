#pragma once

#include "configurepath.cmakegenerated.hpp"
#include <hsk_exception.hpp>

#include <hsk_glm.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>
#include <unordered_map>

#include <scenegraph/hsk_scenegraph.hpp>
#include <hsk_env.hpp>
#include <hsk_rtrpf.hpp>
#include <stdint.h>
#include "stages/hsk_gbuffer.hpp"
#include "stages/hsk_imguistage.hpp"
#include "stages/hsk_imagetoswapchain.hpp"
#include "stages/hsk_flipimage.hpp"
#include "stages/hsk_raytracingstage.hpp"
#include <utility/hsk_noisesource.hpp>
#include "customrtstage.hpp"

class RestirProject : public hsk::DefaultAppBase
{
public:
	RestirProject() = default;
	~RestirProject() = default;

protected:
	virtual void Init() override;
	virtual void OnEvent(const hsk::Event* event) override;
	virtual void Update(float delta) override;

	virtual void RecordCommandBuffer(hsk::FrameRenderInfo &renderInfo) override;
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
	hsk::CustomRtStage mRaytraycingStage;

	hsk::ManagedImage mSphericalEnvMap{};

	hsk::NoiseSource mNoiseSource;

	void ConfigureStages();

	std::unordered_map<std::string_view, hsk::ManagedImage *> mOutputs;
	std::string_view mCurrentOutput = "";
	bool mOutputChanged = false;

#ifdef ENABLE_GBUFFER_BENCH
	hsk::BenchmarkLog mDisplayedLog;
#endif // ENABLE_GBUFFER_BENCH

	void UpdateOutputs();
	void ApplyOutput();
};

int main(int argv, char** args)
{
	hsk::OverrideCurrentWorkingDirectory(CWD_OVERRIDE_PATH);
	RestirProject project;
	return project.Run();
}