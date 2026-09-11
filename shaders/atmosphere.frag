#version 460
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"

layout(location = 0) out vec4 outColor;

// Single-scattering atmosphere in body-radius units. Keeping distances and
// coefficients normalized by radius makes the appearance invariant when the
// artistically compressed scene radii change.
float airDensity(float altitude, float scaleHeight) {
    return exp(-max(altitude, 0.0) / scaleHeight);
}

float miePhase(float mu, float g) {
    float g2 = g * g;
    return (3.0 / (8.0 * PI)) * ((1.0 - g2) * (1.0 + mu * mu)) /
           ((2.0 + g2) * pow(max(1.0 + g2 - 2.0 * g * mu, 1e-4), 1.5));
}

void main() {
    // Renderer submits gas (base 1) then Earth (base 0), far to near.
    int bodyIndex = clamp(int(root.base), 0, 1);
    Instance body = root.instances.data[bodyIndex];
    vec3 centre = body.center_radius.xyz;
    float radius = max(body.center_radius.w, 1e-4);
    bool gas = body.rotation_kind.w > 0.5;
    float shellRadius = radius * (gas ? 1.010 : 1.018);

    vec3 ro = root.frame.camera_time.xyz;
    vec3 rd = normalize(rayDirection(gl_FragCoord.xy));
    vec2 atmosphereHit = sphereHit(ro, rd, centre, shellRadius);
    float begin = max(atmosphereHit.x, 0.0);
    float end = atmosphereHit.y;
    if (end <= begin) { outColor = vec4(0.0); return; }

    // Stop integration at the opaque surface. Depth testing is intentionally
    // not required, so the same pass remains correct for a camera in the shell.
    vec2 groundHit = sphereHit(ro, rd, centre, radius);
    if (groundHit.y > 0.0) {
        float nearGround = groundHit.x > 0.0 ? groundHit.x : groundHit.y;
        end = min(end, nearGround);
    }
    // Avoid drawing the shell over a closer body when discs overlap.
    for (int i = 0; i < 3; ++i) if (i != bodyIndex) {
        vec4 other = root.frame.bodies[i];
        vec2 hit = sphereHit(ro, rd, other.xyz, other.w);
        if (hit.x > 0.0) end = min(end, hit.x);
    }
    if (end <= begin) { outColor = vec4(0.0); return; }

    vec3 sunDirection = normalize(root.frame.sun.xyz - centre);
    float solarRadiance = max(root.frame.sun.w, 0.0);
    float mu = dot(rd, sunDirection);
    float phaseR = 3.0 * (1.0 + mu * mu) / (16.0 * PI);
    float phaseM = miePhase(mu, gas ? 0.62 : 0.72);

    // Coefficients are sea-level optical depth per body radius. Rayleigh's
    // wavelength dependence makes a saturated blue grazing path; nearly white
    // Mie scattering keeps daylight air neutral and leaves a warm terminator
    // after blue light is preferentially extinguished on the solar path.
    vec3 betaR = gas ? vec3(15, 23, 34) : vec3(15, 35, 85);
    vec3 betaM = gas ? vec3(15, 12, 10) : vec3(9);
    vec3 scatterR = gas ? vec3(.88, .70, .48) : vec3(.68, .86, 1.00);
    vec3 scatterM = gas ? vec3(1.00, .72, .43) : vec3(1.00, .93, .82);
    float scaleR = gas ? .0015 : .0025;
    float scaleM = gas ? .001 : .001;

    int viewSteps = root.frame.options.z > 0.5 ? 20 : 12;
    int lightSteps = root.frame.options.z > 0.5 ? 6 : 4;
    float segment = (end - begin) / float(viewSteps);
    float ds = segment / radius;
    vec3 viewOpticalDepth = vec3(0.0);
    vec3 integratedRadiance = vec3(0.0);

    for (int i = 0; i < 20; ++i) {
        if (i >= viewSteps) break;
        vec3 p = ro + rd * (begin + (float(i) + .5) * segment);
        float altitude = max(length(p - centre) / radius - 1.0, 0.0);
        float densityR = airDensity(altitude, scaleR);
        float densityM = airDensity(altitude, scaleM);

        vec3 extinctionHere = betaR * densityR + betaM * densityM;
        vec3 viewTransmittance = exp(-viewOpticalDepth);

        float visibility = solarVisibility(p, bodyIndex);
        // Explicit own-body shadow gives a genuinely dark nightside rather than
        // an atmosphere texture glow. Bias scales with the current body.
        vec2 ownHit = sphereHit(p + sunDirection * radius * 1e-5,
                                sunDirection, centre, radius);
        if (ownHit.x > 0.0 && ownHit.y > ownHit.x) visibility = 0.0;

        vec3 lightOpticalDepth = vec3(0.0);
        vec2 lightHit = sphereHit(p, sunDirection, centre, shellRadius);
        float lightLength = max(lightHit.y, 0.0);
        float lightDs = lightLength / (float(lightSteps) * radius);
        for (int j = 0; j < 6; ++j) {
            if (j >= lightSteps) break;
            vec3 sp = p + sunDirection * ((float(j) + .5) * lightLength / float(lightSteps));
            float lightAltitude = max(length(sp - centre) / radius - 1.0, 0.0);
            lightOpticalDepth += (betaR * airDensity(lightAltitude, scaleR) +
                                  betaM * airDensity(lightAltitude, scaleM)) * lightDs;
        }
        vec3 lightTransmittance = exp(-lightOpticalDepth);
        vec3 source = betaR * densityR * scatterR * phaseR +
                      betaM * densityM * scatterM * phaseM;
        integratedRadiance += viewTransmittance * lightTransmittance * source *
                              (solarRadiance * visibility * ds);
        viewOpticalDepth += extinctionHere * ds;
    }

    vec3 transmittance = exp(-viewOpticalDepth);
    float alpha = clamp(1.0 - dot(transmittance, vec3(1.0 / 3.0)), 0.0, .94);
    if (alpha < 1e-5) { outColor = vec4(0.0); return; }
    // The pipeline uses conventional source-alpha blending. Divide the physical
    // premultiplied in-scattered radiance to supply unpremultiplied RGB.
    outColor = vec4(max(integratedRadiance / alpha, vec3(0.0)), alpha);
}
