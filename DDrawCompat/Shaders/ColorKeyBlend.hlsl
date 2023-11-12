sampler2D s_texture : register(s0);
float4 g_colorKey : register(c30);
float4 g_threshold : register(c31);

float4 main(float2 texCoord : TEXCOORD0) : COLOR0
{
	float4 color = tex2D(s_texture, texCoord);
	const float4 diff = abs(color - g_colorKey);
	if (all(diff.rgb < g_threshold.rgb))
	{
		color.a = 0;
	}
	return color;
}
