[[vk::binding(0, 1)]]
Texture2D inpTexture : register(t0);
[[vk::binding(0, 2)]]
Texture2D normalTexture : register(t1);
[[vk::binding(0, 1)]]
SamplerState inpSampler : register(s0);

[[vk::binding(0, 0)]]
cbuffer ConstantBuffer : register(b0)
{
	matrix Transform;
};

[[vk::binding(1, 0)]]
cbuffer FogBuffer : register(b1)
{
	float4 FogColor;
	float FogStartDist;
	float FogEndDist;
};

[[vk::binding(2, 0)]]
cbuffer SunBuffer : register(b2)
{
	float3 SunDirection;
	int BumpOn;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float3 FragPos : COLOR1;
	float3 Norm: NORMAL;
	float4 Color : COLOR0;
	float2 Texcoord : TEXCOORD0;
	float Fog : FOG;
	float3 Tangent : TANGENT0;
	float3 Bitangent : TANGENT1;
};

VS_OUTPUT VS(float4 Pos : POSITION, float4 Normal : NORMAL, float3 Tangent : TANGENT, float3 Bitangent : BITANGENT, float2 Texcoord : TEXCOORD)
{
	VS_OUTPUT output = (VS_OUTPUT)0;
	output.Pos = mul(Pos, Transform);
	output.FragPos = Pos.xyz;
	output.Norm = Normal.xyz * 2 - float3(1,1,1);
	output.Color = float4((Normal.w * 0.5 + 0.5).xxx, 1);
	output.Texcoord = Texcoord;
	output.Fog = clamp((output.Pos.w-FogStartDist) / (FogEndDist-FogStartDist), 0, 1);
	output.Tangent = Tangent * 2 - float3(1,1,1);
	output.Bitangent = Bitangent * 2 - float3(1,1,1);
	return output;
};

float4 PS(VS_OUTPUT input) : SV_Target
{
	// Compute the length of a half pixel in the current mipmap
	float lod = inpTexture.CalculateLevelOfDetail(inpSampler, input.Texcoord);
	float ha = 0.5 / (256u >> (int)ceil(lod));

	float2 stc = clamp(input.Texcoord, float2(ha,ha), float2(1.0-ha,1.0-ha));
	float2 tile = floor(stc / float2(0.25,0.25)); // tile index in the 4x4 atlas
	float2 subuv = fmod(stc, float2(0.25,0.25)); // coords inside the used tile

	float2 fuv = clamp(subuv, float2(ha,ha), float2(0.25-ha,0.25-ha)) + tile*0.25;

	float4 tex = inpTexture.Sample(inpSampler, fuv);
	//float4 tex = float4(1,1,1,1);

	float lum = 0.2;

	if(BumpOn) {
		float3 nrm = normalTexture.Sample(inpSampler, fuv).xyz * 2 - 1;

		float3 m1 = input.Tangent;
		float3 m2 = input.Norm;
		float3 m3 = input.Bitangent;
		float3 actnorm = nrm.xxx * m1 + nrm.yyy * m2 + nrm.zzz * m3;
		lum += clamp(dot(normalize(actnorm), SunDirection), 0, 1);
		lum = clamp(lum, 0, 1);
	}
	else {
		lum += clamp(dot(input.Norm, SunDirection), 0, 1);
		lum = clamp(lum, 0, 1);
	}
	//lum = clamp(lum, 0, 1);
	return lerp(float4(lum.xxx, 1) * input.Color * tex, FogColor, input.Fog);
}

// TODO
//float4 PS_Lake(VS_OUTPUT input) : SV_Target
//{
//	return noise(input.Texcoord);
//}
