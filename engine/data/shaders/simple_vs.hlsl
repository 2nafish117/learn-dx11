struct Input {
	float3 position: POSITION,
	float3 color: COLOR
	// float2 uv0: TEXCOORD0,
};

struct Output {
	float4 color: COLOR
};

Output SimpleVS(Input input) {
	Output output;

	output.color = input.color;

	return output;
}