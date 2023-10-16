#ifndef VISIPAYLOAD_GLSL
#define VISIPAYLOAD_GLSL

struct VisibilityPayload
{
    bool Hit;
};

#ifdef VISIPAYLOAD_OUT
layout(location = 2) rayPayloadEXT VisibilityPayload VisiPayload;
#endif // VISIPAYLOAD_OUT
#ifdef VISIPAYLOAD_IN
layout(location = 3) rayPayloadInEXT VisibilityPayload ReturnPayload;
#endif // VISIPAYLOAD_IN

#endif // VISIPAYLOAD_GLSL