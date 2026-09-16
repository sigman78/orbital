#pragma once
#include "../scene_shared.h"

// Typed views of the packed Frame. The storage stays the float4s the CPU packs
// (scene_shared.h); these name the components, and a helper that needs one
// family of settings takes its record rather than the frame.

struct ViewParams
{
    float3 position; // the camera in the scene frame (camera-relative: the origin)
    float3 right, up, forward;
    float tanHalfFov, aspect;
    float2 size; // viewport, pixels
    float2 jitter, previousJitter;
    float exposure;
    float time; // seconds
    bool highQuality, hud;
};

ViewParams viewParams(Frame* frame)
{
    ViewParams v;
    v.position = frame->camera_time.xyz;
    v.right = frame->right_tan.xyz;
    v.up = frame->up_aspect.xyz;
    v.forward = frame->forward_exposure.xyz;
    v.tanHalfFov = frame->right_tan.w;
    v.aspect = frame->up_aspect.w;
    v.size = frame->options.xy;
    v.jitter = frame->jitter.xy;
    v.previousJitter = frame->jitter.zw;
    v.exposure = frame->forward_exposure.w;
    v.time = frame->camera_time.w;
    v.highQuality = frame->options.z > .5;
    v.hud = frame->options.w > .5;
    return v;
}

struct LightParams
{
    float3 position; // the sun, scene frame
    float radiance;  // solar radiance scale
    float discRadius, limbDarkening; // angular radius in radians
    float ambientFill; // starlight fill on shadow sides
};

LightParams lightParams(Frame* frame)
{
    LightParams l;
    l.position = frame->sun.xyz;
    l.radiance = frame->sun.w;
    l.discRadius = frame->sun_disc.x;
    l.limbDarkening = frame->sun_disc.y;
    l.ambientFill = frame->quality.w;
    return l;
}

struct ScreenSun
{
    float2 position; // clip space
    bool visible;
    float size;
};

ScreenSun screenSun(Frame* frame)
{
    ScreenSun s;
    s.position = frame->screen_sun.xy;
    s.visible = frame->screen_sun.z > .5;
    s.size = frame->screen_sun.w;
    return s;
}

struct BodyGpu
{
    float3 centre; // scene frame
    float radius;
};

int bodyCount(Frame* frame) { return int(frame->scene.x); }
int giantIndex(Frame* frame) { return int(frame->scene.y); }
BodyGpu bodyAt(Frame* frame, int index)
{
    BodyGpu b;
    b.centre = frame->bodies[index].xyz;
    b.radius = frame->bodies[index].w;
    return b;
}

struct BeltParams
{
    float inner, outer, density, thickness; // giant-relative units
    float3 normal;
    float3 centre; // the giant's
    bool lightMap, extinction;
    float2 lightHalf;         // box half width and height
    float lightTexels, lightSlice; // map size, half span of the slice range along the light
};

BeltParams beltParams(Frame* frame)
{
    BeltParams b;
    b.inner = frame->belt_ring.x;
    b.outer = frame->belt_ring.y;
    b.density = frame->belt_ring.z;
    b.thickness = frame->belt_ring.w;
    b.normal = frame->belt_normal.xyz;
    b.centre = frame->bodies[int(frame->scene.y)].xyz;
    b.lightMap = frame->scene.z > .5;
    b.extinction = frame->scene.w > .5;
    b.lightHalf = frame->belt_light.xy;
    b.lightTexels = frame->belt_light.z;
    b.lightSlice = frame->belt_light.w;
    return b;
}

struct BeltDiscParams
{
    float3 right, up;
    float2 half; // extents along right and up
    float lightTexels, rockTexels;
    float weight; // far-tier blend, 0 near paths to 1 baked disc
};

BeltDiscParams beltDiscParams(Frame* frame)
{
    BeltDiscParams d;
    d.right = frame->belt_disc_right.xyz;
    d.up = frame->belt_disc_up.xyz;
    d.half = float2(frame->belt_disc_right.w, frame->belt_disc_up.w);
    d.lightTexels = frame->belt_disc.x;
    d.weight = frame->belt_disc.y;
    d.rockTexels = frame->belt_disc.z;
    return d;
}

struct DustParams
{
    bool enabled;
    float density, brightness, farScale, saturation;
    float3 tint;
};

DustParams dustParams(Frame* frame)
{
    DustParams d;
    d.enabled = frame->quality.z > .5;
    d.density = frame->dust.x;
    d.brightness = frame->dust.y;
    d.farScale = frame->dust.z;
    d.saturation = frame->dust.w;
    d.tint = frame->dust_tint.xyz;
    return d;
}

struct EarthParams
{
    float oceanRoughness, glint;
    float cloudShadow, cloudShadowSoftness; // strength, mip bias
    float windPatchiness, cloudOpacity;
};

EarthParams earthParams(Frame* frame)
{
    EarthParams e;
    e.oceanRoughness = frame->earth.x;
    e.glint = frame->earth.y;
    e.cloudShadow = frame->earth.z;
    e.cloudShadowSoftness = frame->earth.w;
    e.windPatchiness = frame->earth_more.x;
    e.cloudOpacity = frame->earth_more.y;
    return e;
}

struct GiantParams
{
    float flowTimeScale, cycle, turbulence; // x real, seconds
    bool flow;
    float hazeDepth, terminatorWrap, relief, capOpacity;
    float lightningRate, lightningBrightness; // flashes per second
    float capSize, capBlend;                  // 0 off; blend width
    float deckLift, deckShadow, streaks;      // lift in texture units at 45 degrees; 0 off
};

GiantParams giantParams(Frame* frame)
{
    GiantParams g;
    g.flowTimeScale = frame->giant.x;
    g.cycle = frame->giant.y;
    g.turbulence = frame->giant.z;
    g.flow = frame->giant.w > .5;
    g.hazeDepth = frame->giant_more.x;
    g.terminatorWrap = frame->giant_more.y;
    g.relief = frame->giant_more.z;
    g.capOpacity = frame->giant_more.w;
    g.lightningRate = frame->giant_night.x;
    g.lightningBrightness = frame->giant_night.y;
    g.capSize = frame->giant_night.z;
    g.capBlend = frame->giant_night.w;
    g.deckLift = frame->giant_layers.x;
    g.deckShadow = frame->giant_layers.y;
    g.streaks = frame->giant_layers.z;
    return g;
}

struct TemporalParams
{
    bool enabled, historyValid;
    float3 cameraDelta, previousForward;
};

TemporalParams temporalParams(Frame* frame)
{
    TemporalParams t;
    t.enabled = frame->quality.x > .5;
    t.historyValid = frame->previous_camera_delta.w > .5;
    t.cameraDelta = frame->previous_camera_delta.xyz;
    t.previousForward = frame->previous_forward.xyz;
    return t;
}

struct LensParams
{
    float glare, ghosts, starburst, starburstBlades;
    float mainRing, bigCrescent, miniCrescents, streak;
    float ghostSpread, ghostSize;
    float stackDivisor, saturation; // 0: the flare pass is skipped
    float dirtLight, dirtBlur;      // 0 off
};

LensParams lensParams(Frame* frame)
{
    LensParams l;
    l.glare = frame->lens.x;
    l.ghosts = frame->lens.y;
    l.starburst = frame->lens.z;
    l.starburstBlades = frame->lens.w;
    l.mainRing = frame->sun_disc.z;
    l.bigCrescent = frame->sun_disc.w;
    l.miniCrescents = frame->lens_more.x;
    l.streak = frame->lens_more.y;
    l.ghostSpread = frame->lens_more.z;
    l.ghostSize = frame->lens_more.w;
    l.stackDivisor = frame->lens_stack.x;
    l.saturation = frame->lens_stack.y;
    l.dirtLight = frame->lens_stack.z;
    l.dirtBlur = frame->lens_stack.w;
    return l;
}

struct PostParams
{
    float bloom, bloomThreshold, bloomKnee; // intensity 0 off
    float aberration;
    float vignette, grain, blackOffset;
    float flareAdaptation; // how far the flare follows the auto exposure, 0 to 1
    int toneCurve; // 0 ACES filmic, 1 AgX, 2 PBR Neutral
};

PostParams postParams(Frame* frame)
{
    PostParams p;
    p.bloom = frame->post.x;
    p.bloomThreshold = frame->post.y;
    p.bloomKnee = frame->post.z;
    p.aberration = frame->post.w;
    p.vignette = frame->post_more.x;
    p.grain = frame->post_more.y;
    p.blackOffset = frame->post_more.z;
    p.flareAdaptation = frame->post_more.w;
    p.toneCurve = int(frame->quality.y);
    return p;
}

struct DisplayParams
{
    int hdr; // 0 off, 1 scRGB, 2 HDR10
    float paperWhite, headroom; // nits; peak over paper white
    float autoExposure;         // the adaptation's multiplier alone
};

DisplayParams displayParams(Frame* frame)
{
    DisplayParams d;
    d.hdr = int(frame->display.x);
    d.paperWhite = frame->display.y;
    d.headroom = frame->display.z;
    d.autoExposure = frame->display.w;
    return d;
}

struct SkyParams
{
    bool catalogue;
    float starBrightness, starSaturation, milkyWay; // 0 off; 0 the procedural band
    float dustAmplitude, featureSize;                // 0 off; degrees
    int galaxyMode;                                  // 0 splats, 1 layers, 2 original
    float lowGain, cloudGain, filamentGain;
    float dustLacunarity, dustGain, divisor, bandContrast;
};

SkyParams skyParams(Frame* frame)
{
    SkyParams s;
    s.catalogue = frame->stars.x > .5;
    s.starBrightness = frame->stars.y;
    s.starSaturation = frame->stars.z;
    s.milkyWay = frame->stars.w;
    s.dustAmplitude = frame->galaxy.y;
    s.featureSize = frame->galaxy.z;
    s.galaxyMode = int(frame->galaxy.w);
    s.lowGain = frame->galaxy_layers.x;
    s.cloudGain = frame->galaxy_layers.y;
    s.filamentGain = frame->galaxy_layers.z;
    s.dustLacunarity = frame->galaxy_more.x;
    s.dustGain = frame->galaxy_more.y;
    s.divisor = frame->galaxy_more.z;
    s.bandContrast = frame->galaxy_more.w;
    return s;
}

struct MoteParams
{
    float3 latticeOffset, cell; // the camera's offset within its cell, and the cell index
    float cellSize;
    float streaks; // motion streak intensity, 0 off
};

MoteParams moteParams(Frame* frame)
{
    MoteParams m;
    m.latticeOffset = frame->camera_lattice.xyz;
    m.cellSize = frame->camera_lattice.w;
    m.cell = frame->camera_cell.xyz;
    m.streaks = frame->camera_cell.w;
    return m;
}

// The matrices stay out of the records: a record is decoded per fragment or per
// rock, and an unused matrix load is not always removed.
float4x4 viewProjection(Frame* frame) { return frame->view_projection; }
float4x4 previousProjection(Frame* frame) { return frame->previous_projection; }
float4x4 shadowProjection(Frame* frame) { return frame->light_projection; }
float4x4 beltLightProjection(Frame* frame) { return frame->belt_light_projection; }
float4x4 beltDiscProjection(Frame* frame) { return frame->belt_disc_projection; }
