// Render tiles pixel shader
// Just color, nothing else

struct PixelIn
{
	float4 position : SV_POSITION;
	float4 color    : COLOR;
};

float4 RenderPS(PixelIn pin) : SV_TARGET
{
	return pin.color;
}
