#version 460
#extension GL_GOOGLE_include_directive : require
#define ORBITAL_FRAGMENT
#include "common.glsl"
layout(location=0) in vec3 worldPosition;
layout(location=1) in vec3 worldNormal;
layout(location=2) in vec3 localNormal;
layout(location=3) flat in uint instanceIndex;
layout(location=0) out vec4 outColor;
void main() {
    Instance inst=root.instances.data[instanceIndex]; int kind=int(inst.rotation_kind.w+.5);
    if(root.mode==3) {
        vec2 q=localNormal.xy;float rr=dot(q,q);
        float coverage=1.0-smoothstep(1.0-fwidth(rr),1.0+fwidth(rr),rr);
        vec3 n=normalize(root.frame.right_tan.xyz*q.x+root.frame.up_aspect.xyz*q.y-root.frame.forward_exposure.xyz*sqrt(max(0,1-rr)));
        vec3 l=normalize(root.frame.sun.xyz-worldPosition);
        vec3 averageRock=textureLod(sampler2D(textures[11],samplers[1]),vec2(.5),10).rgb;
        vec3 radiance=averageRock*inst.tint.xyz*root.frame.sun.w/PI*max(dot(n,l),.025)*solarVisibility(worldPosition,-1);
        outColor=vec4(radiance,coverage*inst.tint.w);return;
    }
    vec3 N=normalize(worldNormal),V=normalize(root.frame.camera_time.xyz-worldPosition),L=normalize(root.frame.sun.xyz-worldPosition);
    vec2 uv=sphereUV(normalize(localNormal));
    float visibility=solarVisibility(worldPosition,kind<3?kind:-1)*localShadow(worldPosition,N);
    float nl=max(dot(N,L),0.0);
    if(root.mode==1) {
        uv.x+=root.frame.camera_time.w*.0018;
        float density=sampleSphere(textures[5],uv).a;
        float alpha=pow(clamp(density,0,1),1.3)*.88;
        vec3 cloudLight=vec3(.002,.004,.008)+vec3(1.0,.99,.97)*pow(max(nl,0.0),.8)*visibility*root.frame.sun.w/PI*.8;
        outColor=vec4(cloudLight,alpha); return;
    }
    vec3 albedo; float rough=.8,water=0;
    if(kind==0) {
        vec4 material=sampleSphere(textures[3],uv);
        albedo=material.rgb; water=sampleSphere(textures[8],uv).r; rough=mix(.85,.085,water);
        vec3 mapN=sampleSphere(textures[7],uv).xyz*2-1;
        vec3 localT=vec3(-localNormal.z,0,localNormal.x);
        if(dot(localT,localT)<1e-8)localT=vec3(1,0,0);
        vec3 tangent=normalize(rotation(inst.rotation_kind.xyz)*normalize(localT));
        tangent=normalize(tangent-N*dot(N,tangent));
        vec3 bitangent=normalize(cross(N,tangent));
        N=normalize(tangent*mapN.x*.55+bitangent*mapN.y*.55+N*max(mapN.z,.2));
        vec2 shadowUV=sphereUV(normalize(localNormal+transpose(rotation(inst.rotation_kind.xyz))*L*.012));
        shadowUV.x+=root.frame.camera_time.w*.0018;
        float cloud=sampleSphere(textures[5],shadowUV).a;
        visibility*=1.0-pow(clamp(cloud,0,1),1.3)*.5;
    } else if(kind==1) {
        albedo=sampleSphere(textures[4],uv).rgb; rough=.95;
    } else if(kind==2) {
        albedo=sampleSphere(textures[10],uv).rgb*.8;rough=.95;
    } else {
        vec2 rockUV=uv*3.0+inst.tint.x;
        vec2 dx=dFdx(uv),dy=dFdy(uv);dx.x-=round(dx.x);dy.x-=round(dy.x);dx*=3;dy*=3;
        albedo=textureGrad(sampler2D(textures[11],samplers[3]),rockUV,dx,dy).rgb*inst.tint.xyz;
        rough=clamp(textureGrad(sampler2D(textures[13],samplers[3]),rockUV,dx,dy).r*.85+.10+(inst.tint.x-.95)*.5,.24,.98);
        vec3 mapN=textureGrad(sampler2D(textures[12],samplers[3]),rockUV,dx,dy).xyz*2-1;
        vec3 tangent=normalize(cross(N,abs(N.y)<.9?vec3(0,1,0):vec3(1,0,0)));
        N=normalize(tangent*mapN.x*.28+cross(N,tangent)*mapN.y*.28+N*max(mapN.z,.3));
    }
    vec3 normalDx=dFdx(N),normalDy=dFdy(N);
    float normalVariance=min(.20,.25*max(dot(normalDx,normalDx),dot(normalDy,normalDy)));
    rough=sqrt(min(1.0,rough*rough+normalVariance));
    nl=max(dot(N,L),0.0);
    vec3 H=normalize(L+V); float nh=max(dot(N,H),0.0),nv=max(dot(N,V),.001),vh=max(dot(V,H),0.0);
    float a=rough*rough,a2=a*a,den=nh*nh*(a2-1.0)+1.0;
    float D=a2/max(PI*den*den,1e-5),k=(rough+1.0)*(rough+1.0)/8.0;
    float G=(nl/(nl*(1-k)+k))*(nv/(nv*(1-k)+k));
    vec3 F0=vec3(mix(.04,.02,water));
    vec3 F=F0+(1-F0)*pow(1-vh,5);
    vec3 spec=F*D*G/max(4*nv*nl,.001);
    vec3 light=vec3(1.0,.99,.97)*root.frame.sun.w;
    vec3 diffuse=albedo*mix(1.0,.22,water)*(1-F0)/PI;
    vec3 color=(diffuse+spec)*light*nl*visibility;
    color+=albedo*vec3(.003,.004,.008);
    if(kind==0)color+=sampleSphere(textures[9],uv).rgb*pow(1-smoothstep(-.15,.1,dot(N,L)),2.0)*.65;
    outColor=vec4(color,1);
}
