#include "GeometricPrimitive.h"
#include "Effects.h"
#include "DirectXHelpers.h"
#include "DirectXMath.h"
// #include <DirectXHelpers.h>
// #include <DDSTextureLoader.h>
// #include "pch.h"

#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"

#include "strsafe.h"
#include "resource.h"
#include "Grid_Creation11.h"
#include <wrl.h>

#include <assert.h>
#include <memory>
#include <malloc.h>

#include <Exception>
#include <map>

#include "SimpleMath.h"

#include "ConstantBuffer.h"

#include "Effects.h"

#include "Model.h"

using namespace DirectX;

struct SceneState
{
	DirectX::XMFLOAT4X4    mWorld;                         // World matrix
	DirectX::XMFLOAT4X4    mView;                          // View matrix
	DirectX::XMFLOAT4X4    mProjection;                    // Projection matrix
	DirectX::XMFLOAT4X4    mWorldViewProjection;           // WVP matrix
	DirectX::XMFLOAT4X4    mWorldView;                     // WV matrix
	DirectX::XMFLOAT4X4    mViewProjection;                // VP matrix
	DirectX::XMFLOAT4X4    mInvView;                       // Inverse of view matrix
	
	DirectX::XMFLOAT4X4    mLightView;                // VP matrix
	DirectX::XMFLOAT4X4    mWorldLightView;                       // Inverse of view matrix
	DirectX::XMFLOAT4X4    mWorldLightViewProjection;                       // Inverse of view matrix
	DirectX::XMFLOAT4X4    mWorldMirrowViewProjection;                       // Inverse of view matrix

	DirectX::XMFLOAT4      vScreenResolution;              // Screen resolution

	DirectX::XMFLOAT4    viewLightPos;                   //
	DirectX::XMFLOAT4    vTessellationFactor;            // Edge, inside, minimum tessellation factor and 1/desired triangle size
	DirectX::XMFLOAT4    vDetailTessellationHeightScale; // Height scale for detail tessellation of grid surface
	DirectX::XMFLOAT4    vGridSize;                      // Grid size

	DirectX::XMFLOAT4    vDebugColorMultiply;            // Debug colors
	DirectX::XMFLOAT4    vDebugColorAdd;                 // Debug colors

	DirectX::XMFLOAT4    vFrustumPlaneEquation[4];       // View frustum plane equations
};
class ModelMeshPartWithAdjacency : public DirectX::ModelMeshPart{
public:
	uint32_t adjacencyIndexCount;
	Microsoft::WRL::ComPtr<ID3D11Buffer>   adjacencyIndexBuffer;
	void DrawAdjacency(ID3D11DeviceContext* deviceContext, IEffect* ieffect, ID3D11InputLayout* iinputLayout, std::function<void()> setCustomState) const
	{
		deviceContext->IASetInputLayout(iinputLayout);

		auto vb = vertexBuffer.Get();
		UINT vbStride = vertexStride;
		UINT vbOffset = 0;
		deviceContext->IASetVertexBuffers(0, 1, &vb, &vbStride, &vbOffset);

		// Note that if indexFormat is DXGI_FORMAT_R32_UINT, this model mesh part requires a Feature Level 9.2 or greater device
		deviceContext->IASetIndexBuffer(adjacencyIndexBuffer.Get(), indexFormat, 0);

		assert(ieffect != 0);
		ieffect->Apply(deviceContext);

		// Hook lets the caller replace our shaders or state settings with whatever else they see fit.
		if (setCustomState)
		{
			setCustomState();
		}

		// Draw the primitive.
		deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ);

		deviceContext->DrawIndexed(adjacencyIndexCount, startIndex, vertexOffset);
	}
};

extern SceneState main_scene_state;

extern std::unique_ptr< DirectX::ConstantBuffer<SceneState> > main_scene_state_cb;

extern std::unique_ptr<ModelMeshPartWithAdjacency> torus;

extern Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;

template<class T> inline ID3D11Buffer** constantBuffersToArray(DirectX::ConstantBuffer<T> &cb){
	static ID3D11Buffer* cbs[10];
	cbs[0] = cb.GetBuffer();
	return cbs;
};

void DrawQuad(ID3D11DeviceContext* pd3dImmediateContext, _In_ IEffect* effect,
	_In_opt_ std::function<void __cdecl()> setCustomState = nullptr){
	effect->Apply(pd3dImmediateContext);
	setCustomState();

	pd3dImmediateContext->IASetInputLayout(nullptr);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pd3dImmediateContext->Draw(4, 0);
}

void light_set_position(ID3D11DeviceContext* pd3dImmediateContext, XMFLOAT3& pos){
	auto lookAt = XMFLOAT3(1, 0, 0.5 + 0.33 / 2 + 0.15);

	DirectX::XMStoreFloat4x4(&main_scene_state.mLightView, DirectX::XMMatrixTranspose(DirectX::XMMatrixLookAtLH(XMLoadFloat3(&pos), XMLoadFloat3(&lookAt), XMLoadFloat3(&XMFLOAT3(0, 0, 1)))));

	auto L = XMVector3TransformCoord(XMLoadFloat3(&pos), XMMatrixTranspose(XMLoadFloat4x4(&main_scene_state.mView)));
	DirectX::XMStoreFloat4(&main_scene_state.viewLightPos, L);

	DirectX::XMMATRIX wvp;

	wvp = XMMatrixTranspose(XMLoadFloat4x4(&main_scene_state.mWorld)) * XMMatrixTranspose(XMLoadFloat4x4(&main_scene_state.mLightView));
	DirectX::XMStoreFloat4x4(&main_scene_state.mWorldLightView, DirectX::XMMatrixTranspose(wvp));
	wvp = wvp * XMMatrixTranspose(XMLoadFloat4x4(&main_scene_state.mProjection));// *DirectX::XMMatrixScaling(0.5, -0.5, 1) * DirectX::XMMatrixTranslation(0.5, 0.5, 0);
	DirectX::XMStoreFloat4x4(&main_scene_state.mWorldLightViewProjection, DirectX::XMMatrixTranspose(wvp));

	main_scene_state_cb->SetData(pd3dImmediateContext, main_scene_state);
}

void floor_set_world_matrix(ID3D11DeviceContext* pd3dImmediateContext){

	DirectX::XMMATRIX wvp;

	wvp = DirectX::XMMatrixRotationX(0)*DirectX::XMMatrixScaling(5, 5, 5);
	DirectX::XMStoreFloat4x4(&main_scene_state.mWorld, DirectX::XMMatrixTranspose(wvp));
	wvp = wvp * XMMatrixTranspose(XMLoadFloat4x4(&main_scene_state.mView));
	DirectX::XMStoreFloat4x4(&main_scene_state.mWorldView, DirectX::XMMatrixTranspose(wvp));
	wvp = wvp * XMMatrixTranspose(XMLoadFloat4x4(&main_scene_state.mProjection));
	DirectX::XMStoreFloat4x4(&main_scene_state.mWorldViewProjection, DirectX::XMMatrixTranspose(wvp));

	main_scene_state_cb->SetData(pd3dImmediateContext, main_scene_state);
}

void torus_set_world_matrix(ID3D11DeviceContext* pd3dImmediateContext, XMFLOAT3& pos){
	DirectX::XMMATRIX wvp;

	wvp = DirectX::XMMatrixTranslationFromVector(XMLoadFloat3(&pos));
	DirectX::XMStoreFloat4x4(&main_scene_state.mWorld, DirectX::XMMatrixTranspose(wvp));
	wvp = wvp * XMMatrixTranspose(XMLoadFloat4x4(&main_scene_state.mView));
	DirectX::XMStoreFloat4x4(&main_scene_state.mWorldView, DirectX::XMMatrixTranspose(wvp));
	wvp = wvp * XMMatrixTranspose(XMLoadFloat4x4(&main_scene_state.mProjection));
	DirectX::XMStoreFloat4x4(&main_scene_state.mWorldViewProjection, DirectX::XMMatrixTranspose(wvp));

	main_scene_state_cb->SetData(pd3dImmediateContext, main_scene_state);
}

DirectX::XMFLOAT4X4 mirrow_view(DirectX::XMFLOAT4& plane){
	auto reflection = DirectX::XMMatrixReflect(XMLoadFloat4(&plane));

	auto view = XMMatrixTranspose(XMLoadFloat4x4(&main_scene_state.mView));

	auto invView = DirectX::XMMatrixInverse(0, view);

	invView.r[0] = -1 * XMVector4Transform(invView.r[0], reflection);
	invView.r[1] = XMVector4Transform(invView.r[1], reflection);
	invView.r[2] = XMVector4Transform(invView.r[2], reflection);
	invView.r[3] = XMVector4Transform(invView.r[3], reflection);

	view = DirectX::XMMatrixInverse(0, invView);
	DirectX::XMStoreFloat4x4(&main_scene_state.mView, DirectX::XMMatrixTranspose(view));

	//DirectX::XMStoreFloat4x4(&main_scene_state.mView, DirectX::XMMatrixTranspose( (view.Invert() * reflection).Invert() ));
	//DirectX::XMStoreFloat4x4(&main_scene_state.mView, DirectX::XMMatrixTranspose(reflection * view));

	return main_scene_state.mView;
}

void mirrow_set_matrix(ID3D11DeviceContext* pd3dImmediateContext, DirectX::XMFLOAT4X4& mirrowView){
	DirectX::XMMATRIX wvp;

	wvp = XMMatrixTranspose(XMLoadFloat4x4(&main_scene_state.mWorld)) * XMMatrixTranspose(XMLoadFloat4x4(&mirrowView)) * XMMatrixTranspose(XMLoadFloat4x4(&main_scene_state.mProjection));
	DirectX::XMStoreFloat4x4(&main_scene_state.mWorldMirrowViewProjection, DirectX::XMMatrixTranspose(wvp));

	main_scene_state_cb->SetData(pd3dImmediateContext, main_scene_state);
}

void mirrow_set_world_matrix(ID3D11DeviceContext* pd3dImmediateContext, XMFLOAT3& scale, DirectX::XMMATRIX& orintation, XMFLOAT3& pos){
	DirectX::XMMATRIX wvp;

	wvp = DirectX::XMMatrixScalingFromVector(XMLoadFloat3(&scale)) * orintation * DirectX::XMMatrixTranslationFromVector(XMLoadFloat3(&pos));
	DirectX::XMStoreFloat4x4(&main_scene_state.mWorld, DirectX::XMMatrixTranspose(wvp));
	wvp = wvp * XMMatrixTranspose(XMLoadFloat4x4(&main_scene_state.mView));
	DirectX::XMStoreFloat4x4(&main_scene_state.mWorldView, DirectX::XMMatrixTranspose(wvp));
	wvp = wvp * XMMatrixTranspose(XMLoadFloat4x4(&main_scene_state.mProjection));
	DirectX::XMStoreFloat4x4(&main_scene_state.mWorldViewProjection, DirectX::XMMatrixTranspose(wvp));

	main_scene_state_cb->SetData(pd3dImmediateContext, main_scene_state);
}

void mirrow_draw(ID3D11DeviceContext* pd3dImmediateContext, IEffect* effect, _In_opt_ std::function<void __cdecl()> setCustomState = nullptr){

	DrawQuad(pd3dImmediateContext, effect, [=]{
		pd3dImmediateContext->VSSetConstantBuffers(0, 1, constantBuffersToArray(*main_scene_state_cb));

		setCustomState();
	});

}

void floor_draw(ID3D11DeviceContext* pd3dImmediateContext, IEffect* effect, _In_opt_ std::function<void __cdecl()> setCustomState = nullptr){

	DrawQuad(pd3dImmediateContext, effect, [=]{
		pd3dImmediateContext->VSSetConstantBuffers(0, 1, constantBuffersToArray(*main_scene_state_cb));

		setCustomState();
	});
}

void torus_draw(ID3D11DeviceContext* pd3dImmediateContext,IEffect* effect,_In_opt_ std::function<void __cdecl()> setCustomState = nullptr){

	torus->Draw(pd3dImmediateContext, effect, inputLayout.Get(), [=]{
		pd3dImmediateContext->VSSetConstantBuffers(0, 1, constantBuffersToArray(*main_scene_state_cb));
		pd3dImmediateContext->PSSetConstantBuffers(0, 1, constantBuffersToArray(*main_scene_state_cb));

		setCustomState();
	});
}

void torus_shadow_draw(	ID3D11DeviceContext* pd3dImmediateContext,	IEffect* effect,	_In_opt_ std::function<void __cdecl()> setCustomState = nullptr){

	torus->DrawAdjacency(pd3dImmediateContext, effect, inputLayout.Get(), [=]{
		pd3dImmediateContext->VSSetConstantBuffers(0, 1, constantBuffersToArray(*main_scene_state_cb));
		pd3dImmediateContext->GSSetConstantBuffers(0, 1, constantBuffersToArray(*main_scene_state_cb));

		setCustomState();
	});
}