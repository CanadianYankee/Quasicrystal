// Render tiles shader.  
// Input is vertex and color.
// Output is vertex position transformed to fit viewport.

cbuffer cbFrameVariables : register(b0)
{
	float4x4 fv_ViewTransform;
};

// Particle input to vertex shader 
struct VSRenderIn
{
	float2	position : POSITION;	// Vertex position
	float4	color	 : COLOR;		// Vertex color
};

// Particle output from vertex shader
struct VSRenderOut
{
	float4 position : SV_POSITION;	// Vertex position
	float4 color	: COLOR;		// Vertex color
};


VSRenderOut RenderVS( VSRenderIn input )
{
	VSRenderOut output;

	output.position = mul(float4(input.position, 0.0f, 1.0f), fv_ViewTransform);
	output.color = input.color;

	return output;
}
