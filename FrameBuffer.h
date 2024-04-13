#pragma once

#include "./Helpers/d3dUtil.h"
#include "./Helpers/MathHelper.h"
#include "./Helpers/UploadBuffer.h"

// pair to the cbuffer cbObject in the shader source(BasicShader.hlsl)
struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();			
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();	
	UINT MaterialIndex;
	UINT ObjPad0;
	UINT ObjPad1;
	UINT ObjPad2;
};

// pair to the cbuffer cbCommon in the shader source(BasicShader.hlsl)
struct CommonConstants
{
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT3 CameraPosW = { 0.0f, 0.0f, 0.0f };
	float CommonPad0 = 0.0f;
	DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;

	DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

	Light Lights[MaxLights];		// #define MaxLights 16 in d3dUtil.h
};

struct MaterialParameter
{
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 64.0f;

	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();

	UINT DiffuseMapIndex = 0;
	UINT matPad0;
	UINT matPad1;
	UINT matPad2;
};


struct Vertex
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
};

class FrameBuffer
{
public:
	FrameBuffer(ID3D12Device* device, UINT commonCount, UINT objectCount, UINT materialCount);
	FrameBuffer(const FrameBuffer& rhs) = delete;
	FrameBuffer(FrameBuffer&& rhs) = delete;
	FrameBuffer& operator=(const FrameBuffer& rhs) = delete;
	~FrameBuffer();

public:
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	std::unique_ptr<UploadBuffer<CommonConstants>> CommonCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

	std::unique_ptr<UploadBuffer<MaterialParameter>> MaterialBuffer = nullptr;

	UINT64 Fence = 0;
};