#pragma once

#include "WindowCluster.h"
#include "DrawTimer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

#if (defined(DEBUG) || defined(_DEBUG))
#define D3DDEBUGNAME(pobj, name) pobj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name)
#else
#define D3DDEBUGNAME(pobj, name)
#endif 

class CSaverBase
{
public:
	CSaverBase(void);
	virtual ~CSaverBase(void);

	BOOL Initialize(const CWindowCluster::SAVER_WINDOW &myWindow, int iSaverIndex, int iNumSavers);
	void Tick();
	void Pause();
	void Resume();
	void ResumeResized(int cx, int cy);
	void CleanUp();

	HRESULT MapDataIntoBuffer(const void *pData, size_t nSize, ComPtr<ID3D12Resource> pResource, UINT Subresource = 0);

protected:
	// Override these functions to implement your saver - return FALSE to error out and shut down
	virtual BOOL InitSaverData() = 0;
	virtual BOOL IterateSaver(float dt, float T) = 0;
	virtual BOOL PauseSaver() = 0;
	virtual BOOL ResumeSaver() = 0;
	virtual BOOL OnResizeSaver() = 0;
	virtual void CleanUpSaver() = 0;

	// Utility functions for saver class to use
	void WaitForPreviousFrame();
	HRESULT LoadShaderBytecode(const std::wstring &strFileName, std::vector<char> &byteCode);
//	HRESULT LoadTexture(const std::wstring &strFileName, ComPtr<ID3D11Texture2D> *ppTexture, ComPtr<ID3D11ShaderResourceView> *ppSRVTexture);

private:
	// Internal utility functions
	HRESULT InitD3DPipeline();
	HRESULT GetMyAdapter(IDXGIAdapter1 **ppAdapter); 
	HRESULT LoadBinaryFile(const std::wstring &strPath, std::vector<char> &buffer);
	BOOL OnResize();

protected:
	ComPtr<ID3D12Device> m_pD3DDevice;
	ComPtr<ID3D12CommandQueue> m_pCommandQueue;
	ComPtr<ID3D12CommandAllocator> m_pCommandAllocator;
	ComPtr<IDXGISwapChain3> m_pSwapChain;
	ComPtr<ID3D12DescriptorHeap> m_pRTVHeap;
	ComPtr<ID3D12DescriptorHeap> m_pCBVHeap;
	std::vector<ComPtr<ID3D12Resource>> m_arrRenderTargets;
	UINT m_rtvDescriptorSize;
	D3D12_VIEWPORT m_viewport;
	D3D12_RECT m_scissorRect;

	UINT m_4xMsaaQuality;
	bool m_bEnable4xMsaa;
	UINT m_nFrameCount;
	UINT m_iFrameIndex;

	HANDLE m_hFrameFenceEvent;
	ComPtr<ID3D12Fence> m_pFrameFence;
	UINT64 m_uFrameFenceValue;

	std::wstring m_strShaderPath;
	std::wstring m_strGFXResoucePath;

	HWND m_hMyWindow;
	int m_iSaverIndex;
	int m_iNumSavers;
	int m_iClientWidth;
	int m_iClientHeight;
	bool m_bFullScreen;
	float m_fAspectRatio;
	bool m_bRunning;
	CDrawTimer m_Timer;
	float m_fMinFrameRefreshTime;
};

