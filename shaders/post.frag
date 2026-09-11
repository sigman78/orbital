#version 460
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(location=0) out vec4 outColor;
vec3 sampleHDR(vec2 uv) { return texture(sampler2D(textures[16],samplers[0]),uv).rgb; }
vec3 aces(vec3 x) { return clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0,1); }
void main() {
    vec2 resolution=root.frame.options.xy;
    if(root.mode==4) {
        vec2 uv=gl_FragCoord.xy/16.0;
        vec3 c=sampleHDR(uv);float lum=dot(c,vec3(.2126,.7152,.0722));
        outColor=vec4(log(max(lum,.0001)),lum,0,1);return;
    }
    if(root.mode==3) { outColor=texture(sampler2D(textures[6],samplers[0]),gl_FragCoord.xy/resolution); return; }
    if(root.mode==1||root.mode==2) {
        vec2 uv=gl_FragCoord.xy/max(floor(resolution*.25),vec2(1)); vec3 sum=vec3(0);
        for(int i=-4;i<=4;i++) {
            vec2 delta=(root.mode==1?vec2(i,0):vec2(0,i))*4.0/resolution;
            vec3 c=root.mode==1?sampleHDR(uv+delta):texture(sampler2D(textures[1],samplers[0]),uv+delta).rgb;
            if(root.mode==1) c=max(c-vec3(.85),0);
            sum+=c*exp(-float(i*i)/8.0);
        }
        outColor=vec4(sum/4.898,1); return;
    }
    vec2 uv=gl_FragCoord.xy/resolution;
    vec3 color=sampleHDR(uv)+texture(sampler2D(textures[2],samplers[0]),uv).rgb*.24;
    vec2 p=uv*2-1,sun=root.frame.screen_sun.xy;
    float vis=root.frame.screen_sun.z;
    vec2 aspect=vec2(resolution.x/resolution.y,1);
    float dist=length((p-sun)*aspect);
    color+=vec3(1,.6,.3)*vis*.014/(dist*dist+.012);
    for(int i=0;i<4;i++) {
        vec2 center=-sun*(.25+float(i)*.35);
        float r=length((p-center)*aspect),radius=.035+float(i)*.018;
        color+=vec3(.17,.23,.31)*vis*.1*exp(-pow((r-radius)*90,2));
    }
    color=aces(color*root.frame.forward_exposure.w);
    color*=1-.17*pow(length(p*.65),2.0);
    float dither=(hash(vec3(gl_FragCoord.xy,0))-.5)/255.0;
    if(root.frame.options.w>.5) {
        vec2 hp=(gl_FragCoord.xy-vec2(32,26))/vec2(1024,256);
        if(all(greaterThanEqual(hp,vec2(0)))&&all(lessThan(hp,vec2(1)))) {
            float a=texture(sampler2D(textures[14],samplers[0]),hp).a;
            color=mix(color,vec3(.68,.75,.81),a*.8);
        }
    }
    vec3 encoded=mix(12.92*color,1.055*pow(max(color,0),vec3(1.0/2.4))-.055,step(vec3(.0031308),color));
    encoded=clamp(encoded+dither,0,1);
    vec3 decoded=mix(encoded/12.92,pow((encoded+.055)/1.055,vec3(2.4)),step(vec3(.04045),encoded));
    outColor=vec4(decoded,1); // sRGB swapchain performs the final transfer.
}
