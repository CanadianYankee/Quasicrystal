#pragma once
#include "saverbase.h"
#include "ConfigData.h"

inline float frand()
{
	return (float)rand() / (float)RAND_MAX;
}

class CInteractSaver : 	public CSaverBase
{
public:
	CInteractSaver(void);
	virtual ~CInteractSaver(void);

protected:
	virtual BOOL InitSaverData();
	virtual BOOL IterateSaver(float dt, float T);
	virtual BOOL PauseSaver();
	virtual BOOL ResumeSaver();
	virtual BOOL OnResizeSaver();
	virtual void CleanUpSaver();

	struct COLOR_VERTEX
	{
		XMFLOAT4 Color;
	};

	struct PARTICLE_POSVEL
	{
		XMFLOAT4 Position;
		XMFLOAT4 Velocity;
	};

	struct WORLD_PHYSICS
	{
		WORLD_PHYSICS(const CConfigData &data) :
			g_fScale(data.fScale()), g_fParticleRadius(data.m_fParticleRadius),
			g_fSpringLength(data.m_fSpringLength), g_fSpringConstant(data.m_fSpringConstant),
			g_iParticleCount(data.m_iParticleCount), g_bCentralForce(data.m_bCentralForce), 
			g_fFrictionCoeff(data.m_fFrictionCoeff), g_fSpeedLimit(data.fSpeedLimit()),
			g_iNumBlocks(data.iNumBlocks())
		{ }
		float g_fScale;
		float g_fParticleRadius;
		float g_fFrictionCoeff;
		float g_fSpeedLimit;

		float g_fSpringLength;
		float g_fSpringConstant;
		float wpfDummy0;
		float wpfDummy1;

		UINT g_iParticleCount;
		UINT g_iNumBlocks;
		UINT g_bCentralForce;
		UINT wpiDummy0;
	};

	static const UINT NUM_PATTERNS = 3;
	struct SPRING_CONFIG
	{
		UINT g_iPattern;
		UINT g_iModulus;
		UINT g_iAntiModulus;
		UINT g_iParticleCount;
	};

	struct FRAME_VARIABLES
	{
		XMFLOAT4X4 g_matWorldTrans;
		XMFLOAT4X4 g_matViewTrans;
		XMFLOAT4X4 g_matProjectionTrans;
		float g_fGlobalTime;
		float g_fElapsedTime;
		float fvfDummy0;
		float fvfDummy1;
	};


	void RandomizeVector(XMFLOAT4 *pVector, float fWValue, float fRadius, bool bFixed = false);
	HRESULT InitializeParticles();
	HRESULT LoadShaders();
	HRESULT PrepareShaderConstants();

	BOOL UpdateScene(float dt, float T, SPRING_CONFIG **pSpringConfig);
	void RandomizeSprings(SPRING_CONFIG **pSpringConfig);
	BOOL ComputeScene(const SPRING_CONFIG *springConfig);
	BOOL RenderScene();

	template <class T> void SwapComPtr(ComPtr<T> &ptr1, ComPtr<T> &ptr2)
	{
		ComPtr<T> temp = ptr1;  ptr1 = ptr2;  ptr2 = temp;
	}

	CConfigData m_ConfigData;
	concurrency::task_group m_InitTasks;

	ComPtr<ID3D11Buffer> m_pVBParticleColors;
	ComPtr<ID3D11Buffer> m_pSBPosVel;
	ComPtr<ID3D11Buffer> m_pSBPosVelNext;
	ComPtr<ID3D11ShaderResourceView> m_pSRVPosVel;
	ComPtr<ID3D11ShaderResourceView> m_pSRVPosVelNext;
	ComPtr<ID3D11UnorderedAccessView> m_pUAVPosVel;
	ComPtr<ID3D11UnorderedAccessView> m_pUAVPosVelNext;
	ComPtr<ID3D11ShaderResourceView> m_pSRVParticleDraw;

	ComPtr<ID3D11Texture2D> m_pTexSpringLookup;
	ComPtr<ID3D11ShaderResourceView> m_pSRVSpringLookup;
	ComPtr<ID3D11UnorderedAccessView> m_pUAVSpringLookup;

	ComPtr<ID3D11SamplerState> m_pTextureSampler;
	ComPtr<ID3D11BlendState> m_pRenderBlendState;
	ComPtr<ID3D11DepthStencilState> m_pRenderDepthState;

	ComPtr<ID3D11Buffer> m_pCBWorldPhysics;
	ComPtr<ID3D11Buffer> m_pCBFrameVariables;
	ComPtr<ID3D11Buffer> m_pCBSpringConfig;

	ComPtr<ID3D11VertexShader> m_pRenderVS;
	ComPtr<ID3D11GeometryShader> m_pRenderGS;
	ComPtr<ID3D11PixelShader> m_pRenderPS;
	ComPtr<ID3D11InputLayout> m_pRenderIL;

	ComPtr<ID3D11ComputeShader> m_pConfigSpringsCS;
	ComPtr<ID3D11ComputeShader> m_pPhysicsCS;

	XMFLOAT4X4 m_matWorld;
	XMFLOAT4 m_vecSpinAxis;
	FRAME_VARIABLES m_varsFrame;
	float m_fLastRandomize;
	BOOL m_bInitialRandomizeDone;
};
