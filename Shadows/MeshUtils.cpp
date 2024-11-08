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
//---------------------------------------------------------------------------------
struct aligned_deleter { void operator()(void* p) { _aligned_free(p); } };

typedef std::unique_ptr<float[], aligned_deleter> ScopedAlignedArrayFloat;

typedef std::unique_ptr<DirectX::XMVECTOR[], aligned_deleter> ScopedAlignedArrayXMVECTOR;

using namespace DirectX;

namespace
{
	//---------------------------------------------------------------------------------
	// Compute tangent and bi-tangent for each vertex
	//---------------------------------------------------------------------------------
	template<class index_t>
	HRESULT ComputeTangentFrameImpl(
		_In_reads_(nFaces * 3) const index_t* indices, size_t nFaces,
		_In_reads_(nVerts) const XMFLOAT3* positions,
		_In_reads_(nVerts) const XMFLOAT3* normals,
		_In_reads_(nVerts) const XMFLOAT2* texcoords,
		size_t nVerts,
		_Out_writes_opt_(nVerts) XMFLOAT3* tangents3,
		_Out_writes_opt_(nVerts) XMFLOAT4* tangents4,
		_Out_writes_opt_(nVerts) XMFLOAT3* bitangents)
	{
		if (!indices || !nFaces || !positions || !normals || !texcoords || !nVerts)
			return E_INVALIDARG;

		if (nVerts >= index_t(-1))
			return E_INVALIDARG;

		if ((uint64_t(nFaces) * 3) >= UINT32_MAX)
			return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

		static const float EPSILON = 0.0001f;
		static const XMVECTORF32 s_flips = { { { 1.f, -1.f, -1.f, 1.f } } };

		ScopedAlignedArrayXMVECTOR temp(static_cast<XMVECTOR*>(_aligned_malloc(sizeof(XMVECTOR)* nVerts * 2, 16)));
		if (!temp)
			return E_OUTOFMEMORY;

		memset(temp.get(), 0, sizeof(XMVECTOR)* nVerts * 2);

		XMVECTOR* tangent1 = temp.get();
		XMVECTOR* tangent2 = temp.get() + nVerts;

		for (size_t face = 0; face < nFaces; ++face)
		{
			index_t i0 = indices[face * 3];
			index_t i1 = indices[face * 3 + 1];
			index_t i2 = indices[face * 3 + 2];

			if (i0 == index_t(-1)
				|| i1 == index_t(-1)
				|| i2 == index_t(-1))
				continue;

			if (i0 >= nVerts
				|| i1 >= nVerts
				|| i2 >= nVerts)
				return E_UNEXPECTED;

			XMVECTOR t0 = XMLoadFloat2(&texcoords[i0]);
			XMVECTOR t1 = XMLoadFloat2(&texcoords[i1]);
			XMVECTOR t2 = XMLoadFloat2(&texcoords[i2]);

			XMVECTOR s = XMVectorMergeXY(XMVectorSubtract(t1, t0), XMVectorSubtract(t2, t0));

			XMFLOAT4A tmp;
			XMStoreFloat4A(&tmp, s);

			float d = tmp.x * tmp.w - tmp.z * tmp.y;
			d = (fabsf(d) <= EPSILON) ? 1.f : (1.f / d);
			s = XMVectorScale(s, d);
			s = XMVectorMultiply(s, s_flips);

			XMMATRIX m0;
			m0.r[0] = XMVectorPermute<3, 2, 6, 7>(s, g_XMZero);
			m0.r[1] = XMVectorPermute<1, 0, 4, 5>(s, g_XMZero);
			m0.r[2] = m0.r[3] = g_XMZero;

			XMVECTOR p0 = XMLoadFloat3(&positions[i0]);
			XMVECTOR p1 = XMLoadFloat3(&positions[i1]);
			XMVECTOR p2 = XMLoadFloat3(&positions[i2]);

			XMMATRIX m1;
			m1.r[0] = XMVectorSubtract(p1, p0);
			m1.r[1] = XMVectorSubtract(p2, p0);
			m1.r[2] = m1.r[3] = g_XMZero;

			XMMATRIX uv = XMMatrixMultiply(m0, m1);

			tangent1[i0] = XMVectorAdd(tangent1[i0], uv.r[0]);
			tangent1[i1] = XMVectorAdd(tangent1[i1], uv.r[0]);
			tangent1[i2] = XMVectorAdd(tangent1[i2], uv.r[0]);

			tangent2[i0] = XMVectorAdd(tangent2[i0], uv.r[1]);
			tangent2[i1] = XMVectorAdd(tangent2[i1], uv.r[1]);
			tangent2[i2] = XMVectorAdd(tangent2[i2], uv.r[1]);
		}

		for (size_t j = 0; j < nVerts; ++j)
		{
			// Gram-Schmidt orthonormalization
			XMVECTOR b0 = XMLoadFloat3(&normals[j]);
			b0 = XMVector3Normalize(b0);

			XMVECTOR tan1 = tangent1[j];
			XMVECTOR b1 = XMVectorSubtract(tan1, XMVectorMultiply(XMVector3Dot(b0, tan1), b0));
			b1 = XMVector3Normalize(b1);

			XMVECTOR tan2 = tangent2[j];
			XMVECTOR b2 = XMVectorSubtract(XMVectorSubtract(tan2, XMVectorMultiply(XMVector3Dot(b0, tan2), b0)), XMVectorMultiply(XMVector3Dot(b1, tan2), b1));
			b2 = XMVector3Normalize(b2);

			// handle degenerate vectors
			float len1 = XMVectorGetX(XMVector3Length(b1));
			float len2 = XMVectorGetY(XMVector3Length(b2));

			if ((len1 <= EPSILON) || (len2 <= EPSILON))
			{
				if (len1 > 0.5f)
				{
					// Reset bi-tangent from tangent and normal
					b2 = XMVector3Cross(b0, b1);
				}
				else if (len2 > 0.5f)
				{
					// Reset tangent from bi-tangent and normal
					b1 = XMVector3Cross(b2, b0);
				}
				else
				{
					// Reset both tangent and bi-tangent from normal
					XMVECTOR axis;

					float d0 = fabs(XMVectorGetX(XMVector3Dot(g_XMIdentityR0, b0)));
					float d1 = fabs(XMVectorGetX(XMVector3Dot(g_XMIdentityR1, b0)));
					float d2 = fabs(XMVectorGetX(XMVector3Dot(g_XMIdentityR2, b0)));
					if (d0 < d1)
					{
						axis = (d0 < d2) ? g_XMIdentityR0 : g_XMIdentityR2;
					}
					else if (d1 < d2)
					{
						axis = g_XMIdentityR1;
					}
					else
					{
						axis = g_XMIdentityR2;
					}

					b1 = XMVector3Cross(b0, axis);
					b2 = XMVector3Cross(b0, b1);
				}
			}

			if (tangents3)
			{
				XMStoreFloat3(&tangents3[j], b1);
			}

			if (tangents4)
			{
				XMVECTOR bi = XMVector3Cross(b0, tan1);
				float w = XMVector3Less(XMVector3Dot(bi, tan2), g_XMZero) ? -1.f : 1.f;

				bi = XMVectorSetW(b1, w);
				XMStoreFloat4(&tangents4[j], bi);
			}

			if (bitangents)
			{
				XMStoreFloat3(&bitangents[j], b2);
			}
		}

		return S_OK;
	}
}

//=====================================================================================
// Entry-points
//=====================================================================================

//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT ComputeTangentFrame(
const uint16_t* indices, size_t nFaces,
const XMFLOAT3* positions, const XMFLOAT3* normals, const XMFLOAT2* texcoords,
size_t nVerts, XMFLOAT3* tangents, XMFLOAT3* bitangents)
{
	if (!tangents && !bitangents)
		return E_INVALIDARG;

	return ComputeTangentFrameImpl<uint16_t>(indices, nFaces, positions, normals, texcoords, nVerts, tangents, nullptr, bitangents);
}


//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT ComputeTangentFrame(
const uint32_t* indices, size_t nFaces,
const XMFLOAT3* positions, const XMFLOAT3* normals, const XMFLOAT2* texcoords,
size_t nVerts, XMFLOAT3* tangents, XMFLOAT3* bitangents)
{
	if (!tangents && !bitangents)
		return E_INVALIDARG;

	return ComputeTangentFrameImpl<uint32_t>(indices, nFaces, positions, normals, texcoords, nVerts, tangents, nullptr, bitangents);
}


//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT ComputeTangentFrame(
const uint16_t* indices, size_t nFaces,
const XMFLOAT3* positions, const XMFLOAT3* normals, const XMFLOAT2* texcoords,
size_t nVerts, XMFLOAT4* tangents, XMFLOAT3* bitangents)
{
	if (!tangents && !bitangents)
		return E_INVALIDARG;

	return ComputeTangentFrameImpl<uint16_t>(indices, nFaces, positions, normals, texcoords, nVerts, nullptr, tangents, bitangents);
}


//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT ComputeTangentFrame(
const uint32_t* indices, size_t nFaces,
const XMFLOAT3* positions, const XMFLOAT3* normals, const XMFLOAT2* texcoords,
size_t nVerts, XMFLOAT4* tangents, XMFLOAT3* bitangents)
{
	if (!tangents && !bitangents)
		return E_INVALIDARG;

	return ComputeTangentFrameImpl<uint32_t>(indices, nFaces, positions, normals, texcoords, nVerts, nullptr, tangents, bitangents);
}


//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT ComputeTangentFrame(
const uint16_t* indices, size_t nFaces,
const XMFLOAT3* positions, const XMFLOAT3* normals, const XMFLOAT2* texcoords,
size_t nVerts, XMFLOAT4* tangents)
{
	if (!tangents)
		return E_INVALIDARG;

	return ComputeTangentFrameImpl<uint16_t>(indices, nFaces, positions, normals, texcoords, nVerts, nullptr, tangents, nullptr);
}


//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT ComputeTangentFrame(
const uint32_t* indices, size_t nFaces,
const XMFLOAT3* positions, const XMFLOAT3* normals, const XMFLOAT2* texcoords,
size_t nVerts, XMFLOAT4* tangents)
{
	if (!tangents)
		return E_INVALIDARG;

	return ComputeTangentFrameImpl<uint32_t>(indices, nFaces, positions, normals, texcoords, nVerts, nullptr, tangents, nullptr);
}
//////////////////////////////////////////////////////////////
template<typename T>
void CreateBuffer(_In_ ID3D11Device* device, T const& data, D3D11_BIND_FLAG bindFlags, _Outptr_ ID3D11Buffer** pBuffer)
{
	assert(pBuffer != 0);

	D3D11_BUFFER_DESC bufferDesc = { 0 };

	bufferDesc.ByteWidth = (UINT)data.size() * sizeof(T::value_type);
	bufferDesc.BindFlags = bindFlags;
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA dataDesc = { 0 };

	dataDesc.pSysMem = data.data();

	if (
		device->CreateBuffer(&bufferDesc, &dataDesc, pBuffer) != S_OK
		)
		throw std::exception();

	SetDebugObjectName(*pBuffer, "DirectXTK:GeometricPrimitive");
}
void CreateBuffer(_In_ ID3D11Device* device, std::vector<VertexPositionNormalTexture> const& data, D3D11_BIND_FLAG bindFlags, _Outptr_ ID3D11Buffer** pBuffer){
	CreateBuffer<std::vector<VertexPositionNormalTexture> >(device, data, bindFlags, pBuffer);
}
void CreateBuffer(_In_ ID3D11Device* device, std::vector<VertexPositionNormalTangentColorTexture> const& data, D3D11_BIND_FLAG bindFlags, _Outptr_ ID3D11Buffer** pBuffer){
	CreateBuffer<std::vector<VertexPositionNormalTangentColorTexture> >(device, data, bindFlags, pBuffer);
}
void CreateBuffer(_In_ ID3D11Device* device, std::vector<uint16_t> const& data, D3D11_BIND_FLAG bindFlags, _Outptr_ ID3D11Buffer** pBuffer){
	CreateBuffer<std::vector<uint16_t> >(device, data, bindFlags, pBuffer);
}
/////
class   Edge
{
	uint16_t from, to;
public:
	Edge(uint16_t f, uint16_t t)
	{
		from = f;
		to = t;
	}

	bool    operator == (const Edge& e) const
	{
		return from == e.from && to == e.to;
	}

	bool    operator < (const Edge& e) const
	{
		if (from < e.from)
			return true;

		if (from > e.from)
			return false;

		return to < e.to;
	}
};
void createAdjacencyIndex(std::vector<uint16_t> & adjacency_indices, const VertexPositionNormalTexture * vertex, int numIndex, const uint16_t * index)
{
	int numTris = numIndex / 3;

	uint16_t * res = new uint16_t[2 * numIndex];    // it will double number of elements
	uint16_t * src = new uint16_t[numIndex];    // it will double number of elements

	int newNumTris = 0;
    // удаляем выражденные треугольники  
	std::vector<VertexPositionNormalTexture> proccessVertex;
	for (int i = 0; i < numTris; i++)
	{
		SimpleMath::Vector3 a = SimpleMath::Vector3(vertex[index[i * 3 + 0]].position.x, vertex[index[i * 3 + 0]].position.y, vertex[index[i * 3 + 0]].position.z);
		SimpleMath::Vector3 b = SimpleMath::Vector3(vertex[index[i * 3 + 1]].position.x, vertex[index[i * 3 + 1]].position.y, vertex[index[i * 3 + 1]].position.z);
		SimpleMath::Vector3 c = SimpleMath::Vector3(vertex[index[i * 3 + 2]].position.x, vertex[index[i * 3 + 2]].position.y, vertex[index[i * 3 + 2]].position.z);

		auto va = b - a;
		auto vb = c - a;
		if (va.Cross(vb).LengthSquared() < 0.00000001){
			proccessVertex.push_back(vertex[index[i * 3 + 0]]);
			proccessVertex.push_back(vertex[index[i * 3 + 1]]);
			proccessVertex.push_back(vertex[index[i * 3 + 2]]);
			continue;
		}

		src[newNumTris * 3 + 0] = index[i * 3 + 0];
		src[newNumTris * 3 + 1] = index[i * 3 + 1];
		src[newNumTris * 3 + 2] = index[i * 3 + 2];
		newNumTris++;
	}
	numTris = newNumTris;
	numIndex = numTris * 3;
	//удалим дубли вершины
	for (int i = 0; i < proccessVertex.size(); i++){
		SimpleMath::Vector3 current = SimpleMath::Vector3(proccessVertex[i].position.x, proccessVertex[i].position.y, proccessVertex[i].position.z);
		for (int j = 0; j < proccessVertex.size(); j++)
		{
			if (i == j) continue;
			SimpleMath::Vector3 test = SimpleMath::Vector3(proccessVertex[j].position.x, proccessVertex[j].position.y, proccessVertex[j].position.z);
			if ((current - test).LengthSquared() < 0.00000001){
				proccessVertex.erase(proccessVertex.begin() + j);
				j--;
			}
		}
	}
	//найдем все индексы на вершину
	std::vector<std::vector<uint16_t> > proccessIndex;
	for (int i = 0; i < proccessVertex.size(); i++){
		SimpleMath::Vector3 current = SimpleMath::Vector3(proccessVertex[i].position.x, proccessVertex[i].position.y, proccessVertex[i].position.z);
		proccessIndex.push_back(std::vector<uint16_t>());
		for (int j = 0; j <numIndex; j++)
		{
			SimpleMath::Vector3 test = SimpleMath::Vector3(vertex[src[j]].position.x, vertex[src[j]].position.y, vertex[src[j]].position.z);
			if ((current - test).LengthSquared() < 0.00000001){
				proccessIndex[i].push_back(j);
			}
		}
	}
	//заменим все индексы в группе на первый
	for (int i = 0; i < proccessIndex.size(); i++){
		for (int j = 1; j <proccessIndex[i].size(); j++)
		{
			src[proccessIndex[i][j]] = src[proccessIndex[i][0]];
		}
	}

	std::map<Edge, uint16_t> adj;                // for every edge keep vertex of triangle
	// build adjacence map
	for (int i = 0; i < numTris; i++)
	{
		int j = 3 * i;

		adj[Edge(src[j + 0], src[j + 1])] = src[j + 2];
		adj[Edge(src[j + 1], src[j + 2])] = src[j + 0];
		adj[Edge(src[j + 2], src[j + 0])] = src[j + 1];
	}

	// now fill new structure
	for (int i = 0; i < numTris; i++)
	{
		int j = 6 * i;
		int k = 3 * i;

		res[j + 0] = src[k + 0];          // source triangle
		res[j + 2] = src[k + 1];
		res[j + 4] = src[k + 2];
		// now adjacent vertices
		if (adj.find(Edge(src[k + 1], src[k + 0])) == adj.end())
			throw std::exception();
		if (adj.find(Edge(src[k + 2], src[k + 1])) == adj.end())
			throw std::exception();
		if (adj.find(Edge(src[k + 0], src[k + 2])) == adj.end())
			throw std::exception();

		res[j + 1] = adj[Edge(src[k + 1], src[k + 0])];
		res[j + 3] = adj[Edge(src[k + 2], src[k + 1])];
		res[j + 5] = adj[Edge(src[k + 0], src[k + 2])];
	}

	adjacency_indices = std::vector<uint16_t>(res, &res[2 * numIndex]);

	delete res;

	delete src;
}