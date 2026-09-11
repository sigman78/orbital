#include "common.slang"
struct SurfaceVaryings {
    float4 position : SV_Position;
    [[vk::location(0)]] float3 worldPosition : WORLD_POSITION;
    [[vk::location(1)]] float3 worldNormal : WORLD_NORMAL;
    [[vk::location(2)]] float3 localNormal : LOCAL_NORMAL;
    [[vk::location(3)]] nointerpolation uint instanceIndex : INSTANCE_INDEX;
};

[shader("vertex")]
SurfaceVaryings vertexMain(uint vertexIndex : SV_VertexID, uint instanceId : SV_InstanceID)
{
    SurfaceVaryings output;
    output.instanceIndex = root.base + instanceId;
    Instance inst = root.instances[output.instanceIndex];
    if (root.mode == 3)
    {
        const float2 corners[6] = {float2(-1,-1),float2(1,-1),float2(1,1),float2(-1,-1),float2(1,1),float2(-1,1)};
        float2 q = corners[vertexIndex];
        output.worldPosition = inst.center_radius.xyz + (root.frame->right_tan.xyz*q.x + root.frame->up_aspect.xyz*q.y)*inst.center_radius.w;
        output.worldNormal = -root.frame->forward_exposure.xyz; output.localNormal = float3(q,0);
        output.position = mul(root.frame->view_projection, float4(output.worldPosition,1));
        return output;
    }
    Vertex vertex = root.vertices[vertexIndex];
    float3x3 rotate = rotation(inst.rotation_kind.xyz);
    float scale = inst.center_radius.w * (root.mode == 1 ? 1.009 : 1.0);
    output.worldPosition = inst.center_radius.xyz + mul(rotate, vertex.position.xyz) * scale;
    output.worldNormal = normalize(mul(rotate, vertex.normal.xyz)); output.localNormal = normalize(vertex.position.xyz);
    output.position = mul(root.mode == 2 ? root.frame->light_projection : root.frame->view_projection, float4(output.worldPosition, 1.0));
    return output;
}

[shader("fragment")]
float4 fragmentMain(SurfaceVaryings input) : SV_Target0
{
    Instance inst=root.instances[input.instanceIndex]; int kind=int(inst.rotation_kind.w+.5);
    if(root.mode==3) {
        float2 q=input.localNormal.xy; float rr=dot(q,q); float coverage=1.0-smoothstep(1.0-fwidth(rr),1.0+fwidth(rr),rr);
        float3 n=normalize(root.frame->right_tan.xyz*q.x+root.frame->up_aspect.xyz*q.y-root.frame->forward_exposure.xyz*sqrt(max(0,1-rr)));
        float3 l=normalize(root.frame->sun.xyz-input.worldPosition); float3 averageRock=textures[11].SampleLevel(samplers[1],float2(.5),10).rgb;
        float3 radiance=averageRock*inst.tint.xyz*root.frame->sun.w/PI*max(dot(n,l),.025)*solarVisibility(input.worldPosition,-1);
        return float4(radiance,coverage*inst.tint.w);
    }
    float3 N=normalize(input.worldNormal),V=normalize(root.frame->camera_time.xyz-input.worldPosition),L=normalize(root.frame->sun.xyz-input.worldPosition);
    float2 uv=sphereUV(normalize(input.localNormal)); float visibility=solarVisibility(input.worldPosition,kind<3?kind:-1)*localShadow(input.worldPosition,N);
    float nl=max(dot(N,L),0.0);
    if(root.mode==1) {
        uv.x+=root.frame->camera_time.w*.0018; float density=sampleSphere(textures[5],uv).a; float alpha=pow(clamp(density,0,1),1.3)*.88;
        float3 cloudLight=float3(.002,.004,.008)+float3(1.0,.99,.97)*pow(max(nl,0.0),.8)*visibility*root.frame->sun.w/PI*.8;
        return float4(cloudLight,alpha);
    }
    float3 albedo; float rough=.8,water=0;
    if(kind==0) {
        float4 material=sampleSphere(textures[3],uv); albedo=material.rgb; water=sampleSphere(textures[8],uv).r; rough=lerp(.85,.085,water);
        float3 mapN=sampleSphere(textures[7],uv).xyz*2-1; float3 localT=float3(-input.localNormal.z,0,input.localNormal.x);
        if(dot(localT,localT)<1e-8)localT=float3(1,0,0); float3 tangent=normalize(mul(rotation(inst.rotation_kind.xyz),normalize(localT)));
        tangent=normalize(tangent-N*dot(N,tangent)); float3 bitangent=normalize(cross(N,tangent));
        N=normalize(tangent*mapN.x*.55+bitangent*mapN.y*.55+N*max(mapN.z,.2));
        float2 shadowUV=sphereUV(normalize(input.localNormal+mul(transpose(rotation(inst.rotation_kind.xyz)),L)*.012));
        shadowUV.x+=root.frame->camera_time.w*.0018; float cloud=sampleSphere(textures[5],shadowUV).a; visibility*=1.0-pow(clamp(cloud,0,1),1.3)*.5;
    } else if(kind==1) { albedo=sampleSphere(textures[4],uv).rgb; rough=.95;
    } else if(kind==2) { albedo=sampleSphere(textures[10],uv).rgb*.8;rough=.95;
    } else {
        float2 rockUV=uv*3.0+inst.tint.x, dx=ddx(uv), dy=ddy(uv);
        dx.x-=round(dx.x);dy.x-=round(dy.x);dx*=3;dy*=3;
        albedo=textures[11].SampleGrad(samplers[3],rockUV,dx,dy).rgb*inst.tint.xyz;
        // Scanned microsurface roughness plus stable body-to-body variation.
        // Rock stays dielectric; smoother patches produce broad glints, not metal.
        rough=clamp(textures[13].SampleGrad(samplers[3],rockUV,dx,dy).r*.85+.10+(inst.tint.x-.95)*.5,.24,.98);
        float3 mapN=textures[12].SampleGrad(samplers[3],rockUV,dx,dy).xyz*2-1;
        float3 tangent=normalize(cross(N,abs(N.y)<.9?float3(0,1,0):float3(1,0,0)));
        N=normalize(tangent*mapN.x*.28+cross(N,tangent)*mapN.y*.28+N*max(mapN.z,.3));
    }
    // Normal variance broadens unresolved highlights instead of sparkling.
    float3 normalDx=ddx(N),normalDy=ddy(N);
    float normalVariance=min(.20,.25*max(dot(normalDx,normalDx),dot(normalDy,normalDy)));
    rough=sqrt(min(1.0,rough*rough+normalVariance));
    nl=max(dot(N,L),0.0); float3 H=normalize(L+V); float nh=max(dot(N,H),0.0),nv=max(dot(N,V),.001),vh=max(dot(V,H),0.0);
    float a=rough*rough,a2=a*a,den=nh*nh*(a2-1.0)+1.0; float D=a2/max(PI*den*den,1e-5),k=(rough+1.0)*(rough+1.0)/8.0;
    float G=(nl/(nl*(1-k)+k))*(nv/(nv*(1-k)+k)); float3 F0=float3(lerp(.04,.02,water)); float3 F=F0+(1-F0)*pow(1-vh,5);
    float3 spec=F*D*G/max(4*nv*nl,.001); float3 light=float3(1.0,.99,.97)*root.frame->sun.w;
    float3 diffuse=albedo*lerp(1.0,.22,water)*(1-F0)/PI; float3 color=(diffuse+spec)*light*nl*visibility+albedo*float3(.003,.004,.008);
    if(kind==0)color+=sampleSphere(textures[9],uv).rgb*pow(1-smoothstep(-.15,.1,dot(N,L)),2.0)*.65;
    return float4(color,1);
}
