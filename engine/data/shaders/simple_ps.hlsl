struct PSInput {
	float4 position: SV_POSITION;
	float4 ws_position: POSITION;
	float3 ws_normal: NORMAL;
	float3 color: COLOR;
	float2 uv0: TEXCOORD0;
};

uniform Texture2D tex;
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

float4 PSMain(PSInput psInput) : SV_TARGET {

	float4 color = tex.Sample(texSampler, psInput.uv0);
	
	// @TODO: material cbuffer
	float ka = 1;
	float kd = 0.5;
	float ks = 0.6;
	float alpha = 32;

	// phong shading
	float3 lightDir = normalize(Pos - psInput.ws_position.xyz);
	float shading = max(dot(lightDir, psInput.ws_normal), 0);

	float3 diffuse = float3(Col * shading);
	float3 ambient = float3(0.1, 0.1, 0.1);

	// reflect assumes incident vector is going into the surface
	float3 reflectDir = reflect(-lightDir, psInput.ws_normal);
	float3 camPos = float3(-worldToView[3][0], -worldToView[3][1], -worldToView[3][2]);
	float3 viewDir = normalize(camPos - psInput.ws_position.xyz);

	float specTemp = max(dot(reflectDir, viewDir), 0);
	float specular = pow(specTemp, alpha);

	float4 pixelColor = color * float4((ka* ambient + kd * diffuse + ks * specular), 1.0);

	return pixelColor;
}