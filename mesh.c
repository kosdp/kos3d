/* mesh.c - turns the tile map into renderable triangles */
#include "mesh.h"
#include "dungeon.h"
#include "math3d.h"

#include <stdlib.h>
#include <math.h>

const float CUBE[108] = {
 -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f,
 -0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
 -0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f,  0.5f,-0.5f, 0.5f,
 -0.5f,-0.5f, 0.5f, -0.5f, 0.5f, 0.5f,  0.5f, 0.5f, 0.5f,
 -0.5f, 0.5f,-0.5f,  0.5f, 0.5f,-0.5f,  0.5f, 0.5f, 0.5f,
 -0.5f, 0.5f,-0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
 -0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f,  0.5f,-0.5f,-0.5f,
 -0.5f,-0.5f,-0.5f, -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,
 -0.5f,-0.5f,-0.5f, -0.5f, 0.5f,-0.5f, -0.5f, 0.5f, 0.5f,
 -0.5f,-0.5f,-0.5f, -0.5f, 0.5f, 0.5f, -0.5f,-0.5f, 0.5f,
  0.5f,-0.5f,-0.5f,  0.5f, 0.5f, 0.5f,  0.5f, 0.5f,-0.5f,
  0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f,
};

/* growable float buffer */
typedef struct { float *data; size_t count, cap; } FBuf;
static void fb_push(FBuf *b, float v){
    if(b->count==b->cap){ b->cap = b->cap? b->cap*2 : 4096; b->data=realloc(b->data,b->cap*sizeof(float)); }
    b->data[b->count++]=v;
}
static void push_vert(FBuf *b, v3 p, v3 n, float u, float v, float type){
    fb_push(b,p.x); fb_push(b,p.y); fb_push(b,p.z);
    fb_push(b,n.x); fb_push(b,n.y); fb_push(b,n.z);
    fb_push(b,u);   fb_push(b,v);   fb_push(b,type);
}
static void push_quad(FBuf *b, v3 a, v3 c, v3 d, v3 e, v3 n,
                      float ua,float va,float uc,float vc,float ud,float vd,float ue,float ve,
                      float type){
    push_vert(b,a,n,ua,va,type); push_vert(b,c,n,uc,vc,type); push_vert(b,d,n,ud,vd,type);
    push_vert(b,a,n,ua,va,type); push_vert(b,d,n,ud,vd,type); push_vert(b,e,n,ue,ve,type);
}

int build_world_geometry(float **out){
    FBuf b={0};
    for(int cy=0;cy<MAP_H;cy++) for(int cx=0;cx<MAP_W;cx++){
        if(!g_tiles[cy][cx]) continue;
        float x0=cx*CELL, x1=(cx+1)*CELL, z0=cy*CELL, z1=(cy+1)*CELL;
        /* floor (type 0) / ceiling (type 1) */
        push_quad(&b, v3v(x0,0,z0),v3v(x0,0,z1),v3v(x1,0,z1),v3v(x1,0,z0), v3v(0,1,0),
            (float)cx,(float)cy,(float)cx,(float)cy+1,(float)cx+1,(float)cy+1,(float)cx+1,(float)cy, 0.0f);
        push_quad(&b, v3v(x0,WALL_H,z0),v3v(x1,WALL_H,z0),v3v(x1,WALL_H,z1),v3v(x0,WALL_H,z1), v3v(0,-1,0),
            (float)cx,(float)cy,(float)cx+1,(float)cy,(float)cx+1,(float)cy+1,(float)cx,(float)cy+1, 1.0f);
        /* walls (type 2) facing solid neighbours */
        if(!cell_open(cx,cy-1)) push_quad(&b, v3v(x0,0,z0),v3v(x1,0,z0),v3v(x1,WALL_H,z0),v3v(x0,WALL_H,z0), v3v(0,0,1), 0,0,1,0,1,1,0,1, 2.0f);
        if(!cell_open(cx,cy+1)) push_quad(&b, v3v(x1,0,z1),v3v(x0,0,z1),v3v(x0,WALL_H,z1),v3v(x1,WALL_H,z1), v3v(0,0,-1),0,0,1,0,1,1,0,1, 2.0f);
        if(!cell_open(cx-1,cy)) push_quad(&b, v3v(x0,0,z1),v3v(x0,0,z0),v3v(x0,WALL_H,z0),v3v(x0,WALL_H,z1), v3v(1,0,0), 0,0,1,0,1,1,0,1, 2.0f);
        if(!cell_open(cx+1,cy)) push_quad(&b, v3v(x1,0,z0),v3v(x1,0,z1),v3v(x1,WALL_H,z1),v3v(x1,WALL_H,z0), v3v(-1,0,0),0,0,1,0,1,1,0,1, 2.0f);
    }
    *out=b.data;
    return (int)(b.count/9);
}

static v3 sph(float theta, float phi){
    return v3v(0.5f*sinf(theta)*cosf(phi), 0.5f*cosf(theta), 0.5f*sinf(theta)*sinf(phi));
}
static void push_pos(FBuf *b, v3 p){ fb_push(b,p.x); fb_push(b,p.y); fb_push(b,p.z); }

int make_sphere(float **out){
    const int stacks=16, slices=24;
    FBuf b={0};
    for(int i=0;i<stacks;i++){
        float t0=PI*(float)i/stacks, t1=PI*(float)(i+1)/stacks;
        for(int j=0;j<slices;j++){
            float p0=2.0f*PI*(float)j/slices, p1=2.0f*PI*(float)(j+1)/slices;
            v3 a=sph(t0,p0), c=sph(t1,p0), d=sph(t1,p1), e=sph(t0,p1);
            push_pos(&b,a); push_pos(&b,c); push_pos(&b,d);
            push_pos(&b,a); push_pos(&b,d); push_pos(&b,e);
        }
    }
    *out=b.data;
    return (int)(b.count/3);
}

int make_cylinder(float **out){
    const int seg=24; const float r=0.5f, y0=-0.5f, y1=0.5f;
    FBuf b={0};
    for(int i=0;i<seg;i++){
        float a0=2.0f*PI*(float)i/seg, a1=2.0f*PI*(float)(i+1)/seg;
        v3 p00=v3v(r*cosf(a0),y0,r*sinf(a0)), p01=v3v(r*cosf(a0),y1,r*sinf(a0));
        v3 p10=v3v(r*cosf(a1),y0,r*sinf(a1)), p11=v3v(r*cosf(a1),y1,r*sinf(a1));
        push_pos(&b,p00); push_pos(&b,p10); push_pos(&b,p11);  /* side */
        push_pos(&b,p00); push_pos(&b,p11); push_pos(&b,p01);
        push_pos(&b,v3v(0,y0,0)); push_pos(&b,p10); push_pos(&b,p00); /* bottom cap */
        push_pos(&b,v3v(0,y1,0)); push_pos(&b,p01); push_pos(&b,p11); /* top cap */
    }
    *out=b.data;
    return (int)(b.count/3);
}
