#include "common.slang"

float airDensity(float altitude, float scaleHeight) { return exp(-max(altitude, 0.0) / scaleHeight); }
float miePhase(float mu, float g) {
    float g2 = g * g;
    return (3.0 / (8.0 * PI)) * ((1.0 - g2) * (1.0 + mu * mu)) /
           ((2.0 + g2) * pow(max(1.0 + g2 - 2.0 * g * mu, 1e-4), 1.5));
}

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    int bodyIndex = clamp(int(root.base), 0, 1);
    Instance body = root.instances[bodyIndex];
    float3 centre = body.center_radius.xyz;
    float radius = max(body.center_radius.w, 1e-4);
    bool gas = body.rotation_kind.w > .5;
    float shellRadius = radius * (gas ? 1.010 : 1.018);
    float3 ro = root.frame.camera_time.xyz;
    float3 rd = normalize(rayDirection(position.xy));
    float2 atmosphereHit = sphereHit(ro, rd, centre, shellRadius);
    float begin = max(atmosphereHit.x, 0.0);
    float end = atmosphereHit.y;
    if (end <= begin) return float4(0.0, 0.0, 0.0, 0.0);

    float2 groundHit = sphereHit(ro, rd, centre, radius);
    if (groundHit.y > 0.0) {
        float nearGround = groundHit.x > 0.0 ? groundHit.x : groundHit.y;
        end = min(end, nearGround);
    }
    for (int i = 0; i < 3; ++i) if (i != bodyIndex) {
        float4 other = root.frame.bodies[i];
        float2 hit = sphereHit(ro, rd, other.xyz, other.w);
        if (hit.x > 0.0) end = min(end, hit.x);
    }
    // Anything already drawn in front of the shell (asteroids, the moon) ends the march.
    end = min(end, sceneDistance(position.xy, rd));
    if (end <= begin) return float4(0.0, 0.0, 0.0, 0.0);

    float3 sunDirection = normalize(root.frame.sun.xyz - centre);
    float solarRadiance = max(root.frame.sun.w, 0.0);
    float mu = dot(rd, sunDirection);
    float phaseR = 3.0 * (1.0 + mu * mu) / (16.0 * PI);
    float phaseM = miePhase(mu, gas ? .62 : .72);
    float3 betaR = gas ? float3(15.0, 23.0, 34.0) : float3(15.0, 35.0, 85.0);
    float3 betaM = gas ? float3(15.0, 12.0, 10.0) : float3(9.0, 9.0, 9.0);
    float3 scatterR = gas ? float3(.88, .70, .48) : float3(.68, .86, 1.0);
    float3 scatterM = gas ? float3(1.0, .72, .43) : float3(1.0, .93, .82);
    float scaleR = gas ? .0015 : .0025;
    float scaleM = .001;
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
        float visibility = solarVisibility(p, bodyIndex);
        float2 ownHit = sphereHit(p + sunDirection * radius * 1e-5, sunDirection, centre, radius);
        if (ownHit.x > 0.0 && ownHit.y > ownHit.x) visibility = 0.0;

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
