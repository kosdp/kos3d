/* shader.c - all GLSL program sources + compile/link helpers */
#include "shader.h"

#include <stdio.h>
#include <stdlib.h>

/* ---- world: procedural stone/brick with fbm detail + bump mapping ---- */
const char *WORLD_VS =
"#version 330 core\n"
"layout(location=0) in vec3 aPos;\n"
"layout(location=1) in vec3 aNormal;\n"
"layout(location=2) in vec2 aUV;\n"
"layout(location=3) in float aType;\n"
"uniform mat4 uVP;\n"
"out vec3 vPos; out vec3 vN; out vec2 vUV; flat out int vType;\n"
"void main(){ vPos=aPos; vN=aNormal; vUV=aUV; vType=int(aType); gl_Position=uVP*vec4(aPos,1.0);}\n";

const char *WORLD_FS =
"#version 330 core\n"
"in vec3 vPos; in vec3 vN; in vec2 vUV; flat in int vType;\n"
"out vec4 frag;\n"
"uniform int  uNumLights;\n"
"uniform vec3 uLightPos[24];\n"
"uniform vec3 uLightColor[24];\n"
"uniform float uLightRange[24];\n"
"uniform vec3 uViewPos;\n"
"float hash(vec2 p){ return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}\n"
"float vnoise(vec2 p){ vec2 i=floor(p),f=fract(p); f=f*f*(3.0-2.0*f);\n"
"  float a=hash(i),b=hash(i+vec2(1,0)),c=hash(i+vec2(0,1)),d=hash(i+vec2(1,1));\n"
"  return mix(mix(a,b,f.x),mix(c,d,f.x),f.y);}\n"
"float fbm(vec2 p){ float s=0.0,a=0.5; for(int i=0;i<4;i++){ s+=a*vnoise(p); p*=2.0; a*=0.5;} return s;}\n"
/* per-material height fields (for bump mapping) */
"float hWall(vec2 uv){ float row=floor(uv.y*4.0); float off=mod(row,2.0)*0.5;\n"
"  vec2 b=vec2(uv.x*2.0+off,uv.y*4.0); vec2 f=fract(b);\n"
"  float edge=min(min(f.x,1.0-f.x),min(f.y,1.0-f.y));\n"
"  float m=smoothstep(0.05,0.11,edge);\n"
"  return mix(0.0,0.8+fbm(b*4.0)*0.2,m);}\n"
"float hFloor(vec2 uv){ vec2 t=uv*1.5; vec2 f=fract(t);\n"
"  float edge=min(min(f.x,1.0-f.x),min(f.y,1.0-f.y));\n"
"  float m=smoothstep(0.03,0.08,edge);\n"
"  return mix(0.0,0.7+fbm(t*5.0)*0.15,m);}\n"
"float hCeil(vec2 uv){ return fbm(uv*3.0)*0.5; }\n"
"float heightAt(int t, vec2 uv){ return t==0?hFloor(uv): t==1?hCeil(uv): hWall(uv);}\n"
/* albedo + per-material specular params */
"vec3 albedoAt(int t, vec2 uv, out float shininess, out float sstr){\n"
"  if(t==0){ vec2 tt=uv*1.5; vec2 f=fract(tt);\n"
"    float edge=min(min(f.x,1.0-f.x),min(f.y,1.0-f.y)); float m=smoothstep(0.03,0.08,edge);\n"
"    float tn=hash(floor(tt)); vec3 stone=mix(vec3(0.15,0.17,0.21),vec3(0.31,0.33,0.38),tn);\n"
"    stone*=0.8+0.4*fbm(tt*4.0+floor(tt)*3.0);\n"
"    shininess=48.0; sstr=0.55*m; return mix(vec3(0.03,0.03,0.04),stone,m);}\n"
"  if(t==1){ float n=fbm(uv*2.0); vec3 c=mix(vec3(0.06,0.06,0.08),vec3(0.15,0.14,0.17),n);\n"
"    c*=0.7+0.4*fbm(uv*6.0); shininess=8.0; sstr=0.04; return c;}\n"
"  float row=floor(uv.y*4.0); float off=mod(row,2.0)*0.5;\n"
"  vec2 b=vec2(uv.x*2.0+off,uv.y*4.0); vec2 f=fract(b);\n"
"  float edge=min(min(f.x,1.0-f.x),min(f.y,1.0-f.y)); float m=smoothstep(0.05,0.11,edge);\n"
"  float bn=hash(floor(b)); vec3 brick=mix(vec3(0.27,0.14,0.10),vec3(0.53,0.33,0.25),bn);\n"
"  brick*=0.75+0.5*fbm(b*6.0);\n"
"  vec3 mortar=vec3(0.10,0.09,0.08)*(0.8+0.3*fbm(b*8.0));\n"
"  shininess=16.0; sstr=0.08; return mix(mortar,brick,m);}\n"
"void main(){\n"
"  float shininess,sstr; vec3 albedo=albedoAt(vType,vUV,shininess,sstr);\n"
"  vec3 N=normalize(vN);\n"
"  vec3 T,B; if(abs(N.y)>0.5){ T=vec3(1,0,0); B=vec3(0,0,1);} else { B=vec3(0,1,0); T=normalize(cross(B,N)); }\n"
"  float e=0.02, h0=heightAt(vType,vUV);\n"
"  float hx=heightAt(vType,vUV+vec2(e,0.0)), hy=heightAt(vType,vUV+vec2(0.0,e));\n"
"  N=normalize(N - (T*(hx-h0)+B*(hy-h0))/e*0.4);\n"
"  vec3 V=normalize(uViewPos-vPos);\n"
"  vec3 col=albedo*0.05;\n"
"  for(int i=0;i<uNumLights;i++){\n"
"    vec3 L=uLightPos[i]-vPos; float d=length(L); L/=max(d,0.001);\n"
"    float att=1.0/(1.0+(d*d)/(uLightRange[i]*uLightRange[i]));\n"
"    float diff=max(dot(N,L),0.0);\n"
"    vec3 H=normalize(L+V); float spec=pow(max(dot(N,H),0.0),shininess)*sstr;\n"
"    col += (albedo*diff + spec)*uLightColor[i]*att;\n"
"  }\n"
"  float fog=exp(-length(uViewPos-vPos)*0.05);\n"
"  col=mix(vec3(0.01,0.01,0.02),col,clamp(fog,0.0,1.0));\n"
"  col=col/(col+vec3(1.0)); col=pow(col,vec3(1.0/2.2));\n"
"  frag=vec4(col,1.0);}\n";

/* ---- lit object: model transform + same lights + base/emissive (monsters) ---- */
const char *LIT_VS =
"#version 330 core\n"
"layout(location=0) in vec3 aPos;\n"
"uniform mat4 uMVP; uniform mat4 uModel; uniform mat3 uNormalMat;\n"
"out vec3 vPos; out vec3 vN; out float vLy;\n"
"void main(){ vec4 w=uModel*vec4(aPos,1.0); vPos=w.xyz; vN=uNormalMat*aPos; vLy=aPos.y;\n"
"  gl_Position=uMVP*vec4(aPos,1.0);}\n";
const char *LIT_FS =
"#version 330 core\n"
"in vec3 vPos; in vec3 vN; in float vLy; out vec4 frag;\n"
"uniform int uNumLights; uniform vec3 uLightPos[24]; uniform vec3 uLightColor[24]; uniform float uLightRange[24];\n"
"uniform vec3 uViewPos; uniform vec3 uBase; uniform vec3 uEmissive; uniform float uFade;\n"
"void main(){ vec3 N=normalize(vN); vec3 V=normalize(uViewPos-vPos);\n"
"  vec3 col=uBase*0.06 + uEmissive;\n"
"  for(int i=0;i<uNumLights;i++){ vec3 L=uLightPos[i]-vPos; float d=length(L); L/=max(d,0.001);\n"
"    float att=1.0/(1.0+(d*d)/(uLightRange[i]*uLightRange[i])); float diff=max(dot(N,L),0.0);\n"
"    vec3 H=normalize(L+V); float spec=pow(max(dot(N,H),0.0),16.0)*0.4;\n"
"    col+=(uBase*diff+spec)*uLightColor[i]*att; }\n"
"  float fog=exp(-length(uViewPos-vPos)*0.05); col=mix(vec3(0.01,0.01,0.02),col,clamp(fog,0.0,1.0));\n"
"  col=col/(col+vec3(1.0)); col=pow(col,vec3(1.0/2.2));\n"
/* wraith bottom-fade: local y is -0.5 (feet) .. +0.5 (head); dissolve the base */
"  float a=1.0; if(uFade>0.5){ a=smoothstep(-0.5,-0.05,vLy)*0.85; if(a<0.03) discard; }\n"
"  frag=vec4(col,a);}\n";

/* ---- emissive props: relics, bolts, exit, staff tip. Fake-shaded so
        spheres read as glowing 3D orbs. ---- */
const char *EMIT_VS =
"#version 330 core\n"
"layout(location=0) in vec3 aPos;\n"
"uniform mat4 uMVP; uniform mat4 uModel;\n"
"out vec3 vLocal; out vec3 vWorld;\n"
"void main(){ vLocal=aPos; vWorld=(uModel*vec4(aPos,1.0)).xyz; gl_Position=uMVP*vec4(aPos,1.0);}\n";
const char *EMIT_FS =
"#version 330 core\n"
"in vec3 vLocal; in vec3 vWorld; out vec4 frag;\n"
"uniform vec3 uColor; uniform float uPulse; uniform float uAlpha; uniform vec3 uViewPos;\n"
"void main(){ vec3 n=normalize(vLocal); vec3 V=normalize(uViewPos-vWorld);\n"
"  float fres=pow(1.0-max(dot(n,V),0.0),2.5);\n"        /* glassy edge glow */
"  vec3 L=normalize(vec3(0.4,0.7,0.55)); float diff=0.4+0.6*max(dot(n,L),0.0);\n"
"  vec3 c=uColor*(0.7+0.6*uPulse)*diff + uColor*fres*1.3 + uColor*0.12;\n"
"  c=c/(c+vec3(1.0)); c=pow(c,vec3(1.0/2.2));\n"
"  float a=mix(uAlpha,1.0,fres);\n"                     /* transparent core, solid rim */
"  frag=vec4(c,a);}\n";

/* ---- particles: additive round point sprites ---- */
const char *POINT_VS =
"#version 330 core\n"
"layout(location=0) in vec3 aPos;\n"
"layout(location=1) in vec4 aColor;\n"
"layout(location=2) in float aSize;\n"
"uniform mat4 uVP; out vec4 vColor;\n"
"void main(){ vec4 p=uVP*vec4(aPos,1.0); gl_Position=p; vColor=aColor;\n"
"  gl_PointSize = clamp(aSize*50.0/max(p.w,0.001), 2.0, 42.0);}\n";
const char *POINT_FS =
"#version 330 core\n"
"in vec4 vColor; out vec4 frag;\n"
"void main(){ vec2 d=gl_PointCoord-0.5; float r2=dot(d,d); if(r2>0.25) discard;\n"
"  float a=vColor.a*(1.0-r2*4.0); frag=vec4(vColor.rgb,a);}\n";

/* ---- HUD: unit quad scaled/offset into NDC (bars, vignette) + crosshair ---- */
const char *HUD_VS =
"#version 330 core\n"
"layout(location=0) in vec2 aPos;\n"
"uniform vec2 uOffset; uniform vec2 uScale; uniform float uRot;\n"
"void main(){ vec2 c=aPos-0.5; float s=sin(uRot),co=cos(uRot);\n"
"  vec2 r=vec2(c.x*co-c.y*s, c.x*s+c.y*co)+0.5;\n"
"  gl_Position=vec4(r*uScale+uOffset,0.0,1.0);}\n";
const char *HUD_FS =
"#version 330 core\n"
"out vec4 frag; uniform vec3 uColor; uniform float uAlpha;\n"
"void main(){ frag=vec4(uColor,uAlpha);}\n";

GLuint compile(GLenum type, const char *src){
    GLuint s=glCreateShader(type);
    glShaderSource(s,1,&src,NULL); glCompileShader(s);
    GLint ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){ char log[1024]; glGetShaderInfoLog(s,1024,NULL,log);
        fprintf(stderr,"shader compile error:\n%s\n",log); exit(1); }
    return s;
}
GLuint make_program(const char *vs, const char *fs){
    GLuint v=compile(GL_VERTEX_SHADER,vs), f=compile(GL_FRAGMENT_SHADER,fs);
    GLuint p=glCreateProgram(); glAttachShader(p,v); glAttachShader(p,f); glLinkProgram(p);
    GLint ok; glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){ char log[1024]; glGetProgramInfoLog(p,1024,NULL,log);
        fprintf(stderr,"link error:\n%s\n",log); exit(1); }
    glDeleteShader(v); glDeleteShader(f); return p;
}

LightU light_locs(GLuint p){
    LightU u;
    u.np  =glGetUniformLocation(p,"uNumLights");
    u.lp  =glGetUniformLocation(p,"uLightPos");
    u.lc  =glGetUniformLocation(p,"uLightColor");
    u.lr  =glGetUniformLocation(p,"uLightRange");
    u.view=glGetUniformLocation(p,"uViewPos");
    return u;
}
void upload_lights(LightU u, int nl, v3 *lp, v3 *lc, float *lr, v3 eye){
    glUniform1i(u.np,nl);
    glUniform3fv(u.lp,nl,(const float*)lp);
    glUniform3fv(u.lc,nl,(const float*)lc);
    glUniform1fv(u.lr,nl,lr);
    glUniform3f(u.view,eye.x,eye.y,eye.z);
}
