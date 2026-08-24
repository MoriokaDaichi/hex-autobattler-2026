/*!
 * @brief	ヘックスグリッド(盤面のグリッド線・ゾーン塗り)描画用シェーダー。
 */

cbuffer VSCb : register(b0) {
	float4x4 mVP;
};

struct VSInput
{
	float3 position : Position;
	float4 color : Color;
};

struct PSInput
{
	float4 position : SV_Position;
	float4 color : Color;
};

PSInput VSMain(VSInput input)
{
	PSInput output;
	output.position = mul(mVP, float4(input.position, 1.0f));
	output.color = input.color;
	return output;
}

float4 PSMain(PSInput input) : SV_Target
{
	return input.color;
}
