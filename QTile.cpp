#include "stdafx.h"
#include "QTile.h"
#include "QuasiCalculator.h"

CQTile::CQTile(CQuasiCalculator *pParent) : m_pParent(pParent)
{
	assert(m_pParent != nullptr);

	m_iDOffsets.reserve(m_pParent->m_iSymmetry);
}

CQTile::~CQTile()
{
}

bool CQTile::operator< (const CQTile &rhs) const
{
	if (m_nMaxDLineSet < rhs.m_nMaxDLineSet) return true;
	if (m_nMaxDLineSet > rhs.m_nMaxDLineSet) return false;

	if (m_nMinDLineSet < rhs.m_nMinDLineSet) return true;
	if (m_nMinDLineSet > rhs.m_nMinDLineSet) return false;

	if (m_iDOffsets[m_nMaxDLineSet] < rhs.m_iDOffsets[rhs.m_nMaxDLineSet]) return true;
	if (m_iDOffsets[m_nMaxDLineSet] > rhs.m_iDOffsets[rhs.m_nMaxDLineSet]) return false;

	if (m_iDOffsets[m_nMinDLineSet] < rhs.m_iDOffsets[rhs.m_nMinDLineSet]) return true;

	return false;
}

bool CQTile::operator== (const CQTile &rhs) const
{
	return (m_nMaxDLineSet == rhs.m_nMaxDLineSet) && (m_nMinDLineSet == rhs.m_nMinDLineSet) &&
		(m_iDOffsets[m_nMaxDLineSet] == rhs.m_iDOffsets[rhs.m_nMaxDLineSet]) &&
		(m_iDOffsets[m_nMinDLineSet] == rhs.m_iDOffsets[rhs.m_nMinDLineSet]);
}

// Returns the "species" of the tile. Smaller species have a sharper angle at the zeroth vertex
UINT CQTile::Species() const
{
	UINT s = m_nMaxDLineSet - m_nMinDLineSet;
	return (s <= m_pParent->m_iSymmetry / 2) ? s : m_pParent->m_iSymmetry - s;
}


int CQTile::OffsetsSum() const
{
	int nTotal = 0;
	for (size_t i = 0; i < m_iDOffsets.size(); i++)
	{
		nTotal += m_iDOffsets[i];
	}
	return nTotal;
}

int CQTile::Winding() const
{
	return (m_nMaxDLineSet - m_nMinDLineSet <= m_pParent->m_iSymmetry / 2) ? 1 : -1;
}

void CQTile::GetVertices(std::vector<XMFLOAT2> &vecVertices) const
{
	assert(vecVertices.size() == 4);

	for (size_t i = 0; i < 4; i++)
	{
		vecVertices[i] = m_QVertices[i]->GetPosition();
	}
}
