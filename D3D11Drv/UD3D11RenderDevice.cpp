
#include "Precomp.h"
#include "UD3D11RenderDevice.h"
#include "UTF16.h"
#include "FileResource.h"
#include <set>

IMPLEMENT_CLASS(UD3D11RenderDevice);

UD3D11RenderDevice::UD3D11RenderDevice()
{
}

void UD3D11RenderDevice::StaticConstructor()
{
	guard(UD3D11RenderDevice::StaticConstructor);

	SpanBased = 0;
	FullscreenOnly = 0;
	SupportsFogMaps = 1;
	SupportsDistanceFog = 0;
	SupportsTC = 1;
	SupportsLazyTextures = 0;
	PrefersDeferredLoad = 0;
	UseVSync = 1;
	Multisample = 0;
	UsePrecache = 0;

	UseLightmapAtlas = 0;
	SupportsUpdateTextureRect = 1;
	MaxTextureSize = 4096;
	NeedsMaskedFonts = 0;

	D3DBrightness = 0.0f;
	D3DContrast = 1.0f;
	D3DSaturation = 1.0f;
	D3DGrayFormula = 1;

	new(GetClass(), TEXT("UseLightmapAtlas"), RF_Public) UBoolProperty(CPP_PROPERTY(UseLightmapAtlas), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("UseVSync"), RF_Public) UBoolProperty(CPP_PROPERTY(UseVSync), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("UsePrecache"), RF_Public) UBoolProperty(CPP_PROPERTY(UsePrecache), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("Multisample"), RF_Public) UIntProperty(CPP_PROPERTY(Multisample), TEXT("Display"), CPF_Config);

	new(GetClass(), TEXT("D3DBrightness"), RF_Public) UFloatProperty(CPP_PROPERTY(D3DBrightness), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("D3DContrast"), RF_Public) UFloatProperty(CPP_PROPERTY(D3DContrast), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("D3DSaturation"), RF_Public) UFloatProperty(CPP_PROPERTY(D3DSaturation), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("D3DGrayFormula"), RF_Public) UIntProperty(CPP_PROPERTY(D3DGrayFormula), TEXT("Display"), CPF_Config);

	unguard;
}

UBOOL UD3D11RenderDevice::Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	guard(UD3D11RenderDevice::Init);

	Viewport = InViewport;

	try
	{
		std::vector<D3D_FEATURE_LEVEL> featurelevels =
		{
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0
		};

		DXGI_SWAP_CHAIN_DESC swapDesc = {};
		swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapDesc.BufferDesc.Width = NewX;
		swapDesc.BufferDesc.Height = NewY;
		swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapDesc.BufferCount = 2;
		swapDesc.SampleDesc.Count = 1;
		swapDesc.OutputWindow = (HWND)Viewport->GetWindow();
		swapDesc.Windowed = Fullscreen ? FALSE : TRUE;
		swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		HRESULT result = D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			0,
			0/*D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT*/,
			featurelevels.data(), (UINT)featurelevels.size(),
			D3D11_SDK_VERSION,
			&swapDesc,
			&SwapChain,
			&Device,
			&FeatureLevel,
			&Context);
		ThrowIfFailed(result, "D3D11CreateDeviceAndSwapChain failed");

		result = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&BackBuffer);
		ThrowIfFailed(result, "SwapChain.GetBuffer failed");

		result = Device->CreateRenderTargetView(BackBuffer, nullptr, &BackBufferView);
		ThrowIfFailed(result, "CreateRenderTargetView(BackBuffer) failed");

		CreateScenePass();
		CreatePresentPass();

		Textures.reset(new TextureManager(this));
		Uploads.reset(new UploadManager(this));

		ResizeSceneBuffers(NewX, NewY);
	}
	catch (const std::exception& e)
	{
		debugf(TEXT("Could not create d3d11 renderer: %s"), to_utf16(e.what()).c_str());
		Exit();
		return 0;
	}

	if (!SetRes(NewX, NewY, NewColorBytes, Fullscreen))
	{
		Exit();
		return 0;
	}

	return 1;
	unguard;
}

UBOOL UD3D11RenderDevice::SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	guard(UD3D11RenderDevice::SetRes);

	if (SceneBuffers.Width != NewX || SceneBuffers.Height != NewY)
	{
		HRESULT result = SwapChain->ResizeBuffers(2, NewX, NewY, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
		if (FAILED(result))
			return FALSE;
	}

	if (!Viewport->ResizeViewport(Fullscreen ? (BLIT_Fullscreen | BLIT_Direct3D) : (BLIT_HardwarePaint | BLIT_Direct3D), NewX, NewY, NewColorBytes))
		return 0;

	ResizeSceneBuffers(NewX, NewY);

	HRESULT result = SwapChain->SetFullscreenState(Fullscreen ? TRUE : FALSE, nullptr);
	if (FAILED(result))
		return FALSE;

	SaveConfig();

	Flush(1);

	return 1;
	unguard;
}

void UD3D11RenderDevice::Exit()
{
	guard(UD3D11RenderDevice::Exit);

	Uploads.reset();
	Textures.reset();
	ReleaseObject(PresentPass.PPStepLayout);
	ReleaseObject(PresentPass.PPStep);
	ReleaseObject(PresentPass.PPStepVertexBuffer);
	ReleaseObject(PresentPass.Present);
	ReleaseObject(PresentPass.PresentConstantBuffer);
	ReleaseObject(PresentPass.DitherTextureView);
	ReleaseObject(PresentPass.DitherTexture);
	ReleaseObject(PresentPass.BlendState);
	ReleaseObject(PresentPass.DepthStencilState);
	ReleaseObject(ScenePass.VertexShader);
	ReleaseObject(ScenePass.InputLayout);
	ReleaseObject(ScenePass.VertexBuffer);
	ReleaseObject(ScenePass.IndexBuffer);
	ReleaseObject(ScenePass.ConstantBuffer);
	ReleaseObject(ScenePass.RasterizerState);
	ReleaseObject(ScenePass.PixelShader);
	ReleaseObject(ScenePass.PixelShaderAlphaTest);
	for (auto& sampler : ScenePass.Samplers)
	{
		ReleaseObject(sampler);
	}
	for (auto& pipeline : ScenePass.Pipelines)
	{
		ReleaseObject(pipeline.BlendState);
		ReleaseObject(pipeline.DepthStencilState);
	}
	ReleaseObject(SceneBuffers.ColorBufferView);
	ReleaseObject(SceneBuffers.DepthBufferView);
	ReleaseObject(SceneBuffers.PPImageShaderView);
	ReleaseObject(SceneBuffers.PPImageView);
	ReleaseObject(SceneBuffers.ColorBuffer);
	ReleaseObject(SceneBuffers.DepthBuffer);
	ReleaseObject(SceneBuffers.PPImage);
	ReleaseObject(BackBufferView);
	ReleaseObject(BackBuffer);
	ReleaseObject(SwapChain);
	ReleaseObject(Context);
	ReleaseObject(Device);

	unguard;
}

void UD3D11RenderDevice::ResizeSceneBuffers(int width, int height)
{
	if (SceneBuffers.Width == width && SceneBuffers.Height == height && !SceneBuffers.ColorBuffer && !SceneBuffers.DepthBuffer && !SceneBuffers.PPImage)
		return;

	ReleaseObject(SceneBuffers.ColorBufferView);
	ReleaseObject(SceneBuffers.DepthBufferView);
	ReleaseObject(SceneBuffers.PPImageShaderView);
	ReleaseObject(SceneBuffers.PPImageView);
	ReleaseObject(SceneBuffers.ColorBuffer);
	ReleaseObject(SceneBuffers.DepthBuffer);
	ReleaseObject(SceneBuffers.PPImage);

	SceneBuffers.Width = width;
	SceneBuffers.Height = height;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
	texDesc.Width = SceneBuffers.Width;
	texDesc.Height = SceneBuffers.Height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	texDesc.SampleDesc.Count = std::max(Multisample, 1);
	texDesc.SampleDesc.Quality = Multisample > 1 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0;
	HRESULT result = Device->CreateTexture2D(&texDesc, nullptr, &SceneBuffers.ColorBuffer);
	ThrowIfFailed(result, "CreateTexture2D(ColorBuffer) failed");

	texDesc = {};
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	texDesc.Width = SceneBuffers.Width;
	texDesc.Height = SceneBuffers.Height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_D32_FLOAT;
	texDesc.SampleDesc.Count = std::max(Multisample, 1);
	texDesc.SampleDesc.Quality = Multisample > 1 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0;
	result = Device->CreateTexture2D(&texDesc, nullptr, &SceneBuffers.DepthBuffer);
	ThrowIfFailed(result, "CreateTexture2D(DepthBuffer) failed");

	texDesc = {};
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.Width = SceneBuffers.Width;
	texDesc.Height = SceneBuffers.Height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	result = Device->CreateTexture2D(&texDesc, nullptr, &SceneBuffers.PPImage);
	ThrowIfFailed(result, "CreateTexture2D(PPImage) failed");

	result = Device->CreateRenderTargetView(SceneBuffers.ColorBuffer, nullptr, &SceneBuffers.ColorBufferView);
	ThrowIfFailed(result, "CreateRenderTargetView(ColorBuffer) failed");

	result = Device->CreateDepthStencilView(SceneBuffers.DepthBuffer, nullptr, &SceneBuffers.DepthBufferView);
	ThrowIfFailed(result, "CreateDepthStencilView(DepthBuffer) failed");

	result = Device->CreateRenderTargetView(SceneBuffers.PPImage, nullptr, &SceneBuffers.PPImageView);
	ThrowIfFailed(result, "CreateRenderTargetView(PPImage) failed");

	result = Device->CreateShaderResourceView(SceneBuffers.PPImage, nullptr, &SceneBuffers.PPImageShaderView);
	ThrowIfFailed(result, "CreateShaderResourceView(PPImage) failed");
}

void UD3D11RenderDevice::CreateScenePass()
{
	std::vector<uint8_t> vscode = CompileHlsl("shaders/Scene.vert", "vs");
	HRESULT result = Device->CreateVertexShader(vscode.data(), vscode.size(), nullptr, &ScenePass.VertexShader);
	ThrowIfFailed(result, "CreateVertexShader(ScenePass.VertexShader) failed");

	std::vector<D3D11_INPUT_ELEMENT_DESC> elements =
	{
		{ "AttrFlags", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SceneVertex, Flags), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "AttrPos", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(SceneVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "AttrTexCoord", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SceneVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "AttrTexCoord2", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SceneVertex, TexCoord2), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "AttrTexCoord3", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SceneVertex, TexCoord3), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "AttrTexCoord4", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SceneVertex, TexCoord4), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "AttrColor", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(SceneVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	result = Device->CreateInputLayout(elements.data(), (UINT)elements.size(), vscode.data(), vscode.size(), &ScenePass.InputLayout);
	ThrowIfFailed(result, "CreateInputLayout(ScenePass.InputLayout) failed");

	std::vector<uint8_t> pscode = CompileHlsl("shaders/Present.frag", "ps");
	result = Device->CreatePixelShader(pscode.data(), pscode.size(), nullptr, &ScenePass.PixelShader);
	ThrowIfFailed(result, "CreatePixelShader(ScenePass.PixelShader) failed");

	std::vector<uint8_t> pscodeAT = CompileHlsl("shaders/Present.frag", "ps", { "ALPHATEST" });
	result = Device->CreatePixelShader(pscodeAT.data(), pscodeAT.size(), nullptr, &ScenePass.PixelShaderAlphaTest);
	ThrowIfFailed(result, "CreatePixelShader(ScenePass.PixelShaderAlphaTest) failed");

	for (int i = 0; i < 4; i++)
	{
		D3D11_FILTER filter = (i & 1) ? D3D11_FILTER_MIN_MAG_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		D3D11_TEXTURE_ADDRESS_MODE addressmode = (i & 2) ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.MinLOD = -FLT_MAX;
		samplerDesc.MaxLOD = FLT_MAX;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		samplerDesc.BorderColor[0] = 1.0f;
		samplerDesc.BorderColor[1] = 1.0f;
		samplerDesc.BorderColor[2] = 1.0f;
		samplerDesc.BorderColor[3] = 1.0f;
		samplerDesc.MaxAnisotropy = 8.0f;
		samplerDesc.MipLODBias = -0.5f;
		samplerDesc.Filter = filter;
		samplerDesc.AddressU = addressmode;
		samplerDesc.AddressV = addressmode;
		samplerDesc.AddressW = addressmode;
		Device->CreateSamplerState(&samplerDesc, &ScenePass.Samplers[i]);
	}

	D3D11_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.FrontCounterClockwise = FALSE;
	rasterizerDesc.DepthClipEnable = FALSE; // Avoid clipping the weapon. The UE1 engine clips the geometry anyway.
	rasterizerDesc.MultisampleEnable = Multisample > 1 ? TRUE : FALSE;
	result = Device->CreateRasterizerState(&rasterizerDesc, &ScenePass.RasterizerState);
	ThrowIfFailed(result, "CreateRasterizerState(ScenePass.Pipelines.RasterizerState) failed");

	for (int i = 0; i < 32; i++)
	{
		D3D11_BLEND_DESC blendDesc = {};
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		switch (i & 3)
		{
		case 0: // PF_Translucent
			blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_COLOR;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
			break;
		case 1: // PF_Modulated
			blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_COLOR;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_DEST_ALPHA;
			blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_COLOR;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
			break;
		case 2: // PF_Highlighted
			blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
			break;
		case 3: // Hmm, is it faster to keep the blend mode enabled or to toggle it?
			blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
			break;
		}
		if (i & 4) // PF_Invisible
			blendDesc.RenderTarget[0].RenderTargetWriteMask = 0;
		else
			blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		result = Device->CreateBlendState(&blendDesc, &ScenePass.Pipelines[i].BlendState);
		ThrowIfFailed(result, "CreateBlendState(ScenePass.Pipelines.BlendState) failed");

		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		if (i & 8) // PF_Occlude
			depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		else
			depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		result = Device->CreateDepthStencilState(&depthStencilDesc, &ScenePass.Pipelines[i].DepthStencilState);
		ThrowIfFailed(result, "CreateDepthStencilState(ScenePass.Pipelines.DepthStencilState) failed");

		if (i & 16) // PF_Masked
			ScenePass.Pipelines[i].PixelShader = ScenePass.PixelShaderAlphaTest;
		else
			ScenePass.Pipelines[i].PixelShader = ScenePass.PixelShader;
	}

	D3D11_BUFFER_DESC bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufDesc.ByteWidth = SceneVertexBufferSize * sizeof(SceneVertex);
	bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	result = Device->CreateBuffer(&bufDesc, nullptr, &ScenePass.VertexBuffer);
	ThrowIfFailed(result, "CreateBuffer(ScenePass.VertexBuffer) failed");

	bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufDesc.ByteWidth = SceneIndexBufferSize * sizeof(uint32_t);
	bufDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	result = Device->CreateBuffer(&bufDesc, nullptr, &ScenePass.IndexBuffer);
	ThrowIfFailed(result, "CreateBuffer(ScenePass.IndexBuffer) failed");

	bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_DEFAULT;
	bufDesc.ByteWidth = sizeof(ScenePushConstants);
	bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	result = Device->CreateBuffer(&bufDesc, nullptr, &ScenePass.ConstantBuffer);
	ThrowIfFailed(result, "CreateBuffer(ScenePass.ConstantBuffer) failed");
}

UD3D11RenderDevice::ScenePipelineState* UD3D11RenderDevice::GetPipeline(DWORD PolyFlags)
{
	// Adjust PolyFlags according to Unreal's precedence rules.
	if (!(PolyFlags & (PF_Translucent | PF_Modulated)))
		PolyFlags |= PF_Occlude;
	else if (PolyFlags & PF_Translucent)
		PolyFlags &= ~PF_Masked;

	int index;
	if (PolyFlags & PF_Translucent)
	{
		index = 0;
	}
	else if (PolyFlags & PF_Modulated)
	{
		index = 1;
	}
	else if (PolyFlags & PF_Highlighted)
	{
		index = 2;
	}
	else
	{
		index = 3;
	}

	if (PolyFlags & PF_Invisible)
	{
		index |= 4;
	}
	if (PolyFlags & PF_Occlude)
	{
		index |= 8;
	}
	if (PolyFlags & PF_Masked)
	{
		index |= 16;
	}

	return &ScenePass.Pipelines[index];
}

void UD3D11RenderDevice::CreatePresentPass()
{
	std::vector<vec2> positions =
	{
		vec2(-1.0, -1.0),
		vec2( 1.0, -1.0),
		vec2(-1.0,  1.0),
		vec2(-1.0,  1.0),
		vec2( 1.0, -1.0),
		vec2( 1.0,  1.0)
	};

	D3D11_BUFFER_DESC bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufDesc.ByteWidth = positions.size() * sizeof(vec2);
	bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = positions.data();

	HRESULT result = Device->CreateBuffer(&bufDesc, &initData, &PresentPass.PPStepVertexBuffer);
	ThrowIfFailed(result, "CreateBuffer(PresentPass.PPStepVertexBuffer) failed");

	bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_DEFAULT;
	bufDesc.ByteWidth = sizeof(PresentPushConstants);
	bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	result = Device->CreateBuffer(&bufDesc, nullptr, &PresentPass.PresentConstantBuffer);
	ThrowIfFailed(result, "CreateBuffer(PresentPass.PresentConstantBuffer) failed");

	std::vector<uint8_t> ppstep = CompileHlsl("shaders/PPStep.vert", "vs");
	result = Device->CreateVertexShader(ppstep.data(), ppstep.size(), nullptr, &PresentPass.PPStep);
	ThrowIfFailed(result, "CreateVertexShader(PresentPass.PPStep) failed");

	std::vector<uint8_t> present = CompileHlsl("shaders/Present.frag", "ps");
	result = Device->CreatePixelShader(present.data(), present.size(), nullptr, &PresentPass.Present);
	ThrowIfFailed(result, "CreatePixelShader(PresentPass.Present) failed");

	std::vector<D3D11_INPUT_ELEMENT_DESC> elements =
	{
		{ "AttrPos", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	result = Device->CreateInputLayout(elements.data(), (UINT)elements.size(), ppstep.data(), ppstep.size(), &PresentPass.PPStepLayout);
	ThrowIfFailed(result, "CreateInputLayout(PresentPass.PPStepLayout) failed");

	static const float ditherdata[64] =
	{
		.0078125, .2578125, .1328125, .3828125, .0234375, .2734375, .1484375, .3984375,
		.7578125, .5078125, .8828125, .6328125, .7734375, .5234375, .8984375, .6484375,
		.0703125, .3203125, .1953125, .4453125, .0859375, .3359375, .2109375, .4609375,
		.8203125, .5703125, .9453125, .6953125, .8359375, .5859375, .9609375, .7109375,
		.0390625, .2890625, .1640625, .4140625, .0546875, .3046875, .1796875, .4296875,
		.7890625, .5390625, .9140625, .6640625, .8046875, .5546875, .9296875, .6796875,
		.1015625, .3515625, .2265625, .4765625, .1171875, .3671875, .2421875, .4921875,
		.8515625, .6015625, .9765625, .7265625, .8671875, .6171875, .9921875, .7421875
	};

	initData = {};
	initData.pSysMem = ditherdata;
	initData.SysMemPitch = sizeof(float) * 8;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Usage = D3D11_USAGE_IMMUTABLE;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.Format = DXGI_FORMAT_R32_FLOAT;
	texDesc.Width = 8;
	texDesc.Height = 8;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.SampleDesc.Count = 1;
	result = Device->CreateTexture2D(&texDesc, &initData, &PresentPass.DitherTexture);
	ThrowIfFailed(result, "CreateTexture2D(DitherTexture) failed");

	result = Device->CreateShaderResourceView(PresentPass.DitherTexture, nullptr, &PresentPass.DitherTextureView);
	ThrowIfFailed(result, "CreateShaderResourceView(DitherTexture) failed");

	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	result = Device->CreateBlendState(&blendDesc, &PresentPass.BlendState);
	ThrowIfFailed(result, "CreateBlendState(PresentPass.BlendState) failed");

	D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	result = Device->CreateDepthStencilState(&depthStencilDesc, &PresentPass.DepthStencilState);
	ThrowIfFailed(result, "CreateDepthStencilState(PresentPass.DepthStencilState) failed");

	D3D11_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	result = Device->CreateRasterizerState(&rasterizerDesc, &PresentPass.RasterizerState);
	ThrowIfFailed(result, "CreateRasterizerState(PresentPass.RasterizerState) failed");
}

void UD3D11RenderDevice::Flush(UBOOL AllowPrecache)
{
	guard(UD3D11RenderDevice::Flush);

	ClearTextureCache();

	if (AllowPrecache && UsePrecache && !GIsEditor)
		PrecacheOnFlip = 1;

	unguard;
}

UBOOL UD3D11RenderDevice::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	guard(UD3D11RenderDevice::Exec);

	if (URenderDevice::Exec(Cmd, Ar))
	{
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("d3d_contrast")))
	{
		float value = _wtof(Cmd);
		D3DContrast = clamp(value, 0.1f, 3.f);
		SaveConfig();
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("d3d_saturation")))
	{
		float value = _wtof(Cmd);
		D3DSaturation = clamp(value, -0.8f, 0.8f);
		SaveConfig();
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("d3d_brightness")))
	{
		float value = _wtof(Cmd);
		D3DBrightness = clamp(value, -15.0f, 15.f);
		SaveConfig();
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("d3d_grayformula")))
	{
		int value = _wtoi(Cmd);
		D3DGrayFormula = clamp(value, 0, 2);
		SaveConfig();
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("DGL")))
	{
		if (ParseCommand(&Cmd, TEXT("BUFFERTRIS")))
		{
			return 1;
		}
		else if (ParseCommand(&Cmd, TEXT("BUILD")))
		{
			return 1;
		}
		else if (ParseCommand(&Cmd, TEXT("AA")))
		{
			return 1;
		}
		return 0;
	}
	else if (ParseCommand(&Cmd, TEXT("GetRes")))
	{
		struct Resolution
		{
			int X;
			int Y;

			// For sorting highest resolution first
			bool operator<(const Resolution& other) const { if (X != other.X) return X > other.X; else return Y > other.Y; }
		};

		std::set<Resolution> resolutions;

		// Always include what the monitor is currently using
		HDC screenDC = GetDC(0);
		int screenWidth = GetDeviceCaps(screenDC, HORZRES);
		int screenHeight = GetDeviceCaps(screenDC, VERTRES);
		resolutions.insert({ screenWidth, screenHeight });
		ReleaseDC(0, screenDC);

		FString Str;
		for (const Resolution& resolution : resolutions)
		{
			Str += FString::Printf(TEXT("%ix%i "), (INT)resolution.X, (INT)resolution.Y);
		}
		Ar.Log(*Str.LeftChop(1));
		return 1;
	}
	else
	{
		return 0;
	}

	unguard;
}

void UD3D11RenderDevice::Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData, INT* HitSize)
{
	guard(UD3D11RenderDevice::Lock);

	FlashScale = InFlashScale;
	FlashFog = InFlashFog;

	FLOAT color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	Context->ClearRenderTargetView(SceneBuffers.ColorBufferView, color);
	Context->ClearDepthStencilView(SceneBuffers.DepthBufferView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	Context->OMSetRenderTargets(1, &SceneBuffers.ColorBufferView, SceneBuffers.DepthBufferView);

	D3D11_RECT box = {};
	box.right = Viewport->SizeX;
	box.bottom = Viewport->SizeY;
	Context->RSSetScissorRects(1, &box);

	IsLocked = true;

	unguard;
}

void UD3D11RenderDevice::Unlock(UBOOL Blit)
{
	guard(UD3D11RenderDevice::Unlock);

	if (Blit)
	{
		if (Multisample > 1)
		{
			Context->ResolveSubresource(SceneBuffers.PPImage, 0, SceneBuffers.ColorBuffer, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
		}
		else
		{
			Context->CopyResource(SceneBuffers.PPImage, SceneBuffers.ColorBuffer);
		}

		Context->OMSetRenderTargets(1, &BackBufferView, nullptr);

		D3D11_VIEWPORT viewport = {};
		viewport.Width = Viewport->SizeX;
		viewport.Height = Viewport->SizeY;
		viewport.MaxDepth = 1.0f;
		Context->RSSetViewports(1, &viewport);

		UINT stride = sizeof(vec2);
		UINT offset = 0;
		ID3D11ShaderResourceView* psResources[] = { SceneBuffers.PPImageShaderView, PresentPass.DitherTextureView };
		Context->IASetVertexBuffers(0, 1, &PresentPass.PPStepVertexBuffer, &stride, &offset);
		Context->IASetInputLayout(PresentPass.PPStepLayout);
		Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Context->VSSetShader(PresentPass.PPStep, nullptr, 0);
		Context->RSSetState(PresentPass.RasterizerState);
		Context->PSSetShader(PresentPass.Present, nullptr, 0);
		Context->PSSetConstantBuffers(0, 1, &PresentPass.PresentConstantBuffer);
		Context->PSSetShaderResources(0, 2, psResources);
		Context->OMSetDepthStencilState(PresentPass.DepthStencilState, 0);
		Context->OMSetBlendState(PresentPass.BlendState, nullptr, 0xffffffff);

		PresentPushConstants pushconstants;
		pushconstants.InvGamma = 1.0f / (1.5f * Viewport->GetOuterUClient()->Brightness * 2.0f);
		pushconstants.Contrast = clamp(D3DContrast, 0.1f, 3.f);
		pushconstants.Saturation = clamp(D3DSaturation, -0.8f, 0.8f);
		pushconstants.Brightness = clamp(D3DBrightness, -15.0f, 15.f);
		pushconstants.GrayFormula = clamp(D3DGrayFormula, 0, 2);
		Context->UpdateSubresource(PresentPass.PresentConstantBuffer, 0, nullptr, &pushconstants, 0, 0);

		Context->Draw(6, 0);

		SwapChain->Present(UseVSync ? 1 : 0, 0);
	}

	Context->OMSetRenderTargets(0, nullptr, nullptr);

	IsLocked = false;

	unguard;
}

UBOOL UD3D11RenderDevice::SupportsTextureFormat(ETextureFormat Format)
{
	guard(UD3D11RenderDevice::SupportsTextureFormat);

	return Uploads->SupportsTextureFormat(Format) ? TRUE : FALSE;

	unguard;
}

void UD3D11RenderDevice::UpdateTextureRect(FTextureInfo& Info, INT U, INT V, INT UL, INT VL)
{
	guard(UD3D11RenderDevice::UpdateTextureRect);

	Textures->UpdateTextureRect(&Info, U, V, UL, VL);

	unguard;
}

void UD3D11RenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet)
{
	guard(UD3D11RenderDevice::DrawComplexSurface);

	unguard;
}

void UD3D11RenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, int NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guard(UD3D11RenderDevice::DrawGouraudPolygon);

	if (NumPts < 3) return; // This can apparently happen!!

	unguard;
}

void UD3D11RenderDevice::DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags)
{
	guard(UD3D11RenderDevice::DrawTile);

	unguard;
}

void UD3D11RenderDevice::Draw3DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2)
{
	guard(UD3D11RenderDevice::Draw3DLine);

	P1 = P1.TransformPointBy(Frame->Coords);
	P2 = P2.TransformPointBy(Frame->Coords);
	if (Frame->Viewport->IsOrtho())
	{
		P1.X = (P1.X) / Frame->Zoom + Frame->FX2;
		P1.Y = (P1.Y) / Frame->Zoom + Frame->FY2;
		P1.Z = 1;
		P2.X = (P2.X) / Frame->Zoom + Frame->FX2;
		P2.Y = (P2.Y) / Frame->Zoom + Frame->FY2;
		P2.Z = 1;

		if (Abs(P2.X - P1.X) + Abs(P2.Y - P1.Y) >= 0.2)
		{
			Draw2DLine(Frame, Color, LineFlags, P1, P2);
		}
		else if (Frame->Viewport->Actor->OrthoZoom < ORTHO_LOW_DETAIL)
		{
			Draw2DPoint(Frame, Color, LINE_None, P1.X - 1, P1.Y - 1, P1.X + 1, P1.Y + 1, P1.Z);
		}
	}
	else
	{
		/*SetNoTexture(0);
		SetBlend(PF_Highlighted);
		glColor3fv(&Color.X);
		glBegin(GL_LINES);
		glVertex3fv(&P1.X);
		glVertex3fv(&P2.X);
		glEnd();*/
	}

	unguard;
}

void UD3D11RenderDevice::Draw2DClippedLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2)
{
	guard(UD3D11RenderDevice::Draw2DClippedLine);
	URenderDevice::Draw2DClippedLine(Frame, Color, LineFlags, P1, P2);
	unguard;
}

void UD3D11RenderDevice::Draw2DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2)
{
	guard(UD3D11RenderDevice::Draw2DLine);
	//SetBlend(PF_Highlighted | PF_Occlude);
	//glColor3fv( &Color.X );
	//glBegin(GL_LINES);
	//glVertex3f( RFX2*P1.Z*(P1.X-Frame->FX2), RFY2*P1.Z*(P1.Y-Frame->FY2), P1.Z );
	//glVertex3f( RFX2*P2.Z*(P2.X-Frame->FX2), RFY2*P2.Z*(P2.Y-Frame->FY2), P2.Z );
	//glEnd();
	unguard;
}

void UD3D11RenderDevice::Draw2DPoint(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z)
{
	guard(UD3D11RenderDevice::Draw2DPoint);
	//SetBlend(PF_Highlighted | PF_Occlude);
	//SetNoTexture(0);
	//glColor3fv(&Color.X);
	//glBegin(GL_TRIANGLE_FAN);
	//glVertex3f( RFX2*Z*(X1-Frame->FX2), RFY2*Z*(Y1-Frame->FY2), Z );
	//glVertex3f( RFX2*Z*(X2-Frame->FX2), RFY2*Z*(Y1-Frame->FY2), Z );
	//glVertex3f( RFX2*Z*(X2-Frame->FX2), RFY2*Z*(Y2-Frame->FY2), Z );
	//glVertex3f( RFX2*Z*(X1-Frame->FX2), RFY2*Z*(Y2-Frame->FY2), Z );
	//glEnd();
	unguard;
}

void UD3D11RenderDevice::ClearZ(FSceneNode* Frame)
{
	guard(UD3D11RenderDevice::ClearZ);

	Context->ClearDepthStencilView(SceneBuffers.DepthBufferView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	unguard;
}

void UD3D11RenderDevice::PushHit(const BYTE* Data, INT Count)
{
	guard(UD3D11RenderDevice::PushHit);
	unguard;
}

void UD3D11RenderDevice::PopHit(INT Count, UBOOL bForce)
{
	guard(UD3D11RenderDevice::PopHit);
	unguard;
}

void UD3D11RenderDevice::GetStats(TCHAR* Result)
{
	guard(UD3D11RenderDevice::GetStats);
	Result[0] = 0;
	unguard;
}

void UD3D11RenderDevice::ReadPixels(FColor* Pixels)
{
	guard(UD3D11RenderDevice::GetStats);

	unguard;
}

void UD3D11RenderDevice::EndFlash()
{
	guard(UD3D11RenderDevice::EndFlash);
	if (FlashScale != FPlane(0.5f, 0.5f, 0.5f, 0.0f) || FlashFog != FPlane(0.0f, 0.0f, 0.0f, 0.0f))
	{
		//

		if (CurrentFrame)
			SetSceneNode(CurrentFrame);
	}
	unguard;
}

void UD3D11RenderDevice::SetSceneNode(FSceneNode* Frame)
{
	guard(UD3D11RenderDevice::SetSceneNode);

	unguard;
}

void UD3D11RenderDevice::PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags)
{
	guard(UD3D11RenderDevice::PrecacheTexture);
	Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));
	unguard;
}

void UD3D11RenderDevice::ClearTextureCache()
{
	Textures->ClearCache();
}

std::vector<uint8_t> UD3D11RenderDevice::CompileHlsl(const std::string& filename, const std::string& shadertype, const std::vector<std::string> defines)
{
	std::string code = FileResource::readAllText(filename);

	std::string target;
	switch (FeatureLevel)
	{
	default:
	case D3D_FEATURE_LEVEL_11_1: target = shadertype + "_5_0"; break;
	case D3D_FEATURE_LEVEL_11_0: target = shadertype + "_5_0"; break;
	case D3D_FEATURE_LEVEL_10_1: target = shadertype + "_4_1"; break;
	case D3D_FEATURE_LEVEL_10_0: target = shadertype + "_4_0"; break;
	}

	std::vector<D3D_SHADER_MACRO> macros;
	for (const std::string& define : defines)
	{
		D3D_SHADER_MACRO macro = {};
		macro.Name = define.c_str();
		macro.Definition = "1";
		macros.push_back(macro);
	}
	macros.push_back({});

	ID3DBlob* blob = nullptr;
	ID3DBlob* errors = nullptr;
	HRESULT result = D3DCompile(code.data(), code.size(), filename.c_str(), macros.data(), nullptr, "main", target.c_str(), D3D10_SHADER_ENABLE_STRICTNESS | D3D10_SHADER_OPTIMIZATION_LEVEL3, 0, &blob, &errors);
	if (FAILED(result))
	{
		std::string msg((const char*)errors->GetBufferPointer(), errors->GetBufferSize());
		if (!msg.empty() && msg.back() == 0) msg.pop_back();
		ReleaseObject(errors);
		throw std::runtime_error(msg);
	}
	ReleaseObject(errors);
	ThrowIfFailed(result, "D3DCompile failed");

	std::vector<uint8_t> bytecode;
	bytecode.resize(blob->GetBufferSize());
	memcpy(bytecode.data(), blob->GetBufferPointer(), bytecode.size());
	ReleaseObject(blob);
	return bytecode;
}
