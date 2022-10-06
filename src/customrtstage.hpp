#pragma once
#include <stages/hsk_raytracingstage.hpp>

namespace hsk
{
    class CustomRtStage : public RaytracingStage
    {
    public:
        virtual void Init(const VkContext *context, Scene *scene, ManagedImage *envmap, ManagedImage *noiseSource);
        virtual void CreateRaytraycingPipeline() override;
        virtual void OnShadersRecompiled(ShaderCompiler* shaderCompiler) override;

        virtual void Destroy() override;
        virtual void DestroyShaders() override;

        struct RtStageShader
        {
            std::string Path = "";
            ShaderModule Module;

            void Create(const VkContext *context);
            void Destroy();
        };

    protected:
        RtStageShader mRaygen{"shaders/raygen.rgen.spv"};
        RtStageShader mDefault_AnyHit{"shaders/ray-default/anyhit.rahit.spv"};
        RtStageShader mDefault_ClosestHit{"shaders/ray-default/closesthit.rchit.spv"};
        RtStageShader mDefault_Miss{"shaders/ray-default/miss.rmiss.spv"};
    };
} // namespace hsk
