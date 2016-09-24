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

	hr = CreateD3DAssets();
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

	WaitForPreviousFrame();
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
	
	return S_OK;
}

HRESULT CQuasiSaver::CreateD3DAssets()
{
	HRESULT hr = S_OK;

	hr = CreateRootSignature();
	if (FAILED(hr)) return hr;

	hr = CreatePipeline();
	if (FAILED(hr)) return hr;

	hr = CreateBuffers();
	if (FAILED(hr)) return hr;

	// Wait for the GPU to finish setting up
	WaitForPreviousFrame();

	return hr;
}

HRESULT CQuasiSaver::CreateRootSignature()
{
	HRESULT hr = S_OK;

	// Create a root signature with a descriptor table with a single CBV
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(m_pD3DDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
	CD3DX12_ROOT_PARAMETER1 rootParameters[1];

	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);

	// Allow input layout and deny uneccessary acces to certain pipeline stages
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error);
	if (SUCCEEDED(hr))
	{
		hr = m_pD3DDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature));
	}
	
	return hr;
}

HRESULT CQuasiSaver::CreatePipeline()
{
	HRESULT hr = S_OK;

	// Load the shaders
	std::vector<char> codeVS;
	std::vector<char> codePS;
	hr = LoadShaderBytecode(L"RenderVS.cso", codeVS);
	if (FAILED(hr)) return hr;
	hr = LoadShaderBytecode(L"RenderPS.cso", codePS);
	if (FAILED(hr)) return hr;

	const D3D12_INPUT_ELEMENT_DESC vertexDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(CTileDrawer::DXVertex, Pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(CTileDrawer::DXVertex, Color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	// Describe and create the graphics pipeline state object
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { vertexDesc, _countof(vertexDesc) };
	psoDesc.pRootSignature = m_pRootSignature.Get();
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(codeVS.data(), codeVS.size());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(codePS.data(), codePS.size());
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	if (m_bEnable4xMsaa && m_4xMsaaQuality > 0)
	{
		psoDesc.SampleDesc.Count = 4;
		psoDesc.SampleDesc.Quality = m_4xMsaaQuality - 1;
	}
	else
	{
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;
	}

	hr = m_pD3DDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPipelineState));
	if (FAILED(hr)) return hr;

	// Create the command list for this pipeline
	hr = m_pD3DDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCommandAllocator.Get(), m_pPipelineState.Get(), IID_PPV_ARGS(&m_pRenderCommandList));

	if (SUCCEEDED(hr))
	{
		hr = m_pRenderCommandList->Close();
	}

	return hr;
}

HRESULT CQuasiSaver::CreateBuffers()
{
	HRESULT hr = S_OK;

	// Create the vertex buffer 
	size_t iVertexBufferSize = m_pQuasiCalculator->m_nMaxTiles * 4 * sizeof(CTileDrawer::DXVertex);

	hr = m_pD3DDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(iVertexBufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, 
		IID_PPV_ARGS(&m_pVertexBuffer));
	if (FAILED(hr)) return hr;

	m_vertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.StrideInBytes = sizeof(CTileDrawer::DXVertex);
	m_vertexBufferView.SizeInBytes = iVertexBufferSize;

	// Create the index buffer 
	size_t iIndexBufferSize = m_pQuasiCalculator->m_nMaxTiles * 6 * sizeof(UINT);

	hr = m_pD3DDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(iIndexBufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&m_pIndexBuffer));
	if (FAILED(hr)) return hr;

	m_indexBufferView.BufferLocation = m_pIndexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_indexBufferView.SizeInBytes = iIndexBufferSize;

	// Create the constant buffer
	size_t iConstantBufferSize = (sizeof(FRAME_VARIABLES) + 255) & ~255;
	hr = m_pD3DDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(iConstantBufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&m_pCBFrameVariables));
	if (FAILED(hr)) return hr;

	m_pCBFrameVariablesView.BufferLocation = m_pCBFrameVariables->GetGPUVirtualAddress();
	m_pCBFrameVariablesView.SizeInBytes = iConstantBufferSize;

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
	// Generate more tiles as called for
	if (T - m_fLastGenerated > m_fGenerationPeriod)
	{
		int n = m_pTileDrawer->DrawNextTiles((size_t)floor(sqrt(m_pQuasiCalculator->GetRadius())) + 1 );
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

	if (!m_pRenderCommandList || !m_pSwapChain)
	{
		assert(false);
		return FALSE;
	}

	// Update our varous buffers
	hr = MapDataIntoBuffer(&m_sFrameVariables, sizeof(m_sFrameVariables), m_pCBFrameVariables);
	if (FAILED(hr))
	{
		assert(false);
		return FALSE;
	}
	int nIndices = m_pTileDrawer->RemapBuffers(this, m_pVertexBuffer, m_pIndexBuffer);

	// Populate the command list
	hr = m_pCommandAllocator->Reset();
	if (FAILED(hr)) return hr;

	hr = m_pRenderCommandList->Reset(m_pCommandAllocator.Get(), m_pPipelineState.Get());
	if (FAILED(hr)) return hr;

<<<<<<< HEAD
	m_pRenderCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());

	ID3D12DescriptorHeap *ppHeaps[] = { m_pCBVHeap.Get() };
	m_pRenderCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	m_pRenderCommandList->SetGraphicsRootDescriptorTable(0, m_pCBVHeap->GetGPUDescriptorHandleForHeapStart());
	m_pRenderCommandList->RSSetViewports(1, &m_viewport);
	m_pRenderCommandList->RSSetScissorRects(1, &m_scissorRect);

	// Indicate that the back buffer will be used as a render target
	m_pRenderCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_arrRenderTargets[m_iFrameIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pRTVHeap->GetCPUDescriptorHandleForHeapStart(), m_iFrameIndex, m_rtvDescriptorSize);
	m_pRenderCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	float background[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	m_pRenderCommandList->ClearRenderTargetView(rtvHandle, background, 0, nullptr);
	m_pRenderCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pRenderCommandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	m_pRenderCommandList->IASetIndexBuffer(&m_indexBufferView);
	m_pRenderCommandList->DrawIndexedInstanced(nIndices, 1, 0, 0, 0);

	// Indicate that the back buffer will now be used to present
	m_pRenderCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_arrRenderTargets[m_iFrameIndex].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	hr = m_pRenderCommandList->Close();

	if (SUCCEEDED(hr))
	{
		hr = m_pSwapChain->Present(1, 0);
		WaitForPreviousFrame();
	}
=======
	m_pD3DContext->DrawIndexed(nIndices, 0, 0);
	
	hr = m_pSwapChain->Present(1, 0);
>>>>>>> refs/remotes/origin/master

	return SUCCEEDED(hr);
}
