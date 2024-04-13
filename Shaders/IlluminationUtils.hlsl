// IlluminationUtil.hlsl

#define Lights  16

struct LightProperty
{
    float3 LightStrength;
    float falloffStart;
    float3 LightDirection;
    float falloffEnd;
    float3 LightPosition;
    float SpotLightPower;
};

struct ObjectProperty
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Shininess;
};

float LinearLightAttenuation(float dist, float falloffS, float falloffE)
{
    float attenuation = saturate((falloffE - dist) / (falloffE - falloffS));
    return attenuation;
}

// Schlick approximation to the Fresnel reflectance of the given light

float3 SchlickFresnel(float3 R0, float3 normal, float3 propVec)
{
    float IncidentTheta = saturate(dot(normal, propVec));

    float f0 = 1.0 - IncidentTheta;
    float3 reflectance = R0 +(1.0f - R0) * (f0 * f0 * f0 * f0 * f0);

    return reflectance;
}

// Blinn-Phong Reflection Model
float3 BlinnPhong(float3 LStrength, float3 propVec, float3 normal, float3 toEye, ObjectProperty prop)
{
    const float m = prop.Shininess * 256.0f;
    float3 midVec = normalize(toEye + propVec);

    float roughnessFactor = (m + 8.0f)*pow(max(dot(midVec, normal), 0.0f), m) / 8.0f;
    float3 fresnelFactor = SchlickFresnel(prop.FresnelR0, midVec, propVec);

    float3 specularAlbedo = fresnelFactor * roughnessFactor;

    float regulator = 1.0f;
    specularAlbedo = specularAlbedo / (specularAlbedo + regulator);

    float3 c_sd = (prop.DiffuseAlbedo.rgb + specularAlbedo) * LStrength;

    return c_sd;
}

// Directional Light
float3 ComputeDirectionalLight(LightProperty L, ObjectProperty obj, float3 normal, float3 toEye)
{
    float Proj_n_l = max(dot(-L.LightDirection, normal), 0.0f);
    float3 L_Intensity = L.LightStrength * Proj_n_l;

    return BlinnPhong(L_Intensity, -L.LightDirection, normal, toEye, obj);   
}

// Point Light
float3 ComputePointLight(LightProperty L, ObjectProperty obj, float3 pos, float3 normal, float3 toEye)
{
    float3 light = L.LightPosition - pos;

    float dist = length(light);

    if(dist > L.falloffEnd)
        return 0.0f;    // no light from that far point

    // normalization
    light /= dist;

    // Apply Lambert's cosine law.
    float Proj_n_l = max(dot(light, normal), 0.0f);
    float3 L_Intensity = L.LightStrength * Proj_n_l;

    // linear light intensity attenuation according to the distance.
    float att = LinearLightAttenuation(dist, L.falloffStart, L.falloffEnd);
    L_Intensity *= att;

    return BlinnPhong(L_Intensity, light, normal, toEye, obj);
}

// Spot Light
float3 ComputeSpotLight(LightProperty L, ObjectProperty obj, float3 pos, float normal, float3 toEye)
{
    // vector from the illumination spot to the light
    float3 light = L.LightPosition - pos;

    float dist = length(light);

    if(dist > L.falloffEnd)
        return 0.0f;    // no light from that far point

    // normalization
    light /= dist;

    // Apply Lambert's cosine law.
    float Proj_n_l = max(dot(light, normal), 0.0f);
    float3 L_Intensity = L.LightStrength * Proj_n_l;

    // linear light intensity attenuation according to the distance.
    float att = LinearLightAttenuation(dist, L.falloffStart, L.falloffEnd);
    L_Intensity *= att;

    // Scale by spotlight
    float spotFactor = pow(max(dot(-light, L.LightDirection), 0.0f), L.SpotLightPower);
    L_Intensity *= spotFactor;

    return BlinnPhong(L_Intensity, light, normal, toEye, obj);
}

float4 ComputeLighting(LightProperty gLights[Lights], ObjectProperty obj, float3 pos, float3 normal, float3 toEye, float3 shadowFactor)
{
    float3 getLux = 0.0f;

    int i;
#if (DIR_LIGHTS > 0)
    for(i = 0; i < DIR_LIGHTS; ++i)
    {
        getLux += shadowFactor[i] * ComputeDirectionalLight(gLights[i], obj, normal, toEye);
    }
#endif

#if (POINT_LIGHTS > 0)
    for(i = DIR_LIGHTS; i < DIR_LIGHTS + POINT_LIGHTS; ++i)
    {
        getLux += ComputePointLight(gLights[i].obj, pos, normal, toEye);
    }
#endif

#if(SPOT_LIGHTS > 0)
    for(i = DIR_LIGHTS + POINT_LIGHTS; i < DIR_LIGHTS + POINT_LIGHTS + SPOT_LIGHTS; ++i)
    {
        getLux += ComputeSpotLight(gLights[i], obj, pos, normal, toEye);
    }
#endif

    return float4(getLux, 0.0f);
}

