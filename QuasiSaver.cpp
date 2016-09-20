#include "stdafx.h"
#include "QuasiSaver.h"
#include "QuasiCalculator.h"
#include "TileDrawer.h"


CQuasiSaver::CQuasiSaver()
{
	m_pQuasiCalculator = nullptr;
	m_pTileDrawer = nullptr;

#ifdef BASICDEBUG
	XMMATRIX I = XMMatrixIdentity();
	XMStoreFloat4x4(&m_matWorld, I);
	XMStoreFloat4x4(&m_matView, I);
	XMStoreFloat4x4(&m_matProj, I);
#endif
}

CQuasiSaver::~CQuasiSaver()
{
}

BOOL CQuasiSaver::InitSaverData()
{
	HRESULT hr = S_OK;

	hr = InitializeTiling();
	if (FAILED(hr)) return FALSE;

	hr = CreateGeometryBuffers();
	if (FAILED(hr)) return FALSE;

	hr = LoadShaders();
	if (FAILED(hr)) return FALSE;

	hr = PrepareShaderConstants();
	if (FAILED(hr)) return FALSE;

	return SUCCEEDED(hr);
}

void CQuasiSaver::CleanUpSaver()
{
	if (m_pTileDrawer)
	{
		delete m_pTileDrawer;
		m_pTileDrawer = nullptr;
	}

	if (m_pQuasiCalculator)
	{
		delete m_pQuasiCalculator;
		m_pQuasiCalculator = nullptr;
	}

	if (m_pD3DContext)
	{
		m_pD3DContext->ClearState();
	}
}

HRESULT CQuasiSaver::InitializeTiling()
{
	m_pQuasiCalculator = new CQuasiCalculator(5);
	m_pTileDrawer = new CTileDrawer(m_pQuasiCalculator);

	int n = m_pTileDrawer->PrepareNextTiles(1);
	assert(n == 1);

	m_fZoom = 0.5f;
	
	return S_OK;
}

HRESULT CQuasiSaver::CreateGeometryBuffers()
{
	HRESULT hr = S_OK;

	// Create dynamic vertex buffer with no real data yet
#ifdef BASICDEBUG
	size_t iVertexBufferSize = m_pQuasiCalculator->m_nMaxTiles * 4 * sizeof(BasicVertex);
#else
	size_t iVertexBufferSize = m_pQuasiCalculator->m_nMaxTiles * 4 * sizeof(CTileDrawer::DXVertex);
#endif
	CD3D11_BUFFER_DESC vbd(iVertexBufferSize, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	hr = m_pD3DDevice->CreateBuffer(&vbd, nullptr, &m_pVertexBuffer);
	if (FAILED(hr)) return hr;
	D3DDEBUGNAME(m_pVertexBuffer, "Tiles Vertex Buffer");

	// Create index buffer with no real data yet
	size_t iIndexBufferSize = m_pQuasiCalculator->m_nMaxTiles * 6 * sizeof(UINT);
	CD3D11_BUFFER_DESC ibd(iIndexBufferSize, D3D10_BIND_INDEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	hr = m_pD3DDevice->CreateBuffer(&ibd, nullptr, &m_pIndexBuffer);
	if (FAILED(hr)) return hr;
	D3DDEBUGNAME(m_pIndexBuffer, "Tiles Index Buffer");

	return hr;
}

HRESULT CQuasiSaver::LoadShaders()
{
	HRESULT hr = S_OK;

	ComPtr<ID3D11DeviceChild> pShader;

	const D3D11_INPUT_ELEMENT_DESC vertexDesc[] =
	{
#ifdef BASICDEBUG
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
#else
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(CTileDrawer::DXVertex, Pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(CTileDrawer::DXVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0 }
#endif
	};

	VS_INPUTLAYOUTSETUP ILS;
	ILS.pInputDesc = vertexDesc;
	ILS.NumElements = ARRAYSIZE(vertexDesc);
	ILS.pInputLayout = NULL;
	hr = LoadShader(VertexShader, L"RenderVS.cso", nullptr, &pShader, &ILS);
	if (SUCCEEDED(hr))
	{
		hr = pShader.As<ID3D11VertexShader>(&m_pVertexShader);
		m_pInputLayout = ILS.pInputLayout;
		ILS.pInputLayout->Release();
		D3DDEBUGNAME(m_pVertexShader, "Vertex Shader");
		D3DDEBUGNAME(m_pInputLayout, "Input Layout");
	}
	if (FAILED(hr)) return hr;

	hr = LoadShader(PixelShader, L"RenderPS.cso", nullptr, &pShader);
	if (SUCCEEDED(hr))
	{
		hr = pShader.As<ID3D11PixelShader>(&m_pPixelShader);
		if (SUCCEEDED(hr))
			D3DDEBUGNAME(m_pPixelShader, "Pixel Shader");
	}
	if (FAILED(hr)) return hr;

	return hr;
}

HRESULT CQuasiSaver::PrepareShaderConstants()
{
	HRESULT hr = S_OK;

	// Create the constant buffer
#ifdef BASICDEBUG
	CD3D11_BUFFER_DESC desc(sizeof(FRAME_VARIABLES), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
#else
	CD3D11_BUFFER_DESC desc(sizeof(FRAME_VARIABLES), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
#endif
	hr = m_pD3DDevice->CreateBuffer(&desc, nullptr, &m_pCBFrameVariables);

	return hr;
}

BOOL CQuasiSaver::OnResizeSaver()
{
	// Nothing required (?)
#ifdef BASICDEBUG
	XMMATRIX P = XMMatrixPerspectiveFovLH(XM_PIDIV4, m_fAspectRatio, 1.0f, 1000.0f);
	XMStoreFloat4x4(&m_matProj, P);
#endif
		
	return TRUE;
}

BOOL CQuasiSaver::PauseSaver()
{
	return TRUE;
}

BOOL CQuasiSaver::ResumeSaver()
{
	return TRUE;
}

BOOL CQuasiSaver::IterateSaver(float dt, float T)
{
	BOOL bResult = UpdateScene(dt, T);

	if (bResult)
	{
		bResult = RenderScene();
	}

	return bResult;
}

BOOL CQuasiSaver::UpdateScene(float dt, float T)
{

#if false
	float fRadius = 5.0f + 2.0f * sinf(T);
	float fPhi = (sinf(T * .1243f) + 1.0f);
	float fTheta = sinf(1.414f * T) * XM_PI;
	float x = fRadius * sinf(fPhi)*cosf(fTheta);
	float z = fRadius * sinf(fPhi)*sinf(fTheta);
	float y = fRadius * cosf(fPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	target = XMVectorSetW(target, 1.0f);
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX V = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&m_matView, V);

	XMMATRIX W = XMLoadFloat4x4(&m_matWorld);
	XMMATRIX P = XMLoadFloat4x4(&m_matProj);

	XMMATRIX mWVP = XMMatrixTranspose(W * V * P);
	XMStoreFloat4x4(&m_sFrameVariables.fv_ViewTransform, mWVP);
#else
#pragma message("TODO: generate some tiles")
	// Generate more tiles

	// Translate to origin
	XMMATRIX mat = XMMatrixTranslationFromVector(-m_pQuasiCalculator->GetOrigin());

	// Time-based spin
	mat *= XMMatrixRotationZ(T);

#pragma message("TODO: dynamic zooming")
	// Zoom to fit viewport, adjusting for aspect ratio
	if (m_fAspectRatio > 1.0f)
	{
		mat *= XMMatrixScaling(m_fZoom / m_fAspectRatio, m_fZoom, m_fZoom);
	}
	else
	{
		mat *= XMMatrixScaling(m_fZoom, m_fZoom * m_fAspectRatio, m_fZoom);
	}

	// Save the matrix in the frame variables
	XMStoreFloat4x4(&(m_sFrameVariables.fv_ViewTransform), XMMatrixTranspose(mat));
#endif

	return TRUE;
}

BOOL CQuasiSaver::RenderScene()
{
	HRESULT hr = S_OK;

	if (!m_pD3DContext || !m_pSwapChain)
	{
		assert(false);
		return FALSE;
	}

	float background[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	m_pD3DContext->ClearRenderTargetView(m_pRenderTargetView.Get(), background);
	m_pD3DContext->ClearDepthStencilView(m_pDepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	m_pD3DContext->IASetInputLayout(m_pInputLayout.Get());
	m_pD3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	hr = MapDataIntoBuffer(&m_sFrameVariables, sizeof(m_sFrameVariables), m_pCBFrameVariables);
	if (FAILED(hr))
	{
		assert(false);
		return hr;
	}

	int nIndices = m_pTileDrawer->RemapBuffers(this, m_pVertexBuffer, m_pIndexBuffer);

#ifdef BASICDEBUG
	UINT stride = sizeof(BasicVertex);
#else
	UINT stride = sizeof(CTileDrawer::DXVertex);
#endif
	UINT offset = 0;
	m_pD3DContext->IASetVertexBuffers(0, 1, m_pVertexBuffer.GetAddressOf(), &stride, &offset);
	m_pD3DContext->IASetIndexBuffer(m_pIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	m_pD3DContext->VSSetShader(m_pVertexShader.Get(), NULL, 0);
	m_pD3DContext->VSSetConstantBuffers(0, 1, m_pCBFrameVariables.GetAddressOf());
	m_pD3DContext->PSSetShader(m_pPixelShader.Get(), NULL, 0);

	m_pD3DContext->DrawIndexed(nIndices, 0, 0);
	
	hr = m_pSwapChain->Present(0, 0);

	return SUCCEEDED(hr);
}
