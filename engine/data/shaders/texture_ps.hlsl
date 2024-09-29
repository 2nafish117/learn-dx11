struct PixelInputType
{
    float4 position : SV_POSITION;
	float4 tex : TEXCOORD0;
};

Texture2D shaderTexture : register(t0);
SamplerState SampleType : register(s0);

float4 TexturePixelShader(PixelInputType input) : SV_TARGET
{
	float4 color = shaderTexture.Sample(SampleType, input.tex);
    return color;
}
