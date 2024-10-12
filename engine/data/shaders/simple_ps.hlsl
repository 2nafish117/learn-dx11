struct PSInput {
	float4 position: SV_POSITION;
	float4 color: COLOR;
	float2 uv0: TEXCOORD0;
};

uniform Texture2D tex;
sampler texSampler;

float4 PSMain(PSInput psInput) : SV_TARGET {

	float4 color = tex.Sample(texSampler, psInput.uv0);
	
	return color;
}