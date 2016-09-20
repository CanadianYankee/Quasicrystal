#pragma once
#include "SaverBase.h"

#define BASICDEBUG

#ifdef BASICDEBUG
struct BasicVertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};
#endif

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
	HRESULT CreateGeometryBuffers();
	HRESULT LoadShaders();
	HRESULT PrepareShaderConstants();

	BOOL UpdateScene(float dt, float T);
	BOOL RenderScene();

	CQuasiCalculator *m_pQuasiCalculator;
	CTileDrawer *m_pTileDrawer;

	ComPtr<ID3D11Buffer> m_pVertexBuffer;
	ComPtr<ID3D11Buffer> m_pIndexBuffer;

	ComPtr<ID3D11VertexShader> m_pVertexShader;
	ComPtr<ID3D11PixelShader> m_pPixelShader;
	ComPtr<ID3D11InputLayout> m_pInputLayout;

	ComPtr<ID3D11Buffer> m_pCBFrameVariables;
	FRAME_VARIABLES m_sFrameVariables;

#ifdef BASICDEBUG
	size_t m_nIndices;
	XMFLOAT4X4 m_matWorld;
	XMFLOAT4X4 m_matView;
	XMFLOAT4X4 m_matProj;
#endif

	float m_fZoom;
};

