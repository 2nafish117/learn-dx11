struct VSInput {
	float4 position: POSITION;
	float3 color: COLOR;
	// float2 uv0: TEXCOORD0,
};

struct PSInput {
	float4 position: SV_POSITION;
	float4 color: COLOR;
};

PSInput VSMain(VSInput vsInput) {
	PSInput psInput;

	psInput.color = float4(vsInput.color, 1);
	psInput.position = vsInput.position;

	return psInput;
}