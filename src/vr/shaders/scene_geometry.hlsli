// Shared raster/ray geometry. The including shader supplies Vertex and
// view_matrix(); neither renderer may invent a different destruction pose,
// visibility test or eye-facing billboard transform.
uint source_billboard_visible(inout Vertex input) {
    if((input.texture.w&134217728U)==0) return 1;
    float size=input.group_a.x,depth=input.group_a.y;
    bool valid=isfinite(size) && isfinite(depth) && size>0 && depth>=128;
    bool glyph=(input.texture.w&1024U)!=0;
    float dimension=valid?trunc(size*256./depth):0;
    if(glyph) {
        // Scaled source text has no whole-object sprite's 240px cap.
        precise float side=dimension*depth/256.;
        precise float left=input.group_b.x*side;
        input.billboard=float2(left+input.billboard.x*side,input.billboard.y*side);
    } else {
        dimension=clamp(dimension,0.,240.);
        input.billboard*=dimension*depth/512.;
    }
    if(!glyph && input.group_b.z==1) {
        // EX aiming stations retain the game-plane basis under head roll.
        input.position.xy+=input.billboard;input.billboard=0;input.texture.w&=~4U;
    }
    input.texture.w&=~134217728U;
    return valid && dimension>0?1:0;
}
uint source_visible(float3 point_a,float3 point_b,float3 point_c) {
    float3 a=mul(view_matrix(),float4(point_a,1)).xyz;
    float3 b=mul(view_matrix(),float4(point_b,1)).xyz;
    float3 c=mul(view_matrix(),float4(point_c,1)).xyz;
    precise float determinant=a.x*(b.y*c.z-b.z*c.y)-a.y*(b.x*c.z-b.z*c.x)+a.z*(b.x*c.y-b.y*c.x);
    float3 maximum=max(max(abs(a),abs(b)),abs(c));
    float scale=max(max(maximum.x,maximum.y),max(maximum.z,1.0));
    return determinant<=scale*scale*scale*1.0e-12;
}
int explosion_round(float value) {return int(sign(value)*floor(abs(value)+0.5));}
int explosion_word(int value) {return int(uint(value)<<16)>>16;}
int explosion_q15(float3 row,float3 value) {
    int3 coefficients=int3(round(row*32768.));
    int3 words=int3(explosion_word(explosion_round(value.x)),explosion_word(explosion_round(value.y)),explosion_word(explosion_round(value.z)));
    int3 products=(coefficients*words)>>15;
    return explosion_word(products.x+products.y+products.z);
}
float3 explosion_rotate(Vertex input,float3 value) {
    if(input.group_c.z!=0) return float3(explosion_q15(input.visibility_a,value),explosion_q15(input.visibility_b,value),explosion_q15(input.visibility_c,value));
    return float3(dot(input.visibility_a,value),dot(input.visibility_b,value),dot(input.visibility_c,value));
}
float4 scene_eye_position(inout Vertex input,out float3 position,out uint visible) {
    bool particle_visible=true;
    if((input.texture.w&0x80000000U)!=0) {
        int3 delta=int3(input.group_b)-int3(input.group_a);
        delta=(delta<<16)>>16;
        float3 current=input.group_a+float3(delta)*input.group_c.x;
        float depth=input.group_c.y+current.z;
        particle_visible=depth>=256 && (input.group_c.z==0 || input.group_c.y+input.group_a.z>=256);
        input.position=input.group_c.z==1?input.group_a:current;
        input.billboard*=depth/128.;
        input.texture.w&=~0x80000000U;
    }
    bool exploding=input.visibility_enabled==2;
    position=input.position;
    if(exploding) {
        float3 source=explosion_rotate(input,input.position);
        if(input.group_c.z!=0) source=float3(
            explosion_word(int(source.x)+explosion_round(input.group_a.x)),
            explosion_word(int(source.y)+explosion_round(input.group_a.y)),
            explosion_word(int(source.z)+explosion_round(input.group_a.z)));
        else source+=input.group_a;
        float3 direction=explosion_rotate(input,input.group_b);
        direction.y=-abs(direction.y);
        int3 rounded=int3(explosion_round(direction.x),explosion_round(direction.y),explosion_round(direction.z));
        float phase=input.group_c.x;
        int low=int(floor(phase)),high=int(ceil(phase));
        source+=lerp(float3((rounded*low)>>2),float3((rounded*high)>>2),phase-float(low));
        position=source*float3(1,-1,-1)/input.group_c.y;
    }
    float4 eye_position=mul(view_matrix(),float4(position,1));
    if((input.texture.w&4)!=0) {
        float sx=length(mul(view_matrix(),float4(1,0,0,0)).xyz);
        float sy=length(mul(view_matrix(),float4(0,1,0,0)).xyz);
        if(exploding) {sx/=input.group_c.y;sy/=input.group_c.y;}
        eye_position.xy+=input.billboard*float2(sx,sy);
    }
    visible=particle_visible?1:0;
    if(input.visibility_enabled==1)
        visible=source_visible(input.visibility_a,input.visibility_b,input.visibility_c);
    if(input.group_enabled!=0) visible&=source_visible(input.group_a,input.group_b,input.group_c);
    return eye_position;
}
