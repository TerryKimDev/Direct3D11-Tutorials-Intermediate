//--------------------------------------------------------------------------------------
// Tutorial 2-1
// File SimpleViewer.cpp derived from:
// File: Tutorial06.cpp
//
// This application demonstrates simple lighting in the vertex shader
//
// http://msdn.microsoft.com/en-us/library/windows/apps/ff729723.aspx
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <windows.h>
#include <d3d11_1.h>
#include <directxmath.h>
#include <directxcolors.h>
#include "resource.h"
#include <vector>
#include "MeshUtils.h"
#include "LineUtils.h"
#include "Renderable.h"

using namespace DirectX;
using namespace std;


//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------

struct ConstantBuffer
{
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProjection;
	XMFLOAT4 vLightDir[2];
	XMFLOAT4 vLightColor[2];
	XMFLOAT4 vOutputColor;
};

struct ConstantBufferDebug
{
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProjection;
};

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
bool RENDER_STYLE_WIREFRAME = false;
bool RENDER_STYLE_TEXTURED = true;
bool DEBUG_VIEW_ENABLED = true;

HINSTANCE               g_hInst = nullptr;
HWND                    g_hWnd = nullptr;
D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*			g_pd3dDevice = nullptr;
ID3D11Device1*			g_pd3dDevice1 = nullptr;
ID3D11DeviceContext*	g_pImmediateContext = nullptr;
ID3D11DeviceContext1*	g_pImmediateContext1 = nullptr;
IDXGISwapChain*			g_pSwapChain = nullptr;
IDXGISwapChain1*		g_pSwapChain1 = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11Texture2D*		g_pDepthStencil = nullptr;
ID3D11DepthStencilView* g_pDepthStencilView = nullptr;
ID3D11PixelShader*		g_pPixelShaderSolid = nullptr;
XMMATRIX                g_World;
XMMATRIX                g_View;
XMMATRIX                g_Projection;
ID3D11RasterizerState*	rasterStateDefault;
ID3D11RasterizerState*	rasterStateWireframe;
ID3D11DepthStencilState* pDSState;
ID3D11Buffer* debugLinesVertexBuffer;

// Main mesh
Renderable meshRenderable;

// Grid mesh
Renderable gridRenderable;

// Used for overriding the main mesh texture
// with a generated white pixel texture to
// simulate no texturing
ComPtr<ID3D11ShaderResourceView> texSRV;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT InitDevice();
HRESULT InitContent();
void CleanupDevice();
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void Update();
void Render();

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	if (FAILED(InitWindow(hInstance, nCmdShow)))
		return 0;

	if (FAILED(InitDevice()))
	{
		CleanupDevice();
		return 0;
	}

	if (FAILED(InitContent()))
	{
		CleanupDevice();
		return 0;
	}

	// Main message loop
	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			Update();
			Render();
		}
	}

	CleanupDevice();

	return (int)msg.wParam;
}


//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
	// Register class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_TUTORIAL1);
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"TutorialWindowClass";
	wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_TUTORIAL1);
	if (!RegisterClassEx(&wcex))
		return E_FAIL;

	// Create window
	g_hInst = hInstance;
	RECT rc = { 0, 0, 800, 600 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd = CreateWindow(L"TutorialWindowClass", L"Direct3D 11 Simple Viewer",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
		nullptr);
	if (!g_hWnd)
		return E_FAIL;

	ShowWindow(g_hWnd, nCmdShow);

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
	HRESULT hr = S_OK;

	RECT rc;
	GetClientRect(g_hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		g_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);

		if (hr == E_INVALIDARG)
		{
			// DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
			hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
				D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
		}

		if (SUCCEEDED(hr))
			break;
	}
	if (FAILED(hr))
		return hr;

	// Obtain DXGI factory from device (since we used nullptr for pAdapter above)
	IDXGIFactory1* dxgiFactory = nullptr;
	{
		IDXGIDevice* dxgiDevice = nullptr;
		hr = g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
		if (SUCCEEDED(hr))
		{
			IDXGIAdapter* adapter = nullptr;
			hr = dxgiDevice->GetAdapter(&adapter);
			if (SUCCEEDED(hr))
			{
				hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
				adapter->Release();
			}
			dxgiDevice->Release();
		}
	}
	if (FAILED(hr))
		return hr;

	// Create swap chain
	IDXGIFactory2* dxgiFactory2 = nullptr;
	hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2));
	if (dxgiFactory2)
	{
		// DirectX 11.1 or later
		hr = g_pd3dDevice->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&g_pd3dDevice1));
		if (SUCCEEDED(hr))
		{
			(void)g_pImmediateContext->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&g_pImmediateContext1));
		}

		DXGI_SWAP_CHAIN_DESC1 sd = {};
		sd.Width = width;
		sd.Height = height;
		sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = 1;

		hr = dxgiFactory2->CreateSwapChainForHwnd(g_pd3dDevice, g_hWnd, &sd, nullptr, nullptr, &g_pSwapChain1);
		if (SUCCEEDED(hr))
		{
			hr = g_pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&g_pSwapChain));
		}

		dxgiFactory2->Release();
	}
	else
	{
		// DirectX 11.0 systems
		DXGI_SWAP_CHAIN_DESC sd = {};
		sd.BufferCount = 1;
		sd.BufferDesc.Width = width;
		sd.BufferDesc.Height = height;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = g_hWnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;

		hr = dxgiFactory->CreateSwapChain(g_pd3dDevice, &sd, &g_pSwapChain);
	}

	// Note this tutorial doesn't handle full-screen swapchains so we block the ALT+ENTER shortcut
	dxgiFactory->MakeWindowAssociation(g_hWnd, DXGI_MWA_NO_ALT_ENTER);

	dxgiFactory->Release();

	if (FAILED(hr))
		return hr;

	// Create a render target view
	ID3D11Texture2D* pBackBuffer = nullptr;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
	if (FAILED(hr))
		return hr;

	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
		return hr;

	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC descDepth = {};
	descDepth.Width = width;
	descDepth.Height = height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = g_pd3dDevice->CreateTexture2D(&descDepth, nullptr, &g_pDepthStencil);
	if (FAILED(hr))
		return hr;

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
	if (FAILED(hr))
		return hr;

	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pImmediateContext->RSSetViewports(1, &vp);

	// Initialize the projection matrix
	g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, width / (FLOAT)height, 0.01f, 1000.0f);

	return hr;
}

void InitDebugTexture()
{
	static const uint32_t s_pixel = 0xffffffff;

	D3D11_SUBRESOURCE_DATA initData = { &s_pixel, sizeof(uint32_t), 0 };

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = desc.Height = desc.MipLevels = desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	ComPtr<ID3D11Texture2D> tex;
	HRESULT hr = g_pd3dDevice->CreateTexture2D(&desc, &initData, tex.GetAddressOf());

	if (SUCCEEDED(hr))
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MipLevels = 1;

		hr = g_pd3dDevice->CreateShaderResourceView(tex.Get(),
			&SRVDesc, texSRV.GetAddressOf());
	}
	assert(!FAILED(hr));
}

void InitRasterizerStates()
{
	D3D11_RASTERIZER_DESC rasterDesc;
	ZeroMemory(&rasterDesc, sizeof(rasterDesc));

	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.CullMode = D3D11_CULL_NONE;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.FillMode = D3D11_FILL_WIREFRAME;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.ScissorEnable = false;
	rasterDesc.SlopeScaledDepthBias = 0.0f;

	HRESULT hr = g_pd3dDevice->CreateRasterizerState(&rasterDesc, &rasterStateWireframe);
	assert(!FAILED(hr));

	ZeroMemory(&rasterDesc, sizeof(rasterDesc));

	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.CullMode = D3D11_CULL_BACK;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.ScissorEnable = false;
	rasterDesc.SlopeScaledDepthBias = 0.0f;

	hr = g_pd3dDevice->CreateRasterizerState(&rasterDesc, &rasterStateDefault);
	assert(!FAILED(hr));
}

void InitDepthStates()
{
	D3D11_DEPTH_STENCIL_DESC dsDesc;

	// Depth test parameters
	dsDesc.DepthEnable = true;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS;

	// Stencil test parameters
	dsDesc.StencilEnable = true;
	dsDesc.StencilReadMask = 0xFF;
	dsDesc.StencilWriteMask = 0xFF;

	// Stencil operations if pixel is front-facing
	dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Stencil operations if pixel is back-facing
	dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	dsDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Create depth stencil state
	g_pd3dDevice->CreateDepthStencilState(&dsDesc, &pDSState);
}

HRESULT InitContent()
{
	InitDebugTexture();
	InitRasterizerStates();
	InitDepthStates();

	HRESULT hr = S_OK;

	//////////////////////////////////////////
	//Create mesh render components
	//////////////////////////////////////////
	{
		// Generate the geometry
		SimpleMesh<SimpleVertex> mesh;
		MeshUtils::makeCubePNT(mesh);

		// Create the vertex buffers from the generated SimpleMesh
		hr = meshRenderable.CreateBuffers(
			g_pd3dDevice,
			mesh.indicesList,
			(float*)mesh.vertexList.data(),
			sizeof(SimpleVertex),
			mesh.vertexList.size());

		// Load the Texture
		std::string filename = "seafloor.dds";
		hr = meshRenderable.CreateTextureFromFile(g_pd3dDevice, filename);
		if (FAILED(hr))
			return hr;

		// Create the sampler state
		hr = meshRenderable.CreateDefaultSampler(g_pd3dDevice);

		// Define the input layout
		D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		// Create the shaders
		hr = meshRenderable.CreateVertexShaderAndInputLayoutFromFile(g_pd3dDevice, "Tutorial06_VS.cso", layout, ARRAYSIZE(layout));
		hr = meshRenderable.CreatePixelShaderFromFile(g_pd3dDevice, "Tutorial06_PS.cso");

		// Create the shader constant buffer
		hr = meshRenderable.CreateConstantBuffer(g_pd3dDevice, sizeof(ConstantBuffer));
	}

	// Create grid render components
	{
		// Generate the geometry
		DebugLines lines;
		LineUtils::MakeGrid(lines);

		// Create the vertex buffers from the lines vector
		hr = gridRenderable.CreateVertexBuffer(g_pd3dDevice, (float*)lines.vertexList.data(), sizeof(ColorVertex), (int)lines.vertexList.size());

		gridRenderable.primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;

		// Define the input layout
		D3D11_INPUT_ELEMENT_DESC lineLayoutDesc[] =
		{
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
		};

		// Create the shaders
		hr = gridRenderable.CreateVertexShaderAndInputLayoutFromFile(g_pd3dDevice, "Debug_VS.cso", lineLayoutDesc, ARRAYSIZE(lineLayoutDesc));
		hr = gridRenderable.CreatePixelShaderFromFile(g_pd3dDevice, "Debug_PS.cso");

		// Create the shader constant buffer
		hr = gridRenderable.CreateConstantBuffer(g_pd3dDevice, sizeof(ConstantBufferDebug));
	}

	// load and create the pixel shader for the light markers
	auto ps_blob = load_binary_blob("PSSolid.cso");
	hr = g_pd3dDevice->CreatePixelShader(ps_blob.data(), ps_blob.size(), nullptr, &g_pPixelShaderSolid);
	if (FAILED(hr))
		return hr;

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
	if (g_pImmediateContext) g_pImmediateContext->ClearState();
	if (rasterStateDefault) rasterStateDefault->Release();
	if (rasterStateWireframe) rasterStateWireframe->Release();
	if (g_pPixelShaderSolid) g_pPixelShaderSolid->Release();
	if (g_pDepthStencil) g_pDepthStencil->Release();
	if (g_pDepthStencilView) g_pDepthStencilView->Release();
	if (g_pRenderTargetView) g_pRenderTargetView->Release();
	if (g_pSwapChain1) g_pSwapChain1->Release();
	if (g_pSwapChain) g_pSwapChain->Release();
	if (g_pImmediateContext1) g_pImmediateContext1->Release();
	if (g_pImmediateContext) g_pImmediateContext->Release();
	if (g_pd3dDevice1) g_pd3dDevice1->Release();
	if (g_pd3dDevice) g_pd3dDevice->Release();
	if (pDSState) pDSState->Release();
	if (debugLinesVertexBuffer) debugLinesVertexBuffer->Release();
}


//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_KEYDOWN:
		switch (wParam)
		{
		case '1':
			cout << "Keypressed - 1" << endl;
			// toggle wireframe state
			RENDER_STYLE_WIREFRAME = !RENDER_STYLE_WIREFRAME;
			cout << "RENDER_STYLE_WIREFRAME: " << RENDER_STYLE_WIREFRAME << endl;
			break;
		case '2':
			cout << "Keypressed - 2" << endl;
			// toggle textured state
			RENDER_STYLE_TEXTURED = !RENDER_STYLE_TEXTURED;
			cout << "RENDER_STYLE_TEXTURED: " << RENDER_STYLE_TEXTURED << endl;
			break;
		}
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}



XMFLOAT4 vLightDirs[2] =
{
	XMFLOAT4(-0.577f, 0.577f, -0.577f, 1.0f),
	XMFLOAT4(0.0f, 0.0f, -1.0f, 1.0f),
};
XMFLOAT4 vLightColors[2] =
{
	XMFLOAT4(0.75f, 0.75f, 0.75f, 1.0f),
	XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)
};

//--------------------------------------------------------------------------------------
// Update
//--------------------------------------------------------------------------------------
void Update()
{
	// Update our time
	static float t = 0.0f;
	if (g_driverType == D3D_DRIVER_TYPE_REFERENCE)
	{
		t += (float)XM_PI * 0.0125f;
	}
	else
	{
		static ULONGLONG timeStart = 0;
		ULONGLONG timeCur = GetTickCount64();
		if (timeStart == 0)
			timeStart = timeCur;
		t = (timeCur - timeStart) / 1000.0f;
	}

	// Rotate cube around the origin
	g_World = XMMatrixRotationY(t);
	// Initialize the world matrices
	//g_World = XMMatrixIdentity();

	// Initialize the view matrix
	// Stationary camera
	XMVECTOR Eye = XMVectorSet(0.0f, 4.0f, -10.0f, 0.0f);
	// Orbit camera
	//XMVECTOR Eye = XMVectorSet(cos(t / 2) * 10, 4.0f, sin(t / 2) * 10, 0.0f);
	XMVECTOR At = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	g_View = XMMatrixLookAtLH(Eye, At, Up);


	// Setup our lighting parameters
	XMStoreFloat4(&vLightDirs[0], { -0.577f, 0.577f, -0.577f, 1.0f });
	XMStoreFloat4(&vLightDirs[1], { 0.577f, 0.2577f, -0.577f, 1.0f });

	XMStoreFloat4(&vLightColors[0], { 0.75f, 0.75f, 0.75f, 1.0f });
	XMStoreFloat4(&vLightColors[1], { 0.2f, 0.2f, 0.5f, 1.0f });

	// Rotate the second light around the origin
	XMMATRIX mRotate = XMMatrixRotationY(-1.0f * t);
	XMVECTOR vLightDir = XMLoadFloat4(&vLightDirs[1]);
	// rotates the second light
	//vLightDir = XMVector3Transform(vLightDir, mRotate);
	XMStoreFloat4(&vLightDirs[1], vLightDir);
}

//--------------------------------------------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------
void Render()
{
	//
	// Clear the back buffer
	//
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, Colors::MidnightBlue);

	//
	// Clear the depth buffer to 1.0 (max depth)
	//
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	// Set Depth write on
	g_pImmediateContext->OMSetDepthStencilState(pDSState, 1);

	//
	// Render the grid
	//
	ConstantBufferDebug cbDebug;
	cbDebug.mWorld = XMMatrixIdentity();
	cbDebug.mView = XMMatrixTranspose(g_View);
	cbDebug.mProjection = XMMatrixTranspose(g_Projection);
	g_pImmediateContext->UpdateSubresource(gridRenderable.constantBuffer.Get(), 0, nullptr, &cbDebug, 0, 0);

	gridRenderable.Bind(g_pImmediateContext);
	gridRenderable.Draw(g_pImmediateContext);

	//
	// Update matrix variables and lighting variables
	//
	ConstantBuffer cb1;

	// TODO:POSITION CUBE
	// raise the bottom of the cube up to the origin
	// comment in
	g_World.r[3] = { 0.0f, 1.0f, 0.0f, 1.0f };

	cb1.mWorld = XMMatrixTranspose(g_World);
	cb1.mView = XMMatrixTranspose(g_View);
	cb1.mProjection = XMMatrixTranspose(g_Projection);
	cb1.vLightDir[0] = vLightDirs[0];
	cb1.vLightDir[1] = vLightDirs[1];
	cb1.vLightColor[0] = vLightColors[0];
	cb1.vLightColor[1] = vLightColors[1];
	cb1.vOutputColor = XMFLOAT4(0, 0, 0, 0);
	g_pImmediateContext->UpdateSubresource(meshRenderable.constantBuffer.Get(), 0, nullptr, &cb1, 0, 0);
	
	// TODO:RASTER STATE SOLID 
	g_pImmediateContext->RSSetState(rasterStateDefault);


	//
	// Render the cube
	//
	meshRenderable.Bind(g_pImmediateContext);

	// set the solid raster state
	g_pImmediateContext->RSSetState(rasterStateDefault);

	// if not textured set, set the current texture to the generated white texture
	if (!RENDER_STYLE_TEXTURED)
	{
		g_pImmediateContext->PSSetShaderResources(0, 1, texSRV.GetAddressOf());
	}

	// Draw the mesh
	meshRenderable.Draw(g_pImmediateContext);

	if (RENDER_STYLE_WIREFRAME)
	{
		// Set the color of the wireframe
		cb1.vOutputColor = { 0.9f, 0.9f, 0.9f, 1.0f };
		g_pImmediateContext->UpdateSubresource(meshRenderable.constantBuffer.Get(), 0, nullptr, &cb1, 0, 0);

		// set the solid pixel shader
		g_pImmediateContext->PSSetShader(g_pPixelShaderSolid, nullptr, 0);

		// set the wireframe raster state
		g_pImmediateContext->RSSetState(rasterStateWireframe);
	}
	//else
	//{
	//	// set the solid raster state
	//	g_pImmediateContext->RSSetState(rasterStateDefault);
	//}
	// Draw the mesh
	meshRenderable.Draw(g_pImmediateContext);



	//
	//  TODO:EXTRA CUBES
	//	Render the cube two more times here
	// 
	if (false)
	{
		// Leave commented out until wireframe color part of the exercise
		// TODO:WIREFRAME COLOR
		cb1.vOutputColor = { 1.0f, 1.0f, 1.0f, 0.5f };
		g_pImmediateContext->PSSetShader(g_pPixelShaderSolid, nullptr, 0);

		// Leave commented out until Raster State part of the exercise
		// TODO:RASTER STATE WIREFRAME
		g_pImmediateContext->RSSetState(rasterStateWireframe);

		g_World.r[3] = { 3.0f, 1.0f, 0.0f, 1.0f };
		cb1.mWorld = XMMatrixTranspose(g_World);
		g_pImmediateContext->UpdateSubresource(meshRenderable.constantBuffer.Get(), 0, nullptr, &cb1, 0, 0);
		meshRenderable.Draw(g_pImmediateContext);

		g_World.r[3] = { -3.0f, 1.0f, 0.0f, 1.0f };
		cb1.mWorld = XMMatrixTranspose(g_World);
		g_pImmediateContext->UpdateSubresource(meshRenderable.constantBuffer.Get(), 0, nullptr, &cb1, 0, 0);
		meshRenderable.Draw(g_pImmediateContext);
	}

	//
	// Present our back buffer to our front buffer
	//
	g_pSwapChain->Present(0, 0);
}