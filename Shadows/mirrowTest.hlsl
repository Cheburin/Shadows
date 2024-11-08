struct ClipPosTex4d
{
    float4 tex            : TEXCOORD0;
    float4 clip_pos       : SV_POSITION; // Output position
};

cbuffer cbMain : register( b0 )
{
	matrix g_mWorld;                            // World matrix
	matrix g_mView;                             // View matrix
	matrix g_mProjection;                       // Projection matrix
    matrix g_mWorldViewProjection;              // WVP matrix
    matrix g_mWorldView;                        // WV matrix
    matrix g_mViewProjection;                   // VP matrix
    matrix g_mInvView;                          // Inverse of view matrix
    
	matrix g_mLightView;                // VP matrix
	matrix g_mWorldLightView;                       // Inverse of view matrix
	matrix g_mWorldLightViewProjection; 
	matrix g_mWorldMirrowViewProjection;

    float4 g_vScreenResolution;                 // Screen resolution
    
    float4 g_viewLightPos;                          //
    float4 g_vTessellationFactor;               // Edge, inside, minimum tessellation factor and 1/desired triangle size
    float4 g_vDetailTessellationHeightScale;    // Height scale for detail tessellation of grid surface
    float4 g_vGridSize;                         // Grid size
    
    float4 g_vDebugColorMultiply;               // Debug colors
    float4 g_vDebugColorAdd;                    // Debug colors
    
    float4 g_vFrustumPlaneEquation[4];          // View frustum plane equations
};

Texture2DMS<float4> map : register( t0 );

SamplerState linearSampler : register( s0 );

struct PosNormalTex2d
{
    float3 pos : SV_Position;
    float3 normal   : NORMAL;
    float2 tex      : TEXCOORD0;
};

ClipPosTex4d BILLBOARD_MIRROW(in PosNormalTex2d i)
{
    ClipPosTex4d output;

    output.clip_pos = mul( float4( i.pos.xyz, 1.0 ), g_mWorldViewProjection );

    float4 tex = mul( float4( i.pos.xyz, 1.0 ), g_mWorldViewProjection );
    
    output.tex = tex;

    return output;
} 

float4 MIRROW_COLOR(in float4 tex : TEXCOORD0):SV_TARGET
{ 
   float4 pos = tex / tex.w;

   float2 tx = float2(pos.x*0.5+0.5,pos.y*-0.5+0.5);

   return float4(0.2,0.2,0.2,0)+map.Load( int2(tx.x*g_vScreenResolution.x, tx.y*g_vScreenResolution.y), 0 );
}