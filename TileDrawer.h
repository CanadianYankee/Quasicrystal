#pragma once
using Microsoft::WRL::ComPtr;

class CQuasiCalculator;

class CTileDrawer
{
public:
	CTileDrawer(CQuasiCalculator *pQuasiCalculator);
	~CTileDrawer();

	struct DXVertex
	{
		XMFLOAT2 Pos;
		XMFLOAT4 Color;

		DXVertex() : Pos(0, 0), Color(0, 0, 0, 0) {}
	};

	size_t PrepareNextTiles(size_t nTiles);
	size_t RemapBuffers(ComPtr<ID3D11DeviceContext> pContext, ComPtr<ID3D11Buffer> pVertexBuffer, ComPtr<ID3D11Buffer> pIndexBuffer);

private:
	bool PrepareNextTile();
	
	CQuasiCalculator *m_pQuasiCalculator;
	std::vector<DXVertex> m_arrVertices;
	std::vector<UINT> m_arrIndices;
	size_t m_nVertices;
	size_t m_nIndices;
};

