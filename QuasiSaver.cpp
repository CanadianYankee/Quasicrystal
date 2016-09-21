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
	// If we're re-initialzing, then clean up the existing stuff
	if (m_pTileDrawer != nullptr)
	{
		delete m_pTileDrawer;
		m_pTileDrawer = nullptr;
	}
	if (m_pQuasiCalculator != nullptr)
	{
		delete m_pQuasiCalculator;
		m_pQuasiCalculator = nullptr;
	}

	m_pQuasiCalculator = new CQuasiCalculator((rand() % 5) * 2 + 5);
	m_pTileDrawer = new CTileDrawer(m_pQuasiCalculator);

	m_fCurrentScale = 1.0f;
	m_fLastGenerated = 0.0f;
	m_fRotationAngle = 0.0f;
	m_fRotationSpeedBase = frand() * 16.0f - 8.0f;
	
	m_bFirstFrame = false;
	m_fPreviousFrame = 0;

	return S_OK;
}

HRESULT CQuasiSaver::CreateGeometryBuffers()
{
	HRESULT hr = S_OK;

	// Create dynamic vertex buffer with no real data yet
	size_t iVertexBufferSize = m_pQuasiCalculator->m_nMaxTiles * 4 * sizeof(CTileDrawer::DXVertex);
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
		{
			D3DDEBUGNAME(m_pPixelShader, "Pixel Shader");
		}
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
	if (!m_bFirstFrame)
	{
		m_bFirstFrame = true;
	}
	else if (T - m_fPreviousFrame >= 1.0f / 60.0f)
	{
		dt = T - m_fPreviousFrame;
	}
	else
	{
		::Sleep(0);
		return TRUE;
	}

	m_fPreviousFrame = T;

	BOOL bResult = UpdateScene(dt, T);

	if (bResult)
	{
		bResult = RenderScene();
	}

	return bResult;
}

BOOL CQuasiSaver::UpdateScene(float dt, float T)
{
	// Generate more tiles as called for
	if (T - m_fLastGenerated > m_fGenerationPeriod)
	{
		int n = m_pTileDrawer->DrawNextTiles(1);
		if (n == 0)
		{
			// We've hit our max - restart!
			InitializeTiling();
			m_pTileDrawer->DrawNextTiles(1);
		}
		m_fLastGenerated = T;
	}

	// Spin
	m_fRotationAngle += m_fRotationSpeedBase * dt / m_fCurrentScale;
	XMMATRIX mat = XMMatrixRotationZ(m_fRotationAngle);

	// Zoom to fit viewport, adjusting for aspect ratio
	float fDesiredScale = m_pQuasiCalculator->GetRadius();
	float fScaleVelocity = (fDesiredScale > m_fCurrentScale) ? fDesiredScale - m_fCurrentScale : 0;
	m_fCurrentScale += fScaleVelocity * m_fScaleInertia * dt;
	float fZoom = 1.0f / m_fCurrentScale;

	if (m_fAspectRatio > 1.0f)
	{
		mat *= XMMatrixScaling(fZoom, fZoom * m_fAspectRatio, fZoom);
	}
	else
	{
		mat *= XMMatrixScaling(fZoom / m_fAspectRatio, fZoom, fZoom);
	}

	// Save the matrix in the frame variables
	XMStoreFloat4x4(&(m_sFrameVariables.fv_ViewTransform), XMMatrixTranspose(mat));

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

	UINT stride = sizeof(CTileDrawer::DXVertex);
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
