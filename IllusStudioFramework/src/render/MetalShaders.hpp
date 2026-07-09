//
//  MetalShaders.hpp
//  IllusStudioFramework — runtime Metal source (T1-3 / T1-4)
//

#pragma once

namespace illus::metal_shaders {

// Dab stamp into RGBA8 texture (read-modify-write). Matches StrokeRasterizer coverage curve.
inline constexpr const char* kLibrarySource = R"METAL(
#include <metal_stdlib>
using namespace metal;

struct DabUniforms {
    float2 center;
    float radius;
    float hardness;
    float4 color;   // rgb 0..1, a = brush alpha 0..1
    int erase;      // 0 paint, 1 dest-out
    int width;
    int height;
    int originX;
    int originY;
    int pad0;
    int pad1;
    int pad2;
};

float coverageAt(float dist, float radius, float hardness) {
    if (radius <= 0.0f || dist >= radius) return 0.0f;
    float t = dist / radius;
    float softStart = clamp(1.0f - hardness, 0.05f, 1.0f);
    if (t <= softStart) return 1.0f;
    float u = (t - softStart) / (1.0f - softStart);
    return 1.0f - (u * u * (3.0f - 2.0f * u));
}

kernel void stampDab(
    texture2d<float, access::read_write> tex [[texture(0)]],
    constant DabUniforms& u [[buffer(0)]],
    uint2 gid [[thread_position_in_grid]]
) {
    int2 pix = int2(gid) + int2(u.originX, u.originY);
    if (pix.x < 0 || pix.y < 0 || pix.x >= u.width || pix.y >= u.height) return;
    float2 p = float2(pix) + 0.5f;
    float2 d = p - u.center;
    float dist2 = dot(d, d);
    float r2 = u.radius * u.radius;
    if (dist2 > r2) return;
    float cov = coverageAt(sqrt(dist2), u.radius, u.hardness);
    if (cov <= 0.0f) return;
    float sa = clamp(cov * u.color.a, 0.0f, 1.0f);
    if (sa <= 0.0f) return;

    uint2 upix = uint2(pix);
    float4 dst = tex.read(upix);
    if (u.erase != 0) {
        float inv = 1.0f - sa;
        dst.rgb *= inv;
        dst.a *= inv;
    } else {
        float inv = 1.0f - sa;
        dst.rgb = u.color.rgb * sa + dst.rgb * inv;
        dst.a = sa + dst.a * inv;
    }
    tex.write(dst, upix);
}

struct CompositeUniforms {
    float opacity;
};

struct VertexOut {
    float4 position [[position]];
    float2 uv;
};

vertex VertexOut compositeVertex(uint vid [[vertex_id]]) {
    float2 pos;
    if (vid == 0) pos = float2(-1.0, -1.0);
    else if (vid == 1) pos = float2( 3.0, -1.0);
    else pos = float2(-1.0,  3.0);
    VertexOut out;
    out.position = float4(pos, 0.0, 1.0);
    out.uv = float2(pos.x * 0.5 + 0.5, 1.0 - (pos.y * 0.5 + 0.5));
    return out;
}

fragment float4 compositeFragment(
    VertexOut in [[stage_in]],
    texture2d<float> src [[texture(0)]],
    sampler samp [[sampler(0)]],
    constant CompositeUniforms& u [[buffer(0)]]
) {
    float4 c = src.sample(samp, in.uv);
    c.a *= u.opacity;
    c.rgb *= u.opacity;
    return c;
}
)METAL";

} // namespace illus::metal_shaders
