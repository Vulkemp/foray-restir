#pragma once
#include <stages/foray_renderstage.hpp>

namespace foray {
    class RestirStage : public foray::stages::Render
    {
      public:
        virtual void Init(const foray::core::VkContext* context,
                          foray::scene::Scene*          scene,
                          foray::core::ManagedImage*    envmap,
                          foray::core::ManagedImage*    noiseSource,
                          foray::stages::GBufferStage*  gbufferStage);
        
        virtual void RecordFrame(base::FrameRenderInfo& renderInfo) override;
    };
}  // namespace foray
