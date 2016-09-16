#pragma once

#include "QTile.h"

class CQTileStore
{
public:
	CQTileStore();
	virtual ~CQTileStore();

	const CQTile *FindMatch(const CQTile &tileMatch) const;
	const CQTile *Add(const CQTile &newTile);
	const CQTile *DrawOne();

	float GetRadius() const { return m_fLargestDrawn; }

protected:
	struct QDRAW
	{
		QDRAW(const CQTile *p) : pTile(p) {}
		const CQTile *pTile;

		bool operator< (const QDRAW &rhs) const
		{
			return this->pTile->Radius() < rhs.pTile->Radius();
		}
	};

	std::set<CQTile> m_QTiles;
	std::set<QDRAW> m_QToDraw;
	float m_fLargestDrawn;
};
