#include "stdafx.h"
#include "SaverBase.h"
//#include "DDSTextureLoader.h"

CSaverBase::CSaverBase(void) :
	m_hMyWindow(NULL),
	m_bRunning(false),	
	m_4xMsaaQuality(0),
	m_bEnable4xMsaa(false),
	m_nFrameCount(2),
	m_iFrameIndex(0),
	m_uFrameFenceValue(0),
	m_hFrameFenceEvent(0),
	m_iSaverIndex(0),
	m_iNumSavers(0),
	m_fMinFrameRefreshTime(1.0f / 60.0f)
{
	TCHAR path[_MAX_PATH];
	TCHAR drive[_MAX_DRIVE];
	TCHAR dir[_MAX_DIR];
	TCHAR fname[_MAX_FNAME];
	TCHAR ext[_MAX_EXT];
	GetModuleFileName(NULL, path, _MAX_PATH);
	_tsplitpath_s(path, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, ext, _MAX_EXT);
	_tmakepath_s(path, _MAX_PATH, drive, dir, NULL, NULL);
	m_strShaderPath = path;
	m_strGFXResoucePath = path;
}

CSaverBase::~CSaverBase(void)
{
}

BOOL CSaverBase::Initialize(const CWindowCluster::SAVER_WINDOW &myWindow, int iSaverIndex, int iNumSavers)
{
	HRESULT hr = S_OK;

	if(!myWindow.hWnd)
	{
		assert(false);
		return FALSE;
	}

	m_hMyWindow = myWindow.hWnd;
	m_iSaverIndex = iSaverIndex;
	m_iNumSavers = iNumSavers;
	m_iClientWidth = myWindow.sClientArea.cx;
	m_iClientHeight = myWindow.sClientArea.cy;
	m_fAspectRatio = (float)m_iClientWidth/(float)m_iClientHeight;
	m_bFullScreen = myWindow.bSaverMode;

	m_Timer.Reset();

	hr = InitD3DPipeline();
	if (FAILED(hr))	return(FALSE);

	BOOL bSuccess = SUCCEEDED(hr);
	if(bSuccess)
	{
		bSuccess = InitSaverData();
		if(bSuccess)
			m_bRunning = true;
	}

	return bSuccess;
}

// Create D3D device, context, and swap chain
HRESULT CSaverBase::InitD3DPipeline()
{
	HRESULT hr = S_OK;

#if defined(DEBUG) || defined(_DEBUG)  
	// Enable the D3D12 debug layer
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
		}
	}
#endif

	// Create the D3D device
	ComPtr<IDXGIAdapter1> pAdapter;
	hr = GetMyAdapter(&pAdapter);
	if(SUCCEEDED(hr))
	{
		hr = D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_pD3DDevice));

		assert(SUCCEEDED(hr));

		// Check multisampling
		if (SUCCEEDED(hr) && m_bEnable4xMsaa)
		{
			D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msaaData = {};
			msaaData.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			msaaData.SampleCount = 4;
			m_pD3DDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msaaData, sizeof(msaaData));
			assert(msaaData.NumQualityLevels > 0);
			m_4xMsaaQuality = msaaData.NumQualityLevels;
		}
	}

	if (SUCCEEDED(hr))
	{
		// Create the command queue for rendering
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		hr = m_pD3DDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pCommandQueue));
		if (SUCCEEDED(hr))
		{
			hr = m_pD3DDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_pCommandAllocator));
		}
	}

	if (SUCCEEDED(hr))
	{
			// Fill out a DXGI_SWAP_CHAIN_DESC to describe our swap chain.
		DXGI_SWAP_CHAIN_DESC1 sd = {};
		sd.BufferCount = m_nFrameCount;
		sd.Width = m_iClientWidth;
		sd.Height = m_iClientHeight;
		sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		if (m_bEnable4xMsaa  && m_4xMsaaQuality > 0)
		{
			sd.SampleDesc.Count = 4;
			sd.SampleDesc.Quality = m_4xMsaaQuality - 1;
		}
		else
		{
			sd.SampleDesc.Count = 1;
			sd.SampleDesc.Quality = 0;
		}

		ComPtr<IDXGIFactory4> dxgiFactory;
		hr = pAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));

		if(SUCCEEDED(hr))
		{
			ComPtr<IDXGISwapChain1> swapChain1;
			hr = dxgiFactory->CreateSwapChainForHwnd(m_pCommandQueue.Get(), m_hMyWindow, &sd, nullptr, nullptr, &swapChain1);
			if (SUCCEEDED(hr))
			{
				// Disable Alt-Enter transitions
				dxgiFactory->MakeWindowAssociation(m_hMyWindow, DXGI_MWA_NO_ALT_ENTER);
				hr = swapChain1.As(&m_pSwapChain);
				if (SUCCEEDED(hr))
					m_iFrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
			}
		}
	}

	// Create the main synchronization object
	hr = m_pD3DDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFrameFence));
	if (FAILED(hr))	return(hr);

	m_uFrameFenceValue = 1;
	m_hFrameFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_hFrameFenceEvent == nullptr)
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}

	if(SUCCEEDED(hr))
	{
		hr = OnResize() ? S_OK : E_FAIL;
	}

	return hr;
}

// Look up the DXGI adapter based on the hosting HWND
HRESULT CSaverBase::GetMyAdapter(IDXGIAdapter1 **ppAdapter)
{
	HRESULT hr = S_OK;
	*ppAdapter = nullptr;
	bool bFound = false;

	assert(m_hMyWindow);
	HMONITOR hMonitor = MonitorFromWindow(m_hMyWindow, MONITOR_DEFAULTTOPRIMARY);
	MONITORINFOEX monitorInfo;
	ZeroMemory(&monitorInfo, sizeof(monitorInfo));
	monitorInfo.cbSize = sizeof(monitorInfo);
	GetMonitorInfo(hMonitor, &monitorInfo);

	ComPtr<IDXGIAdapter1> pAdapter;
	ComPtr<IDXGIFactory4> pFactory;
	hr = CreateDXGIFactory1(IID_PPV_ARGS(&pFactory));
	if(SUCCEEDED(hr))
	{
		for(UINT iAdapter = 0; !bFound; iAdapter++)
		{
			// When this call fails, there are no more adapters left
			HRESULT hrIter = pFactory->EnumAdapters1(iAdapter, &pAdapter);
			if(FAILED(hrIter)) break;

			// See if the adapter is a hardware adapter 
			DXGI_ADAPTER_DESC1 desc;
			pAdapter->GetDesc1(&desc);
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				continue;

			// Make sure it supports D3D12
			if (FAILED(D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
				continue;

			// Finally, check to see if it is displaying on our monitor
			for(UINT iOutput = 0; !bFound; iOutput++)
			{
				ComPtr<IDXGIOutput> pOutput;
				hrIter = pAdapter->EnumOutputs(iOutput, &pOutput);
				if(FAILED(hrIter)) break;

				DXGI_OUTPUT_DESC outputDesc;
				hr = pOutput->GetDesc(&outputDesc);
				if(SUCCEEDED(hr) && (_tcscmp(outputDesc.DeviceName, monitorInfo.szDevice) == 0))
				{
					bFound = true;
					*ppAdapter = pAdapter.Detach();
				}
			}
		}
	}

	if(SUCCEEDED(hr))
		return bFound ? S_OK : E_FAIL;
	else
		return hr;
}

HRESULT CSaverBase::LoadShaderBytecode(const std::wstring &strFileName, std::vector<char> &byteCode)
{
	HRESULT hr = S_OK;

	std::wstring strFullPath = m_strShaderPath + strFileName;

	std::vector<char> buffer;
	hr = LoadBinaryFile(strFullPath, byteCode);

	return hr;
}

//HRESULT CSaverBase::LoadTexture(const std::wstring &strFileName, ComPtr<ID3D11Texture2D> *ppTexture, ComPtr<ID3D11ShaderResourceView> *ppSRVTexture)
//{
//	HRESULT hr = S_OK;
//
//	std::wstring strFullPath = m_strGFXResoucePath + strFileName;
//
//	std::vector<char> buffer;
//	hr = LoadBinaryFile(strFullPath, buffer);
//
//	if(ppTexture) *ppTexture = nullptr;
//	if(ppSRVTexture) *ppSRVTexture = nullptr;
//	ComPtr<ID3D11Resource> pResource;
//	ComPtr<ID3D11ShaderResourceView> pSRV;
//	uint8_t *pData = reinterpret_cast<uint8_t *>(buffer.data());
//	hr = CreateDDSTextureFromMemory(m_pD3DDevice.Get(), pData, buffer.size(), &pResource, &pSRV);
//	if(SUCCEEDED(hr))
//	{
//		if(ppSRVTexture)
//			pSRV.As<ID3D11ShaderResourceView>(ppSRVTexture);
//		if(ppTexture)
//			pResource.As<ID3D11Texture2D>(ppTexture);
//	}
//
//	return hr;
//}

HRESULT CSaverBase::LoadBinaryFile(const std::wstring &strPath, std::vector<char> &buffer)
{
	HRESULT hr = S_OK;

	std::ifstream fin(strPath, std::ios::binary);

	fin.seekg(0, std::ios_base::end);
	int size = (int)fin.tellg();
	if(size > 0)
	{
		fin.seekg(0, std::ios_base::beg);
		buffer.resize(size);
		fin.read(buffer.data(), size);
	}
	else
	{
		hr = E_FAIL;
	}
	fin.close();

	return hr;
}

void CSaverBase::Tick()
{
	if(m_bRunning)
	{
		if (m_Timer.Tick(m_fMinFrameRefreshTime))
		{
			if (!IterateSaver(m_Timer.DeltaTime(), m_Timer.TotalTime()))
			{
				PostMessage(m_hMyWindow, WM_DESTROY, 0, 0);
			}
		}
		else
		{
			::Sleep(0);
		}
	}
	else
	{
	}
}

void CSaverBase::WaitForPreviousFrame()
{
	HRESULT hr = S_OK;
	
	// Signal and increment the fence value
	const UINT64 fence = m_uFrameFenceValue;
	hr = m_pCommandQueue->Signal(m_pFrameFence.Get(), fence);
	if (SUCCEEDED(hr))
	{
		m_uFrameFenceValue++;

		// Wait until the previous frame is finished
		if (m_pFrameFence->GetCompletedValue() < fence)
		{
			hr = m_pFrameFence->SetEventOnCompletion(fence, m_hFrameFenceEvent);
			if (SUCCEEDED(hr))
			{
				WaitForSingleObject(m_hFrameFenceEvent, INFINITE);
			}
		}
	}

	m_iFrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
}

void CSaverBase::Pause()
{
	if(m_bRunning)
	{
		if(!PauseSaver())
		{
			PostMessage(m_hMyWindow, WM_DESTROY, 0, 0);
		}
		m_bRunning = false;
		m_Timer.Stop();
	}
}

void CSaverBase::Resume()
{
	if(!m_bRunning)
	{
		if(!ResumeSaver())
		{
			PostMessage(m_hMyWindow, WM_DESTROY, 0, 0);
		}
		m_bRunning = true;
		m_Timer.Start();
	}
}


void CSaverBase::ResumeResized(int cx, int cy)
{
	if(m_bRunning)
		Pause();

	m_iClientWidth = cx;
	m_iClientHeight = cy;
	m_fAspectRatio = (float)m_iClientWidth/(float)m_iClientHeight;

	if(OnResize())
	{
		Resume();
	}
	else
	{
		PostMessage(m_hMyWindow, WM_DESTROY, 0, 0);
	}
}

// Create/recreate the depth/stencil view and render target based on new size
BOOL CSaverBase::OnResize()
{
	HRESULT hr = S_OK;

	// Set the viewport and scissor rectangle
	m_viewport.TopLeftX = 0.0f;
	m_viewport.TopLeftY = 0.0f;
	m_viewport.Width = static_cast<float>(m_iClientWidth);
	m_viewport.Height = static_cast<float>(m_iClientHeight);
	m_viewport.MaxDepth = 1.0f;
	m_viewport.MinDepth = 0.0f;
	
	m_scissorRect.left = 0;
	m_scissorRect.top = 0;
	m_scissorRect.right = m_iClientWidth;
	m_scissorRect.bottom = m_iClientHeight;

	if (m_pSwapChain)
	{
		// Release the old views, as they hold references to the buffers we
		// will be destroying.  
		m_pRTVHeap = nullptr;

		// Resize the swap chain
		hr = m_pSwapChain->ResizeBuffers(1, m_iClientWidth, m_iClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

		// Create descriptor heap for render target view
		if (SUCCEEDED(hr))
		{
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
			rtvHeapDesc.NumDescriptors = m_nFrameCount;
			rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			hr = m_pD3DDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_pRTVHeap));
			m_rtvDescriptorSize = m_pD3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}

		if (SUCCEEDED(hr))
		{
			// Create descriptor heap for constant buffer views
			D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
			cbvHeapDesc.NumDescriptors = 1;
			cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			hr = m_pD3DDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_pCBVHeap));
		}

		// Create frame resources
		if (SUCCEEDED(hr))
		{
			m_arrRenderTargets.resize(m_nFrameCount);
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pRTVHeap->GetCPUDescriptorHandleForHeapStart());
			for (UINT n = 0; n < m_nFrameCount; n++)
			{
				hr = m_pSwapChain->GetBuffer(n, IID_PPV_ARGS(&m_arrRenderTargets[n]));
				if (FAILED(hr)) break;
				m_pD3DDevice->CreateRenderTargetView(m_arrRenderTargets[n].Get(), nullptr, rtvHandle);
				rtvHandle.Offset(1, m_rtvDescriptorSize);
			}
		}
	}

	assert(SUCCEEDED(hr));

	return OnResizeSaver();
}

// Helper function for frequently-updated buffers
HRESULT CSaverBase::MapDataIntoBuffer(const void *pData, size_t nSize, ComPtr<ID3D12Resource> pResource, UINT Subresource)
{
	if (pResource == nullptr)
	{
		assert(false);
		return E_FAIL;
	}

	HRESULT hr = S_OK;

	UINT8 *pMapData;
	CD3DX12_RANGE readRange(0, 0);
	hr = pResource->Map(Subresource, &readRange, reinterpret_cast<void**>(&pMapData));
	if (SUCCEEDED(hr))
	{
		CopyMemory(pMapData, pData, nSize);
		pResource->Unmap(Subresource, nullptr);
	}

	return hr;
}

void CSaverBase::CleanUp()
{
	if(m_bRunning)
		Pause();

	CleanUpSaver();

	CloseHandle(m_hFrameFenceEvent);
}


