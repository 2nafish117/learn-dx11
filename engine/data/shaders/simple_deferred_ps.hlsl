#include "testinclude.hlsl"

struct PSInput {
	float4 cs_position: SV_POSITION;
	float3 ws_position: POSITION;
	float3 ws_normal: NORMAL;
	float3 color: COLOR;
	float2 uv0: TEXCOORD0;
};

struct PSOutput {
	float4 albedo: SV_TARGET0;
	float4 ws_position: SV_TARGET1;
	float4 ws_normal: SV_TARGET2;
};

uniform Texture2D albedoTex;

sampler texSampler;

PSOutput PSMain(PSInput psInput)
{
	PSOutput psOutput;

	float4 albedo = albedoTex.Sample(texSampler, psInput.uv0);
	float3 ws_position = psInput.ws_position;
	float3 ws_normal = psInput.ws_normal;

	psOutput.albedo = albedo;
	const float xxunused = 1;
	psOutput.ws_position = float4(ws_position, xxunused);
	psOutput.ws_normal = float4(ws_normal, xxunused);

	return psOutput;
}