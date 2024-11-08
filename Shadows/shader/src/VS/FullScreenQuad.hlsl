ClipPosTex2d VS(uint VertexID : SV_VERTEXID)
{
    ClipPosTex2d output;

	float2 tmp = float2( (VertexID << 1) & 2, VertexID & 2 );

	float4 pos = float4( tmp * float2( 2.0f, -2.0f ) + float2( -1.0f, 1.0f), 0.0f, 1.0f );

    output.tex = float2(pos.x * 0.5 + 0.5f, pos.y * -0.5 + 0.5f);

    output.clip_pos = pos;

    return output;
} 

ClipPosTex4d BILLBOARD(uint VertexID : SV_VERTEXID)
{
    ClipPosTex4d output;

	float2 tmp = float2( (VertexID >> 1) & 1, VertexID & 1 );

	float2 pos = float2( tmp * float2( 2.0f, -2.0f ) + float2( -1.0f, 1.0f));

    output.clip_pos = mul( float4( pos.xy, 0.0, 1.0 ), g_mWorldViewProjection );

    float4 tex = mul( float4( pos.xy, 0.0, 1.0 ), g_mWorldLightViewProjection );
    
    output.tex = tex;

    return output;
} 


ClipPosTex4d BILLBOARD_DEPTH(uint VertexID : SV_VERTEXID)
{
    ClipPosTex4d output;

	float2 tmp = float2( (VertexID >> 1) & 1, VertexID & 1 );

	float2 pos = float2( tmp * float2( 2.0f, -2.0f ) + float2( -1.0f, 1.0f));

    output.clip_pos = mul( float4( pos.xy, 0.0, 1.0 ), g_mWorldLightViewProjection );

    output.tex = output.clip_pos;

    return output;
} 

ClipPosTex4d BILLBOARD_MIRROW(uint VertexID : SV_VERTEXID)
{
    ClipPosTex4d output;

	float2 tmp = float2( (VertexID >> 1) & 1, VertexID & 1 );

	float2 pos = float2( tmp * float2( 2.0f, -2.0f ) + float2( -1.0f, 1.0f));

    //output.clip_pos = mul( float4( pos.xy, 0.0, 1.0 ), g_mWorldViewProjection );
    matrix mvp = mul(transpose(g_mProjection), mul(transpose(g_mView), transpose(g_mWorld)));
    output.clip_pos = mul( mvp, float4( pos.xy, 0.0, 1.0 ) );

    float4 tex = mul( float4( pos.xy, 0.0, 1.0 ), g_mWorldMirrowViewProjection );
    
    output.tex = tex;

    return output;
} 