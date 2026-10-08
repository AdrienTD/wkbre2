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

[[vk::binding(2, 0)]]
cbuffer SunBuffer : register(b2)
{
	float3 SunDirection;
	int BumpOn;
};

//cbuffer SceneBuffer : register(b3)
struct SceneBufferStruct
{
	int EntityFlags;
	float EntityTime;
};

[[vk::push_constant]]
SceneBufferStruct sceneBuffer;

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float3 Norm: NORMAL;
	float4 Color : COLOR0;
	float2 Texcoord : TEXCOORD0;
	float Fog : FOG;
};

VS_OUTPUT VS(float3 Pos : POSITION, float3 Normal : NORMAL, float2 Texcoord : TEXCOORD, float4x4 EntityTransform : TRANSFORM)
{
	VS_OUTPUT output = (VS_OUTPUT)0;
	float4x4 trans = mul(EntityTransform, Transform);
	output.Pos = mul(float4(Pos, 1), trans);
	if(sceneBuffer.EntityFlags & 1) {
		output.Norm = float3(0,0,0);
		output.Color = float4(1,1,1,1);
	}
	else {
		output.Norm = normalize(mul(float4(Normal * 2 - float3(1,1,1), 0), EntityTransform).xyz);
		output.Color = float4( clamp((dot(output.Norm, normalize(SunDirection)) + 1.0) * 0.5, 160.0/255.0, 1.0) * float3(1,1,1), 1 );
	}
	output.Texcoord = Texcoord;
	output.Fog = clamp((output.Pos.w-FogStartDist) / (FogEndDist-FogStartDist), 0, 1);
	return output;
};

VS_OUTPUT VS_Anim(float3 Pos1 : POSITION0, float3 Pos2 : POSITION1, float3 Normal1 : NORMAL0, float3 Normal2 : NORMAL1, float2 Texcoord : TEXCOORD, float4x4 EntityTransform : TRANSFORM)
{
	return VS(lerp(Pos1, Pos2, sceneBuffer.EntityTime), lerp(Normal1, Normal2, sceneBuffer.EntityTime), Texcoord, EntityTransform);
};

float4 PS(VS_OUTPUT input) : SV_Target
{
	float4 tex = inpTexture.Sample(inpSampler, input.Texcoord);
	return lerp(input.Color * tex, FogColor, input.Fog);
}

static const float ALPHA_REF = 0.94;
float4 PS_Alpha(VS_OUTPUT input) : SV_Target
{
	float4 tex = inpTexture.Sample(inpSampler, input.Texcoord);
	if(tex.a < ALPHA_REF) discard;
	return lerp(input.Color * tex, FogColor, input.Fog);
}
