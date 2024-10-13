struct VSInput {
	float4 position: POSITION;
	float3 normal: NORMAL;
	float3 color: COLOR;
	float2 uv0: TEXCOORD0;
};

struct PSInput {
	float4 position: SV_POSITION;
	float4 ws_position: POSITION;
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

PSInput VSMain(VSInput vsInput) {
	PSInput psInput;

	vsInput.position.w = 1;
	psInput.position = vsInput.position;
	psInput.position = mul(psInput.position, modelToWorld);
	psInput.position = mul(psInput.position, worldToView);
	psInput.position = mul(psInput.position, viewToProjection);

	psInput.ws_position = vsInput.position;
	psInput.ws_position = mul(psInput.ws_position, modelToWorld);
	
	psInput.ws_normal = vsInput.normal;
	// @TODO: use normal scaling if scaling non uniformly
	// see https://learnopengl.com/Lighting/Basic-Lighting
	psInput.ws_normal = mul(float4(psInput.ws_normal, 0), modelToWorld).xyz;

	psInput.color = vsInput.color;
	psInput.uv0 = vsInput.uv0;

	return psInput;
}