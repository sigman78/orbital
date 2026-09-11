#ifndef ORBITAL_COMMON
#define ORBITAL_COMMON
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
struct Instance { vec4 center_radius; vec4 rotation_kind; vec4 tint; };
struct Vertex { vec4 position; vec4 normal; };
layout(buffer_reference, std430, buffer_reference_align=16) readonly buffer FrameRef {
    mat4 view_projection;
    vec4 camera_time, right_tan, up_aspect, forward_exposure, sun;
    vec4 bodies[3];
    vec4 options, screen_sun;
    mat4 light_projection;
    mat4 previous_projection;
    vec4 previous_camera_delta, previous_forward, jitter;
};
layout(buffer_reference, std430, buffer_reference_align=16) readonly buffer VertexRef { Vertex data[]; };
layout(buffer_reference, std430, buffer_reference_align=16) readonly buffer InstanceRef { Instance data[]; };
layout(push_constant, std430) uniform Push { FrameRef frame; VertexRef vertices; InstanceRef instances; uint base; uint mode; } root;
layout(set=0,binding=0) uniform texture2D textures[24];
layout(set=0,binding=1) uniform sampler samplers[4];
const float PI=3.141592653589793;
vec3 rayDirection(vec2 pixel) {
    vec2 xy=(pixel-root.frame.jitter.xy)/root.frame.options.xy*2.0-1.0;
    return normalize(root.frame.forward_exposure.xyz + root.frame.right_tan.xyz*xy.x*root.frame.right_tan.w*root.frame.up_aspect.w - root.frame.up_aspect.xyz*xy.y*root.frame.right_tan.w);
}
vec2 sphereHit(vec3 ro,vec3 rd,vec3 center,float radius) {
    vec3 oc=ro-center; float b=dot(oc,rd),d=b*b-dot(oc,oc)+radius*radius;
    if(d<0.0) return vec2(1e20,-1e20);
    d=sqrt(d); return vec2(-b-d,-b+d);
}
float solarVisibility(vec3 point,int skip) {
    vec3 light=normalize(root.frame.sun.xyz-point); float visibility=1.0;
    for(int i=0;i<3;i++) if(i!=skip) {
        vec3 q=root.frame.bodies[i].xyz-point; float t=dot(q,light);
        if(t>0.0) {
            float distanceToRay=length(q-light*t),r=root.frame.bodies[i].w;
            float penumbra=max(t*.007,0.015);
            visibility*=smoothstep(r-penumbra,r+penumbra,distanceToRay);
        }
    }
    return visibility;
}
float localShadow(vec3 point,vec3 normal) {
    vec4 light=root.frame.light_projection*vec4(point+normal*.015,1);
    vec3 coord=light.xyz/light.w;vec2 uv=coord.xy*.5+.5;
    if(any(lessThan(uv,vec2(.001)))||any(greaterThan(uv,vec2(.999)))||coord.z<0||coord.z>1)return 1;
    float s=0;vec2 texel=1.0/vec2(textureSize(sampler2DShadow(textures[15],samplers[2]),0));
    for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++)s+=texture(sampler2DShadow(textures[15],samplers[2]),vec3(uv+vec2(x,y)*texel,coord.z-.0003));
    return s/9.0;
}
mat3 rotation(vec3 a) {
    vec3 c=cos(a),s=sin(a);
    return mat3(c.z,s.z,0,-s.z,c.z,0,0,0,1)*mat3(c.y,0,-s.y,0,1,0,s.y,0,c.y)*mat3(1,0,0,0,c.x,s.x,0,-s.x,c.x);
}
vec2 sphereUV(vec3 n) { return vec2(atan(n.z,n.x)/(2.0*PI)+0.5,0.5-asin(clamp(n.y,-1.0,1.0))/PI); }
#ifdef ORBITAL_FRAGMENT
vec4 sampleSphere(texture2D tex,vec2 uv) {
    vec2 dx=dFdx(uv),dy=dFdy(uv);
    // atan's discontinuity is a coordinate wrap, not a one-texture footprint.
    dx.x-=round(dx.x);dy.x-=round(dy.x);
    return textureGrad(sampler2D(tex,samplers[1]),uv,dx,dy);
}
#endif
float hash(vec3 p) { p=fract(p*.1031); p+=dot(p,p.yzx+33.33); return fract((p.x+p.y)*p.z); }
float noise(vec3 p) {
    vec3 i=floor(p),f=fract(p); f=f*f*(3.0-2.0*f);
    return mix(mix(mix(hash(i),hash(i+vec3(1,0,0)),f.x),mix(hash(i+vec3(0,1,0)),hash(i+vec3(1,1,0)),f.x),f.y),mix(mix(hash(i+vec3(0,0,1)),hash(i+vec3(1,0,1)),f.x),mix(hash(i+vec3(0,1,1)),hash(i+vec3(1,1,1)),f.x),f.y),f.z);
}
float fbm(vec3 p) { float n=0,a=.5; for(int i=0;i<4;i++){n+=a*noise(p);p=p*2.03+vec3(2.1,1.7,4.3);a*=.5;} return n; }
#endif
