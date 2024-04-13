// Basic Shaders : Vertex Shaders and Pixel Shaders

// Light sources
#ifndef DIR_LIGHTS
    #define DIR_LIGHTS  3
#endif

#ifndef POINT_LIGHTS
    #define POINT_LIGHTS    0
#endif

#ifndef SPOT_LIGHTS
    #define SPOT_LIGHTS     0
#endif

#include "IlluminationUtils.hlsl"

struct MaterialParameter
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float4x4 MatTransform;
    uint DiffuseMapIndex;
    uint pad0;     
    uint pad1;
    uint pad2;
};

// An global array of textures
Texture2D gDiffuseMap[7] : register(t0);        // seven textures

StructuredBuffer<MaterialParameter> gMaterialParameters : register(t0, space1); // use space1 to avoid memory overwriting

// global common sampler states
SamplerState gsamPointWrap          :   register(s0);
SamplerState gsamPointClamp         :   register(s1);
SamplerState gsamLinearWrap         :   register(s2);
SamplerState gsamLinearClamp        :   register(s3);
SamplerState gsamAnisotropicWrap    :   register(s4);
SamplerState gsamAnisotropicClamp   :   register(s5);

// constnat buffer for object to be drawn
cbuffer cbObject : register(b0)
{
    float4x4 gWorld;                // (global) world matrix of the given object
    float4x4 gTexTransform;         // (global) texture transform matrix
    uint gMaterialIndex;            // (global) material index for a material for which we wish to assign to this object.
    uint gObjectPad0;               // (global) padding variable....
    uint gObjectPad1;
    uint gObjectPad2;
};

// constant buffer for storing common parameters
cbuffer cbCommon : register(b1)
{
    float4x4 gView;                  // (global)view matrix
    float4x4 gInvView;               // (global)inverse view matrix
    float4x4 gProj;                  // (global)projection matrix (projection to a 2D screen)
    float4x4 gInvProj;               // (global)inverse projection matrix
    float4x4 gViewProj;              // (global)view*projection matrix
    float4x4 gInvViewProj;           // (global)inverse view*projection matrix
    float3 gCameraPosW;              // (global)camera coordinates in world space
    float gcbCommonPad0;             // padding variable
    float2 gRenderTargetSize;        // (global)Rendering target size (width, height)
    float2 gInvRenderTargetSize;     // (global)Inverse Rendering target size (1/width, 1/height)
    float gNearZ;                    // (global)near plane z depth
    float gFarZ;                     // (global)far plane z depth
    float gTotalTime;                // (global)total time since initializing the app.
    float gDeltaTime;                // (global)measured time interval between neighboring frames.
    float4 gAmbientLight;            // (global)ambient light 

    LightProperty gLights[Lights];   // (global) array of lights with own properties
};

struct VertexInput
{
    float3 PosL     :   POSITION;           // local position of a vertex 
    float3 NormalL  :   NORMAL;             // local normal vector attached to a vertex 
    float2 TexC     :   TEXCOORD;           // texture coordinates
};

struct VertexOutput
{
    float4 PosH     : SV_POSITION;          // position of a vertex in homogeneous clip space
    float3 PosW     : POSITION;             // position of a vertex in world space
    float3 NormalW  : NORMAL;               // normal vector in world space
    float2 TexC     : TEXCOORD;             // texture coordinates
};

VertexOutput VS(VertexInput vin)
{
    VertexOutput vout = (VertexOutput)0.0f;

    // fetch the material data.
    MaterialParameter matParam = gMaterialParameters[gMaterialIndex];

    // transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;

    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

    // transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);

    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = mul(texC, matParam.MatTransform).xy;

    return vout;
}

float4 PS(VertexOutput pin) : SV_Target
{
    // fetch the material parameters
    MaterialParameter matParam = gMaterialParameters[gMaterialIndex];
    float4 diffuseAlbedo = matParam.DiffuseAlbedo;
    float3 fresnelR0 = matParam.FresnelR0;
    float roughness = matParam.Roughness;
    uint diffuseTexIndex = matParam.DiffuseMapIndex;

    diffuseAlbedo *= gDiffuseMap[diffuseTexIndex].Sample(gsamLinearWrap, pin.TexC);

    pin.NormalW = normalize(pin.NormalW);

    // vectir from a point on the object's surface to the camera.
    float3 toCameraW = normalize(gCameraPosW - pin.PosW);

    float4 ambient = gAmbientLight*diffuseAlbedo;

    const float shininess = 1.0f - roughness;

    ObjectProperty objProp = {diffuseAlbedo, fresnelR0, shininess};
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(gLights, objProp, pin.PosW, pin.NormalW, toCameraW, shadowFactor);

    float4 litColor = ambient + directLight;

    litColor.a = diffuseAlbedo.a;

    return litColor;
}







