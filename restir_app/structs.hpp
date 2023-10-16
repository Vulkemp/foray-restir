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
        vec4  normal;
        uint  materialIndex;
        uint  reserved1;
        uint  reserved2;
        uint  reserved3;
    };

#ifdef __cplusplus
}
#endif