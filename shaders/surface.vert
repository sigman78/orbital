#version 460
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(location=0) out vec3 worldPosition;
layout(location=1) out vec3 worldNormal;
layout(location=2) out vec3 localNormal;
layout(location=3) flat out uint instanceIndex;
void main() {
    instanceIndex=root.base+gl_InstanceIndex;
    Instance inst=root.instances.data[instanceIndex];
    if(root.mode==3) {
        const vec2 corners[6]=vec2[6](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));
        vec2 q=corners[gl_VertexIndex];
        worldPosition=inst.center_radius.xyz+(root.frame.right_tan.xyz*q.x+root.frame.up_aspect.xyz*q.y)*inst.center_radius.w;
        worldNormal=-root.frame.forward_exposure.xyz;localNormal=vec3(q,0);
        gl_Position=root.frame.view_projection*vec4(worldPosition,1);return;
    }
    Vertex v=root.vertices.data[gl_VertexIndex];
    mat3 rot=rotation(inst.rotation_kind.xyz);
    float scale=inst.center_radius.w*(root.mode==1?1.009:1.0);
    worldPosition=inst.center_radius.xyz+rot*v.position.xyz*scale;
    worldNormal=normalize(rot*v.normal.xyz);
    localNormal=normalize(v.position.xyz);
    gl_Position=(root.mode==2?root.frame.light_projection:root.frame.view_projection)*vec4(worldPosition,1);
}
