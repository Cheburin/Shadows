//--------------------------------------------------------------------------------------
// File: ConstantBuffer.h
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#pragma once
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

namespace DirectX
{
	// Strongly typed wrapper around a D3D constant buffer.
	template<typename T>
	class ConstantBuffer
	{
	public:
		explicit ConstantBuffer(_In_ ID3D11Device* device)
		{
			Create(device);
		}


		void Create(_In_ ID3D11Device* device)
		{
			D3D11_BUFFER_DESC desc = { 0 };

			desc.ByteWidth = sizeof(T);
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			auto hr = device->CreateBuffer(&desc, nullptr, mConstantBuffer.ReleaseAndGetAddressOf());
			if (hr != S_OK)
				throw std::exception();
		}


		// Writes new data into the constant buffer.
		void SetData(_In_ ID3D11DeviceContext* deviceContext, T const& value)
		{
			assert(mConstantBuffer.Get() != 0);

			D3D11_MAPPED_SUBRESOURCE mappedResource;

			HRESULT hr = deviceContext->Map(mConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

			if (hr != S_OK)
				throw std::exception();

			*(T*)mappedResource.pData = value;

			deviceContext->Unmap(mConstantBuffer.Get(), 0);
		}


		// Looks up the underlying D3D constant buffer.
		ID3D11Buffer* GetBuffer()
		{
			return mConstantBuffer.Get();
		}


	private:
		// The underlying D3D object.
		Microsoft::WRL::ComPtr<ID3D11Buffer> mConstantBuffer;


		// Prevent copying.
		ConstantBuffer(ConstantBuffer const&) = delete;
		ConstantBuffer& operator= (ConstantBuffer const&) = delete;
	};
}
