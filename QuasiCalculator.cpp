#include "stdafx.h"
#include "QuasiCalculator.h"
#include "QuasiSaver.h"


CQuasiCalculator::CQuasiCalculator(int iSymmetry) : m_iSymmetry(iSymmetry), m_nReleasedTiles(0)
{
	// Create the parallel line sets used for the dual of quasicrystal
	XMMATRIX mRotation = XMMatrixRotationZ(XM_PI * (2.0f / (float) m_iSymmetry));
	XMVECTOR vNormal = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	m_lines.reserve(iSymmetry);
	for (size_t i = 0; i < m_iSymmetry; i++)
	{
		LINESET line;
		XMStoreFloat2(&(line.basis), XMVector2Orthogonal(vNormal));
		XMStoreFloat2(&(line.normal), vNormal);
		XMStoreFloat2(&(line.origin), XMVectorScale(vNormal, frand() - 0.5f)); 
		m_lines.push_back(line);

		vNormal = XMVector2Transform(vNormal, mRotation);
	}
	assert(m_lines.size() == m_iSymmetry);

	// Make the first tile and save its center
	const CQTile *pTile;
	LocateQTile(0, 0, 1, 0, pTile);
	m_QOrigin = pTile->m_QCenter;
}

CQuasiCalculator::~CQuasiCalculator()
{
}

// Pull the next tile out of the tile store, prepping the adjacent ones as well
const CQTile *CQuasiCalculator::DrawNextTile()
{
	const CQTile *pTile = m_QTileStore.DrawOne();

	if (pTile)
	{
		std::vector<const CQTile *> vecNeighbors;
		Find4Neighbors(*pTile, vecNeighbors);
	}

	return pTile;
}


// For a given intersection of two lines in dual space, calculate all interesting data for the 
// corresponding tile in quasi space (or just find it in the store if it's already been calculated)
void CQuasiCalculator::LocateQTile(UINT nDLineSet0, int nDOffset0, UINT nDLineSet1, int nDOffset1, const CQTile *&pTile)
{
	assert((nDLineSet0 < m_iSymmetry) && (nDLineSet1 < m_iSymmetry) && (nDLineSet0 != nDLineSet1));

	CQTile testTile(this);
	testTile.m_iDOffsets.resize(m_iSymmetry);
	testTile.m_nMinDLineSet = min(nDLineSet0, nDLineSet1);
	testTile.m_nMaxDLineSet = max(nDLineSet0, nDLineSet1);
	testTile.m_iDOffsets[nDLineSet0] = nDOffset0;
	testTile.m_iDOffsets[nDLineSet1] = nDOffset1;

	const CQTile *pMatch = m_QTileStore.FindMatch(testTile);

	if (pMatch == nullptr)
	{
		// Get the dual-grid lines that make up this intersection
		XMVECTOR vLine0Point1, vLine0Point2, vLine1Point1, vLine1Point2, vDIntersection;
		m_lines[nDLineSet0].XMLinePoints(vLine0Point1, vLine0Point2, nDOffset0);
		m_lines[nDLineSet1].XMLinePoints(vLine1Point1, vLine1Point2, nDOffset1);

		vDIntersection = XMVector2IntersectLine(vLine0Point1, vLine0Point2, vLine1Point1, vLine1Point2);
		XMStoreFloat2(&testTile.m_DIntersection, vDIntersection);
		for (size_t i = 0; i < m_iSymmetry; i++)
		{
			if (i == nDLineSet0)
			{
				testTile.m_iDOffsets[i] = nDOffset0;
			}
			else if (i == nDLineSet1)
			{
				testTile.m_iDOffsets[i] = nDOffset1;
			}
			else
			{
				testTile.m_iDOffsets[i] = m_lines[i].OffsetTo(vDIntersection);
			}
		}

		// Get the vertices in quasi space, making sure we keep the winding consistent
		UINT nBasis0, nBasis1;
		if (testTile.Winding() > 0)
		{
			nBasis0 = testTile.m_nMinDLineSet;
			nBasis1 = testTile.m_nMaxDLineSet;
		}
		else
		{
			nBasis0 = testTile.m_nMaxDLineSet;
			nBasis1 = testTile.m_nMinDLineSet;
		}
		std::vector<int> offsets = testTile.m_iDOffsets;
		LocateQVertex(offsets, testTile.m_QVertices[0]);
		offsets[nBasis0]++;
		LocateQVertex(offsets, testTile.m_QVertices[1]);
		offsets[nBasis1]++;
		LocateQVertex(offsets, testTile.m_QVertices[2]);
		offsets[nBasis0]--;
		LocateQVertex(offsets, testTile.m_QVertices[3]);

		// Save the center point radius
		XMVECTOR vCenter = XMVectorScale(XMVectorAdd(XMLoadFloat2(&testTile.m_QVertices[0]->m_QPosition), XMLoadFloat2(&testTile.m_QVertices[2]->m_QPosition)), 0.5f);
		XMStoreFloat2(&testTile.m_QCenter, vCenter);
		XMStoreFloat(&testTile.m_fRadius, XMVector2Length(vCenter));

		pMatch = m_QTileStore.Add(testTile);
	}

	pTile = pMatch;
}

void CQuasiCalculator::LocateQVertex(const std::vector<int> &offsets, const CQVertex *&pVertex)
{
	assert(offsets.size() == m_iSymmetry);

	CQVertex testVertex;
	testVertex.m_DPolygonIndex = offsets;

	std::set<CQVertex>::iterator iterMatch = m_QVertices.find(testVertex);
	if (iterMatch == m_QVertices.end())
	{
		XMVECTOR vPoint = XMVectorZero();
		for (size_t i = 0; i < m_iSymmetry; i++)
		{
			vPoint = XMVectorAdd(vPoint, XMVectorScale(XMLoadFloat2(&m_lines[i].basis), (float)(offsets[i])));
		}
		XMStoreFloat2(&testVertex.m_QPosition, vPoint);

		auto ret = m_QVertices.insert(testVertex);
		assert(ret.second);
		iterMatch = ret.first;
	}

	pVertex = &(*iterMatch);
}

void CQuasiCalculator::Find4Neighbors(const CQTile &tile, std::vector<const CQTile *> &vecNeighbors)
{
	vecNeighbors.clear();
	vecNeighbors.reserve(4);
	std::vector<const CQTile *> vecSub;

	Find2Neighbors(tile, true, vecSub);
	assert(vecSub.size() == 2);
	vecNeighbors.insert(vecNeighbors.end(), vecSub.begin(), vecSub.end());

	Find2Neighbors(tile, false, vecSub);
	assert(vecSub.size() == 2);
	vecNeighbors.insert(vecNeighbors.end(), vecSub.begin(), vecSub.end());
}

void CQuasiCalculator::Find2Neighbors(const CQTile &tile, bool bAlongMin, std::vector<const CQTile *> &vecNeighbors)
{
	vecNeighbors.clear();
	vecNeighbors.reserve(2);

	UINT nAlongDLineSet, nAgainstDLineSet;
	if (bAlongMin)
	{
		nAlongDLineSet = tile.m_nMinDLineSet;
		nAgainstDLineSet = tile.m_nMaxDLineSet;
	}
	else
	{
		nAlongDLineSet = tile.m_nMaxDLineSet;
		nAgainstDLineSet = tile.m_nMinDLineSet;
	}

	XMVECTOR vecAlong = XMLoadFloat2(&m_lines[nAlongDLineSet].basis);
	int signNextPos, signNextNeg;

	float distNextPos, distNextNeg;
	XMStoreFloat(&distNextPos, XMVector2Dot(vecAlong, XMLoadFloat2(&m_lines[nAgainstDLineSet].normal)));
	distNextPos = 1.0f / distNextPos;

	if (distNextPos < 0.0f)
	{
		distNextNeg = distNextPos;
		distNextPos = -distNextPos;
		signNextPos = -1;
		signNextNeg = 1;
	}
	else
	{
		distNextNeg = -distNextPos;
		signNextPos = 1;
		signNextNeg = -1;
	}

	UINT nNextDPos = nAgainstDLineSet;
	UINT nNextDNeg = nAgainstDLineSet;
	XMVECTOR vecDIntersection = XMLoadFloat2(&tile.m_DIntersection);
	for (UINT i = 0; i < m_iSymmetry; i++)
	{
		// Only look for neighbors that don't belong to this intersection's linesets
		if ((i == nAlongDLineSet) || (i == nAgainstDLineSet))
			continue;

		float fDist; 
		XMStoreFloat(&fDist, XMVector2Dot(XMVectorSubtract(vecDIntersection, XMLoadFloat2(&m_lines[i].origin)), XMLoadFloat2(&m_lines[i].basis)));
		fDist -= floorf(fDist);
		assert(fDist > 0.0f && fDist < 1.0f);

		float cosTheta;
		XMStoreFloat(&cosTheta, XMVector2Dot(vecAlong, XMLoadFloat2(&m_lines[i].normal)));
		float distNeg, distPos;
		int signPos;
		if (cosTheta < 0)
		{
			distNeg = (1.0f - fDist) / cosTheta;
			distPos = -fDist / cosTheta;
			signPos = 0;
		}
		else
		{
			distPos = (1.0f - fDist) / cosTheta;
			distNeg = -fDist / cosTheta;
			signPos = 1;
		}

		if (distPos < distNextPos)
		{
			nNextDPos = i;
			distNextPos = distPos;
			signNextPos = signPos;
		}
		
		if (distNeg < distNextNeg)
		{
			nNextDNeg = i;
			distNextNeg = distNeg;
			signNextNeg = 1 - signPos;
		}
	}

	assert(distNextNeg < 0.0f && distNextPos > 0.0f);
	assert(nNextDPos != nAlongDLineSet  && nNextDNeg != nAlongDLineSet);

	const CQTile *pTile = nullptr;
	LocateQTile(nAlongDLineSet, tile.m_iDOffsets[nAlongDLineSet], nNextDPos, tile.m_iDOffsets[nNextDPos] + signNextPos, pTile);
	vecNeighbors.push_back(pTile);
	LocateQTile(nAlongDLineSet, tile.m_iDOffsets[nAlongDLineSet], nNextDNeg, tile.m_iDOffsets[nNextDNeg] + signNextNeg, pTile);
	vecNeighbors.push_back(pTile);
}
