#include "stdafx.h"
#include "ConfigData.h"


CConfigData::CConfigData(void)
{
	Reload();
}


CConfigData::~CConfigData(void)
{
}

void CConfigData::Reload()
{
	m_bSpin = TRUE;
	m_iParticleCount = 1680;
	m_fParticleRadius = 40.0f; //20.0f;
	m_fSpringLength = 20.0f;
	m_fSpringConstant = 6.0f; //3.0f;
	m_bCentralForce = TRUE; //FALSE;
	m_fFrictionCoeff = 0.002f; // 0.005f;
	m_fRandomizeInterval = 30.0;
}

void CConfigData::Save()
{
}
