#include "testinclude.hlsl"

struct PSInput {
	float4 cs_position: SV_POSITION;
	float2 uv0: TEXCOORD0;
};

struct PSOutput {
	float4 combined: SV_TARGET0;
};

uniform Texture2D albedoTex: register(t0);
// uniform Texture2D ws_positionTex: register(t1);
// uniform Texture2D ws_normalTex: register(t2);

sampler texSampler;

cbuffer PointLightBuffer: register(b0)
{
	float3 Pos;
	float3 Col;
};

cbuffer MatrixBuffer : register(b1)
{
    float4x4 modelToWorld;
    float4x4 worldToView;
    float4x4 viewToProjection;
};

// @TODO: point light only for now
float3 PhongBRDF(float3 wsLightPos, float3 wsCamPos, float3 wsFragPos, float3 wsFragNormal)
{
	// @TODO: material cbuffer, and param into brdf
	float ka = 1;
	float kd = 0.5;
	float ks = 0.6;
	float alpha = 32;

	float3 lightDir = normalize(wsLightPos - wsFragPos);
	float shading = max(dot(lightDir, wsFragNormal), 0);

	float3 diffuse = float3(Col * shading);
	float3 ambient = float3(0.1, 0.1, 0.1);

	// reflect assumes incident vector is going into the surface
	float3 reflectDir = reflect(-lightDir, wsFragNormal);
	
	float3 viewDir = normalize(wsCamPos - wsFragPos);

	float specTemp = max(dot(reflectDir, viewDir), 0);
	float specular = pow(specTemp, alpha);

	return ka * ambient + kd * diffuse + ks * specular;
}

PSOutput PSMain(PSInput psInput)
{
	PSOutput psOutput;

	float4 albedo = albedoTex.Sample(texSampler, psInput.uv0);
	// float3 ws_position = ws_positionTex.Sample(texSampler, psInput.uv0).rgb;
	// float3 ws_normal = ws_normalTex.Sample(texSampler, psInput.uv0).rgb;
	
#if 0
	float3 wsCamPos = float3(-worldToView[3][0], -worldToView[3][1], -worldToView[3][2]);
	float3 brdf = PhongBRDF(Pos, wsCamPos, psInput.ws_position, psInput.ws_normal);

	float4 pixelColor = albedo * float4(brdf, 1.0);
#endif

	psOutput.combined = albedo;

	return psOutput;
}