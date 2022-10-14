#ifdef __cplusplus
#pragma once
#include <glm/glm.hpp>

namespace shader {
    using vec4 = glm::vec4;
    using uint = uint32_t;

#endif

    struct TriLight
    {
        vec4  p1;
        vec4  p2;
        vec4  p3;
        uint  materialIndex;
        float normalArea;
    };

#ifdef __cplusplus
}
#endif