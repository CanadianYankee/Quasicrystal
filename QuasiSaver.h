#pragma once
#include "SaverBase.h"

class CQuasiCalculator;
class CTileDrawer;

inline float frand()
{
	return (float)rand() / (float)RAND_MAX;
}


class CQuasiSaver :
	public CSaverBase
{
public:
	CQuasiSaver();
	virtual ~CQuasiSaver();

protected:
	virtual BOOL InitSaverData();
	virtual BOOL IterateSaver(float dt, float T);
	virtual BOOL PauseSaver();
	virtual BOOL ResumeSaver();
	virtual BOOL OnResizeSaver();
	virtual void CleanUpSaver();

	struct FRAME_VARIABLES
	{
		XMFLOAT4X4 fv_ViewTransform;
	};

private:
	HRESULT InitializeTiling();
	HRESULT CreateD3DAssets();
	HRESULT CreateRootSignature();
	HRESULT CreatePipeline();
	HRESULT CreateBuffers();

	BOOL UpdateScene(float dt, float T);
	BOOL RenderScene();

	CQuasiCalculator *m_pQuasiCalculator;
	CTileDrawer *m_pTileDrawer;

	ComPtr<ID3D12RootSignature> m_pRootSignature;
	ComPtr<ID3D12PipelineState> m_pPipelineState;
	ComPtr<ID3D12GraphicsCommandList> m_pRenderCommandList;

	ComPtr<ID3D12Resource> m_pVertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
#if !defined(SIMPLE_DEBUG)
	ComPtr<ID3D12Resource> m_pIndexBuffer;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
	ComPtr<ID3D12Resource> m_pCBFrameVariables;
	D3D12_CONSTANT_BUFFER_VIEW_DESC m_pCBFrameVariablesView;
#endif
	FRAME_VARIABLES m_sFrameVariables;

	float m_fCurrentScale;
	const float m_fScaleInertia = 1.0f;
	const float m_fGenerationPeriod = 0.2f;
	float m_fRotationAngle;
	float m_fRotationSpeedBase;
	float m_fLastGenerated;
};

