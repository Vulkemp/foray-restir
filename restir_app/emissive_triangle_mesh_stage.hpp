#pragma once
#include "structs.hpp"
#include <foray_api.hpp>
#include <foray_vulkan.hpp>
#include <scene/foray_scene.hpp>

class EmissiveTriangleMeshStage : public foray::stages::RasterizedRenderStage
{
  public:
    // overrides
    virtual void RecordFrame(VkCommandBuffer cmdBuffer, foray::base::FrameRenderInfo& renderInfo) override;
    virtual void Destroy() override;
    virtual void Resize(const VkExtent2D& extent) override{};
    virtual void OnShadersRecompiled(const std::unordered_set<uint64_t>& recompiled) override{};
    virtual ~EmissiveTriangleMeshStage() {}

    virtual void SetupDescriptors() override;
    virtual void CreateDescriptorSets() override;
    virtual void UpdateDescriptors() override{};
    virtual void CreatePipelineLayout() override;

    void Init(foray::core::Context* context, std::vector<shader::TriLight>* triangles, foray::core::ManagedImage* depth, foray::core::ManagedImage* output, foray::scene::Scene* scene)
    {
        mContext   = context;
        mTriangles = triangles;
        mDepthImage = depth;
		mOutput = output;
        mScene = scene;

		CreateTriangleVertexBuffer();
        SetupDescriptors();
		CreateDescriptorSets();
		CreatePipelineLayout();
		PrepareRenderpass();
        CreateShaders();
        CreatePipeline();
    }

    // individual
    void       CreatePipeline();
    void       CreateShaders();

    static inline const std::string VERT_FILE = "shaders/emissive_triangle_mesh/etm.vert";
    static inline const std::string FRAG_FILE = "shaders/emissive_triangle_mesh/etm.frag";

    foray::core::ShaderModule mShaderModuleVert;
    foray::core::ShaderModule mShaderModuleFrag;

    std::vector<glm::vec3>         mTriangleVertices;
    std::vector<shader::TriLight>* mTriangles;
    foray::core::ManagedBuffer     mVertexBuffer;
    void                           CreateTriangleVertexBuffer();

	foray::core::ManagedImage* mDepthImage;
    foray::core::ManagedImage* mOutput;
    foray::scene::Scene*              mScene;

	void PrepareRenderpass();


  protected:
    /// @brief Override this to reload all shaders and rebuild pipelines after a registered shader has been recompiled.
    virtual void ReloadShaders() override{};
};