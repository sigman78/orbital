#version 460
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(location=0) out vec4 outColor;

vec3 filteredStars(vec3 p) {
    // Integrate the original Gaussian against an approximate Gaussian pixel
    // footprint.  Stars retain their energy as the kernel widens instead of
    // becoming brighter, dimmer, or disappearing with the jitter phase.
    const float baseVariance=1.0/220.0; // exp(-r^2*110)
    vec3 px=dFdx(p),py=dFdy(p);
    float variance=baseVariance+(dot(px,px)+dot(py,py))/12.0;
    variance=min(variance,.12);
    float energyScale=baseVariance/variance; // projected kernel is two-dimensional
    vec3 first=floor(p-.5)+.5,result=vec3(0);
    for(int z=0;z<2;z++)for(int y=0;y<2;y++)for(int x=0;x<2;x++) {
        vec3 centre=first+vec3(x,y,z),cell=centre-.5,delta=p-centre;
        float id=hash(cell),r=length(delta);
        // The smooth compact support reaches zero before this eight-cell
        // neighbourhood changes, eliminating cell-boundary tail pops.
        float kernel=exp(-dot(delta,delta)/(2.0*variance))*(1.0-smoothstep(.72,.95,r));
        vec3 tint=mix(vec3(.65,.78,1.0),vec3(1.0,.83,.62),hash(cell+7.0));
        result+=tint*kernel*step(.992,id);
    }
    return result*energyScale*2.2;
}

void main() {
    vec3 rd=rayDirection(gl_FragCoord.xy);
    float galactic=pow(max(0.0,1.0-abs(dot(rd,normalize(vec3(.3,.8,.4))))),24.0);
    vec3 color=vec3(.0005,.0007,.0012)+vec3(.008,.009,.014)*galactic*(.4+fbm(rd*14.0));
    // Fixed directional cells make stars stable under camera movement.
    color+=filteredStars(rd*480.0);
    vec3 sunDir=normalize(root.frame.sun.xyz-root.frame.camera_time.xyz);
    float angle=acos(clamp(dot(rd,sunDir),-1,1));
    float disc=1.0-smoothstep(.006,.008,angle);
    color+=vec3(1.0,.85,.59)*disc*35.0;
    color+=vec3(1.0,.57,.25)*.06*exp(-angle*34.0);
    outColor=vec4(color,1);
}
