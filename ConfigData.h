#pragma once
class CConfigData
{
public:
	CConfigData(void);
	~CConfigData(void);

	void Reload();	// Reload from storage
	void Save();	// Commit to storage

	// User-defined parameters
	BOOL	m_bSpin;			// Rotation flag
	UINT	m_iParticleCount;	// Total number of interacting particles
	float	m_fParticleRadius;	// Size of each particle
	float	m_fSpringLength;	// Equilibrium length of spring between attractive particles
	float	m_fSpringConstant;	// Hooke constant of attractive force
	BOOL	m_bCentralForce;	// Flag enabling attractive central force
	float	m_fFrictionCoeff;	// Dynamic friction coefficient
	float	m_fRandomizeInterval;	// Number of seconds between spring reconfigurations

	// Constants
	static const UINT m_iBlockSize = 128;	// Number of threads in a physics compute block

	// Derived parameters

	// General scaling factor
	inline float fScale() const { return 1.5f * sqrt((float)m_iParticleCount) * m_fSpringLength; }
	// Top allowable speed of particles 
	inline float fSpeedLimit() const { return 5.0f * m_fSpringLength; }
	// Number of blocks to run in the physics compute shader
	inline UINT iNumBlocks() const { return (UINT)(ceil((double)m_iParticleCount / (double) m_iBlockSize)); }
};

