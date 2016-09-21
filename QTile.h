#pragma once

class CQVertex
{
	friend class CQuasiCalculator;

protected:
	CQVertex() {}
public:
	virtual ~CQVertex() {}

	const bool operator< (const CQVertex &rhs) const
	{
		assert(m_DPolygonIndex.size() == rhs.m_DPolygonIndex.size());

		for (size_t i = 0; i < m_DPolygonIndex.size(); i++)
		{
			if (m_DPolygonIndex[i] < rhs.m_DPolygonIndex[i])
			{
				return true;
			}
			else if (m_DPolygonIndex[i] > rhs.m_DPolygonIndex[i])
			{
				return false;
			}
		}
		// both are equal, "less than" still false
		return false;
	}

	XMFLOAT2 GetPosition() const { return m_QPosition;  }

protected:
	std::vector<int> m_DPolygonIndex;
	XMFLOAT2 m_QPosition;
};

class CQTile
{
	friend class CQuasiCalculator;

protected:
	CQTile(CQuasiCalculator *pParent);
public:
	virtual ~CQTile();

	bool operator< (const CQTile &rhs) const;
	bool operator== (const CQTile &rhs) const;

	// Returns the "species" of the tile. Smaller species have a sharper angle at the zeroth vertex
	UINT Species() const;
	int OffsetsSum() const;
	int Winding() const;
	void GetVertices (std::vector<XMFLOAT2> &vecVertices) const;
	XMFLOAT2 GetCenter() const { return m_QCenter; }

	inline float Radius() const { return m_fRadius; }

protected:
	CQuasiCalculator *m_pParent;
	UINT m_nMinDLineSet;			// Minimum-indexed lineset for dual space vertex
	UINT m_nMaxDLineSet;			// Maximum-indexed lineset for dual space vertex
	std::vector<int> m_iDOffsets;	// Offsets for each lineset in dual space
	XMFLOAT2 m_DIntersection;		// Coordinate of intersection in dual space
	XMFLOAT2 m_QCenter;				// Center of quasi tile
	float m_fRadius;				// Distance of center of quasi tile from the origin
	std::array<const CQVertex*, 4> m_QVertices;
	std::vector<const CQTile*> m_QNeighbors;
};
