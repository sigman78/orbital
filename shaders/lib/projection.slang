#pragma once
// Projection limits shared by view reconstruction and distant geometry.
static const float NEAR_Z = .02;
static const float FAR_Z = 2000.0;

float linearViewDepth(float d) { return (NEAR_Z * FAR_Z) / max(FAR_Z - d * (FAR_Z - NEAR_Z), 1e-5); }
