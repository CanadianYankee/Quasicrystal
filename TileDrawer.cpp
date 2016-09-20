#include "stdafx.h"
#include "TileDrawer.h"
#include "QuasiCalculator.h"
#include "QuasiSaver.h"

CTileDrawer::CTileDrawer(CQuasiCalculator *pQuasiCalculator) : m_pQuasiCalculator(pQuasiCalculator)
{
	size_t nMaxTiles = m_pQuasiCalculator->m_nMaxTiles;
	m_arrVertices.resize(nMaxTiles * 4);
	m_arrIndices.resize(nMaxTiles * 6, 0);
	m_nIndices = 0;
	m_nVertices = 0;
}

CTileDrawer::~CTileDrawer()
{
}

size_t CTileDrawer::PrepareNextTiles(size_t nTiles)
{
	size_t i = 0;
	for (i = 0; i < nTiles; i++)
	{
		bool bSuccess = PrepareNextTile();
		if (!bSuccess)
			break;
	}

	return i;
}

bool CTileDrawer::PrepareNextTile()
{
	bool bSuccess = false;

	// Check for exceeding max
	if (m_nIndices + 6 > m_arrIndices.size() || m_nVertices + 4 > m_arrVertices.size())
		return false;

	const CQTile *pTile = m_pQuasiCalculator->DrawNextTile();
	XMFLOAT4 color = { frand(), frand(), frand(), 1.0f };

	std::vector<XMFLOAT2> vecVertices(4);
	pTile->GetVertices(vecVertices);

	for (size_t i = 0; i < 4; i++)
	{
		m_arrVertices[m_nVertices + i].Pos = vecVertices[i];
		m_arrVertices[m_nVertices + i].Color = color;
	}

	if (pTile->Winding() > 0)
	{
		m_arrIndices[m_nIndices]     = m_nVertices;
		m_arrIndices[m_nIndices + 1] = m_nVertices + 1;
		m_arrIndices[m_nIndices + 2] = m_nVertices + 3;

		m_arrIndices[m_nIndices + 3] = m_nVertices + 1;
		m_arrIndices[m_nIndices + 4] = m_nVertices + 2;
		m_arrIndices[m_nIndices + 5] = m_nVertices + 3;
	}
	else
	{
		m_arrIndices[m_nIndices]     = m_nVertices;
		m_arrIndices[m_nIndices + 1] = m_nVertices + 3;
		m_arrIndices[m_nIndices + 2] = m_nVertices + 1;

		m_arrIndices[m_nIndices + 3] = m_nVertices + 1;
		m_arrIndices[m_nIndices + 4] = m_nVertices + 3;
		m_arrIndices[m_nIndices + 5] = m_nVertices + 2;
	}

	m_nVertices += 4;
	m_nIndices += 6;

	return true;
}

size_t CTileDrawer::RemapBuffers(CQuasiSaver *pDXSaver, ComPtr<ID3D11Buffer> pVertexBuffer, ComPtr<ID3D11Buffer> pIndexBuffer)
{
	HRESULT hr = S_OK;

	hr = pDXSaver->MapDataIntoBuffer(m_arrVertices.data(), m_nVertices * sizeof(DXVertex), pVertexBuffer);
	if (SUCCEEDED(hr))
	{
		hr = pDXSaver->MapDataIntoBuffer(m_arrIndices.data(), m_nIndices * sizeof(UINT), pIndexBuffer);
	}

	return m_nIndices;
}
