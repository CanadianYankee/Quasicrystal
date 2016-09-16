#include "stdafx.h"
#include "InteractSaver.h"


CInteractSaver::CInteractSaver(void) :
	m_bInitialRandomizeDone(FALSE)
{
	m_bEnable4xMsaa = FALSE;

	XMMATRIX I = XMMatrixIdentity();
	XMStoreFloat4x4(&m_matWorld, I);
	m_vecSpinAxis = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);

	ZeroMemory(&m_varsFrame, sizeof(FRAME_VARIABLES));
	XMStoreFloat4x4(&m_varsFrame.g_matWorldTrans, I);
	XMStoreFloat4x4(&m_varsFrame.g_matViewTrans, I);
	XMStoreFloat4x4(&m_varsFrame.g_matProjectionTrans, I);
}

CInteractSaver::~CInteractSaver(void)
{
}

BOOL CInteractSaver::InitSaverData()
{
	m_InitTasks.run([this]() { 
		InitializeParticles();
	});

	m_InitTasks.run([this]() { 
		LoadShaders();
	});

	m_InitTasks.run([this]() { 
		PrepareShaderConstants();
	});

	return TRUE;
}

void CInteractSaver::CleanUpSaver()
{
	m_InitTasks.cancel();
	m_InitTasks.wait();
	
	if(m_pD3DContext)
	{
		m_pD3DContext->ClearState();
	}
}

void CInteractSaver::RandomizeVector(XMFLOAT4 *pVector, float fWValue, float fRadius, bool bFixed)
{
	XMVECTOR v;
	for(;;)
	{
		XMFLOAT4 v4(frand() - 0.5f, frand() - 0.5f, frand() - 0.5f, 0.0f);
		v = XMLoadFloat4(&v4);

		if(XMVectorGetX(XMVector3LengthSq(v)) <= 0.25) 
		{
			v = XMVectorScale(v, 2.0f * fRadius);
			if(bFixed)
				v = XMVector3ClampLength(v, fRadius, fRadius);

			break;
		}
	}

	XMStoreFloat4(pVector, v);
	pVector->w = fWValue;
}

HRESULT CInteractSaver::InitializeParticles()
{
	// Each "vertex" is just a color.  Particle positions are stored in a texture.

	HRESULT hr = S_OK;

	// Create the raw data
	std::vector<COLOR_VERTEX> vecColors;
	vecColors.resize(m_ConfigData.m_iParticleCount);
	std::vector<PARTICLE_POSVEL> vecPosVel;
	vecPosVel.resize(m_ConfigData.m_iParticleCount);

	float fr = 100.0f * (frand() + 0.25f) / (float)m_ConfigData.m_iParticleCount;
	float fg = 100.0f * (frand() + 0.25f) / (float)m_ConfigData.m_iParticleCount;
	float fb = 100.0f * (frand() + 0.25f) / (float)m_ConfigData.m_iParticleCount;

	float or = frand() * 2.0f * XM_PI;
	float og = frand() * 2.0f * XM_PI;
	float ob = frand() * 2.0f * XM_PI;

	std::vector<COLOR_VERTEX>::iterator itColor = vecColors.begin();
	std::vector<PARTICLE_POSVEL>::iterator itPosVel = vecPosVel.begin();
	for (UINT i = 0; i < m_ConfigData.m_iParticleCount; i++, itColor++, itPosVel++)
	{
		float r = 0.5f * (sin(fr * (float)i + or) + 1.0f);
		float g = 0.5f * (sin(fg * (float)i + og) + 1.0f);
		float b = 0.5f * (sin(fb * (float)i + ob) + 1.0f);

		itColor->Color = XMFLOAT4(r, g, b, 0.75f);	

		RandomizeVector(&itPosVel->Position, 1.0f, 0.1f * m_ConfigData.fScale());
		itPosVel->Velocity = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	// Create color vertex buffer
	D3D11_SUBRESOURCE_DATA vinitData = {0};
    vinitData.pSysMem = &vecColors.front();
	vinitData.SysMemPitch = 0;
	vinitData.SysMemSlicePitch = 0;
	CD3D11_BUFFER_DESC vbdColors(sizeof(COLOR_VERTEX) * m_ConfigData.m_iParticleCount, D3D11_BIND_VERTEX_BUFFER,
		D3D11_USAGE_IMMUTABLE);
	hr = m_pD3DDevice->CreateBuffer(&vbdColors, &vinitData, &m_pVBParticleColors);
	if(FAILED(hr)) return hr;
	D3DDEBUGNAME(m_pVBParticleColors, "Particle colors");

	// Create the position/velocity structured buffers
    CD3D11_BUFFER_DESC vbdPV(m_ConfigData.m_iParticleCount * sizeof(PARTICLE_POSVEL), 
		D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, 
		D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, sizeof(PARTICLE_POSVEL));
	vinitData.pSysMem = &vecPosVel.front();
	hr = m_pD3DDevice->CreateBuffer(&vbdPV, &vinitData, &m_pSBPosVel);
	if(FAILED(hr)) return hr;
	D3DDEBUGNAME(m_pSBPosVel, "Position/Velocity");
	hr = m_pD3DDevice->CreateBuffer(&vbdPV, &vinitData, &m_pSBPosVelNext);
	if(FAILED(hr)) return hr;
	D3DDEBUGNAME(m_pSBPosVelNext, "Next Position/Velocity");

	// Create the position/velocity resource views (read-only in shaders)
	CD3D11_SHADER_RESOURCE_VIEW_DESC srvPV(D3D11_SRV_DIMENSION_BUFFER, DXGI_FORMAT_UNKNOWN);
	srvPV.Buffer.FirstElement = 0;
	srvPV.Buffer.NumElements = m_ConfigData.m_iParticleCount;
	hr = m_pD3DDevice->CreateShaderResourceView(m_pSBPosVel.Get(), &srvPV, &m_pSRVPosVel);
	if(FAILED(hr)) return hr;
	D3DDEBUGNAME(m_pSRVPosVel, "Position/Velocity SRV");
	hr = m_pD3DDevice->CreateShaderResourceView(m_pSBPosVelNext.Get(), &srvPV, &m_pSRVPosVelNext);
	if(FAILED(hr)) return hr;
	D3DDEBUGNAME(m_pSRVPosVelNext, "Next Position/Velocity SRV");

	// Create the position/velocity access view (write-access in shaders)
	CD3D11_UNORDERED_ACCESS_VIEW_DESC uavPV(m_pSBPosVel.Get(), DXGI_FORMAT_UNKNOWN, 0, m_ConfigData.m_iParticleCount);
	hr = m_pD3DDevice->CreateUnorderedAccessView(m_pSBPosVel.Get(), &uavPV, &m_pUAVPosVel);
	if(FAILED(hr)) return hr;
	D3DDEBUGNAME(m_pUAVPosVel, "Position/Velocity UAV");
	hr = m_pD3DDevice->CreateUnorderedAccessView(m_pSBPosVelNext.Get(), &uavPV, &m_pUAVPosVelNext);
	if(FAILED(hr)) return hr;
	D3DDEBUGNAME(m_pUAVPosVelNext, "Position/Velocity Next UAV");

	return hr;
}

HRESULT CInteractSaver::LoadShaders()
{
	HRESULT hr = S_OK;

	// Shaders for rendering

	ComPtr<ID3D11DeviceChild> pShader;

	const D3D11_INPUT_ELEMENT_DESC vertexDesc[] = 
	{
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	VS_INPUTLAYOUTSETUP ILS;
	ILS.pInputDesc = vertexDesc;
	ILS.NumElements = ARRAYSIZE(vertexDesc);
	ILS.pInputLayout = NULL;
	hr = LoadShader(VertexShader, L"RenderVS.cso", nullptr, &pShader, &ILS);
	if(SUCCEEDED(hr))
	{
		hr = pShader.As<ID3D11VertexShader>(&m_pRenderVS);
		m_pRenderIL = ILS.pInputLayout;
		ILS.pInputLayout->Release();
		D3DDEBUGNAME(m_pRenderVS, "Render VS");
		D3DDEBUGNAME(m_pRenderIL, "Render IL");
	}
	if(FAILED(hr)) return hr;

	hr = LoadShader(GeometryShader, L"RenderGS.cso", nullptr, &pShader);
	if(SUCCEEDED(hr))
	{
		hr = pShader.As<ID3D11GeometryShader>(&m_pRenderGS);
		D3DDEBUGNAME(m_pRenderGS, "Render GS");
	}
	if(FAILED(hr)) return hr;

	hr = LoadShader(PixelShader, L"RenderPS.cso", nullptr, &pShader);
	if(SUCCEEDED(hr))
	{
		hr = pShader.As<ID3D11PixelShader>(&m_pRenderPS);
		D3DDEBUGNAME(m_pRenderPS, "Render PS");
	}
	if(FAILED(hr)) return hr;

	hr = LoadShader(ComputeShader, L"ConfigSpringsCS.cso", nullptr, &pShader);
	if(SUCCEEDED(hr))
	{
		hr = pShader.As<ID3D11ComputeShader>(&m_pConfigSpringsCS);
		D3DDEBUGNAME(m_pConfigSpringsCS, "Config Springs CS");
	}
	if(FAILED(hr)) return hr;

	hr = LoadShader(ComputeShader, L"PhysicsCS.cso", nullptr, &pShader);
	if(SUCCEEDED(hr))
	{
		hr = pShader.As<ID3D11ComputeShader>(&m_pPhysicsCS);
		D3DDEBUGNAME(m_pPhysicsCS, "Physics CS");
	}
	if(FAILED(hr)) return hr;

	return hr;
}

HRESULT CInteractSaver::PrepareShaderConstants()
{
	HRESULT hr = S_OK;

	// Allocate the various constant buffers
	WORLD_PHYSICS cbWorldPhysics(m_ConfigData);
	
	// World physics is just set once and remains unchanged for the life of the saver
	D3D11_SUBRESOURCE_DATA cbData;
	cbData.pSysMem = &cbWorldPhysics;
	cbData.SysMemPitch = 0;
	cbData.SysMemSlicePitch = 0;
	D3D11_BUFFER_DESC Desc;
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = 0;
	Desc.MiscFlags = 0;
	Desc.ByteWidth = sizeof(WORLD_PHYSICS);
	hr = m_pD3DDevice->CreateBuffer(&Desc, &cbData, &m_pCBWorldPhysics);
	if(FAILED(hr)) return hr;
	D3DDEBUGNAME(m_pCBWorldPhysics, "WorldPhysics CB");

	// Frame variables will be set for each render frame
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Desc.MiscFlags = 0;
	Desc.ByteWidth = sizeof(FRAME_VARIABLES);
	hr = m_pD3DDevice->CreateBuffer(&Desc, NULL, &m_pCBFrameVariables);
	if(FAILED(hr)) return hr;
	D3DDEBUGNAME(m_pCBFrameVariables, "FrameVariables CB");

	// Spring configuration variables will be set on each randomize springs event
	Desc.ByteWidth = sizeof(SPRING_CONFIG);
	hr = m_pD3DDevice->CreateBuffer(&Desc, NULL, &m_pCBSpringConfig);
	if(FAILED(hr)) return hr;
	D3DDEBUGNAME(m_pCBSpringConfig, "SpringConfig CB");
	
	// Create the spring interaction texture look-up table and its read/write views
	UINT texSize = m_ConfigData.m_iParticleCount; // (UINT)ceil((float)(m_ConfigData.m_iParticleCount) / CS_BLOCKSIZE) * CS_BLOCKSIZE;
	CD3D11_TEXTURE2D_DESC texDesc(DXGI_FORMAT_R32_UINT, texSize, texSize, 1, 0, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	hr = m_pD3DDevice->CreateTexture2D(&texDesc, 0, &m_pTexSpringLookup);
	if(FAILED(hr)) return hr;
	D3DDEBUGNAME(m_pTexSpringLookup, "Spring Lookup Texture");
	
	hr = m_pD3DDevice->CreateShaderResourceView(m_pTexSpringLookup.Get(), 0, &m_pSRVSpringLookup);
	if(FAILED(hr)) return hr;
	D3DDEBUGNAME(m_pSRVSpringLookup, "Spring Lookup SRV");
	hr = m_pD3DDevice->CreateUnorderedAccessView(m_pTexSpringLookup.Get(), 0, &m_pUAVSpringLookup);
	if(FAILED(hr)) return hr;
	D3DDEBUGNAME(m_pUAVSpringLookup, "Spring Lookup UAV");

	// Load the particle drawing texture
	hr = LoadTexture(L"particle.dds", nullptr, &m_pSRVParticleDraw);
	if(FAILED(hr)) return hr;
	D3DDEBUGNAME(m_pSRVParticleDraw, "ParticleDraw SRV");

	// Set up the texture sampler, blend state, and depth/stencil state
    D3D11_SAMPLER_DESC SamplerDesc;
    ZeroMemory( &SamplerDesc, sizeof(SamplerDesc) );
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	hr = m_pD3DDevice->CreateSamplerState(&SamplerDesc, &m_pTextureSampler);
    if(FAILED(hr)) return hr;
	D3DDEBUGNAME(m_pTextureSampler, "Texture Sampler");

    D3D11_BLEND_DESC BlendStateDesc;
    ZeroMemory( &BlendStateDesc, sizeof(BlendStateDesc) );
    BlendStateDesc.RenderTarget[0].BlendEnable = TRUE;
    BlendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    BlendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
    BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = 0x0F;
	hr = m_pD3DDevice->CreateBlendState(&BlendStateDesc, &m_pRenderBlendState);
    if(FAILED(hr)) return hr;
	D3DDEBUGNAME(m_pRenderBlendState, "Render Blend State");

    D3D11_DEPTH_STENCIL_DESC DepthStencilDesc;
    ZeroMemory( &DepthStencilDesc, sizeof(DepthStencilDesc) );
    DepthStencilDesc.DepthEnable = FALSE;
    DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	hr = m_pD3DDevice->CreateDepthStencilState(&DepthStencilDesc, &m_pRenderDepthState);
	if(FAILED(hr)) return hr;
	D3DDEBUGNAME(m_pRenderDepthState, "Render Depth State");

	// Randomize spin axis
	RandomizeVector(&m_vecSpinAxis, 0.0f, 1.0f, true);

	// Set up view matrix
	float fDistance = m_ConfigData.fScale() * (m_ConfigData.m_bCentralForce ? 0.7f : 1.2f);
	XMVECTOR vecEye = XMLoadFloat4(&XMFLOAT4(0.0f, 0.0f, -fDistance, 1.0f));
	XMVECTOR vecLookAt = XMLoadFloat4(&XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
	XMVECTOR vecUp = XMLoadFloat4(&XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f));

	XMMATRIX matView = XMMatrixLookAtLH(vecEye, vecLookAt, vecUp);
	XMStoreFloat4x4(&m_varsFrame.g_matViewTrans, XMMatrixTranspose(matView));

	return hr;
}

BOOL CInteractSaver::OnResizeSaver()
{
	// The window resized, so recompute the projection matrix based on the new aspect ratio.
	XMMATRIX P = XMMatrixPerspectiveFovLH(XM_PIDIV2, m_fAspectRatio, 0.1f, m_ConfigData.fScale() * 10.0f);
	XMStoreFloat4x4(&m_varsFrame.g_matProjectionTrans, XMMatrixTranspose(P)); // XMMatrixInverse(&XMMatrixDeterminant(P), P));

	return TRUE;
}

BOOL CInteractSaver::PauseSaver()
{ 
	return TRUE;
}

BOOL CInteractSaver::ResumeSaver()
{
	return TRUE;
}

BOOL CInteractSaver::IterateSaver(float dt, float T)
{
	m_InitTasks.wait();

	SPRING_CONFIG *springConfig = nullptr;

	BOOL bResult = UpdateScene(dt, T, &springConfig);
	if(bResult)
	{
		bResult = ComputeScene(springConfig);
	}

	if(springConfig != nullptr)
	{
		delete springConfig;
		springConfig = nullptr;
	}

	if(bResult)
	{
		bResult = RenderScene();
	}

	return bResult;
}

BOOL CInteractSaver::UpdateScene(float dt, float T, SPRING_CONFIG **pSpringConfig)
{
	// Spin the world if necessary, periodically reconfigure the spring 
	// settings and set up the frame variables.
	if(m_ConfigData.m_bSpin)
	{
		float fAngle = -0.2f * dt;
		float fWobble = 0.127f * dt;
		XMVECTOR vAxis = XMLoadFloat4(&m_vecSpinAxis);
		vAxis = XMVector4Transform(vAxis, XMMatrixRotationZ(fWobble));
		XMStoreFloat4(&m_vecSpinAxis, vAxis);
		XMMATRIX newWorld = XMLoadFloat4x4(&m_matWorld) * XMMatrixRotationNormal(vAxis, fAngle);
		XMStoreFloat4x4(&m_matWorld, newWorld);
		XMStoreFloat4x4(&m_varsFrame.g_matWorldTrans, XMMatrixTranspose(newWorld));
	}

	m_varsFrame.g_fElapsedTime = dt;
	m_varsFrame.g_fGlobalTime = T;

	if(!m_bInitialRandomizeDone || (T - m_fLastRandomize > m_ConfigData.m_fRandomizeInterval))
	{
		RandomizeSprings(pSpringConfig);
		m_fLastRandomize = T;
		if(!m_bInitialRandomizeDone)
		{
			m_fLastRandomize -= m_ConfigData.m_fRandomizeInterval * (float)m_iSaverIndex / (float)m_iNumSavers;
			m_bInitialRandomizeDone = TRUE;
		}
	}

	return TRUE;
}

void CInteractSaver::RandomizeSprings(SPRING_CONFIG **pSpringConfig)
{
	assert(*pSpringConfig == nullptr);

	SPRING_CONFIG *pNewConfig = new SPRING_CONFIG;
	*pSpringConfig = pNewConfig;

	pNewConfig->g_iParticleCount = m_ConfigData.m_iParticleCount;
	pNewConfig->g_iPattern = (rand() % (int)NUM_PATTERNS);

	switch(pNewConfig->g_iPattern)
	{
	case 0: // Modulus
		pNewConfig->g_iModulus = rand() % (m_ConfigData.m_iParticleCount/2 + 1);
		pNewConfig->g_iAntiModulus = 0; // Not used for this pattern
		break;

	case 1: // String
		do
		{
			pNewConfig->g_iModulus = rand() % m_ConfigData.m_iParticleCount + 1;
		} while((m_ConfigData.m_iParticleCount  % pNewConfig->g_iModulus) != 0);
		pNewConfig->g_iAntiModulus = m_ConfigData.m_iParticleCount / pNewConfig->g_iModulus;
		break;

	case 2: // Sheet
		do
		{
			pNewConfig->g_iModulus = rand() % m_ConfigData.m_iParticleCount/2 + 1;
		} while((m_ConfigData.m_iParticleCount  % pNewConfig->g_iModulus) != 0);
		pNewConfig->g_iAntiModulus = m_ConfigData.m_iParticleCount / pNewConfig->g_iModulus;
		break;
	}
}

BOOL CInteractSaver::ComputeScene(const SPRING_CONFIG *springConfig)
{
	ID3D11UnorderedAccessView* pUAVNULL[1] = { nullptr };
	ID3D11ShaderResourceView* pSRVNULL[1] = { nullptr };

	// If springs have been newly randomized, need to compute the spring lookup matrix
	if(springConfig != nullptr)
	{
		m_pD3DContext->CSSetShader(m_pConfigSpringsCS.Get(), NULL, 0);

		// Map the spring configuration info into graphics memory and send it to the shader
		D3D11_MAPPED_SUBRESOURCE MappedResource;
		m_pD3DContext->Map(m_pCBSpringConfig.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
		CopyMemory(MappedResource.pData, springConfig, sizeof(SPRING_CONFIG));
		m_pD3DContext->Unmap(m_pCBSpringConfig.Get(), 0);
		m_pD3DContext->CSSetConstantBuffers(0, 1, m_pCBSpringConfig.GetAddressOf());

		// Bind the output view of the spring lookup table
		m_pD3DContext->CSSetUnorderedAccessViews(0, 1, m_pUAVSpringLookup.GetAddressOf(), nullptr);

		// Dispatch the compute shader in a 2D array - spring blocksize is 16
		UINT iNumBlocks = (UINT)(ceil((double)m_ConfigData.m_iParticleCount / 16.0));
		m_pD3DContext->Dispatch(iNumBlocks, iNumBlocks, 1);

		// Clean up our resources
		m_pD3DContext->CSSetUnorderedAccessViews(0, 1, pUAVNULL, nullptr); 
	}

	// Select the shader that calculates all the particle forces and then updates positions and velocities
	m_pD3DContext->CSSetShader(m_pPhysicsCS.Get(), NULL, 0);

	// Map the frame varables into graphics memory (this sets them up for the render geometry shader as well).
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	m_pD3DContext->Map(m_pCBFrameVariables.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	CopyMemory(MappedResource.pData, &m_varsFrame, sizeof(FRAME_VARIABLES));
	m_pD3DContext->Unmap(m_pCBFrameVariables.Get(), 0);

	// Set the constant buffers defining the world physics and the frame variables
	ID3D11Buffer *CSCBuffers[2] = {m_pCBWorldPhysics.Get(), m_pCBFrameVariables.Get() };
	m_pD3DContext->CSSetConstantBuffers(0, 2, CSCBuffers);

	// Input to the shader - spring constants and current particle kinematics
	ID3D11ShaderResourceView *CSRViews[2] = { m_pSRVSpringLookup.Get(), m_pSRVPosVel.Get() };
	m_pD3DContext->CSSetShaderResources(0, 2, CSRViews);

	// Output from the shader - updated particle kinematics
	m_pD3DContext->CSSetUnorderedAccessViews(0, 1, m_pUAVPosVelNext.GetAddressOf(), nullptr);

	// Run the compute shader - acts on a 1D array of particles
	m_pD3DContext->Dispatch(m_ConfigData.iNumBlocks(), 1, 1);

	// Unbind the resources
	m_pD3DContext->CSSetUnorderedAccessViews(0, 1, pUAVNULL, nullptr);
	m_pD3DContext->CSSetShaderResources(0, 1, pSRVNULL);

	// Swap the new kinematic results with the current ones for rendering
	SwapComPtr<ID3D11Buffer>(m_pSBPosVel, m_pSBPosVelNext);
	SwapComPtr<ID3D11ShaderResourceView>(m_pSRVPosVel, m_pSRVPosVelNext);
	SwapComPtr<ID3D11UnorderedAccessView>(m_pUAVPosVel, m_pUAVPosVelNext);

	return TRUE;
}

BOOL CInteractSaver::RenderScene()
{
	assert(m_pD3DContext && m_pSwapChain);

	float colorBlack[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	m_pD3DContext->ClearRenderTargetView(m_pRenderTargetView.Get(), colorBlack);
	m_pD3DContext->ClearDepthStencilView(m_pDepthStencilView.Get(), D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);

	// Cache initial state of drawing context
	ComPtr<ID3D11BlendState> oldBlendState;
	ComPtr<ID3D11DepthStencilState> oldDepthStencilState;
	UINT oldSampleMask, oldStencilRef;
	XMFLOAT4 oldBlendFactor;
	m_pD3DContext->OMGetBlendState(&oldBlendState, &oldBlendFactor.x, &oldSampleMask);
	m_pD3DContext->OMGetDepthStencilState(&oldDepthStencilState, &oldStencilRef);

	// Set the render shaders
	m_pD3DContext->VSSetShader(m_pRenderVS.Get(), NULL, 0);
	m_pD3DContext->GSSetShader(m_pRenderGS.Get(), NULL, 0);
	m_pD3DContext->PSSetShader(m_pRenderPS.Get(), NULL, 0);

	// Set IA parameters
	m_pD3DContext->IASetInputLayout(m_pRenderIL.Get());

	UINT stride = sizeof(COLOR_VERTEX);
	UINT offset = 0;
	m_pD3DContext->IASetVertexBuffers(0, 1, m_pVBParticleColors.GetAddressOf(), &stride, &offset);
	m_pD3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	// Set vertex shader resources
	m_pD3DContext->VSSetShaderResources(0, 1, m_pSRVPosVel.GetAddressOf());
		
	// Set geometry shader resources (note that frame variables should have been updated in the compute function)
	ID3D11Buffer *GSbuffers[2] = { m_pCBWorldPhysics.Get(), m_pCBFrameVariables.Get() };
	m_pD3DContext->GSSetConstantBuffers(0, 2, GSbuffers);

	// Set pixel shader resources
	m_pD3DContext->PSSetShaderResources(0, 1, m_pSRVParticleDraw.GetAddressOf());
	m_pD3DContext->PSSetSamplers(0, 1, m_pTextureSampler.GetAddressOf());
	m_pD3DContext->PSSetConstantBuffers(0, 1, m_pCBWorldPhysics.GetAddressOf());

	// Set OM parameters
	m_pD3DContext->OMSetBlendState(m_pRenderBlendState.Get(), colorBlack, 0xFFFFFFFF);
	m_pD3DContext->OMSetDepthStencilState(m_pRenderDepthState.Get(), 0);

	// Draw the particles
	m_pD3DContext->Draw(m_ConfigData.m_iParticleCount, 0);

	// Restore the initial state
	ID3D11ShaderResourceView* pSRVNULL[1] = { nullptr };
	m_pD3DContext->VSSetShaderResources(0, 1, pSRVNULL);
	m_pD3DContext->PSSetShaderResources(0, 1, pSRVNULL);
	m_pD3DContext->GSSetShader(NULL, NULL, 0);
	m_pD3DContext->OMSetBlendState(oldBlendState.Get(), &oldBlendFactor.x, oldSampleMask);
	m_pD3DContext->OMSetDepthStencilState(oldDepthStencilState.Get(), oldStencilRef);

	HRESULT hr = m_pSwapChain->Present(0, 0);

	return SUCCEEDED(hr);
}
