#include "scene/frame_bindings.slang"
#include "scene/view.slang"
#include "scene/visibility.slang"
#include "lib/math.slang"
#include "lib/scattering.slang"
#include "clouds.slang"

float airDensity(float altitude, float scaleHeight) { return exp(-max(altitude, 0.0) / scaleHeight); }

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    int bodyIndex = clamp(int(root.base), 0, MAX_OCCLUDING_BODIES - 1);
    Instance body = root.instances[bodyIndex];
    float3 centre = body.center_radius.xyz;
    float radius = max(body.center_radius.w, 1e-4);
    int kind = int(body.rotation_kind.w + .5);
    bool gas = kind == 1, desert = kind == 4;
    float shellRadius = radius * (gas ? 1.010 : desert ? 1.014 : 1.018);
    float3 ro = root.frame.camera_time.xyz;
    float3 rd = normalize(rayDirection(root.frame, position.xy));
    float2 atmosphereHit = sphereHit(ro, rd, centre, shellRadius);
    float begin = max(atmosphereHit.x, 0.0);
    float end = atmosphereHit.y;
    if (end <= begin) return float4(0.0, 0.0, 0.0, 0.0);

    float2 groundHit = sphereHit(ro, rd, centre, radius);
    if (groundHit.y > 0.0) {
        float nearGround = groundHit.x > 0.0 ? groundHit.x : groundHit.y;
        end = min(end, nearGround);
    }
    int bodyCount = int(root.frame.scene.x);
    for (int i = 0; i < MAX_OCCLUDING_BODIES; ++i) if (i < bodyCount && i != bodyIndex) {
        float4 other = root.frame.bodies[i];
        float2 hit = sphereHit(ro, rd, other.xyz, other.w);
        if (hit.x > 0.0) end = min(end, hit.x);
    }
    // Anything already drawn in front of the shell (asteroids, the moon) ends the march.
    end = min(end, sceneDistance(root.frame, textures[TEX_DEPTH], position.xy, rd));
    if (end <= begin) return float4(0.0, 0.0, 0.0, 0.0);

    float3 sunDirection = normalize(root.frame.sun.xyz - centre);
    float solarRadiance = max(root.frame.sun.w, 0.0);
    float mu = dot(rd, sunDirection);
    float phaseR = 3.0 * (1.0 + mu * mu) / (16.0 * PI);
    // Earth: blue Rayleigh sky. Gas giant: warm haze. Desert: a thin, dusty
    // butterscotch atmosphere dominated by forward-scattering dust.
    float phaseM = cornetteShanks(mu, gas ? .62 : desert ? .55 : .72);
    float3 betaR = gas ? float3(15.0, 23.0, 34.0) : desert ? float3(4.0, 5.0, 7.0) : float3(15.0, 35.0, 85.0);
    float3 betaM = gas ? float3(15.0, 12.0, 10.0) : desert ? float3(9.0, 6.0, 3.5) : float3(9.0, 9.0, 9.0);
    float3 scatterR = gas ? float3(.88, .70, .48) : desert ? float3(.85, .72, .58) : float3(.68, .86, 1.0);
    float3 scatterM = gas ? float3(1.0, .72, .43) : desert ? float3(1.0, .66, .40) : float3(1.0, .93, .82);
    float scaleR = gas ? .0015 : desert ? .004 : .0025;
    float scaleM = desert ? .003 : .001;
    // Earth: the air below the cloud layer is shadowed by it, so cloud shadows
    // read over the sea as darker haze the way they do from orbit. Shadows are
    // broad next to the low path a ray takes under the clouds, so one read at
    // the ray's lowest point stands for the whole of it.
    float lowShadow = 1.0;
    if (kind == 0 && root.frame.earth.z > 0.0) {
        float3x3 rotate = rotation(body.rotation_kind.xyz);
        float3 localLight = mul(transpose(rotate), sunDirection);
        float lowest = groundHit.y > 0.0 && groundHit.x > 0.0 ? groundHit.x : clamp(dot(centre - ro, rd), begin, end);
        float3 low = ro + rd * lowest;
        float altitude = max(length(low - centre) / radius - 1.0, 0.0);
        float3 localDirection = mul(transpose(rotate), normalize(low - centre));
        float2 shellUV = sphereUV(cloudShellDirection(localDirection, min(altitude, CLOUD_HEIGHT), localLight));
        float density = cloudShadowDensityLevel(cloudUV(shellUV, root.frame.camera_time.w), 2.0 + root.frame.earth.w);
        lowShadow = 1.0 - density * root.frame.earth.z;
    }
    int viewSteps = root.frame.options.z > .5 ? 20 : 12;
    int lightSteps = root.frame.options.z > .5 ? 6 : 4;
    float segment = (end - begin) / float(viewSteps);
    float ds = segment / radius;
    float3 viewOpticalDepth = float3(0.0, 0.0, 0.0);
    float3 integratedRadiance = float3(0.0, 0.0, 0.0);

    for (int i = 0; i < 20; ++i) {
        if (i >= viewSteps) break;
        float3 p = ro + rd * (begin + (float(i) + .5) * segment);
        float altitude = max(length(p - centre) / radius - 1.0, 0.0);
        float densityR = airDensity(altitude, scaleR);
        float densityM = airDensity(altitude, scaleM);
        float3 extinctionHere = betaR * densityR + betaM * densityM;
        float3 viewTransmittance = exp(-viewOpticalDepth);
        float visibility = solarVisibility(root.frame, p, bodyIndex);
        float2 ownHit = sphereHit(p + sunDirection * radius * 1e-5, sunDirection, centre, radius);
        if (ownHit.x > 0.0 && ownHit.y > ownHit.x) visibility = 0.0;
        if (altitude < CLOUD_HEIGHT) visibility *= lowShadow;

        float3 lightOpticalDepth = float3(0.0, 0.0, 0.0);
        float2 lightHit = sphereHit(p, sunDirection, centre, shellRadius);
        float lightLength = max(lightHit.y, 0.0);
        float lightDs = lightLength / (float(lightSteps) * radius);
        for (int j = 0; j < 6; ++j) {
            if (j >= lightSteps) break;
            float3 sp = p + sunDirection * ((float(j) + .5) * lightLength / float(lightSteps));
            float lightAltitude = max(length(sp - centre) / radius - 1.0, 0.0);
            lightOpticalDepth += (betaR * airDensity(lightAltitude, scaleR) +
                                  betaM * airDensity(lightAltitude, scaleM)) * lightDs;
        }
        float3 lightTransmittance = exp(-lightOpticalDepth);
        float3 source = betaR * densityR * scatterR * phaseR + betaM * densityM * scatterM * phaseM;
        integratedRadiance += viewTransmittance * lightTransmittance * source * (solarRadiance * visibility * ds);
        viewOpticalDepth += extinctionHere * ds;
    }
    float3 transmittance = exp(-viewOpticalDepth);
    float alpha = clamp(1.0 - dot(transmittance, float3(1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0)), 0.0, .94);
    if (alpha < 1e-5) return float4(0.0, 0.0, 0.0, 0.0);
    return float4(max(integratedRadiance / alpha, float3(0.0, 0.0, 0.0)), alpha);
}
