#include "stdafx.h"
#include "QuasiSaver.h"
#include "QuasiCalculator.h"
#include "TileDrawer.h"

CQuasiSaver::CQuasiSaver()
{
	m_pQuasiCalculator = nullptr;
	m_pTileDrawer = nullptr;
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

	m_fZoom = 2.0f;
	
	return S_OK;
}

HRESULT CQuasiSaver::CreateGeometryBuffers()
{
	HRESULT hr = S_OK;

	// Create dynamic vertex buffer with no real data yet
	size_t iVertices = m_pQuasiCalculator->m_nMaxTiles * 4;
	CD3D11_BUFFER_DESC vbd(iVertices * sizeof(CTileDrawer::DXVertex), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	hr = m_pD3DDevice->CreateBuffer(&vbd, nullptr, &m_pVertexBuffer);
	if (FAILED(hr)) return hr;
	D3DDEBUGNAME(m_pVertexBuffer, "Tiles Vertex Buffer");

	// Create index buffer with no real data yet
	size_t iIndices = m_pQuasiCalculator->m_nMaxTiles * 6;
	CD3D11_BUFFER_DESC ibd(iIndices, D3D10_BIND_INDEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
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
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(CTileDrawer::DXVertex, Pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(CTileDrawer::DXVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0 }
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
	CD3D11_BUFFER_DESC desc(sizeof(FRAME_VARIABLES), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	hr = m_pD3DDevice->CreateBuffer(&desc, nullptr, &m_pCBFrameVariables);

	return hr;
}

BOOL CQuasiSaver::OnResizeSaver()
{
	// Nothing required (?)
		
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

	XMStoreFloat4x4(&(m_sFrameVariables.fv_ViewTransform), XMMatrixIdentity());
	return TRUE;

#pragma message("TODO: generate some tiles")
	// Generate more tiles


	// Translate to origin
	XMMATRIX mat = XMMatrixTranslationFromVector(-m_pQuasiCalculator->GetOrigin());

#pragma message("TODO: dynamic zooming")
	// Move towards fit-all-tiles 
	mat /= m_fZoom;

	// Time-based spin
	mat *= XMMatrixRotationZ(T);

	// Adjust for aspect ratio
	if (m_fAspectRatio > 1.0f)
	{
		mat *= XMMatrixScaling(1.0f / m_fAspectRatio, 1.0f, 1.0f);
	}
	else
	{
		mat *= XMMatrixScaling(1.0f, m_fAspectRatio, 1.0f);
	}

	// Put the origin in the vieport center
	mat *= XMMatrixTranslation(0.5f, 0.5f, 0.0f);

	// Save the matrix in the frame variables
	XMStoreFloat4x4(&(m_sFrameVariables.fv_ViewTransform), mat);

	return TRUE;
}

BOOL CQuasiSaver::RenderScene()
{
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

	m_pD3DContext->UpdateSubresource(m_pCBFrameVariables.Get(), 0, NULL, &m_sFrameVariables, 0, 0);

//	int nIndices = m_pTileDrawer->RemapBuffers(m_pD3DContext, m_pVertexBuffer, m_pIndexBuffer);

	int nIndices = 6;
	CTileDrawer::DXVertex vb[4];
	vb[0].Pos.x = 0.2f; vb[0].Pos.y = 0.25f;
	vb[1].Pos.x = 0.4f; vb[1].Pos.y = 0.75f;
	vb[2].Pos.x = 0.8f; vb[2].Pos.y = 0.75f;
	vb[3].Pos.x = 0.6f; vb[3].Pos.y = 0.25f;
	XMFLOAT4 clr = { 1.0f, 1.0f, 0.0f, 1.0f };
	vb[0].Color = vb[1].Color = vb[2].Color = vb[3].Color = clr;

	int ib[6];
	ib[0] = 0; ib[1] = 1; ib[2] = 3;
	ib[3] = 1; ib[4] = 2; ib[5] = 3;

	m_pD3DContext->UpdateSubresource(m_pVertexBuffer.Get(), 0, NULL, vb, 0, 0);
	m_pD3DContext->UpdateSubresource(m_pIndexBuffer.Get(), 0, NULL, ib, 0, 0);

	UINT stride = sizeof(CTileDrawer::DXVertex);
	UINT offset = 0;
	m_pD3DContext->IASetVertexBuffers(0, 1, m_pVertexBuffer.GetAddressOf(), &stride, &offset);
	m_pD3DContext->IASetIndexBuffer(m_pIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	m_pD3DContext->VSSetShader(m_pVertexShader.Get(), NULL, 0);
	m_pD3DContext->VSSetConstantBuffers(0, 1, m_pCBFrameVariables.GetAddressOf());
	m_pD3DContext->PSSetShader(m_pPixelShader.Get(), NULL, 0);

	m_pD3DContext->DrawIndexed(nIndices, 0, 0);
	
	HRESULT hr = m_pSwapChain->Present(0, 0);

	return SUCCEEDED(hr);
}
