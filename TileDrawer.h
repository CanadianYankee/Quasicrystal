#pragma once
using Microsoft::WRL::ComPtr;

class CQuasiSaver;
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

	size_t DrawNextTiles(size_t nTiles);
	size_t RemapBuffers(CQuasiSaver *pSaver, ComPtr<ID3D12Resource> pVertexBuffer, ComPtr<ID3D12Resource> pIndexBuffer);

private:
	bool DrawNextTile();
	
	CQuasiCalculator *m_pQuasiCalculator;
	std::vector<DXVertex> m_arrVertices;
	std::vector<UINT> m_arrIndices;
	size_t m_nVertices;
	size_t m_nIndices;

	bool m_bBuffersMapped;

	std::vector<std::vector<XMFLOAT4>> m_arrColors;	// Color array for drawing
	bool m_bUseColorSpecies;
	bool m_bBorders;
	size_t m_nBaseColors;
};

