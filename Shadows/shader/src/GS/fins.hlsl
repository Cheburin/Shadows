#define EPS 0.001
#define finSize 0.05

void emitEdge(inout ExpandPosNormalTex2d input[6], int i0, int i1, in float3 lightPos, inout TriangleStream<ClipPosColor> SpriteStream){
    ClipPosColor output;

    output.color = float4(0,0,1,0);

    output.clip_pos = mul( input[i0].pos, g_mProjection );
    SpriteStream.Append( output );
        
    //output.clip_pos = mul( input[i0].pos + finSize * float4( input[i0].normal, 0.0 ), g_mProjection );
    output.clip_pos = mul( input[i0].pos + float4(normalize(input[i0].pos.xyz - lightPos)*10, 0.0), g_mProjection );
    SpriteStream.Append( output );
        
    output.clip_pos = mul( input[i1].pos, g_mProjection );
    SpriteStream.Append( output );
    SpriteStream.RestartStrip();

    //output.clip_pos = mul( input[i0].pos + finSize * float4( input[i0].normal, 0.0 ), g_mProjection );
    output.clip_pos = mul( input[i0].pos + float4(normalize(input[i0].pos.xyz - lightPos)*10, 0.0), g_mProjection );
    SpriteStream.Append( output );
       
    //output.clip_pos = mul( input[i1].pos + finSize * float4( input[i1].normal, 0.0 ), g_mProjection );
    output.clip_pos = mul( input[i1].pos + float4(normalize(input[i1].pos.xyz - lightPos)*10, 0.0), g_mProjection );
    SpriteStream.Append( output );
        
    output.clip_pos = mul( input[i1].pos, g_mProjection );
    SpriteStream.Append( output );
    SpriteStream.RestartStrip();
}

float3 calcNormal (inout ExpandPosNormalTex2d input[6], int i0, int i1, int i2 )
{
    float3    va = input[i1].pos.xyz - input[i0].pos.xyz;
    float3    vb = input[i2].pos.xyz - input[i0].pos.xyz;
    
    return normalize ( cross ( va, vb ) );
}

[maxvertexcount(12)]
void FINS(triangleadj ExpandPosNormalTex2d input[6], uint primID : SV_PrimitiveID, inout TriangleStream<ClipPosColor> SpriteStream){
    float3 lightPos = g_viewLightPos.xyz; //mul(float4(0, 2, 4, 1), g_mView).xyz;

    float3 n0 = calcNormal (input, 0, 2, 4 );    // normal for triangle itself
    float3 v0 = normalize(lightPos -  input[0].pos.xyz);
   
    if ( dot ( n0, v0 ) > 0 )          // front-facing
    {
        float3 n1 = calcNormal (input,  1, 2, 0 );
        float3 v1 = normalize(lightPos -  input[1].pos.xyz);

        float3 n3 = calcNormal (input,  3, 4, 2 );
        float3 v3 = normalize(lightPos -  input[3].pos.xyz);

        float3 n5 = calcNormal (input,  5, 0, 4 );
        float3 v5 = normalize(lightPos -  input[5].pos.xyz);
                                            // check other triangles for back-facing
        if ( dot ( n1, v1 ) < 0 )
            emitEdge (input, 0, 2, lightPos, SpriteStream);
            
        if ( dot ( n3, v3 ) < 0 )
            emitEdge (input, 2, 4, lightPos, SpriteStream);
            
        if ( dot ( n5, v5 ) < 0 )
            emitEdge (input, 4, 0, lightPos, SpriteStream);
    }    
}