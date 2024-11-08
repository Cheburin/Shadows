//--------------------------------------------------------------------------------------
// File: DetailTessellation11.cpp
//
// This sample demonstrates the use of detail tessellation for improving the 
// quality of material surfaces in real-time rendering applications.
//
// Developed by AMD Developer Relations team.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "GeometricPrimitive.h"
#include "Effects.h"
#include "DirectXHelpers.h"
#include "DirectXMath.h"
// #include <DirectXHelpers.h>
// #include <DDSTextureLoader.h>
// #include "pch.h"
#include "CommonStates.h"
#include "ConstantBuffer.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "SimpleMath.h"

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
#include "Model.h"
#include <map>
#include <algorithm>
#include <array>
using namespace DirectX;
//--------------------------------------------------------------------------------------
// Macros
//--------------------------------------------------------------------------------------
#define DWORD_POSITIVE_RANDOM(x)    ((DWORD)(( ((x)*rand()) / RAND_MAX )))
#define FLOAT_POSITIVE_RANDOM(x)    ( ((x)*rand()) / RAND_MAX )
#define FLOAT_RANDOM(x)             ((((2.0f*rand())/RAND_MAX) - 1.0f)*(x))

//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
const float GRID_SIZE               = 200.0f;
const float MAX_TESSELLATION_FACTOR = 15.0f;

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct CONSTANT_BUFFER_STRUCT
{
    D3DXMATRIXA16    mWorld;                         // World matrix
    D3DXMATRIXA16    mView;                          // View matrix
    D3DXMATRIXA16    mProjection;                    // Projection matrix
    D3DXMATRIXA16    mWorldViewProjection;           // WVP matrix
    D3DXMATRIXA16    mViewProjection;                // VP matrix
    D3DXMATRIXA16    mInvView;                       // Inverse of view matrix
    D3DXVECTOR4      vScreenResolution;              // Screen resolution
    D3DXVECTOR4      vMeshColor;                     // Mesh color
    D3DXVECTOR4      vTessellationFactor;            // Edge, inside, minimum tessellation factor and 1/desired triangle size
    D3DXVECTOR4      vDetailTessellationHeightScale; // Height scale for detail tessellation of grid surface
    D3DXVECTOR4      vGridSize;                      // Grid size
    D3DXVECTOR4      vDebugColorMultiply;            // Debug colors
    D3DXVECTOR4      vDebugColorAdd;                 // Debug colors
    D3DXVECTOR4      vFrustumPlaneEquation[4];       // View frustum plane equations
};                                                      
                                                        
struct MATERIAL_CB_STRUCT
{
    D3DXVECTOR4     g_materialAmbientColor;  // Material's ambient color
    D3DXVECTOR4     g_materialDiffuseColor;  // Material's diffuse color
    D3DXVECTOR4     g_materialSpecularColor; // Material's specular color
    D3DXVECTOR4     g_fSpecularExponent;     // Material's specular exponent

    D3DXVECTOR4     g_LightPosition;         // Light's position in world space
    D3DXVECTOR4     g_LightDiffuse;          // Light's diffuse color
    D3DXVECTOR4     g_LightAmbient;          // Light's ambient color

    D3DXVECTOR4     g_vEye;                  // Camera's location
    D3DXVECTOR4     g_fBaseTextureRepeat;    // The tiling factor for base and normal map textures
    D3DXVECTOR4     g_fPOMHeightMapScale;    // Describes the useful range of values for the height field

    D3DXVECTOR4     g_fShadowSoftening;      // Blurring factor for the soft shadows computation

    int             g_nMinSamples;           // The minimum number of samples for sampling the height field
    int             g_nMaxSamples;           // The maximum number of samples for sampling the height field
    int             uDummy1;
    int             uDummy2;
};

struct DETAIL_TESSELLATION_TEXTURE_STRUCT
{
    WCHAR* DiffuseMap;                  // Diffuse texture map
    WCHAR* NormalHeightMap;             // Normal and height map (normal in .xyz, height in .w)
    WCHAR* DisplayName;                 // Display name of texture
    float  fHeightScale;                // Height scale when rendering
    float  fBaseTextureRepeat;          // Base repeat of texture coordinates (1.0f for no repeat)
    float  fDensityScale;               // Density scale (used for density map generation)
    float  fMeaningfulDifference;       // Meaningful difference (used for density map generation)
};

//--------------------------------------------------------------------------------------
// Enums
//--------------------------------------------------------------------------------------
enum
{
    TESSELLATIONMETHOD_DISABLED,
    TESSELLATIONMETHOD_DISABLED_POM,
    TESSELLATIONMETHOD_DETAIL_TESSELLATION, 
} eTessellationMethod;

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
// DXUT resources
CDXUTDialogResourceManager          g_DialogResourceManager;    // Manager for shared resources of dialogs
CFirstPersonCamera                  g_Camera;
CD3DSettingsDlg                     g_D3DSettingsDlg;           // Device settings dialog
CDXUTDialog                         g_HUD;                      // Manages the 3D   
CDXUTDialog                         g_SampleUI;                 // Dialog for sample specific controls
CDXUTTextHelper*                    g_pTxtHelper = NULL;

// Shaders
ID3D11VertexShader*                 g_pPOMVS = NULL;
ID3D11PixelShader*                  g_pPOMPS = NULL;
ID3D11VertexShader*                 g_pDetailTessellationVS = NULL;
ID3D11VertexShader*                 g_pNoTessellationVS = NULL;
ID3D11HullShader*                   g_pDetailTessellationHS = NULL;
ID3D11DomainShader*                 g_pDetailTessellationDS = NULL;
ID3D11PixelShader*                  g_pBumpMapPS = NULL;
ID3D11VertexShader*                 g_pParticleVS = NULL;
ID3D11GeometryShader*               g_pParticleGS = NULL;
ID3D11PixelShader*                  g_pParticlePS = NULL;

// Textures
#define NUM_TEXTURES 7
DETAIL_TESSELLATION_TEXTURE_STRUCT DetailTessellationTextures[NUM_TEXTURES + 1] =
{
//    DiffuseMap              NormalHeightMap                 DisplayName,    fHeightScale fBaseTextureRepeat fDensityScale fMeaningfulDifference
    { L"Textures\\rocks.jpg",    L"Textures\\rocks_NM_height.dds",  L"Rocks",       10.0f,       1.0f,              25.0f,        2.0f/255.0f },
    { L"Textures\\stones.bmp",   L"Textures\\stones_NM_height.dds", L"Stones",      5.0f,        1.0f,              10.0f,        5.0f/255.0f },
    { L"Textures\\wall.jpg",     L"Textures\\wall_NM_height.dds",   L"Wall",        8.0f,        1.0f,              7.0f,         7.0f/255.0f },
    { L"Textures\\wood.jpg",     L"Textures\\four_NM_height.dds",   L"Four shapes", 30.0f,       1.0f,              35.0f,        2.0f/255.0f },
    { L"Textures\\concrete.bmp", L"Textures\\bump_NM_height.dds",   L"Bump",        10.0f,       4.0f,              50.0f,        2.0f/255.0f },
    { L"Textures\\concrete.bmp", L"Textures\\dent_NM_height.dds",   L"Dent",        10.0f,       4.0f,              50.0f,        2.0f/255.0f },
    { L"Textures\\wood.jpg",     L"Textures\\saint_NM_height.dds",  L"Saints" ,     20.0f,       1.0f,              40.0f,        2.0f/255.0f },
    { L"",                       L"",                               L"Custom" ,     5.0f,        1.0f,              10.0f,        2.0f/255.0f },
};
DWORD                               g_dwNumTextures = 0;
ID3D11ShaderResourceView*           g_pDetailTessellationBaseTextureRV[NUM_TEXTURES+1];
ID3D11ShaderResourceView*           g_pDetailTessellationHeightTextureRV[NUM_TEXTURES+1];
ID3D11ShaderResourceView*           g_pDetailTessellationDensityTextureRV[NUM_TEXTURES+1];
ID3D11ShaderResourceView*           g_pLightTextureRV = NULL;
WCHAR                               g_pszCustomTextureDiffuseFilename[MAX_PATH] = L"";
WCHAR                               g_pszCustomTextureNormalHeightFilename[MAX_PATH] = L"";
DWORD                               g_bRecreateCustomTextureDensityMap  = false;

// Geometry
ID3D11Buffer*                       g_pGridTangentSpaceVB = NULL;
ID3D11Buffer*                       g_pGridTangentSpaceIB = NULL;
ID3D11Buffer*                       g_pMainCB = NULL;
ID3D11Buffer*                       g_pMaterialCB = NULL;
ID3D11InputLayout*                  g_pTangentSpaceVertexLayout = NULL;
ID3D11InputLayout*                  g_pPositionOnlyVertexLayout = NULL;
ID3D11Buffer*                       g_pGridDensityVB[NUM_TEXTURES+1];
ID3D11ShaderResourceView*           g_pGridDensityVBSRV[NUM_TEXTURES+1];
ID3D11Buffer*                       g_pParticleVB = NULL;

// States
ID3D11RasterizerState*              g_pRasterizerStateSolid = NULL;
ID3D11RasterizerState*              g_pRasterizerStateWireframe = NULL;
ID3D11SamplerState*                 g_pSamplerStateLinear = NULL;
ID3D11BlendState*                   g_pBlendStateNoBlend = NULL;
ID3D11BlendState*                   g_pBlendStateAdditiveBlending = NULL;
ID3D11DepthStencilState*            g_pDepthStencilState = NULL;

// Camera and light parameters
D3DXVECTOR3                         g_vecEye(76.099495f, 43.191154f, -110.108971f);
D3DXVECTOR3                         g_vecAt (75.590889f, 42.676582f, -109.418678f);
D3DXVECTOR4                         g_LightPosition(100.0f, 30.0f, -50.0f, 1.0f);
CDXUTDirectionWidget                g_LightControl; 
D3DXVECTOR4							g_pWorldSpaceFrustumPlaneEquation[6];

// Render settings
int                                 g_nRenderHUD = 2;
DWORD                               g_dwGridTessellation = 50;
bool                                g_bEnableWireFrame = false;
float                               g_fTessellationFactorEdges = 7.0f;
float                               g_fTessellationFactorInside = 7.0f;
int                                 g_nTessellationMethod = TESSELLATIONMETHOD_DETAIL_TESSELLATION;
int                                 g_nCurrentTexture = 0;
bool                                g_bFrameBasedAnimation = FALSE;
bool                                g_bAnimatedCamera = FALSE;
bool                                g_bDisplacementEnabled = TRUE;
D3DXVECTOR3                         g_vDebugColorMultiply(1.0, 1.0, 1.0);
D3DXVECTOR3                         g_vDebugColorAdd(0.0, 0.0, 0.0);
bool                                g_bDensityBasedDetailTessellation = TRUE;
bool                                g_bDistanceAdaptiveDetailTessellation = TRUE;
bool                                g_bScreenSpaceAdaptiveDetailTessellation = FALSE;
bool                                g_bUseFrustumCullingOptimization = TRUE;
bool                                g_bDetailTessellationShadersNeedReloading = FALSE;
bool                                g_bShowFPS = TRUE;
bool                                g_bDrawLightSource = TRUE;
float                               g_fDesiredTriangleSize = 10.0f;

// Frame buffer readback ( 0 means never dump to disk (frame counter starts at 1) )
DWORD                               g_dwFrameNumberToDump = 0; 

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN                        1
#define IDC_TOGGLEREF                               3
#define IDC_CHANGEDEVICE                            4
#define IDC_STATIC_TECHNIQUE                       10
#define IDC_RADIOBUTTON_DISABLED                   11
#define IDC_RADIOBUTTON_POM                        12
#define IDC_RADIOBUTTON_DETAILTESSELLATION         13
#define IDC_STATIC_TEXTURE                         14
#define IDC_COMBOBOX_TEXTURE                       15
#define IDC_CHECKBOX_WIREFRAME                     16
#define IDC_CHECKBOX_DISPLACEMENT                  18
#define IDC_CHECKBOX_DENSITYBASED                  19    
#define IDC_STATIC_ADAPTIVE_SCHEME                 20
#define IDC_RADIOBUTTON_ADAPTIVESCHEME_OFF         21
#define IDC_RADIOBUTTON_ADAPTIVESCHEME_DISTANCE    22
#define IDC_RADIOBUTTON_ADAPTIVESCHEME_SCREENSPACE 23
#define IDC_STATIC_DESIRED_TRIANGLE_SIZE           24
#define IDC_SLIDER_DESIRED_TRIANGLE_SIZE           25
#define IDC_CHECKBOX_FRUSTUMCULLING                26
#define IDC_STATIC_TESS_FACTOR_EDGES               31
#define IDC_SLIDER_TESS_FACTOR_EDGES               32
#define IDC_STATIC_TESS_FACTOR_INSIDE              33
#define IDC_SLIDER_TESS_FACTOR_INSIDE              34
#define IDC_CHECKBOX_LINK_TESS_FACTORS             35
#define IDC_CHECKBOX_ROTATING_CAMERA               36

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo, 
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, 
                                      void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain, 
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, 
                                  double fTime, float fElapsedTime, void* pUserContext );

void ExtractPlanesFromFrustum( D3DXVECTOR4* pPlaneEquation, const D3DXMATRIX* pMatrix, bool bNormalize=TRUE );

void InitApp();
void RenderText();
bool IsNextArg( WCHAR*& strCmdLine, WCHAR* strArg );
bool GetCmdParam( WCHAR*& strCmdLine, WCHAR* strFlag );
void CreateDensityMapFromHeightMap( ID3D11Device* pd3dDevice, ID3D11DeviceContext *pDeviceContext, ID3D11Texture2D* pHeightMap, 
                                    float fDensityScale, ID3D11Texture2D** ppDensityMap, ID3D11Texture2D** ppDensityMapStaging=NULL, 
                                    float fMeaningfulDifference=0.0f);
void CreateEdgeDensityVertexStream( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pDeviceContext, D3DXVECTOR2* pTexcoord, 
                                    DWORD dwVertexStride, void *pIndex, DWORD dwIndexSize, DWORD dwNumIndices, 
                                    ID3D11Texture2D* pDensityMap, ID3D11Buffer** ppEdgeDensityVertexStream, 
                                    float fBaseTextureRepeat);
void CreateStagingBufferFromBuffer( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pContext, ID3D11Buffer* pInputBuffer, 
                                    ID3D11Buffer **ppStagingBuffer);
HRESULT CreateShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                              LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                              ID3DX11ThreadPump* pPump, ID3D11DeviceChild** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                              BOOL bDumpShader = FALSE);
HRESULT CreateVertexShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                    LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                    ID3DX11ThreadPump* pPump, ID3D11VertexShader** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                                    BOOL bDumpShader = FALSE);
HRESULT CreateHullShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                  LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                  ID3DX11ThreadPump* pPump, ID3D11HullShader** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                                  BOOL bDumpShader = FALSE);
HRESULT CreateDomainShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                    LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                    ID3DX11ThreadPump* pPump, ID3D11DomainShader** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                                    BOOL bDumpShader = FALSE);
HRESULT CreateGeometryShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                      LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                      ID3DX11ThreadPump* pPump, ID3D11GeometryShader** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                                      BOOL bDumpShader = FALSE);
HRESULT CreatePixelShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                   LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                   ID3DX11ThreadPump* pPump, ID3D11PixelShader** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                                   BOOL bDumpShader = FALSE);
HRESULT CreateComputeShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                     LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                     ID3DX11ThreadPump* pPump, ID3D11ComputeShader** ppShader, ID3DBlob** ppShaderBlob = NULL, 
                                     BOOL bDumpShader = FALSE);
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
SceneState main_scene_state;

Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;

std::unique_ptr< DirectX::ConstantBuffer<SceneState> > main_scene_state_cb;
std::unique_ptr<DirectX::IEffect> quadEffect, shadowCasterEffect, shadowEffect, quadDepthEffect, shadowCasterDepthEffect, quadOnlyDepthEffect, mirrowEffect;
std::unique_ptr<ModelMeshPartWithAdjacency> torus;
std::unique_ptr<CommonStates> states;

std::unique_ptr<DirectX::Keyboard> m_keyboard;
std::unique_ptr<DirectX::Mouse> m_mouse;
HWND DXUTgetWindow();

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv1, srv2;
Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv1, rtv2;
Microsoft::WRL::ComPtr<ID3D11DepthStencilView> ds_v1, ds_v2;

float m_pitch;
float m_yaw;
const float ROTATION_GAIN = 0.004f;
const float MOVEMENT_GAIN = 0.02f;
DirectX::XMVECTOR m_cameraPos;

Microsoft::WRL::ComPtr<ID3D11DepthStencilState> shadow_stencil_state;
Microsoft::WRL::ComPtr<ID3D11DepthStencilState> pass_with_stencil_state;
//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // DXUT will create and use the best device (either D3D10 or D3D11) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set DXUT callbacks
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackFrameMove( OnFrameMove );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    DXUTSetCallbackKeyboard( OnKeyboard );

    InitApp();
    DXUTInit( true, true );
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"DX11BaseApp" );
    DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, 1024, 768);
	DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    WCHAR szTemp[256];
    
    // Initialize dialogs
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent );

    // Setup the camera's view parameters
    g_Camera.SetRotateButtons( true, false, false );
    g_Camera.SetEnablePositionMovement( true );
    g_Camera.SetViewParams( &g_vecEye, &g_vecAt );
    g_Camera.SetScalers( 0.005f, 100.0f );
    D3DXVECTOR3 vEyePt, vEyeTo;
    vEyePt.x = 76.099495f;
    vEyePt.y = 43.191154f;
    vEyePt.z = -110.108971f;

    vEyeTo.x = 75.590889f;
    vEyeTo.y = 42.676582f;
    vEyeTo.z = -109.418678f; 

    g_Camera.SetViewParams( &vEyePt, &vEyeTo );
    // Initialize the light control
    D3DXVECTOR3 vLightDirection;
    vLightDirection = -(D3DXVECTOR3)g_LightPosition;
    D3DXVec3Normalize( &vLightDirection, &vLightDirection );
    g_LightControl.SetLightDirection( vLightDirection );
    g_LightControl.SetRadius( GRID_SIZE/2.0f );

    // Load default scene
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( ( DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
              pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }

        // Enable 4xMSAA by default
        DXGI_SAMPLE_DESC MSAA4xSampleDesc = { 4, 0 };
        pDeviceSettings->d3d11.sd.SampleDesc = MSAA4xSampleDesc;
    }

    return true;
}

//--------------------------------------------------------------------------------------
// Handle updates to the scene
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove(double fTime, float fElapsedTime, void* pUserContext)
{
	// Update the camera's position based on user input 
	auto mouse = m_mouse->GetState();

	if (mouse.positionMode == Mouse::MODE_RELATIVE)
	{
		SimpleMath::Vector3 delta = SimpleMath::Vector3(float(mouse.x), float(mouse.y), 0.f)
			* ROTATION_GAIN;

		m_pitch -= delta.y;
		m_yaw -= delta.x;

		// limit pitch to straight up or straight down
		// with a little fudge-factor to avoid gimbal lock
		float limit = XM_PI / 2.0f - 0.01f;
		m_pitch = max(-limit, m_pitch);
		m_pitch = min(+limit, m_pitch);

		// keep longitude in sane range by wrapping
		if (m_yaw > XM_PI)
		{
			m_yaw -= XM_PI * 2.0f;
		}
		else if (m_yaw < -XM_PI)
		{
			m_yaw += XM_PI * 2.0f;
		}
	}
	m_mouse->SetMode(mouse.leftButton ? Mouse::MODE_RELATIVE : Mouse::MODE_ABSOLUTE);
	{
		float z = sinf(m_pitch);
		float r = -cosf(m_pitch);
		float y = r*cosf(m_yaw);
		float x = r*sinf(m_yaw);
		XMVECTOR lookAt = m_cameraPos + XMLoadFloat3(&XMFLOAT3(x, y, z));
		auto Up = XMLoadFloat3(&XMFLOAT3(0, 0, 1));
		XMMATRIX view = XMMatrixLookAtLH(m_cameraPos, lookAt, Up);
		DirectX::XMStoreFloat4x4(&main_scene_state.mView, DirectX::XMMatrixTranspose(view));
    }
	auto kb = m_keyboard->GetState();
	{
		SimpleMath::Vector3 move = SimpleMath::Vector3::Zero;

		if (kb.Up || kb.W)
			move.z += 1.f;

		if (kb.Down || kb.S)
			move.z -= 1.f;

		if (kb.Left || kb.A)
			move.x -= 1.f;

		if (kb.Right || kb.D)
			move.x += 1.f;

		if (kb.PageUp || kb.Space)
			move.y += 1.f;

		if (kb.PageDown || kb.X)
			move.y -= 1.f;

		auto invView = DirectX::XMMatrixInverse(0, DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&main_scene_state.mView)));

		move = DirectX::XMVector3TransformNormal(XMLoadFloat3(&move), invView);

		//SimpleMath::Quaternion q = SimpleMath::Quaternion::CreateFromYawPitchRoll(0, m_yaw, m_pitch); //m_yaw, m_pitch

		//move = SimpleMath::Vector3::Transform(move, q);

		move *= MOVEMENT_GAIN;

		m_cameraPos += move;
	}
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( g_bShowFPS && DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    
    g_pTxtHelper->End();
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

	switch(uMsg){
		case WM_INPUT:
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEWHEEL:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
		case WM_MOUSEHOVER:
			Mouse::ProcessMessage(uMsg, wParam, lParam);
			break;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYUP:
			Keyboard::ProcessMessage(uMsg, wParam, lParam);
			break;
	}

    switch( uMsg )
    {
        case WM_KEYDOWN:    // Prevent the camera class to use some prefefined keys that we're already using    
                            switch( (UINT)wParam )
                            {
                                case VK_CONTROL:    
                                case VK_LEFT:        
                                case VK_RIGHT:         
                                case VK_UP:            
                                case VK_DOWN:          
                                case VK_PRIOR:              
                                case VK_NEXT:             

                                case VK_NUMPAD4:     
                                case VK_NUMPAD6:     
                                case VK_NUMPAD8:     
                                case VK_NUMPAD2:     
                                case VK_NUMPAD9:           
                                case VK_NUMPAD3:         

                                case VK_HOME:       return 0;

                                case VK_RETURN:     
                                {
                                    // Reset camera position
                                    g_Camera.SetViewParams( &g_vecEye, &g_vecAt );

                                    // Reset light position
                                    g_LightPosition = D3DXVECTOR4(100.0f, 30.0f, -50.0f, 1.0f);
                                    D3DXVECTOR3 vLightDirection;
                                    vLightDirection = -(D3DXVECTOR3)g_LightPosition;
                                    D3DXVec3Normalize(&vLightDirection, &vLightDirection);
                                    g_LightControl.SetLightDirection(vLightDirection);
                                }
                                break;
                            }
    }

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    // Pass the message to be handled to the light control:
    g_LightControl.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if( bKeyDown )
    {
        switch( nChar )
        {
            case 'V':           // Debug views
                                if (g_vDebugColorMultiply.x==1.0)
                                {
                                    g_vDebugColorMultiply.x=0.0;
                                    g_vDebugColorMultiply.y=0.0;
                                    g_vDebugColorMultiply.z=0.0;
                                    g_vDebugColorAdd.x=1.0;
                                    g_vDebugColorAdd.y=1.0;
                                    g_vDebugColorAdd.z=1.0;
                                }
                                else
                                {
                                    g_vDebugColorMultiply.x=1.0;
                                    g_vDebugColorMultiply.y=1.0;
                                    g_vDebugColorMultiply.z=1.0;
                                    g_vDebugColorAdd.x=0.0;
                                    g_vDebugColorAdd.y=0.0;
                                    g_vDebugColorAdd.z=0.0;
                                }
                                break;

            case 'H':
            case VK_F1:         g_nRenderHUD = (0 ) % 3; break;

        }
    }
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    WCHAR szTemp[256];

    switch( nControlID )
    {
    }
}


//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}

struct EffectShaderFileDef{
	WCHAR * name;
	WCHAR * entry_point;
	WCHAR * shader_ver;
};
std::unique_ptr<DirectX::IEffect> createHlslEffect(ID3D11Device* device, std::map<const WCHAR*, EffectShaderFileDef>& fileDef);
void CreateBuffer(_In_ ID3D11Device* device, std::vector<VertexPositionNormalTexture> const& data, D3D11_BIND_FLAG bindFlags, _Outptr_ ID3D11Buffer** pBuffer);
void CreateBuffer(_In_ ID3D11Device* device, std::vector<uint16_t> const& data, D3D11_BIND_FLAG bindFlags, _Outptr_ ID3D11Buffer** pBuffer);
void createAdjacencyIndex(std::vector<uint16_t> & adjacencyIndex, const VertexPositionNormalTexture * v, int numIndex, const uint16_t * i);
/////
std::unique_ptr<ModelMeshPartWithAdjacency> CreateModelMeshPart(ID3D11Device* device, std::function<void(std::vector<VertexPositionNormalTexture> & _vertices, std::vector<uint16_t> & _indices)> createGeometry){
	std::vector<VertexPositionNormalTexture> vertices;
	std::vector<uint16_t> indices, adjacencyIndices;

	createGeometry(vertices, indices);

	createAdjacencyIndex(adjacencyIndices, vertices.data(), indices.size(), indices.data());

	std::unique_ptr<ModelMeshPartWithAdjacency> modelMeshPArt(new ModelMeshPartWithAdjacency());

	modelMeshPArt->adjacencyIndexCount = adjacencyIndices.size();
	modelMeshPArt->indexCount = indices.size();
	modelMeshPArt->startIndex = 0;
	modelMeshPArt->vertexOffset = 0;
	modelMeshPArt->vertexStride = sizeof(VertexPositionNormalTexture);
	modelMeshPArt->primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	modelMeshPArt->indexFormat = DXGI_FORMAT_R16_UINT;
	modelMeshPArt->vbDecl = std::shared_ptr<std::vector<D3D11_INPUT_ELEMENT_DESC>>(
		new std::vector<D3D11_INPUT_ELEMENT_DESC>(
		&VertexPositionNormalTexture::InputElements[0],
		&VertexPositionNormalTexture::InputElements[VertexPositionNormalTexture::InputElementCount]
		)
	);

	CreateBuffer(device, vertices, D3D11_BIND_VERTEX_BUFFER, modelMeshPArt->vertexBuffer.ReleaseAndGetAddressOf());

	CreateBuffer(device, indices, D3D11_BIND_INDEX_BUFFER, modelMeshPArt->indexBuffer.ReleaseAndGetAddressOf());

	CreateBuffer(device, adjacencyIndices, D3D11_BIND_INDEX_BUFFER, modelMeshPArt->adjacencyIndexBuffer.ReleaseAndGetAddressOf());
	
	return modelMeshPArt;
}
HRESULT CreateDepthStencilState(ID3D11Device* device,
	BOOL DepthEnable,
	D3D11_DEPTH_WRITE_MASK DepthWriteMask,
	D3D11_COMPARISON_FUNC DepthFunc,

	BOOL StencilEnable,
	UINT8 StencilReadMask,
	UINT8 StencilWriteMask,

	D3D11_COMPARISON_FUNC FrontFaceStencilFunc,
	D3D11_STENCIL_OP FrontFaceStencilPassOp,
	D3D11_STENCIL_OP FrontFaceStencilFailOp,
	D3D11_STENCIL_OP FrontFaceStencilDepthFailOp,

	D3D11_COMPARISON_FUNC BackFaceStencilFunc,
	D3D11_STENCIL_OP BackFaceStencilPassOp,
	D3D11_STENCIL_OP BackFaceStencilFailOp,
	D3D11_STENCIL_OP BackFaceStencilDepthFailOp,

	ID3D11DepthStencilState** pResult);
/////
//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	{
		std::map<const WCHAR*, EffectShaderFileDef> shaderDef;
		shaderDef[L"VS"] = { L"Quad.hlsl", L"BILLBOARD_MIRROW", L"vs_5_0" };
		shaderDef[L"PS"] = { L"Quad.hlsl", L"MIRROW_COLOR", L"ps_5_0" };

		mirrowEffect = createHlslEffect(pd3dDevice, shaderDef);
	}
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	{
		std::map<const WCHAR*, EffectShaderFileDef> shaderDef;
		shaderDef[L"VS"] = { L"Quad.hlsl", L"BILLBOARD", L"vs_5_0" };
		shaderDef[L"PS"] = { L"Quad.hlsl", L"CONST_COLOR", L"ps_5_0" };

		quadEffect = createHlslEffect(pd3dDevice, shaderDef);
	}
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	{
		std::map<const WCHAR*, EffectShaderFileDef> shaderDef;
		shaderDef[L"VS"] = { L"Quad.hlsl", L"BILLBOARD", L"vs_5_0" };
		shaderDef[L"PS"] = { L"Quad.hlsl", L"CONST_COLOR2", L"ps_5_0" };

		quadOnlyDepthEffect = createHlslEffect(pd3dDevice, shaderDef);
	}
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	{
		std::map<const WCHAR*, EffectShaderFileDef> shaderDef;
		shaderDef[L"VS"] = { L"Quad.hlsl", L"BILLBOARD_DEPTH", L"vs_5_0" };
		shaderDef[L"PS"] = { L"Quad.hlsl", L"DEPTH", L"ps_5_0" };

		quadDepthEffect = createHlslEffect(pd3dDevice, shaderDef);
	}
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	{
		std::map<const WCHAR*, EffectShaderFileDef> shaderDef;
		shaderDef[L"VS"] = { L"ShadowCaster.hlsl", L"LIGHTED_MODEL", L"vs_5_0" };
		shaderDef[L"PS"] = { L"ShadowCaster.hlsl", L"LAMBERT", L"ps_5_0" };

		shadowCasterEffect = createHlslEffect(pd3dDevice, shaderDef);
	}
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	{
		std::map<const WCHAR*, EffectShaderFileDef> shaderDef;
		shaderDef[L"VS"] = { L"ShadowCasterDepth.hlsl", L"MODEL_DEPTH", L"vs_5_0" };
		shaderDef[L"PS"] = { L"ShadowCasterDepth.hlsl", L"DEPTH", L"ps_5_0" };

		shadowCasterDepthEffect = createHlslEffect(pd3dDevice, shaderDef);
	}
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	{
		std::map<const WCHAR*, EffectShaderFileDef> shaderDef;
		shaderDef[L"VS"] = { L"shadow.hlsl", L"THROWPUT", L"vs_5_0" };
		shaderDef[L"GS"] = { L"shadow.hlsl", L"FINS", L"gs_5_0" };
		shaderDef[L"PS"] = { L"shadow.hlsl", L"COLOR", L"ps_5_0" };

		shadowEffect = createHlslEffect(pd3dDevice, shaderDef);
	}
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Get device context
	ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	states = std::make_unique<CommonStates>(pd3dDevice);
	
	main_scene_state_cb = std::make_unique<ConstantBuffer<SceneState> >(pd3dDevice);

	torus = CreateModelMeshPart(pd3dDevice, [=](std::vector<VertexPositionNormalTexture> & _vertices, std::vector<uint16_t> & _indices){
		GeometricPrimitive::CreateTorus(_vertices, _indices, 1, 0.33, 32U, false);
	});
	torus->CreateInputLayout(pd3dDevice, shadowCasterEffect.get(), &inputLayout);

	m_cameraPos = XMLoadFloat3(&XMFLOAT3(0, 2, 2));
	//auto LookAt = XMLoadFloat3(&XMFLOAT3(0, 0, 1));
	//auto Up = XMLoadFloat3(&XMFLOAT3(0, 0, 1));

	//DirectX::XMStoreFloat4x4(&main_scene_state.mView, DirectX::XMMatrixTranspose(DirectX::XMMatrixLookAtLH(m_cameraPos, LookAt, Up)));

	m_keyboard = std::make_unique<Keyboard>();
	m_mouse = std::make_unique<Mouse>();
	m_mouse->SetWindow(DXUTgetWindow());

    // GUI creation
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    // Light control
    g_LightControl.StaticOnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext );

    //
    // Compile shaders
    //

	CreateDepthStencilState(pd3dDevice,
		/*BOOL DepthEnable*/true,
		/*D3D11_DEPTH_WRITE_MASK DepthWriteMask*/D3D11_DEPTH_WRITE_MASK_ZERO,
		/*D3D11_COMPARISON_FUNC DepthFunc*/D3D11_COMPARISON_LESS,

		/*BOOL StencilEnable*/true,
		/*UINT8 StencilReadMask*/D3D11_DEFAULT_STENCIL_READ_MASK,
		/*UINT8 StencilWriteMask*/D3D11_DEFAULT_STENCIL_READ_MASK,

		/*D3D11_COMPARISON_FUNC FrontFaceStencilFunc*/D3D11_COMPARISON_ALWAYS,
		/*D3D11_STENCIL_OP FrontFaceStencilPassOp*/D3D11_STENCIL_OP_INCR,
		/*D3D11_STENCIL_OP FrontFaceStencilFailOp*/D3D11_STENCIL_OP_KEEP,
		/*D3D11_STENCIL_OP FrontFaceStencilDepthFailOp*/D3D11_STENCIL_OP_KEEP,

		/*D3D11_COMPARISON_FUNC BackFaceStencilFunc*/D3D11_COMPARISON_ALWAYS,
		/*D3D11_STENCIL_OP BackFaceStencilPassOp*/D3D11_STENCIL_OP_DECR,
		/*D3D11_STENCIL_OP BackFaceStencilFailOp*/D3D11_STENCIL_OP_KEEP,
		/*D3D11_STENCIL_OP BackFaceStencilDepthFailOp*/D3D11_STENCIL_OP_KEEP,

		shadow_stencil_state.ReleaseAndGetAddressOf());

	CreateDepthStencilState(pd3dDevice,
		/*BOOL DepthEnable*/true,
		/*D3D11_DEPTH_WRITE_MASK DepthWriteMask*/D3D11_DEPTH_WRITE_MASK_ZERO,
		/*D3D11_COMPARISON_FUNC DepthFunc*/D3D11_COMPARISON_EQUAL,

		/*BOOL StencilEnable*/true,
		/*UINT8 StencilReadMask*/D3D11_DEFAULT_STENCIL_READ_MASK,
		/*UINT8 StencilWriteMask*/D3D11_DEFAULT_STENCIL_READ_MASK,

		/*D3D11_COMPARISON_FUNC FrontFaceStencilFunc*/D3D11_COMPARISON_EQUAL,
		/*D3D11_STENCIL_OP FrontFaceStencilPassOp*/D3D11_STENCIL_OP_KEEP,
		/*D3D11_STENCIL_OP FrontFaceStencilFailOp*/D3D11_STENCIL_OP_KEEP,
		/*D3D11_STENCIL_OP FrontFaceStencilDepthFailOp*/D3D11_STENCIL_OP_KEEP,

		/*D3D11_COMPARISON_FUNC BackFaceStencilFunc*/D3D11_COMPARISON_ALWAYS,
		/*D3D11_STENCIL_OP BackFaceStencilPassOp*/D3D11_STENCIL_OP_KEEP,
		/*D3D11_STENCIL_OP BackFaceStencilFailOp*/D3D11_STENCIL_OP_KEEP,
		/*D3D11_STENCIL_OP BackFaceStencilDepthFailOp*/D3D11_STENCIL_OP_KEEP,

		pass_with_stencil_state.ReleaseAndGetAddressOf());

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / (FLOAT)pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI/4, fAspectRatio, 2.0f, 1000.0f );

    // Set GUI size and locations
    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 245, pBackBufferSurfaceDesc->Height - 660 );
    g_SampleUI.SetSize( 245, 660 );

	DirectX::XMStoreFloat4x4(&main_scene_state.mProjection, DirectX::XMMatrixTranspose(DirectX::XMMatrixPerspectiveFovLH(D3DX_PI / 3, fAspectRatio, 0.1f, 1000.0f)));

	Microsoft::WRL::ComPtr<ID3D11Texture2D> t1, t2;
	//DXGI_FORMAT_R32G32B32A32_FLOAT DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
	CD3D11_TEXTURE2D_DESC fragmentDesc(DXGI_FORMAT_R32G32B32A32_FLOAT, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, 4, 0, 0);

	pd3dDevice->CreateTexture2D(&fragmentDesc, nullptr, t1.ReleaseAndGetAddressOf());
	
	pd3dDevice->CreateShaderResourceView(t1.Get(), nullptr, srv1.ReleaseAndGetAddressOf());

	pd3dDevice->CreateRenderTargetView(t1.Get(), nullptr, rtv1.ReleaseAndGetAddressOf());


	fragmentDesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, 4, 0, 0);

	pd3dDevice->CreateTexture2D(&fragmentDesc, nullptr, t1.ReleaseAndGetAddressOf());

	std::string n("srv2");

	t1->SetPrivateData(WKPDID_D3DDebugObjectName, n.size(), n.c_str());

	pd3dDevice->CreateShaderResourceView(t1.Get(), nullptr, srv2.ReleaseAndGetAddressOf());

	pd3dDevice->CreateRenderTargetView(t1.Get(), nullptr, rtv2.ReleaseAndGetAddressOf());


	CD3D11_TEXTURE2D_DESC depthDesc(DXGI_FORMAT_D24_UNORM_S8_UINT, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, 1, 1, D3D11_BIND_DEPTH_STENCIL, D3D11_USAGE_DEFAULT, 0, 4, 0, 0);

	pd3dDevice->CreateTexture2D(&depthDesc, nullptr, t2.ReleaseAndGetAddressOf());

	pd3dDevice->CreateDepthStencilView(t2.Get(), nullptr, ds_v1.ReleaseAndGetAddressOf());

	pd3dDevice->CreateTexture2D(&depthDesc, nullptr, t2.ReleaseAndGetAddressOf());

	pd3dDevice->CreateDepthStencilView(t2.Get(), nullptr, ds_v2.ReleaseAndGetAddressOf());

	main_scene_state.vScreenResolution = SimpleMath::Vector4(pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, 0, 0);

	main_scene_state_cb->SetData(DXUTGetD3D11DeviceContext(), main_scene_state);

    return S_OK;
}
template<class T> inline ID3D11Buffer** constantBuffersToArray(DirectX::ConstantBuffer<T> &cb){
	static ID3D11Buffer* cbs[10];
	cbs[0] = cb.GetBuffer();
	return cbs;
};
//--------------------------------------------------------------------------------------
void floor_set_world_matrix(ID3D11DeviceContext* pd3dImmediateContext);
void torus_set_world_matrix(ID3D11DeviceContext* pd3dImmediateContext, XMFLOAT3& pos);

void floor_draw(ID3D11DeviceContext* pd3dImmediateContext,	IEffect* effect, _In_opt_ std::function<void __cdecl()> setCustomState = nullptr);
void torus_draw(ID3D11DeviceContext* pd3dImmediateContext, IEffect* effect, _In_opt_ std::function<void __cdecl()> setCustomState = nullptr);
void torus_shadow_draw(ID3D11DeviceContext* pd3dImmediateContext, IEffect* effect, _In_opt_ std::function<void __cdecl()> setCustomState = nullptr);
void light_set_position(ID3D11DeviceContext* pd3dImmediateContext, XMFLOAT3& pos);

void mirrow_draw(ID3D11DeviceContext* pd3dImmediateContext, IEffect* effect, _In_opt_ std::function<void __cdecl()> setCustomState = nullptr);
void mirrow_set_world_matrix(ID3D11DeviceContext* pd3dImmediateContext, XMFLOAT3& scale, DirectX::XMMATRIX& orintation, XMFLOAT3& pos);
void mirrow_set_matrix(ID3D11DeviceContext* pd3dImmediateContext, DirectX::XMFLOAT4X4& mirrowView);
DirectX::XMFLOAT4X4 mirrow_view(DirectX::XMFLOAT4& plane);
inline ID3D11RenderTargetView** renderTargetViewToArray(ID3D11RenderTargetView* rtv1, ID3D11RenderTargetView* rtv2 = 0, ID3D11RenderTargetView* rtv3 = 0){
	static ID3D11RenderTargetView* rtvs[10];
	rtvs[0] = rtv1;
	rtvs[1] = rtv2;
	rtvs[2] = rtv3;
	return rtvs;
};
inline ID3D11ShaderResourceView** shaderResourceViewToArray(ID3D11ShaderResourceView* rtv1, ID3D11ShaderResourceView* rtv2 = 0, ID3D11ShaderResourceView* rtv3 = 0){
	static ID3D11ShaderResourceView* srvs[10];
	srvs[0] = rtv1;
	srvs[1] = rtv2;
	srvs[2] = rtv3;
	return srvs;
};
inline ID3D11SamplerState** samplerStateToArray(ID3D11SamplerState* ss1, ID3D11SamplerState* ss2 = 0){
	static ID3D11SamplerState* sss[10];
	sss[0] = ss1;
	sss[1] = ss2;
	return sss;
};
void OnDrawScene(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext,
	 ID3D11RenderTargetView* pRTV, ID3D11DepthStencilView* pDSV);
//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext,
	double fTime, float fElapsedTime, void* pUserContext)
{
	//Рисуем всю сцену
	OnDrawScene(pd3dDevice, pd3dImmediateContext, DXUTGetD3D11RenderTargetView(), DXUTGetD3D11DepthStencilView());

	SimpleMath::Vector3 pos[4];
	//поставим четыре зеркала по углам периметра
	pos[0] = SimpleMath::Vector3(-1, -1, 0);
	pos[1] = SimpleMath::Vector3(-1, 1, 0);
	pos[2] = SimpleMath::Vector3(1, 1, 0);
	pos[3] = SimpleMath::Vector3(1, -1, 0);

	auto mView = main_scene_state.mView;

	for (int i = 0; i < 4; i++){
		pos[i].Normalize();

		auto mirrowView = mirrow_view(XMFLOAT4(pos[i].x, pos[i].y, pos[i].z, 7.071));

		OnDrawScene(pd3dDevice, pd3dImmediateContext, rtv2.Get(), ds_v2.Get());

		pd3dImmediateContext->OMSetRenderTargets(1, renderTargetViewToArray(DXUTGetD3D11RenderTargetView()), DXUTGetD3D11DepthStencilView());

		main_scene_state.mView = mView;

		auto orintation = DirectX::XMMATRIX(SimpleMath::Vector3(0, 0, 1).Cross(pos[i]), SimpleMath::Vector3(0, 0, 1), pos[i], XMLoadFloat4(&XMFLOAT4(0, 0, 0, 1)));

		mirrow_set_world_matrix(pd3dImmediateContext, SimpleMath::Vector3(1.3, 1.7, 1), orintation, -(7.071 * pos[i]) + SimpleMath::Vector3(0,0,1.7));

		mirrow_set_matrix(pd3dImmediateContext, mirrowView);

		mirrow_draw(pd3dImmediateContext, mirrowEffect.get(), [=]{
			//draw without cull
			pd3dImmediateContext->PSSetShaderResources(0, 1, shaderResourceViewToArray(srv2.Get()));
			pd3dImmediateContext->PSSetSamplers(0, 1, samplerStateToArray(states->LinearWrap()));

			pd3dImmediateContext->OMSetBlendState(states->Opaque(), Colors::Black, 0xFFFFFFFF);
			pd3dImmediateContext->RSSetState(states->CullNone());
			pd3dImmediateContext->OMSetDepthStencilState(states->DepthDefault(), 0);
		});

		ID3D11ShaderResourceView* null[] = { nullptr, nullptr, nullptr };
		pd3dImmediateContext->PSSetShaderResources(0, 1, null);
	}

	RenderText();
}
void OnDrawScene( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, ID3D11RenderTargetView* pRTV, ID3D11DepthStencilView* pDSV)
{
	float ClearColor[4] = { 0.05f, 0.05f, 0.05f, 1.0f };
	pd3dImmediateContext->ClearRenderTargetView(pRTV, ClearColor);
	pd3dImmediateContext->ClearDepthStencilView(pDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	pd3dImmediateContext->OMSetRenderTargets(1, renderTargetViewToArray(pRTV), pDSV);

	if (true){
		floor_set_world_matrix( pd3dImmediateContext);

		floor_draw(pd3dImmediateContext, quadOnlyDepthEffect.get(), [=]{
          //рисуем только в Z буфер
		  pd3dImmediateContext->OMSetBlendState(states->Additive(), Colors::Black, 0xFFFFFFFF);
		  pd3dImmediateContext->RSSetState(states->CullCounterClockwise());
 		  pd3dImmediateContext->OMSetDepthStencilState(states->DepthDefault(), 0);
		});
	}

	if (true){
		/////shadow volume
		torus_set_world_matrix(pd3dImmediateContext, XMFLOAT3(-1, 0, 0.5 + 0.33 / 2 + 0.15));

		light_set_position(pd3dImmediateContext, XMFLOAT3(-1, 2, 4));

		torus_shadow_draw(pd3dImmediateContext, shadowEffect.get(), [=]{
			//рисуем только в стенсил
			pd3dImmediateContext->OMSetBlendState(states->Additive(), Colors::Black, 0xFFFFFFFF);
			pd3dImmediateContext->RSSetState(states->CullNone());
			pd3dImmediateContext->OMSetDepthStencilState(shadow_stencil_state.Get(), 0);
		});

		/////shadow map
		pd3dImmediateContext->ClearRenderTargetView(rtv1.Get(), ClearColor);
		pd3dImmediateContext->ClearDepthStencilView(ds_v1.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		pd3dImmediateContext->OMSetRenderTargets(1, renderTargetViewToArray(rtv1.Get()), ds_v1.Get());

		torus_set_world_matrix(pd3dImmediateContext, XMFLOAT3(1, 0, 0.5 + 0.33 / 2 + 0.15));

		light_set_position(pd3dImmediateContext, XMFLOAT3(1, 2, 4));

		torus_draw(pd3dImmediateContext, shadowCasterDepthEffect.get(), [=]{
			//std draw
			pd3dImmediateContext->OMSetBlendState(states->Opaque(), Colors::Black, 0xFFFFFFFF);
			pd3dImmediateContext->RSSetState(states->CullCounterClockwise());
			pd3dImmediateContext->OMSetDepthStencilState(states->DepthDefault(), 0);
		});

		floor_set_world_matrix(pd3dImmediateContext);

		light_set_position(pd3dImmediateContext, XMFLOAT3(1, 2, 4));

		floor_draw(pd3dImmediateContext, quadDepthEffect.get(), [=]{
			//std draw
			pd3dImmediateContext->OMSetBlendState(states->Opaque(), Colors::Black, 0xFFFFFFFF);
			pd3dImmediateContext->RSSetState(states->CullCounterClockwise());
			pd3dImmediateContext->OMSetDepthStencilState(states->DepthDefault(), 0);
		});

		pd3dImmediateContext->OMSetRenderTargets(1, renderTargetViewToArray(pRTV), pDSV);
        /////

		/////рисуем мир с учетом теней
		torus_set_world_matrix(pd3dImmediateContext, XMFLOAT3(-1, 0, 0.5 + 0.33 / 2 + 0.15));

		light_set_position(pd3dImmediateContext, XMFLOAT3(-1, 2, 4));

		torus_draw(pd3dImmediateContext, shadowCasterEffect.get(), [=]{
			//std draw
			pd3dImmediateContext->OMSetBlendState(states->Opaque(), Colors::Black, 0xFFFFFFFF);
			pd3dImmediateContext->RSSetState(states->CullCounterClockwise());
			pd3dImmediateContext->OMSetDepthStencilState(states->DepthDefault(), 0);
		});

		torus_set_world_matrix(pd3dImmediateContext, XMFLOAT3(1, 0, 0.5 + 0.33 / 2 + 0.15));

		light_set_position(pd3dImmediateContext, XMFLOAT3(1, 2, 4));

		torus_draw(pd3dImmediateContext, shadowCasterEffect.get(), [=]{
			//std draw
			pd3dImmediateContext->OMSetBlendState(states->Opaque(), Colors::Black, 0xFFFFFFFF);
			pd3dImmediateContext->RSSetState(states->CullCounterClockwise());
			pd3dImmediateContext->OMSetDepthStencilState(states->DepthDefault(), 0);
		});

		floor_set_world_matrix(pd3dImmediateContext);

		light_set_position(pd3dImmediateContext, XMFLOAT3(1, 2, 4));

		floor_draw(pd3dImmediateContext, quadEffect.get(), [=]{
			//сверяемся с shadow map
			pd3dImmediateContext->PSSetShaderResources(0, 1, shaderResourceViewToArray(srv1.Get()));
			pd3dImmediateContext->PSSetSamplers(0, 1, samplerStateToArray(states->LinearWrap()));

			//рисуем с тестом трафарета при z equal
			pd3dImmediateContext->OMSetBlendState(states->Opaque(), Colors::Black, 0xFFFFFFFF);
			pd3dImmediateContext->RSSetState(states->CullCounterClockwise());
			pd3dImmediateContext->OMSetDepthStencilState(pass_with_stencil_state.Get(), 0);
		});
	}
	///
	{
		// You can't have the same texture bound as both an input and an output at the same time. The solution is to explicitly clear one binding before applying the new binding.
		ID3D11ShaderResourceView* null[] = { nullptr, nullptr, nullptr };
		pd3dImmediateContext->PSSetShaderResources(0, 1, null);
	}
	///
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();

	srv1.ReleaseAndGetAddressOf();

	rtv1.ReleaseAndGetAddressOf();

	srv2.ReleaseAndGetAddressOf();

	rtv2.ReleaseAndGetAddressOf();

	ds_v1.ReleaseAndGetAddressOf();

	ds_v2.ReleaseAndGetAddressOf();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
	m_keyboard = 0;
	m_mouse = 0;

	quadEffect = 0;
	shadowCasterEffect = 0;
	shadowCasterDepthEffect = 0;
	quadDepthEffect = 0;
	shadowEffect = 0;
	quadOnlyDepthEffect = 0;
	mirrowEffect = 0;

	torus = 0;

	states = 0;
	main_scene_state_cb = 0;

	inputLayout.ReleaseAndGetAddressOf();

	shadow_stencil_state.ReleaseAndGetAddressOf();
	pass_with_stencil_state.ReleaseAndGetAddressOf();

    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    g_LightControl.StaticOnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );

    SAFE_RELEASE( g_pLightTextureRV );
    for ( int i=0; i<(int)g_dwNumTextures; ++i )
    {
        SAFE_RELEASE( g_pGridDensityVBSRV[i] );
        SAFE_RELEASE( g_pGridDensityVB[i] );
        SAFE_RELEASE( g_pDetailTessellationDensityTextureRV[i] );
        SAFE_RELEASE( g_pDetailTessellationHeightTextureRV[i] );
        SAFE_RELEASE( g_pDetailTessellationBaseTextureRV[i] );
    }
    SAFE_RELEASE( g_pParticleVB );
    SAFE_RELEASE( g_pGridTangentSpaceIB )
    SAFE_RELEASE( g_pGridTangentSpaceVB );
    
    SAFE_RELEASE( g_pPositionOnlyVertexLayout );
    SAFE_RELEASE( g_pTangentSpaceVertexLayout );

    SAFE_RELEASE( g_pParticlePS );
    SAFE_RELEASE( g_pParticleGS );
    SAFE_RELEASE( g_pParticleVS );
    SAFE_RELEASE( g_pBumpMapPS );
    SAFE_RELEASE( g_pDetailTessellationDS );
    SAFE_RELEASE( g_pDetailTessellationHS );
    SAFE_RELEASE( g_pDetailTessellationVS );
    SAFE_RELEASE( g_pNoTessellationVS );

    SAFE_RELEASE( g_pPOMPS );
    SAFE_RELEASE( g_pPOMVS );

    SAFE_RELEASE( g_pMaterialCB );
    SAFE_RELEASE( g_pMainCB );

    SAFE_RELEASE( g_pDepthStencilState );
    SAFE_RELEASE( g_pBlendStateAdditiveBlending );
    SAFE_RELEASE( g_pBlendStateNoBlend );
    SAFE_RELEASE( g_pRasterizerStateSolid );
    SAFE_RELEASE( g_pRasterizerStateWireframe );
    SAFE_RELEASE( g_pSamplerStateLinear );
}


//--------------------------------------------------------------------------------------
// Helper function for command line retrieval
//--------------------------------------------------------------------------------------
bool IsNextArg( WCHAR*& strCmdLine, WCHAR* strArg )
{
   int nArgLen = (int)wcslen( strArg );
   int nCmdLen = (int)wcslen( strCmdLine );

   if( ( nCmdLen >= nArgLen ) && 
       ( _wcsnicmp( strCmdLine, strArg, nArgLen ) == 0 ) && 
       ( (strCmdLine[nArgLen] == 0 || strCmdLine[nArgLen] == L':' || strCmdLine[nArgLen] == L'=' ) ) )
   {
      strCmdLine += nArgLen;
      return true;
   }

   return false;
}


//--------------------------------------------------------------------------------------
// Helper function for command line retrieval.  Updates strCmdLine and strFlag 
//      Example: if strCmdLine=="-width:1024 -forceref"
// then after: strCmdLine==" -forceref" and strFlag=="1024"
//--------------------------------------------------------------------------------------
bool GetCmdParam( WCHAR*& strCmdLine, WCHAR* strFlag )
{
   if( *strCmdLine == L':' || *strCmdLine == L'=' )
   {       
      strCmdLine++; // Skip ':'

      // Place NULL terminator in strFlag after current token
      wcscpy_s( strFlag, 256, strCmdLine );
      WCHAR* strSpace = strFlag;
      while ( *strSpace && (*strSpace > L' ') )
         strSpace++;
      *strSpace = 0;

      // Update strCmdLine
      strCmdLine += wcslen( strFlag );
      return true;
   }
   else
   {
      strFlag[0] = 0;
      return false;
   }
}






//--------------------------------------------------------------------------------------
// Create a density texture map from a height map
//--------------------------------------------------------------------------------------
void CreateDensityMapFromHeightMap( ID3D11Device* pd3dDevice, ID3D11DeviceContext *pDeviceContext, ID3D11Texture2D* pHeightMap, 
                                    float fDensityScale, ID3D11Texture2D** ppDensityMap, ID3D11Texture2D** ppDensityMapStaging, 
                                    float fMeaningfulDifference )
{
#define ReadPixelFromMappedSubResource(x, y)       ( *( (RGBQUAD *)((BYTE *)MappedSubResourceRead.pData + (y)*MappedSubResourceRead.RowPitch + (x)*sizeof(DWORD)) ) )
#define WritePixelToMappedSubResource(x, y, value) ( ( *( (DWORD *)((BYTE *)MappedSubResourceWrite.pData + (y)*MappedSubResourceWrite.RowPitch + (x)*sizeof(DWORD)) ) ) = (DWORD)(value) )

    // Get description of source texture
    D3D11_TEXTURE2D_DESC Desc;
    pHeightMap->GetDesc( &Desc );    

    // Create density map with the same properties
    pd3dDevice->CreateTexture2D( &Desc, NULL, ppDensityMap );
    DXUT_SetDebugName( *ppDensityMap, "Density Map" );

    // Create STAGING height map texture
    ID3D11Texture2D* pHeightMapStaging;
    Desc.CPUAccessFlags =   D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
    Desc.Usage =            D3D11_USAGE_STAGING;
    Desc.BindFlags =        0;
    pd3dDevice->CreateTexture2D( &Desc, NULL, &pHeightMapStaging );
    DXUT_SetDebugName( pHeightMapStaging, "Height Map" );

    // Create STAGING density map
    ID3D11Texture2D* pDensityMapStaging;
    pd3dDevice->CreateTexture2D( &Desc, NULL, &pDensityMapStaging );
    DXUT_SetDebugName( pDensityMapStaging, "Density Map" );

    // Copy contents of height map into staging version
    pDeviceContext->CopyResource( pHeightMapStaging, pHeightMap );

    // Compute density map and store into staging version
    D3D11_MAPPED_SUBRESOURCE MappedSubResourceRead, MappedSubResourceWrite;
    pDeviceContext->Map( pHeightMapStaging, 0, D3D11_MAP_READ, 0, &MappedSubResourceRead );

    pDeviceContext->Map( pDensityMapStaging, 0, D3D11_MAP_WRITE, 0, &MappedSubResourceWrite );

    // Loop map and compute derivatives
    for ( int j=0; j <= (int)Desc.Height-1; ++j )
    {
        for ( int i=0; i <= (int)Desc.Width-1; ++i )
        {
            // Edges are set to the minimum value
            if ( ( j == 0 ) || 
                 ( i == 0 ) || 
                 ( j == ( (int)Desc.Height-1 ) ) || 
                 ( i == ((int)Desc.Width-1 ) ) )
            {
                WritePixelToMappedSubResource( i, j, (DWORD)1 );
                continue;
            }

            // Get current pixel
            RGBQUAD CurrentPixel =     ReadPixelFromMappedSubResource( i, j );

            // Get left pixel
            RGBQUAD LeftPixel =        ReadPixelFromMappedSubResource( i-1, j );

            // Get right pixel
            RGBQUAD RightPixel =       ReadPixelFromMappedSubResource( i+1, j );

            // Get top pixel
            RGBQUAD TopPixel =         ReadPixelFromMappedSubResource( i, j-1 );

            // Get bottom pixel
            RGBQUAD BottomPixel =      ReadPixelFromMappedSubResource( i, j+1 );

            // Get top-right pixel
            RGBQUAD TopRightPixel =    ReadPixelFromMappedSubResource( i+1, j-1 );

            // Get bottom-right pixel
            RGBQUAD BottomRightPixel = ReadPixelFromMappedSubResource( i+1, j+1 );
            
            // Get top-left pixel
            RGBQUAD TopLeftPixel =     ReadPixelFromMappedSubResource( i-1, j-1 );
            
            // Get bottom-left pixel
            RGBQUAD BottomLeft =       ReadPixelFromMappedSubResource( i-1, j+1 );

            float fCurrentPixelHeight =            ( CurrentPixel.rgbReserved     / 255.0f );
            float fCurrentPixelLeftHeight =        ( LeftPixel.rgbReserved        / 255.0f );
            float fCurrentPixelRightHeight =       ( RightPixel.rgbReserved       / 255.0f );
            float fCurrentPixelTopHeight =         ( TopPixel.rgbReserved         / 255.0f );
            float fCurrentPixelBottomHeight =      ( BottomPixel.rgbReserved      / 255.0f );
            float fCurrentPixelTopRightHeight =    ( TopRightPixel.rgbReserved    / 255.0f );
            float fCurrentPixelBottomRightHeight = ( BottomRightPixel.rgbReserved / 255.0f );
            float fCurrentPixelTopLeftHeight =     ( TopLeftPixel.rgbReserved     / 255.0f );
            float fCurrentPixelBottomLeftHeight =  ( BottomLeft.rgbReserved       / 255.0f );

            float fDensity = 0.0f;
            float fHorizontalVariation = fabs( ( fCurrentPixelRightHeight - fCurrentPixelHeight ) - 
                                               ( fCurrentPixelHeight - fCurrentPixelLeftHeight ) );
            float fVerticalVariation = fabs( ( fCurrentPixelBottomHeight - fCurrentPixelHeight ) - 
                                             ( fCurrentPixelHeight - fCurrentPixelTopHeight ) );
            float fFirstDiagonalVariation = fabs( ( fCurrentPixelTopRightHeight - fCurrentPixelHeight ) - 
                                                  ( fCurrentPixelHeight - fCurrentPixelBottomLeftHeight ) );
            float fSecondDiagonalVariation = fabs( ( fCurrentPixelBottomRightHeight - fCurrentPixelHeight ) - 
                                                   ( fCurrentPixelHeight - fCurrentPixelTopLeftHeight ) );
            if ( fHorizontalVariation     >= fMeaningfulDifference) fDensity += 0.293f * fHorizontalVariation;
            if ( fVerticalVariation       >= fMeaningfulDifference) fDensity += 0.293f * fVerticalVariation;
            if ( fFirstDiagonalVariation  >= fMeaningfulDifference) fDensity += 0.207f * fFirstDiagonalVariation;
            if ( fSecondDiagonalVariation >= fMeaningfulDifference) fDensity += 0.207f * fSecondDiagonalVariation;
                
            // Scale density with supplied scale                
            fDensity *= fDensityScale;

            // Clamp density
            fDensity = max( min( fDensity, 1.0f ), 1.0f/255.0f );

            // Write density into density map
            WritePixelToMappedSubResource( i, j, (DWORD)( fDensity * 255.0f ) );
        }
    }

    pDeviceContext->Unmap( pDensityMapStaging, 0 );

    pDeviceContext->Unmap( pHeightMapStaging, 0 );
    SAFE_RELEASE( pHeightMapStaging );

    // Copy contents of density map into DEFAULT density version
    pDeviceContext->CopyResource( *ppDensityMap, pDensityMapStaging );
    
    // If a pointer to a staging density map was provided then return it with staging density map
    if ( ppDensityMapStaging )
    {
        *ppDensityMapStaging = pDensityMapStaging;
    }
    else
    {
        SAFE_RELEASE( pDensityMapStaging );
    }
}


//--------------------------------------------------------------------------------------
// Sample a 32-bit texture at the specified texture coordinate (point sampling only)
//--------------------------------------------------------------------------------------
__inline RGBQUAD SampleTexture2D_32bit( DWORD *pTexture2D, DWORD dwWidth, DWORD dwHeight, D3DXVECTOR2 fTexcoord, DWORD dwStride )
{
#define FROUND(x)    ( (int)( (x) + 0.5 ) )

    // Convert texture coordinates to texel coordinates using round-to-nearest
    int nU = FROUND( fTexcoord.x * ( dwWidth-1 ) );
    int nV = FROUND( fTexcoord.y * ( dwHeight-1 ) );

    // Clamp texcoord between 0 and [width-1, height-1]
    nU = nU % dwWidth;
    nV = nV % dwHeight;

    // Return value at this texel coordinate
    return *(RGBQUAD *)( ( (BYTE *)pTexture2D) + ( nV*dwStride ) + ( nU*sizeof(DWORD) ) );
}


//--------------------------------------------------------------------------------------
// Sample density map along specified edge and return maximum value
//--------------------------------------------------------------------------------------
float GetMaximumDensityAlongEdge( DWORD *pTexture2D, DWORD dwStride, DWORD dwWidth, DWORD dwHeight, 
                                  D3DXVECTOR2 fTexcoord0, D3DXVECTOR2 fTexcoord1 )
{
#define GETTEXEL(x, y)       ( *(RGBQUAD *)( ( (BYTE *)pTexture2D ) + ( (y)*dwStride ) + ( (x)*sizeof(DWORD) ) ) )
#define LERP(x, y, a)        ( (x)*(1.0f-(a)) + (y)*(a) )

    // Convert texture coordinates to texel coordinates using round-to-nearest
    int nU0 = FROUND( fTexcoord0.x * ( dwWidth  - 1 )  );
    int nV0 = FROUND( fTexcoord0.y * ( dwHeight - 1 ) );
    int nU1 = FROUND( fTexcoord1.x * ( dwWidth  - 1 )  );
    int nV1 = FROUND( fTexcoord1.y * ( dwHeight - 1 ) );

    // Wrap texture coordinates
    nU0 = nU0 % dwWidth;
    nV0 = nV0 % dwHeight;
    nU1 = nU1 % dwWidth;
    nV1 = nV1 % dwHeight;

    // Find how many texels on edge
    int nNumTexelsOnEdge = max( abs( nU1 - nU0 ), abs( nV1 - nV0 ) ) + 1;

    float fU, fV;
    float fMaxDensity = 0.0f;
    for ( int i=0; i<nNumTexelsOnEdge; ++i )
    {
        // Calculate current texel coordinates
        fU = LERP( (float)nU0, (float)nU1, ( (float)i ) / ( nNumTexelsOnEdge - 1 ) );
        fV = LERP( (float)nV0, (float)nV1, ( (float)i ) / ( nNumTexelsOnEdge - 1 ) );

        // Round texel texture coordinates to nearest
        int nCurrentU = FROUND( fU );
        int nCurrentV = FROUND( fV );

        // Update max density along edge
        fMaxDensity = max( fMaxDensity, GETTEXEL( nCurrentU, nCurrentV ).rgbBlue / 255.0f );
    }
        
    return fMaxDensity;
}


//--------------------------------------------------------------------------------------
// Calculate the maximum density along a triangle edge and
// store it in the resulting vertex stream.
//--------------------------------------------------------------------------------------
void CreateEdgeDensityVertexStream( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pDeviceContext, D3DXVECTOR2* pTexcoord, 
                                    DWORD dwVertexStride, void *pIndex, DWORD dwIndexSize, DWORD dwNumIndices, 
                                    ID3D11Texture2D* pDensityMap, ID3D11Buffer** ppEdgeDensityVertexStream,
                                    float fBaseTextureRepeat )
{
    HRESULT                hr;
    D3D11_SUBRESOURCE_DATA InitData;
    ID3D11Texture2D*       pDensityMapReadFrom;
    DWORD                  dwNumTriangles = dwNumIndices / 3;

    // Create host memory buffer
    D3DXVECTOR4 *pEdgeDensityBuffer = new D3DXVECTOR4[dwNumTriangles];

    // Retrieve density map description
    D3D11_TEXTURE2D_DESC Tex2DDesc;
    pDensityMap->GetDesc( &Tex2DDesc );
 
    // Check if provided texture can be Mapped() for reading directly
    BOOL bCanBeRead = Tex2DDesc.CPUAccessFlags & D3D11_CPU_ACCESS_READ;
    if ( !bCanBeRead )
    {
        // Texture cannot be read directly, need to create STAGING texture and copy contents into it
        Tex2DDesc.CPUAccessFlags =   D3D11_CPU_ACCESS_READ;
        Tex2DDesc.Usage =            D3D11_USAGE_STAGING;
        Tex2DDesc.BindFlags =        0;
        pd3dDevice->CreateTexture2D( &Tex2DDesc, NULL, &pDensityMapReadFrom );
        DXUT_SetDebugName( pDensityMapReadFrom, "DensityMap Read" );

        // Copy contents of height map into staging version
        pDeviceContext->CopyResource( pDensityMapReadFrom, pDensityMap );
    }
    else
    {
        pDensityMapReadFrom = pDensityMap;
    }

    // Map density map for reading
    D3D11_MAPPED_SUBRESOURCE MappedSubResource;
    pDeviceContext->Map( pDensityMapReadFrom, 0, D3D11_MAP_READ, 0, &MappedSubResource );

    // Loop through all triangles
    for ( DWORD i=0; i<dwNumTriangles; ++i )
    {
        DWORD wIndex0, wIndex1, wIndex2;

        // Retrieve indices of current triangle
        if ( dwIndexSize == sizeof(WORD) )
        {
            wIndex0 = (DWORD)( (WORD *)pIndex )[3*i+0];
            wIndex1 = (DWORD)( (WORD *)pIndex )[3*i+1];
            wIndex2 = (DWORD)( (WORD *)pIndex )[3*i+2];
        }
        else
        {
            wIndex0 = ( (DWORD *)pIndex )[3*i+0];
            wIndex1 = ( (DWORD *)pIndex )[3*i+1];
            wIndex2 = ( (DWORD *)pIndex )[3*i+2];
        }

        // Retrieve texture coordinates of each vertex making up current triangle
        D3DXVECTOR2    vTexcoord0 = *(D3DXVECTOR2 *)( (BYTE *)pTexcoord + wIndex0 * dwVertexStride );
        D3DXVECTOR2    vTexcoord1 = *(D3DXVECTOR2 *)( (BYTE *)pTexcoord + wIndex1 * dwVertexStride );
        D3DXVECTOR2    vTexcoord2 = *(D3DXVECTOR2 *)( (BYTE *)pTexcoord + wIndex2 * dwVertexStride );

        // Adjust texture coordinates with texture repeat
        vTexcoord0 *= fBaseTextureRepeat;
        vTexcoord1 *= fBaseTextureRepeat;
        vTexcoord2 *= fBaseTextureRepeat;

        // Sample density map at vertex location
        float fEdgeDensity0 = GetMaximumDensityAlongEdge( (DWORD *)MappedSubResource.pData, MappedSubResource.RowPitch, 
                                                          Tex2DDesc.Width, Tex2DDesc.Height, vTexcoord0, vTexcoord1 );
        float fEdgeDensity1 = GetMaximumDensityAlongEdge( (DWORD *)MappedSubResource.pData, MappedSubResource.RowPitch, 
                                                          Tex2DDesc.Width, Tex2DDesc.Height, vTexcoord1, vTexcoord2 );
        float fEdgeDensity2 = GetMaximumDensityAlongEdge( (DWORD *)MappedSubResource.pData, MappedSubResource.RowPitch, 
                                                          Tex2DDesc.Width, Tex2DDesc.Height, vTexcoord2, vTexcoord0 );

        // Store edge density in x,y,z and store maximum density in .w
        pEdgeDensityBuffer[i] = D3DXVECTOR4( fEdgeDensity0, fEdgeDensity1, fEdgeDensity2, 
                                             max( max( fEdgeDensity0, fEdgeDensity1 ), fEdgeDensity2 ) );
    }

    // Unmap density map
    pDeviceContext->Unmap( pDensityMapReadFrom, 0 );

    // Release staging density map if we had to create it
    if ( !bCanBeRead )
    {
        SAFE_RELEASE( pDensityMapReadFrom );
    }
    
    // Set source buffer for initialization data
    InitData.pSysMem = (void *)pEdgeDensityBuffer;

    // Create density vertex stream buffer: 1 float per entry
    D3D11_BUFFER_DESC bd;
    bd.Usage =            D3D11_USAGE_DEFAULT;
    bd.ByteWidth =        dwNumTriangles * sizeof( D3DXVECTOR4 );
    bd.BindFlags =        D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE;
    bd.CPUAccessFlags =   0;
    bd.MiscFlags =        0;
    hr = pd3dDevice->CreateBuffer( &bd, &InitData, ppEdgeDensityVertexStream );
    if( FAILED( hr ) )
    {
        OutputDebugString( L"Failed to create vertex buffer.\n" );
    }
    DXUT_SetDebugName( *ppEdgeDensityVertexStream, "Edge Density" );

    // Free host memory buffer
    delete [] pEdgeDensityBuffer;
}


//--------------------------------------------------------------------------------------
// Helper function to create a staging buffer from a buffer resource
//--------------------------------------------------------------------------------------
void CreateStagingBufferFromBuffer( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pContext, 
                                   ID3D11Buffer* pInputBuffer, ID3D11Buffer **ppStagingBuffer )
{
    D3D11_BUFFER_DESC BufferDesc;

    // Get buffer description
    pInputBuffer->GetDesc( &BufferDesc );

    // Modify description to create STAGING buffer
    BufferDesc.BindFlags =          0;
    BufferDesc.CPUAccessFlags =     D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
    BufferDesc.Usage =              D3D11_USAGE_STAGING;

    // Create staging buffer
    pd3dDevice->CreateBuffer( &BufferDesc, NULL, ppStagingBuffer );

    // Copy buffer into STAGING buffer
    pContext->CopyResource( *ppStagingBuffer, pInputBuffer );
}


//--------------------------------------------------------------------------------------
// Helper function to create a shader from the specified filename
// This function is called by the shader-specific versions of this
// function located after the body of this function.
//--------------------------------------------------------------------------------------
HRESULT CreateShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                              LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                              ID3DX11ThreadPump* pPump, ID3D11DeviceChild** ppShader, ID3DBlob** ppShaderBlob, 
                              BOOL bDumpShader)
{
    HRESULT   hr = D3D_OK;
    ID3DBlob* pShaderBlob = NULL;
    ID3DBlob* pErrorBlob = NULL;
    WCHAR     wcFullPath[256];
    
    DXUTFindDXSDKMediaFileCch( wcFullPath, 256, pSrcFile );
    // Compile shader into binary blob
    hr = D3DX11CompileFromFile( wcFullPath, pDefines, pInclude, pFunctionName, pProfile, 
                                Flags1, Flags2, pPump, &pShaderBlob, &pErrorBlob, NULL );
    if( FAILED( hr ) )
    {
        OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        SAFE_RELEASE( pErrorBlob );
        return hr;
    }
    
    // Create shader from binary blob
    if ( ppShader )
    {
        hr = E_FAIL;
        if ( strstr( pProfile, "vs" ) )
        {
            hr = pd3dDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(), 
                    pShaderBlob->GetBufferSize(), NULL, (ID3D11VertexShader**)ppShader );
        }
        else if ( strstr( pProfile, "hs" ) )
        {
            hr = pd3dDevice->CreateHullShader( pShaderBlob->GetBufferPointer(), 
                    pShaderBlob->GetBufferSize(), NULL, (ID3D11HullShader**)ppShader ); 
        }
        else if ( strstr( pProfile, "ds" ) )
        {
            hr = pd3dDevice->CreateDomainShader( pShaderBlob->GetBufferPointer(), 
                    pShaderBlob->GetBufferSize(), NULL, (ID3D11DomainShader**)ppShader );
        }
        else if ( strstr( pProfile, "gs" ) )
        {
            hr = pd3dDevice->CreateGeometryShader( pShaderBlob->GetBufferPointer(), 
                    pShaderBlob->GetBufferSize(), NULL, (ID3D11GeometryShader**)ppShader ); 
        }
        else if ( strstr( pProfile, "ps" ) )
        {
            hr = pd3dDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(), 
                    pShaderBlob->GetBufferSize(), NULL, (ID3D11PixelShader**)ppShader ); 
        }
        else if ( strstr( pProfile, "cs" ) )
        {
            hr = pd3dDevice->CreateComputeShader( pShaderBlob->GetBufferPointer(), 
                    pShaderBlob->GetBufferSize(), NULL, (ID3D11ComputeShader**)ppShader );
        }
        if ( FAILED( hr ) )
        {
            OutputDebugString( L"Shader creation failed\n" );
            SAFE_RELEASE( pErrorBlob );
            SAFE_RELEASE( pShaderBlob );
            return hr;
        }
    }

    DXUT_SetDebugName( *ppShader, pFunctionName );

    // If blob was requested then pass it otherwise release it
    if ( ppShaderBlob )
    {
        *ppShaderBlob = pShaderBlob;
    }
    else
    {
        pShaderBlob->Release();
    }

    // Return error code
    return hr;
}


//--------------------------------------------------------------------------------------
// Create a vertex shader from the specified filename
//--------------------------------------------------------------------------------------
HRESULT CreateVertexShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                    LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                    ID3DX11ThreadPump* pPump, ID3D11VertexShader** ppShader, ID3DBlob** ppShaderBlob, 
                                    BOOL bDumpShader )
{
    return CreateShaderFromFile( pd3dDevice, pSrcFile, pDefines, pInclude, pFunctionName, pProfile, 
                                 Flags1, Flags2, pPump, (ID3D11DeviceChild **)ppShader, ppShaderBlob, 
                                 bDumpShader );
}


//--------------------------------------------------------------------------------------
// Create a hull shader from the specified filename
//--------------------------------------------------------------------------------------
HRESULT CreateHullShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                  LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                  ID3DX11ThreadPump* pPump, ID3D11HullShader** ppShader, ID3DBlob** ppShaderBlob, 
                                  BOOL bDumpShader )
{
    return CreateShaderFromFile( pd3dDevice, pSrcFile, pDefines, pInclude, pFunctionName, pProfile, 
                                 Flags1, Flags2, pPump, (ID3D11DeviceChild **)ppShader, ppShaderBlob, 
                                 bDumpShader );
}
//--------------------------------------------------------------------------------------
// Create a domain shader from the specified filename
//--------------------------------------------------------------------------------------
HRESULT CreateDomainShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                    LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                    ID3DX11ThreadPump* pPump, ID3D11DomainShader** ppShader, ID3DBlob** ppShaderBlob, 
                                    BOOL bDumpShader )
{
    return CreateShaderFromFile( pd3dDevice, pSrcFile, pDefines, pInclude, pFunctionName, pProfile, 
                                 Flags1, Flags2, pPump, (ID3D11DeviceChild **)ppShader, ppShaderBlob, 
                                 bDumpShader );
}


//--------------------------------------------------------------------------------------
// Create a geometry shader from the specified filename
//--------------------------------------------------------------------------------------
HRESULT CreateGeometryShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                      LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                      ID3DX11ThreadPump* pPump, ID3D11GeometryShader** ppShader, ID3DBlob** ppShaderBlob, 
                                      BOOL bDumpShader )
{
    return CreateShaderFromFile( pd3dDevice, pSrcFile, pDefines, pInclude, pFunctionName, pProfile, 
                                 Flags1, Flags2, pPump, (ID3D11DeviceChild **)ppShader, ppShaderBlob, 
                                 bDumpShader );
}


//--------------------------------------------------------------------------------------
// Create a pixel shader from the specified filename
//--------------------------------------------------------------------------------------
HRESULT CreatePixelShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                   LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                   ID3DX11ThreadPump* pPump, ID3D11PixelShader** ppShader, ID3DBlob** ppShaderBlob, 
                                   BOOL bDumpShader )
{
    return CreateShaderFromFile( pd3dDevice, pSrcFile, pDefines, pInclude, pFunctionName, pProfile, 
                                 Flags1, Flags2, pPump, (ID3D11DeviceChild **)ppShader, ppShaderBlob, 
                                 bDumpShader );
}


//--------------------------------------------------------------------------------------
// Create a compute shader from the specified filename
//--------------------------------------------------------------------------------------
HRESULT CreateComputeShaderFromFile( ID3D11Device* pd3dDevice, LPCWSTR pSrcFile, CONST D3D_SHADER_MACRO* pDefines, 
                                     LPD3DINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, 
                                     ID3DX11ThreadPump* pPump, ID3D11ComputeShader** ppShader, ID3DBlob** ppShaderBlob, 
                                     BOOL bDumpShader)
{
    return CreateShaderFromFile( pd3dDevice, pSrcFile, pDefines, pInclude, pFunctionName, pProfile, 
                                 Flags1, Flags2, pPump, (ID3D11DeviceChild **)ppShader, ppShaderBlob, 
                                 bDumpShader );
}


//--------------------------------------------------------------------------------------
// Helper function to normalize a plane
//--------------------------------------------------------------------------------------
void NormalizePlane( D3DXVECTOR4* pPlaneEquation )
{
    float mag;
    
    mag = sqrt( pPlaneEquation->x * pPlaneEquation->x + 
                pPlaneEquation->y * pPlaneEquation->y + 
                pPlaneEquation->z * pPlaneEquation->z );
    
    pPlaneEquation->x = pPlaneEquation->x / mag;
    pPlaneEquation->y = pPlaneEquation->y / mag;
    pPlaneEquation->z = pPlaneEquation->z / mag;
    pPlaneEquation->w = pPlaneEquation->w / mag;
}


//--------------------------------------------------------------------------------------
// Extract all 6 plane equations from frustum denoted by supplied matrix
//--------------------------------------------------------------------------------------
void ExtractPlanesFromFrustum( D3DXVECTOR4* pPlaneEquation, const D3DXMATRIX* pMatrix, bool bNormalize )
{
    // Left clipping plane
    pPlaneEquation[0].x = pMatrix->_14 + pMatrix->_11;
    pPlaneEquation[0].y = pMatrix->_24 + pMatrix->_21;
    pPlaneEquation[0].z = pMatrix->_34 + pMatrix->_31;
    pPlaneEquation[0].w = pMatrix->_44 + pMatrix->_41;
    
    // Right clipping plane
    pPlaneEquation[1].x = pMatrix->_14 - pMatrix->_11;
    pPlaneEquation[1].y = pMatrix->_24 - pMatrix->_21;
    pPlaneEquation[1].z = pMatrix->_34 - pMatrix->_31;
    pPlaneEquation[1].w = pMatrix->_44 - pMatrix->_41;
    
    // Top clipping plane
    pPlaneEquation[2].x = pMatrix->_14 - pMatrix->_12;
    pPlaneEquation[2].y = pMatrix->_24 - pMatrix->_22;
    pPlaneEquation[2].z = pMatrix->_34 - pMatrix->_32;
    pPlaneEquation[2].w = pMatrix->_44 - pMatrix->_42;
    
    // Bottom clipping plane
    pPlaneEquation[3].x = pMatrix->_14 + pMatrix->_12;
    pPlaneEquation[3].y = pMatrix->_24 + pMatrix->_22;
    pPlaneEquation[3].z = pMatrix->_34 + pMatrix->_32;
    pPlaneEquation[3].w = pMatrix->_44 + pMatrix->_42;
    
    // Near clipping plane
    pPlaneEquation[4].x = pMatrix->_13;
    pPlaneEquation[4].y = pMatrix->_23;
    pPlaneEquation[4].z = pMatrix->_33;
    pPlaneEquation[4].w = pMatrix->_43;
    
    // Far clipping plane
    pPlaneEquation[5].x = pMatrix->_14 - pMatrix->_13;
    pPlaneEquation[5].y = pMatrix->_24 - pMatrix->_23;
    pPlaneEquation[5].z = pMatrix->_34 - pMatrix->_33;
    pPlaneEquation[5].w = pMatrix->_44 - pMatrix->_43;
    
    // Normalize the plane equations, if requested
    if ( bNormalize )
    {
        NormalizePlane( &pPlaneEquation[0] );
        NormalizePlane( &pPlaneEquation[1] );
        NormalizePlane( &pPlaneEquation[2] );
        NormalizePlane( &pPlaneEquation[3] );
        NormalizePlane( &pPlaneEquation[4] );
        NormalizePlane( &pPlaneEquation[5] );
    }
}

