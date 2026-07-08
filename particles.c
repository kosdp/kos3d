/* particles.c - a small additive point-sprite particle pool */
#include "particles.h"
#include "shader.h"
#include "dungeon.h"   /* frand */

#include <GL/glew.h>
#include <string.h>

#define MAXP 4096

typedef struct { v3 pos, vel; float r,g,b, life, maxlife, size; int alive; } Part;
static Part   P[MAXP];
static GLuint s_vao, s_vbo, s_prog;
static GLint  s_vp;
static float  s_buf[MAXP*8]; /* pos(3) rgba(4) size(1) per particle */

static int find_free(void){
    for(int i=0;i<MAXP;i++) if(!P[i].alive) return i;
    return -1;
}

void particles_init(void){
    s_prog=make_program(POINT_VS,POINT_FS);
    s_vp=glGetUniformLocation(s_prog,"uVP");
    glGenVertexArrays(1,&s_vao); glGenBuffers(1,&s_vbo);
    glBindVertexArray(s_vao); glBindBuffer(GL_ARRAY_BUFFER,s_vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(s_buf),NULL,GL_STREAM_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)0);
    glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(3*sizeof(float)));
    glVertexAttribPointer(2,1,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(7*sizeof(float)));
    glEnableVertexAttribArray(0); glEnableVertexAttribArray(1); glEnableVertexAttribArray(2);
}

void particles_spawn(v3 pos, v3 vel, v3 color, float life, float size){
    int i=find_free(); if(i<0) return;
    P[i].pos=pos; P[i].vel=vel;
    P[i].r=color.x; P[i].g=color.y; P[i].b=color.z;
    P[i].life=P[i].maxlife=life; P[i].size=size; P[i].alive=1;
}

void particles_burst(v3 pos, v3 color, int n, float speed){
    for(int k=0;k<n;k++){
        v3 d=v3norm(v3v(frand()*2-1, frand()*2-1, frand()*2-1));
        v3 vel=v3scale(d, speed*(0.4f+0.6f*frand()));
        particles_spawn(pos, vel, color, 0.35f+0.5f*frand(), 4.0f+4.0f*frand());
    }
}

void particles_update(float dt){
    for(int i=0;i<MAXP;i++){
        if(!P[i].alive) continue;
        P[i].vel.y -= 1.2f*dt;                    /* gentle gravity   */
        P[i].vel=v3scale(P[i].vel, 1.0f-1.5f*dt); /* air drag         */
        P[i].pos=v3add(P[i].pos, v3scale(P[i].vel,dt));
        P[i].life-=dt;
        if(P[i].life<=0) P[i].alive=0;
    }
}

void particles_render(mat4 vp){
    int n=0;
    for(int i=0;i<MAXP;i++){
        if(!P[i].alive) continue;
        float a=P[i].life/P[i].maxlife; if(a>1) a=1;
        float *o=&s_buf[n*8];
        o[0]=P[i].pos.x; o[1]=P[i].pos.y; o[2]=P[i].pos.z;
        o[3]=P[i].r; o[4]=P[i].g; o[5]=P[i].b; o[6]=a; o[7]=P[i].size;
        n++;
    }
    if(!n) return;
    glUseProgram(s_prog);
    glUniformMatrix4fv(s_vp,1,GL_FALSE,vp.m);
    glBindVertexArray(s_vao); glBindBuffer(GL_ARRAY_BUFFER,s_vbo);
    glBufferSubData(GL_ARRAY_BUFFER,0,n*8*sizeof(float),s_buf);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE);   /* additive glow            */
    glDepthMask(GL_FALSE);              /* don't occlude each other */
    glDrawArrays(GL_POINTS,0,n);
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
}
