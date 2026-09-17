#pragma once
// Projection limits shared by view reconstruction and distant geometry. Depth
// is reversed: near lands at 1 and far at 0, the background cleared to 0.
static const float NEAR_Z = .002;
static const float FAR_Z = 2000.0;

float linearViewDepth(float d) { return (NEAR_Z * FAR_Z) / max(NEAR_Z + d * (FAR_Z - NEAR_Z), 1e-9); }

// Cleared background rather than geometry; anything past 1,818 units also lands here.
bool isBackgroundDepth(float d) { return d <= 1e-7; }
