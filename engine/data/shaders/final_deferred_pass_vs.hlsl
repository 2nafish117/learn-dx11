struct VSInput {
	float4 cs_position: POSITION;
	float3 obj_normal: NORMAL;
	float3 color: COLOR;
	float2 uv0: TEXCOORD0;
};

struct VSOutput {
	float4 cs_position: SV_POSITION;
	float2 uv0: TEXCOORD0;
};

VSOutput VSMain(VSInput vsInput) {
	VSOutput vsOutput;

	vsOutput.cs_position = vsInput.cs_position;
	vsOutput.uv0 = vsInput.uv0;

	return vsOutput;
}