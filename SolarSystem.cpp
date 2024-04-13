// ------------------------------------------------------
// Demonstration of solar system in 3D graphics using Microsoft DirectX 12
// code written by H.H. Yoo
//-------------------------------------------------------

#include "SolarSystem.h"
#include "framework.h"
#include "Resource.h"

#include "./Helpers/d3dApp.h"
#include "./Helpers/MathHelper.h"
#include "./Helpers/UploadBuffer.h"
#include "./Helpers/GeometryGenerator.h"
#include "./Helpers/Camera.h"
#include "FrameBuffer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace std;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameBuffers = 3; // the size of the circular array to store resources per frame

class RenderItem
{
public:
	RenderItem() = default;

public:
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	bool isItemStatic = true;

	int numFrameBufferFill = gNumFrameBuffers;		// for static objects

	UINT ObjCBIndex = -1;			// object Constant Buffer Index

	Material* Mat = nullptr;		// Material characteristics assigned to this render item.	
	MeshGeometry* Geo = nullptr;	// Geometry data of this render item.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; // how the rendering pipeline interprets the input geometry

	// parameters of DrawIndexedInstanced method
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

struct CelestialBody
{
	string name;
	float radius;
	float spinRate;
	float orbitRate;
	float orbitSize;
};

class SolarSystem : public D3DApp
{
public:
	SolarSystem(HINSTANCE hInstance);
	SolarSystem(const SolarSystem& rhs) = delete;
	SolarSystem(SolarSystem&& rhs) = delete;
	SolarSystem& operator=(const SolarSystem& rhs) = delete;
	~SolarSystem();

	virtual bool Initialize() override;

private:
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void OnKeyboardInput(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);			
	void UpdateMaterialBuffer(const GameTimer& gt);		
	void UpdateCommonCB(const GameTimer& gt);				


	void PrepareTextures();
	void SetRootSignature();				// set root signature for input to shader.
	void SetDescriptorHeaps();				// set descriptor heaps for texture resource descriptors.
	void SetShadersAndInputLayout();		// compile shader source and set input to IA(Input Assembler) of the rendering pipeline.
	void SetShapeGeometry();				// create and set vertex and index buffers for each object to be rendered.
	void SetPSOs();							// set pipeline state object for various rendering purpose, this application only needs just one configuration of PSO.
	void SetFrameBuffers();					// set frame buffers which carry several rendering resources.
	void SetMaterials();					// set material properties each to-be-rendered object carries.
	void SetRenderingItems();				// set rendering items to be drawn
	void DrawRenderingItems(ID3D12GraphicsCommandList* cmdList, const vector<RenderItem*>& ritems);		
											// draw rendering items using ID3D12GraphicsCommandList::DrawIndexedInstanced(...) method.

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();		// prepare static samplers

private:		
	
	vector<unique_ptr<FrameBuffer>> mFrameBuffers;		// it stores resources required for rendering in a circular array.
	FrameBuffer* mCurrentFrameBuffer = nullptr;
	int mCurrentFrameBufferIndex = 0;

	UINT mCbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	unordered_map<string, unique_ptr<MeshGeometry>> mGeometries;		// mesh geometry categorized by name
	unordered_map<string, unique_ptr<Material>> mMaterials;				// material characteristics categorized by name
	unordered_map<string, unique_ptr<Texture>> mTextures;				// textures categorized by name
	unordered_map<string, ComPtr<ID3DBlob>> mShaders;					// compiled shader memory blob
	unordered_map<string, ComPtr<ID3D12PipelineState>> mPSOs;			// (rendering) pipeline state object

	vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;						// format of data supplied to IA(Input Assembler)

	// List of all the rendering items.
	vector<unique_ptr<RenderItem>> mAllRenderItems;

	// Rendering items divided by PSO.
	vector<RenderItem*> mOpaqueRenderItems;			// this application only deals with opaque objects

	CommonConstants mCommonCB;

	Camera mCamera;		// camera object to compute a view and projection matrix (Camera.h, cpp)

	POINT mLastMousePosition;			// for tracking mouse pointer on the screen.

	float mPace;						// pace of the camera movement

	CelestialBody solarFamily[6];
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		SolarSystem thisApp(hInstance);
		if (!thisApp.Initialize())
		{
			return 0;
		}
		return thisApp.Run();
	}
	catch (DxException& err)
	{
		MessageBox(nullptr, err.ToString().c_str(), L"Initialization Failed...", MB_OK);
		return 0;
	}
}

SolarSystem::SolarSystem(HINSTANCE hInstance) : D3DApp(hInstance), mPace(10.0f)
{
	// Sun
	solarFamily[0].name = "Sun";
	solarFamily[0].radius = 10.0f;
	solarFamily[0].spinRate = 0.1f;
	solarFamily[1].orbitRate = 0.0f;
	solarFamily[0].orbitSize = 0.0f;

	// Mercury
	solarFamily[1].name = "Mercury";
	solarFamily[1].radius = 0.2f;
	solarFamily[1].spinRate = 0.2f;
	solarFamily[1].orbitRate = 0.5f;
	solarFamily[1].orbitSize = 15.0f;

	// Venus
	solarFamily[2].name = "Venus";
	solarFamily[2].radius = 0.7f;
	solarFamily[2].spinRate = 0.2f;
	solarFamily[2].orbitRate = 0.4f;
	solarFamily[2].orbitSize = 30.0f;

	// Earth
	solarFamily[3].name = "Earth";
	solarFamily[3].radius = 0.7f;
	solarFamily[3].spinRate = 0.2f;
	solarFamily[3].orbitRate = 0.3f;
	solarFamily[3].orbitSize = 45.0f;

	// Mars
	solarFamily[4].name = "Mars";
	solarFamily[4].radius = 0.4f;
	solarFamily[4].spinRate = 0.2f;
	solarFamily[4].orbitRate = 0.2f;
	solarFamily[4].orbitSize = 60.0f;
	
	// Jupiter
	solarFamily[5].name = "Jupiter";
	solarFamily[5].radius = 4.0f;
	solarFamily[5].spinRate = 0.1f;
	solarFamily[5].orbitRate = 0.1f;
	solarFamily[5].orbitSize = 80.0f;
}

SolarSystem::~SolarSystem()
{
	if (md3dDevice != nullptr)
	{
		FlushCommandQueue();		// flush command queue before destroy this object(SolarSystem).
	}
}

// ---------- initializing the rendring pipeline ----------
bool SolarSystem::Initialize()
{
	if (!D3DApp::Initialize())
	{
		return false;
	}
	
	ThrowIfFailed(mCommandList->Reset(D3DApp::mDirectCmdListAlloc.Get(), nullptr));

	// Get a descriptor byte size in desciptor heap
	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mCamera.SetPosition(0.0f, 40.0f, -150.0f);		// set initial camera position.

	PrepareTextures();		
	SetRootSignature();
	SetDescriptorHeaps();
	SetShadersAndInputLayout();
	SetShapeGeometry();
	SetMaterials();
	SetRenderingItems();
	SetFrameBuffers();
	SetPSOs();

	// initialize the above settings.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	FlushCommandQueue();

	return true;			// initialization is done.
}


// ---------- real time methods ----------
void SolarSystem::OnResize()
{
	D3DApp::OnResize();

	// reset the field of view upon resize of the window.
	mCamera.SetLens(0.25f * XM_PI, AspectRatio(), 1.0f, 1000.0f);		// view angle : pi/4
}

void SolarSystem::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);

	// access the frame buffer in a circular way.
	mCurrentFrameBufferIndex = (mCurrentFrameBufferIndex + 1) % gNumFrameBuffers;
	mCurrentFrameBuffer = mFrameBuffers[mCurrentFrameBufferIndex].get();

	// if the current frame buffer is yet to be processed by the GPU, wait until it's done. (using windows event)
	if (mCurrentFrameBuffer->Fence != 0 && mFence->GetCompletedValue() < mCurrentFrameBuffer->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFrameBuffer->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateObjectCBs(gt);
	UpdateMaterialBuffer(gt);
	UpdateCommonCB(gt);
}

void SolarSystem::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrentFrameBuffer->CmdListAlloc;

	ThrowIfFailed(cmdListAlloc->Reset());

	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::Black, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	// combine shader resource descriptor heap to the command list.
	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto commonCB = mCurrentFrameBuffer->CommonCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(1, commonCB->GetGPUVirtualAddress());

	// bind all the materials used in this application. (structured buffers in hlsl)
	auto matBuffer = mCurrentFrameBuffer->MaterialBuffer->Resource();
	mCommandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());  // slotRootParameters[2].InitAsShaderResourceView(0, 1)

	// bind all the textures used in this scene.
	mCommandList->SetGraphicsRootDescriptorTable(3, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	DrawRenderingItems(mCommandList.Get(), mOpaqueRenderItems);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	mCurrentFrameBuffer->Fence = ++mCurrentFence;

	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void SolarSystem::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePosition.x = x;
	mLastMousePosition.y = y;

	SetCapture(mhMainWnd);
}

void SolarSystem::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

// rotate the camera on the given camera position.
void SolarSystem::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePosition.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePosition.y));

		// rotate the camera in pitch, yaw direction
		mCamera.Pitch(dy);	
		mCamera.RotateY(dx);
	}
	// update the mouse position with the new input
	mLastMousePosition.x = x;
	mLastMousePosition.y = y;
}

void SolarSystem::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState('W') & 0x8000)
	{
		mCamera.Walk(mPace * dt);
	}
	if (GetAsyncKeyState('S') & 0x8000)
	{
		mCamera.Walk(-mPace * dt);
	}
	if (GetAsyncKeyState('A') & 0x8000)
	{
		mCamera.Strafe(-mPace * dt);
	}
	if (GetAsyncKeyState('D') & 0x8000)
	{
		mCamera.Strafe(mPace * dt);
	}

	mCamera.UpdateViewMatrix();
}

void SolarSystem::UpdateObjectCBs(const GameTimer& gt)
{
	static float t_base = 0.0f;
	t_base += gt.DeltaTime();

	auto currObjectCB = mCurrentFrameBuffer->ObjectCB.get();
	size_t i = 0;

	for (auto& elem : mAllRenderItems)
	{
		if (elem->isItemStatic == false)	// for non-static objects
		{
			XMMATRIX world = XMLoadFloat4x4(&elem->World);
			float spinRate = solarFamily[i].spinRate;
			float orbitRate = solarFamily[i].orbitRate;
			float orbitSize = solarFamily[i].orbitSize;

			i++;

			// world matrix of planetary motion
			world = world * XMMatrixRotationY(spinRate * t_base) * XMMatrixTranslation(orbitSize, 10.0f, 0) * XMMatrixRotationY(orbitRate * t_base);
			XMMATRIX texTransform = XMLoadFloat4x4(&elem->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
			objConstants.MaterialIndex = elem->Mat->MatCBIndex;

			currObjectCB->CopyData(elem->ObjCBIndex, objConstants);
		}
		else		// for static objects
		{
			if (elem->numFrameBufferFill > 0)
			{
				XMMATRIX world = XMLoadFloat4x4(&elem->World);
				XMMATRIX texTransform = XMLoadFloat4x4(&elem->TexTransform);
				ObjectConstants objConstants;
				XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
				XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
				objConstants.MaterialIndex = elem->Mat->MatCBIndex;

				currObjectCB->CopyData(elem->ObjCBIndex, objConstants);
				elem->numFrameBufferFill--;
			}
		}
	}
}

void SolarSystem::UpdateMaterialBuffer(const GameTimer& gt)
{
	auto currentMaterialBuffer = mCurrentFrameBuffer->MaterialBuffer.get();

	for (auto& elem : mMaterials)
	{
		Material* mat = elem.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialParameter matParam;
			matParam.DiffuseAlbedo = mat->DiffuseAlbedo;
			matParam.FresnelR0 = mat->FresnelR0;
			matParam.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matParam.MatTransform, XMMatrixTranspose(matTransform));
			matParam.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;

			currentMaterialBuffer->CopyData(mat->MatCBIndex, matParam);

			mat->NumFramesDirty--;
		}
	}
}

void SolarSystem::UpdateCommonCB(const GameTimer& gt)
{
	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mCommonCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mCommonCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mCommonCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mCommonCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mCommonCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mCommonCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mCommonCB.CameraPosW = mCamera.GetPosition3f();
	mCommonCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mCommonCB.InvRenderTargetSize = XMFLOAT2(1.0f / (float)mClientWidth, 1.0f / (float)mClientHeight);
	mCommonCB.NearZ = 1.0f;
	mCommonCB.FarZ = 1000.0f;
	mCommonCB.TotalTime = gt.TotalTime();
	mCommonCB.DeltaTime = gt.DeltaTime();
	mCommonCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

	// three directional light sources illuminate the scene at which the camera is focused.
	mCommonCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mCommonCB.Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
	mCommonCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mCommonCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
	mCommonCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mCommonCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

	auto currentCommonCB = mCurrentFrameBuffer->CommonCB.get();
	currentCommonCB->CopyData(0, mCommonCB);
}

void SolarSystem::DrawRenderingItems(ID3D12GraphicsCommandList* cmdList, const vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto objectCB = mCurrentFrameBuffer->ObjectCB->Resource();

	// send draw commmand to command list for each render item
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		RenderItem* ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}


// ---------- preparatory methods ----------
void SolarSystem::PrepareTextures()
{
	// ----- space texture -----
	auto spaceTex = make_unique<Texture>();
	spaceTex->Name = "spaceTex";
	spaceTex->Filename = L"Textures/space5.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(), spaceTex->Filename.c_str(),
		spaceTex->Resource, spaceTex->UploadHeap));

	// ----- Sun texture -----
	auto sunTex = make_unique<Texture>();
	sunTex->Name = "sunTex";
	sunTex->Filename = L"Textures/sun1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(), sunTex->Filename.c_str(),
		sunTex->Resource, sunTex->UploadHeap));
	
	// ----- Mercury texture -----
	auto mercuryTex = make_unique<Texture>();
	mercuryTex->Name = "mercuryTex";
	mercuryTex->Filename = L"Textures/mercury1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(), mercuryTex->Filename.c_str(),
		mercuryTex->Resource, mercuryTex->UploadHeap));

	// ----- Venus texture -----
	auto venusTex = make_unique<Texture>();
	venusTex->Name = "venusTex";
	venusTex->Filename = L"Textures/venus1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(), venusTex->Filename.c_str(),
		venusTex->Resource, venusTex->UploadHeap));

	// ----- Earth texture -----
	auto earthTex = make_unique<Texture>();
	earthTex->Name = "earthTex";
	earthTex->Filename = L"Textures/earth1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(), earthTex->Filename.c_str(),
		earthTex->Resource, earthTex->UploadHeap));

	// ----- Mars texture -----
	auto marsTex = make_unique<Texture>();
	marsTex->Name = "marsTex";
	marsTex->Filename = L"Textures/mars1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(), marsTex->Filename.c_str(),
		marsTex->Resource, marsTex->UploadHeap));

	// ----- Jupiter texture -----
	auto jupiterTex = make_unique<Texture>();
	jupiterTex->Name = "jupiterTex";
	jupiterTex->Filename = L"Textures/jupiter1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(), jupiterTex->Filename.c_str(),
		jupiterTex->Resource, jupiterTex->UploadHeap));

	mTextures[spaceTex->Name] = move(spaceTex);
	mTextures[sunTex->Name] = move(sunTex);
	mTextures[mercuryTex->Name] = move(mercuryTex);
	mTextures[venusTex->Name] = move(venusTex);
	mTextures[earthTex->Name] = move(earthTex);
	mTextures[marsTex->Name] = move(marsTex);
	mTextures[jupiterTex->Name] = move(jupiterTex);
}

void SolarSystem::SetRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 7, 0, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameters[4];
	slotRootParameters[0].InitAsConstantBufferView(0);					// cbuffer cbObject : register(b0) in BasicShader.hlsl
	slotRootParameters[1].InitAsConstantBufferView(1);					// cbuffer cbCommon : register(b1) in BasicShader.hlsl
	slotRootParameters[2].InitAsShaderResourceView(0, 1);				// StructuredBuffer<MaterialParameter> gMaterialParameters : register(t0, space1) in BasicShader.hlsl
	slotRootParameters[3].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);	// Texture2D gDiffuseMap[7] : register(t0)	in BasicShader.hlsl

	// static sampler object
	auto staticSamplers = GetStaticSamplers();

	// set a root signature description struct
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameters, (UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature and store them in ComPtr<ID3DBlob>
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;		// for error-checking
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	// show error message if any of them has come up.
	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	// transfer root signature object to mRootSignature.
	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void SolarSystem::SetDescriptorHeaps()
{
	// create the shader resource view heap.
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 7;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	// fill out the srv heap with actual texture resource descriptors.

	// create a descriptor handle
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto spaceTex = mTextures["spaceTex"]->Resource;
	auto sunTex = mTextures["sunTex"]->Resource;
	auto mercuryTex = mTextures["mercuryTex"]->Resource;
	auto venusTex = mTextures["venusTex"]->Resource;
	auto earthTex = mTextures["earthTex"]->Resource;
	auto marsTex = mTextures["marsTex"]->Resource;
	auto jupiterTex = mTextures["jupiterTex"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = spaceTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = spaceTex->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	// descriptor for space texture resource
	md3dDevice->CreateShaderResourceView(spaceTex.Get(), &srvDesc, hDescriptor);

	// descriptor for sun texture resource
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = sunTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = sunTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(sunTex.Get(), &srvDesc, hDescriptor);

	// descriptor for mercury texture resource
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = mercuryTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = mercuryTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(mercuryTex.Get(), &srvDesc, hDescriptor);

	// descriptor for venus texture resource
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = venusTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = venusTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(venusTex.Get(), &srvDesc, hDescriptor);

	// descriptor for earth texture resource
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = earthTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = earthTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(earthTex.Get(), &srvDesc, hDescriptor);

	// descriptor for mars texture resource
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = marsTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = marsTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(marsTex.Get(), &srvDesc, hDescriptor);

	// descriptor for jupiter texture resource
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = jupiterTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = jupiterTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(jupiterTex.Get(), &srvDesc, hDescriptor);
}

void SolarSystem::SetShadersAndInputLayout()
{
	//const D3D_SHADER_MACRO RenderingOpaque;

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\BasicShader.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\BasicShader.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},		// XMFLOAT3 Position from struct Vertex in FrameBuffer.h
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},		// XMFLOAT3 Normal from struct Vertex in FrameBuffer.h
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},		// XMFLOAT2 TexCoord from struct Vertex in FrameBuffer.h
	};
}

void SolarSystem::SetShapeGeometry()
{
	// use GeometryGenerator class defined in GeometryGenerator.h to generate mesh data of sphere shape and large plane .
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData spacePlane = geoGen.CreateGrid(300.0f, 300.0f, 60, 60);
	GeometryGenerator::MeshData celestialSphere = geoGen.CreateSphere(1.0f, 20, 20);

	// put MeshData of these two geometries into one vertex buffer and one index buffer.
	
	// mark the vertex offsets to each object in the unified vertex buffer.
	UINT planeVertexStart = 0;
	UINT sphereVertexStart = (UINT)spacePlane.Vertices.size();

	// mark the starging index for each object in the unified index buffer.
	UINT planeIndexStart = 0;
	UINT sphereIndexStart = (UINT)spacePlane.Indices32.size();

	SubmeshGeometry planeSubmesh;
	planeSubmesh.IndexCount = (UINT)spacePlane.Indices32.size();
	planeSubmesh.StartIndexLocation = planeIndexStart;
	planeSubmesh.BaseVertexLocation = planeVertexStart;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)celestialSphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexStart;
	sphereSubmesh.BaseVertexLocation = sphereVertexStart;

	auto totalVertexNumber = spacePlane.Vertices.size() + celestialSphere.Vertices.size();
	vector<Vertex> vertices(totalVertexNumber);

	// fill up vertices
	size_t k = 0;
	for (size_t i = 0; i < spacePlane.Vertices.size(); ++i, ++k)
	{
		vertices[k].Position = spacePlane.Vertices[i].Position;
		vertices[k].Normal = spacePlane.Vertices[i].Normal;
		vertices[k].TexC = spacePlane.Vertices[i].TexC;
	}
	for (size_t i = 0; i < celestialSphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Position = celestialSphere.Vertices[i].Position;
		vertices[k].Normal = celestialSphere.Vertices[i].Normal;
		vertices[k].TexC = celestialSphere.Vertices[i].TexC;
	}

	// fill up indices
	vector<uint16_t> indices;
	indices.insert(indices.end(), spacePlane.GetIndices16().begin(), spacePlane.GetIndices16().end());
	indices.insert(indices.end(), celestialSphere.GetIndices16().begin(), celestialSphere.GetIndices16().end());

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);

	auto geo = make_unique<MeshGeometry>();
	geo->Name = "shapesGeo";

	// fill up the vertex buffer with the unified vertices in the system memory.
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	// fill up the index buffer with the unified indices in the system memory.
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	// fill up vertex buffer with the unified vertices in the gpu memory.
	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		vertices.data(), vbByteSize, geo->VertexBufferUploader);

	// fill up index buffer with the unified indices in the gpu memory.
	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["plane"] = planeSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;

	mGeometries[geo->Name] = move(geo);
}

void SolarSystem::SetPSOs()
{
	// PSO for opaque rendering objects
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));
}

void SolarSystem::SetFrameBuffers()
{
	for (size_t i = 0; i < gNumFrameBuffers; ++i)
	{
		mFrameBuffers.push_back(make_unique<FrameBuffer>(md3dDevice.Get(), 1,
			(UINT)mAllRenderItems.size(), (UINT)mMaterials.size()));
	}
}

void SolarSystem::SetMaterials()
{
	auto plane = make_unique<Material>();
	plane->Name = "plane";
	plane->MatCBIndex = 0;
	plane->DiffuseSrvHeapIndex = 0;			// it corresponds to a index to access the large plane texture info. in the shader resource descriptor heap.
	plane->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);		// Albedo coefficient
	plane->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);				// Fresnel's 0th order coefficient
	plane->Roughness = 0.3f;

	auto star = make_unique<Material>();
	star->Name = "star";
	star->MatCBIndex = 1;
	star->DiffuseSrvHeapIndex = 1;			// it corresponds to a index to access the sun's texture info. in the shader resource descriptor heap.
	star->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	star->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	star->Roughness = 0.1f;

	auto mercury = make_unique<Material>();
	mercury->Name = "mercury";
	mercury->MatCBIndex = 2;
	mercury->DiffuseSrvHeapIndex = 2;
	mercury->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	mercury->FresnelR0 = XMFLOAT3(0.03f, 0.03f, 0.03f);
	mercury->Roughness = 0.4f;

	auto venus = make_unique<Material>();
	venus->Name = "venus";
	venus->MatCBIndex = 3;
	venus->DiffuseSrvHeapIndex = 3;
	venus->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	venus->FresnelR0 = XMFLOAT3(0.04f, 0.04f, 0.04f);
	venus->Roughness = 0.2f;

	auto earth = make_unique<Material>();
	earth->Name = "earth";
	earth->MatCBIndex = 4;
	earth->DiffuseSrvHeapIndex = 4;
	earth->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	earth->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	earth->Roughness = 0.2f;

	auto mars = make_unique<Material>();
	mars->Name = "mars";
	mars->MatCBIndex = 5;
	mars->DiffuseSrvHeapIndex = 5;
	mars->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	mars->FresnelR0 = XMFLOAT3(0.03f, 0.03f, 0.03f);
	mars->Roughness = 0.2f;

	auto gasGiant = make_unique<Material>();
	gasGiant->Name = "gasGiant";
	gasGiant->MatCBIndex = 6;
	gasGiant->DiffuseSrvHeapIndex = 6;
	gasGiant->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	gasGiant->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	gasGiant->Roughness = 0.3f;

	mMaterials[plane->Name] = move(plane);
	mMaterials[star->Name] = move(star);
	mMaterials[mercury->Name] = move(mercury);
	mMaterials[venus->Name] = move(venus);
	mMaterials[earth->Name] = move(earth);
	mMaterials[mars->Name] = move(mars);
	mMaterials[gasGiant->Name] = move(gasGiant);
}

void SolarSystem::SetRenderingItems()	// combine rendering object data required for rendering pipeline
{
	float scaleFactor = 0.0f;

	auto planeRenderItem = make_unique<RenderItem>();
	planeRenderItem->World = MathHelper::Identity4x4();	
	XMStoreFloat4x4(&planeRenderItem->TexTransform, XMMatrixScaling(4.0f, 4.0f, 1.0f));
	planeRenderItem->ObjCBIndex = 0;	
	planeRenderItem->Mat = mMaterials["plane"].get();
	planeRenderItem->Geo = mGeometries["shapesGeo"].get();
	planeRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	planeRenderItem->IndexCount = planeRenderItem->Geo->DrawArgs["plane"].IndexCount;
	planeRenderItem->StartIndexLocation = planeRenderItem->Geo->DrawArgs["plane"].StartIndexLocation;
	planeRenderItem->BaseVertexLocation = planeRenderItem->Geo->DrawArgs["plane"].BaseVertexLocation;
	planeRenderItem->isItemStatic = true;			// static object
	mAllRenderItems.push_back(move(planeRenderItem));

	// celestial objects...

	// 1. sun
	scaleFactor = solarFamily[0].radius;
	auto sunRenderItem = make_unique<RenderItem>();
	XMStoreFloat4x4(&sunRenderItem->World, XMMatrixScaling(scaleFactor, scaleFactor, scaleFactor));
	XMStoreFloat4x4(&sunRenderItem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	sunRenderItem->ObjCBIndex = 1;
	sunRenderItem->Mat = mMaterials["star"].get();
	sunRenderItem->Geo = mGeometries["shapesGeo"].get();
	sunRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	sunRenderItem->IndexCount = sunRenderItem->Geo->DrawArgs["sphere"].IndexCount;
	sunRenderItem->StartIndexLocation = sunRenderItem->Geo->DrawArgs["sphere"].StartIndexLocation;
	sunRenderItem->BaseVertexLocation = sunRenderItem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	sunRenderItem->isItemStatic = false;		// moving object
	mAllRenderItems.push_back(move(sunRenderItem));

	// 2. mercury
	scaleFactor = solarFamily[1].radius;
	auto mercuryRenderItem = make_unique<RenderItem>();
	XMStoreFloat4x4(&mercuryRenderItem->World, XMMatrixScaling(scaleFactor, scaleFactor, scaleFactor));
	XMStoreFloat4x4(&mercuryRenderItem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	mercuryRenderItem->ObjCBIndex = 2;
	mercuryRenderItem->Mat = mMaterials["mercury"].get();
	mercuryRenderItem->Geo = mGeometries["shapesGeo"].get();
	mercuryRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	mercuryRenderItem->IndexCount = mercuryRenderItem->Geo->DrawArgs["sphere"].IndexCount;
	mercuryRenderItem->StartIndexLocation = mercuryRenderItem->Geo->DrawArgs["sphere"].StartIndexLocation;
	mercuryRenderItem->BaseVertexLocation = mercuryRenderItem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	mercuryRenderItem->isItemStatic = false;	// moving object
	mAllRenderItems.push_back(move(mercuryRenderItem));

	// 3. venus
	scaleFactor = solarFamily[2].radius;
	auto venusRenderItem = make_unique<RenderItem>();
	XMStoreFloat4x4(&venusRenderItem->World, XMMatrixScaling(scaleFactor, scaleFactor, scaleFactor));
	XMStoreFloat4x4(&venusRenderItem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	venusRenderItem->ObjCBIndex = 3;
	venusRenderItem->Mat = mMaterials["venus"].get();
	venusRenderItem->Geo = mGeometries["shapesGeo"].get();
	venusRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	venusRenderItem->IndexCount = venusRenderItem->Geo->DrawArgs["sphere"].IndexCount;
	venusRenderItem->StartIndexLocation = venusRenderItem->Geo->DrawArgs["sphere"].StartIndexLocation;
	venusRenderItem->BaseVertexLocation = venusRenderItem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	venusRenderItem->isItemStatic = false;		// moving object
	mAllRenderItems.push_back(move(venusRenderItem));

	// 4. earth
	scaleFactor = solarFamily[3].radius;
	auto earthRenderItem = make_unique<RenderItem>();
	XMStoreFloat4x4(&earthRenderItem->World, XMMatrixScaling(scaleFactor, scaleFactor, scaleFactor));
	XMStoreFloat4x4(&earthRenderItem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	earthRenderItem->ObjCBIndex = 4;
	earthRenderItem->Mat = mMaterials["earth"].get();
	earthRenderItem->Geo = mGeometries["shapesGeo"].get();
	earthRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	earthRenderItem->IndexCount = earthRenderItem->Geo->DrawArgs["sphere"].IndexCount;
	earthRenderItem->StartIndexLocation = earthRenderItem->Geo->DrawArgs["sphere"].StartIndexLocation;
	earthRenderItem->BaseVertexLocation = earthRenderItem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	earthRenderItem->isItemStatic = false;		// moving object
	mAllRenderItems.push_back(move(earthRenderItem));

	// 5. mars
	scaleFactor = solarFamily[4].radius;
	auto marsRenderItem = make_unique<RenderItem>();
	XMStoreFloat4x4(&marsRenderItem->World, XMMatrixScaling(scaleFactor, scaleFactor, scaleFactor));
	XMStoreFloat4x4(&marsRenderItem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	marsRenderItem->ObjCBIndex = 5;
	marsRenderItem->Mat = mMaterials["mars"].get();
	marsRenderItem->Geo = mGeometries["shapesGeo"].get();
	marsRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	marsRenderItem->IndexCount = marsRenderItem->Geo->DrawArgs["sphere"].IndexCount;
	marsRenderItem->StartIndexLocation = marsRenderItem->Geo->DrawArgs["sphere"].StartIndexLocation;
	marsRenderItem->BaseVertexLocation = marsRenderItem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	marsRenderItem->isItemStatic = false;	// moving object
	mAllRenderItems.push_back(move(marsRenderItem));

	// 6. jupiter
	scaleFactor = solarFamily[5].radius;
	auto jupiterRenderItem = make_unique<RenderItem>();
	XMStoreFloat4x4(&jupiterRenderItem->World, XMMatrixScaling(scaleFactor, scaleFactor, scaleFactor));
	XMStoreFloat4x4(&jupiterRenderItem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	jupiterRenderItem->ObjCBIndex = 6;
	jupiterRenderItem->Mat = mMaterials["gasGiant"].get();
	jupiterRenderItem->Geo = mGeometries["shapesGeo"].get();
	jupiterRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	jupiterRenderItem->IndexCount = jupiterRenderItem->Geo->DrawArgs["sphere"].IndexCount;
	jupiterRenderItem->StartIndexLocation = jupiterRenderItem->Geo->DrawArgs["sphere"].StartIndexLocation;
	jupiterRenderItem->BaseVertexLocation = jupiterRenderItem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	jupiterRenderItem->isItemStatic = false;	// moving object
	mAllRenderItems.push_back(move(jupiterRenderItem));

	// all the rendering items are opaque object : 
	for (auto& elem : mAllRenderItems)
	{
		mOpaqueRenderItems.push_back(elem.get());		// store them the relevant container in a primitive pointer type.
	}
}

// get static samplers for texture mapping
std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> SolarSystem::GetStaticSamplers()
{
	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}