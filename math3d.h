/* math3d.h - vector & matrix math (header-only, column-major / OpenGL layout) */
#ifndef MATH3D_H
#define MATH3D_H

#include <math.h>
#include <string.h>

#define PI 3.14159265358979f

typedef struct { float x, y, z; } v3;
typedef struct { float m[16]; } mat4;

static inline v3 v3v(float x, float y, float z){ v3 r={x,y,z}; return r; }
static inline v3 v3add(v3 a, v3 b){ return v3v(a.x+b.x,a.y+b.y,a.z+b.z); }
static inline v3 v3sub(v3 a, v3 b){ return v3v(a.x-b.x,a.y-b.y,a.z-b.z); }
static inline v3 v3scale(v3 a, float s){ return v3v(a.x*s,a.y*s,a.z*s); }
static inline float v3dot(v3 a, v3 b){ return a.x*b.x+a.y*b.y+a.z*b.z; }
static inline v3 v3cross(v3 a, v3 b){ return v3v(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x); }
static inline float v3len(v3 a){ return sqrtf(v3dot(a,a)); }
static inline v3 v3norm(v3 a){ float l=v3len(a); return l>1e-6f? v3scale(a,1.0f/l):a; }

static inline mat4 mat4_identity(void){
    mat4 r; memset(r.m,0,sizeof(r.m));
    r.m[0]=r.m[5]=r.m[10]=r.m[15]=1.0f; return r;
}
/* result = a * b */
static inline mat4 mat4_mul(mat4 a, mat4 b){
    mat4 r;
    for(int c=0;c<4;c++)
        for(int row=0;row<4;row++){
            float s=0;
            for(int k=0;k<4;k++) s += a.m[k*4+row]*b.m[c*4+k];
            r.m[c*4+row]=s;
        }
    return r;
}
static inline mat4 mat4_perspective(float fovy, float aspect, float znear, float zfar){
    float f = 1.0f/tanf(fovy*0.5f);
    mat4 r; memset(r.m,0,sizeof(r.m));
    r.m[0]=f/aspect; r.m[5]=f;
    r.m[10]=(zfar+znear)/(znear-zfar);
    r.m[11]=-1.0f;
    r.m[14]=(2.0f*zfar*znear)/(znear-zfar);
    return r;
}
static inline mat4 mat4_lookat(v3 eye, v3 center, v3 up){
    v3 f = v3norm(v3sub(center,eye));
    v3 s = v3norm(v3cross(f,up));
    v3 u = v3cross(s,f);
    mat4 r = mat4_identity();
    r.m[0]=s.x; r.m[4]=s.y; r.m[8]=s.z;
    r.m[1]=u.x; r.m[5]=u.y; r.m[9]=u.z;
    r.m[2]=-f.x;r.m[6]=-f.y;r.m[10]=-f.z;
    r.m[12]=-v3dot(s,eye);
    r.m[13]=-v3dot(u,eye);
    r.m[14]= v3dot(f,eye);
    return r;
}
static inline mat4 mat4_translate(v3 t){
    mat4 r=mat4_identity();
    r.m[12]=t.x; r.m[13]=t.y; r.m[14]=t.z; return r;
}
static inline mat4 mat4_scale(v3 s){
    mat4 r=mat4_identity();
    r.m[0]=s.x; r.m[5]=s.y; r.m[10]=s.z; return r;
}
static inline mat4 mat4_rotY(float a){
    mat4 r=mat4_identity();
    float c=cosf(a), s=sinf(a);
    r.m[0]=c; r.m[2]=-s; r.m[8]=s; r.m[10]=c; return r;
}
/* rotation from three orthonormal basis vectors as columns */
static inline mat4 mat4_basis(v3 x, v3 y, v3 z){
    mat4 r=mat4_identity();
    r.m[0]=x.x; r.m[1]=x.y; r.m[2]=x.z;
    r.m[4]=y.x; r.m[5]=y.y; r.m[6]=y.z;
    r.m[8]=z.x; r.m[9]=z.y; r.m[10]=z.z;
    return r;
}

#endif /* MATH3D_H */
