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

	m_bBuffersMapped = false;

	// Define some colors to draw with
	m_nBaseColors = rand() % 8 + 1;
	m_bUseColorSpecies = m_nBaseColors > 1 ? frand() < 0.5 : true;
	size_t nColors = m_bUseColorSpecies ? m_nBaseColors * (m_pQuasiCalculator->m_iSymmetry / 2 + 1) : m_nBaseColors;
	m_arrColors.resize(nColors);
	m_bBorders = frand() < 0.5;
	bool bGradient = frand() < 0.5;
	for (size_t i = 0; i < nColors; i++)
	{
		m_arrColors[i].resize(4);
		XMFLOAT4 clr(frand(), frand(), frand(), 1.0f);
		if (bGradient)
		{
			m_arrColors[i][0] = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
			m_arrColors[i][1] = clr;
			m_arrColors[i][2] = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			m_arrColors[i][3] = clr;
		}
		else
		{
			m_arrColors[i][0] = clr;
			m_arrColors[i][1] = clr;
			m_arrColors[i][2] = clr;
			m_arrColors[i][3] = clr;
		}
	}
}

CTileDrawer::~CTileDrawer()
{
}

size_t CTileDrawer::DrawNextTiles(size_t nTiles)
{
	size_t i = 0;
	for (i = 0; i < nTiles; i++)
	{
		bool bSuccess = DrawNextTile();
		if (!bSuccess)
			break;
	}

	return i;
}

bool CTileDrawer::DrawNextTile()
{
	bool bSuccess = false;

	// Check for exceeding max
	if (m_nIndices + 6 > m_arrIndices.size() || m_nVertices + 4 > m_arrVertices.size())
		return false;

	const CQTile *pTile = m_pQuasiCalculator->DrawNextTile();
	int cIndex = abs(pTile->OffsetsSum()) % m_nBaseColors;
	if (m_bUseColorSpecies)
	{
		cIndex += pTile->Species() * m_nBaseColors;
	}
	const std::vector<std::vector<XMFLOAT4>>::const_reference colors = m_arrColors[cIndex];

	std::vector<XMFLOAT2> vecVertices(4);
	pTile->GetVertices(vecVertices);
	XMVECTOR vCenter;
	if (m_bBorders)
	{
		vCenter = XMLoadFloat2(&pTile->GetCenter());
	}

	for (size_t i = 0; i < 4; i++)
	{
		if (m_bBorders)
		{
			XMVECTOR vVert = XMLoadFloat2(&vecVertices[i]);
			vVert = 0.95f * (vVert - vCenter) +  vCenter;
			XMStoreFloat2(&(m_arrVertices[m_nVertices + i].Pos), vVert);
		}
		else
		{
			m_arrVertices[m_nVertices + i].Pos = vecVertices[i];
		}
		m_arrVertices[m_nVertices + i].Color = colors[i];
	}

	m_arrIndices[m_nIndices]     = m_nVertices;
	m_arrIndices[m_nIndices + 1] = m_nVertices + 3;
	m_arrIndices[m_nIndices + 2] = m_nVertices + 1;

	m_arrIndices[m_nIndices + 3] = m_nVertices + 1;
	m_arrIndices[m_nIndices + 4] = m_nVertices + 3;
	m_arrIndices[m_nIndices + 5] = m_nVertices + 2;

	m_nVertices += 4;
	m_nIndices += 6;

	m_bBuffersMapped = false;

	return true;
}

size_t CTileDrawer::RemapBuffers(CQuasiSaver *pDXSaver, ComPtr<ID3D12Resource> pVertexBuffer, ComPtr<ID3D12Resource> pIndexBuffer)
{
	HRESULT hr = S_OK;
	UINT nIndices = 0;
	if (!m_bBuffersMapped)
	{
		hr = pDXSaver->MapDataIntoBuffer(m_arrVertices.data(), m_nVertices * sizeof(DXVertex), pVertexBuffer);
		if (SUCCEEDED(hr))
		{
			hr = pDXSaver->MapDataIntoBuffer(m_arrIndices.data(), m_nIndices * sizeof(UINT), pIndexBuffer);
		}
		m_bBuffersMapped = true;
	}

	return SUCCEEDED(hr) ? m_nIndices : 0;
}
