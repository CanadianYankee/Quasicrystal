#pragma once

#include "QTileStore.h"

class CQuasiCalculator
{
public:
	CQuasiCalculator(int iSymmetry);
	virtual ~CQuasiCalculator();

	const CQTile *DrawNextTile();
	float GetRadius() const { return m_QTileStore.GetRadius(); }

	const size_t m_iSymmetry;
	const size_t m_nMaxTiles = 2000;

private:
	void LocateQTile(UINT nDLineSet0, int nDOffset0, UINT nDLineSet1, int nDOffset, const CQTile *&Tile);
	void LocateQVertex(const std::vector<int> &offsets, const CQVertex *&qVertex);
	void Find4Neighbors(const CQTile &tile, std::vector<const CQTile *> &vecNeighbors);
	void Find2Neighbors(const CQTile &tile, bool bAlongMin, std::vector<const CQTile *> &vecNeighbors);

	struct LINESET
	{
		XMFLOAT2 basis;
		XMFLOAT2 normal;
		XMFLOAT2 origin;

		inline void XMLinePoints(XMVECTOR &point1, XMVECTOR &point2, int nOffset)
		{
			point1 = XMVectorAdd(XMLoadFloat2(&origin), XMVectorScale(XMLoadFloat2(&normal), (float)nOffset));
			point2 = XMVectorAdd(point1, XMLoadFloat2(&basis));
		}

		inline void XMLinePoints(XMVECTOR &point1, XMVECTOR &point2)
		{
			point1 = XMLoadFloat2(&origin);
			point2 = XMVectorAdd(point1, XMLoadFloat2(&basis));
		}

		inline int OffsetTo(XMVECTOR &point)
		{
			float fDist;
			XMStoreFloat(&fDist, XMVector2Dot(XMVectorSubtract(point, XMLoadFloat2(&origin)), XMLoadFloat2(&normal)));
			return (int)floorf(fDist);
		}
	};

	std::vector<LINESET> m_lines;
	CQTileStore m_QTileStore;
	std::set<CQVertex> m_QVertices;
	size_t m_nReleasedTiles;
};



