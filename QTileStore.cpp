#include "stdafx.h"
#include "QTileStore.h"


CQTileStore::CQTileStore() : m_fLargestDrawn(0.0f)
{
}


CQTileStore::~CQTileStore()
{
}

const CQTile *CQTileStore::FindMatch(const CQTile &tileMatch) const
{
	const CQTile *pMatch = nullptr;

	auto iterMatch = m_QTiles.find(tileMatch);
	if (iterMatch != m_QTiles.end())
	{
		pMatch = &(*iterMatch);
	}

	return pMatch;
}

const CQTile *CQTileStore::Add(const CQTile &newTile)
{
	const CQTile *pInserted = nullptr;

	auto ret = m_QTiles.insert(newTile);
	pInserted = &(*(ret.first));

	if (ret.second)
	{
		m_QToDraw.insert(QDRAW(pInserted));
	}

	return pInserted;
}

const CQTile *CQTileStore::DrawOne()
{
	auto iterFirst = m_QToDraw.begin();
	const CQTile *pTile = iterFirst->pTile;
	m_QToDraw.erase(iterFirst);
	if (pTile->Radius() >= m_fLargestDrawn)
	{
		m_fLargestDrawn = pTile->Radius();
	}

	return pTile;
}

const CQTile *CQTileStore::PeekNextDraw()
{
	auto iterFirst = m_QToDraw.begin();
	const CQTile *pTile = iterFirst->pTile;

	return pTile;
}
