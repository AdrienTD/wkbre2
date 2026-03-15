[[vk::binding(0, 1)]]
Texture2D inpTexture : register(t0);
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

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float4 Color : COLOR0;
	float2 Texcoord : TEXCOORD0;
	float Fog : FOG;
};

static const float ALPHA_REF = 0.94;

VS_OUTPUT VS(float4 Pos : POSITION, float4 Color : COLOR, float2 Texcoord : TEXCOORD)
{
	VS_OUTPUT output = (VS_OUTPUT)0;
	output.Pos = mul(Pos, Transform);
	output.Color = Color.bgra;
	output.Texcoord = Texcoord;
	output.Fog = 0.0;
	return output;
};

VS_OUTPUT VS_Fog(float4 Pos : POSITION, float4 Color : COLOR, float2 Texcoord : TEXCOORD)
{
	VS_OUTPUT output = (VS_OUTPUT)0;
	output.Pos = mul(Pos, Transform);
	output.Color = Color.bgra;
	output.Texcoord = Texcoord;
	output.Fog = clamp((output.Pos.w-FogStartDist) / (FogEndDist-FogStartDist), 0, 1);
	return output;
};

float4 ApplyFog(float4 color, float fog)
{
	return float4(lerp(color.rgb, FogColor.rgb, fog), color.a);
}

float4 PS(VS_OUTPUT input) : SV_Target
{
	float4 tex = inpTexture.Sample(inpSampler, input.Texcoord);
	return ApplyFog(input.Color * tex, input.Fog);
};

float4 PS_AlphaTest(VS_OUTPUT input) : SV_Target
{
	float4 tex = inpTexture.Sample(inpSampler, input.Texcoord);
	if(tex.a < ALPHA_REF) discard;
	return ApplyFog(input.Color * tex, input.Fog);
};

float4 PS_Map(VS_OUTPUT input) : SV_Target
{
	// Compute the length of a half pixel in the current mipmap
	float lod = inpTexture.CalculateLevelOfDetail(inpSampler, input.Texcoord);
	float ha = 0.5 / (256u >> (int)ceil(lod));

	float2 stc = clamp(input.Texcoord, float2(ha,ha), float2(1.0-ha,1.0-ha));
	float2 tile = floor(stc / float2(0.25,0.25)); // tile index in the 4x4 atlas
	float2 subuv = fmod(stc, float2(0.25,0.25)); // coords inside the used tile

	float2 fuv = clamp(subuv, float2(ha,ha), float2(0.25-ha,0.25-ha)) + tile*0.25;
	float4 tex = inpTexture.Sample(inpSampler, fuv);
	return ApplyFog(input.Color * tex, input.Fog);
}

float4 PS_Lake(VS_OUTPUT input) : SV_Target
{
	return ApplyFog(input.Color, input.Fog);
};
