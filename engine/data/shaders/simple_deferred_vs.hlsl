struct VSInput {
	float4 obj_position: POSITION;
	float3 obj_normal: NORMAL;
	float3 color: COLOR;
	float2 uv0: TEXCOORD0;
};

struct VSOutput {
	float4 cs_position: SV_POSITION;
	float3 ws_position: POSITION;
	float3 ws_normal: NORMAL;
	float3 color: COLOR;
	float2 uv0: TEXCOORD0;
};

cbuffer MatrixBuffer : register(b0)
{
    float4x4 modelToWorld;
    float4x4 worldToView;
    float4x4 viewToProjection;
};

VSOutput VSMain(VSInput vsInput) {
	VSOutput vsOutput;

	// @TODO: can i just set w to 1?
	vsInput.obj_position.w = 1;
	vsOutput.cs_position = vsInput.obj_position;
	vsOutput.cs_position = mul(vsOutput.cs_position, modelToWorld);
	vsOutput.cs_position = mul(vsOutput.cs_position, worldToView);
	vsOutput.cs_position = mul(vsOutput.cs_position, viewToProjection);

	float4 ws_pos = vsInput.obj_position;
	ws_pos = mul(ws_pos, modelToWorld);
	vsOutput.ws_position = ws_pos.xyz;

	vsOutput.ws_normal = vsInput.obj_normal;
	// @TODO: use normal scaling if scaling non uniformly
	// see https://learnopengl.com/Lighting/Basic-Lighting
	vsOutput.ws_normal = mul(float4(vsOutput.ws_normal, 0), modelToWorld).xyz;

	vsOutput.color = vsInput.color;
	vsOutput.uv0 = vsInput.uv0;

	return vsOutput;
}