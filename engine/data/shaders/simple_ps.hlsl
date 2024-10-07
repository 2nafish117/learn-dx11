struct PSInput {
	float4 position: SV_POSITION;
	float4 color: COLOR;
};

float4 PSMain(PSInput psInput) : SV_TARGET {
	return psInput.color;
}