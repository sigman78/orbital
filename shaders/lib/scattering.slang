#pragma once
#include "math.slang"

// Phase functions normalized over the sphere; isotropic is 1 / (4 pi).
float henyeyGreenstein(float mu, float g)
{
    float d = 1.0 + g * g - 2.0 * g * mu;
    return (1.0 - g * g) / (4.0 * PI * d * sqrt(d));
}

float cornetteShanks(float mu, float g)
{
    float g2 = g * g;
    return (3.0 / (8.0 * PI)) * ((1.0 - g2) * (1.0 + mu * mu)) /
           ((2.0 + g2) * pow(max(1.0 + g2 - 2.0 * g * mu, 1e-4), 1.5));
}
