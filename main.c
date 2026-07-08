/*
 * kos3d - a procedural 3D FPS dungeon crawler
 * -------------------------------------------
 * Modern OpenGL 3.3 (GLFW + GLEW).
 *
 * Modules:
 *   math3d.h   - vector / matrix math (header-only)
 *   dungeon.*  - map generation, tile queries, collision & LOS
 *   mesh.*     - world triangles + unit cube
 *   shader.*   - GLSL sources + compile/link + light uniforms
 *   entities.* - relics, torches, monsters, bolts, player state
 *   main.c     - window, input, and the render / update loop
 *
 * Controls:  Mouse look | LMB shoot | WASD move | Shift sprint | R new | Esc quit
 */

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "math3d.h"
#include "dungeon.h"
#include "mesh.h"
#include "shader.h"
#include "entities.h"
#include "particles.h"

/* input state (local to the window) */
static double g_last_mx, g_last_my;
static int    g_first_mouse=1;
static int    g_want_regen=0;

static void key_cb(GLFWwindow *w, int key, int sc, int action, int mods){
    (void)sc;(void)mods;
    if(action==GLFW_PRESS){
        if(key==GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w,1);
        if(key==GLFW_KEY_R)      g_want_regen=1;
    }
}
static void mouse_cb(GLFWwindow *w, double mx, double my){
    (void)w;
    if(g_first_mouse){ g_last_mx=mx; g_last_my=my; g_first_mouse=0; }
    float dx=(float)(mx-g_last_mx), dy=(float)(my-g_last_my);
    g_last_mx=mx; g_last_my=my;
    const float sens=0.0022f;
    g_yaw+=dx*sens; g_pitch-=dy*sens;
    float lim=PI*0.5f-0.05f;
    if(g_pitch>lim) g_pitch=lim;
    if(g_pitch<-lim) g_pitch=-lim;
}

int main(void){
    srand((unsigned)time(NULL));
    if(!glfwInit()){ fprintf(stderr,"glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES,4);

    int WW=1280, WH=720;
    GLFWwindow *win=glfwCreateWindow(WW,WH,"kos3d — dungeon crawler",NULL,NULL);
    if(!win){ fprintf(stderr,"window failed\n"); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    glfwSetInputMode(win,GLFW_CURSOR,GLFW_CURSOR_DISABLED);
    if(glfwRawMouseMotionSupported()) glfwSetInputMode(win,GLFW_RAW_MOUSE_MOTION,GLFW_TRUE);
    glfwSetKeyCallback(win,key_cb);
    glfwSetCursorPosCallback(win,mouse_cb);

    glewExperimental=GL_TRUE;
    if(glewInit()!=GLEW_OK){ fprintf(stderr,"glewInit failed\n"); return 1; }
    glGetError();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CCW);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    GLuint prog_world=make_program(WORLD_VS,WORLD_FS);
    GLuint prog_lit  =make_program(LIT_VS,LIT_FS);
    GLuint prog_emit =make_program(EMIT_VS,EMIT_FS);
    GLuint prog_hud  =make_program(HUD_VS,HUD_FS);
    particles_init();

    new_dungeon();

    /* world VAO/VBO */
    float *verts=NULL; int vcount=build_world_geometry(&verts);
    GLuint wvao,wvbo;
    glGenVertexArrays(1,&wvao); glGenBuffers(1,&wvbo);
    glBindVertexArray(wvao); glBindBuffer(GL_ARRAY_BUFFER,wvbo);
    glBufferData(GL_ARRAY_BUFFER,vcount*9*sizeof(float),verts,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,9*sizeof(float),(void*)0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,9*sizeof(float),(void*)(3*sizeof(float)));
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,9*sizeof(float),(void*)(6*sizeof(float)));
    glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,9*sizeof(float),(void*)(8*sizeof(float)));
    for(int i=0;i<4;i++) glEnableVertexAttribArray(i);
    free(verts);

    /* cube VAO/VBO (monsters, props, viewmodel) */
    GLuint cvao,cvbo;
    glGenVertexArrays(1,&cvao); glGenBuffers(1,&cvbo);
    glBindVertexArray(cvao); glBindBuffer(GL_ARRAY_BUFFER,cvbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(float)*108,CUBE,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    /* sphere VAO/VBO (round glowing orbs: relics, bolts, eyes, staff tip) */
    float *sph=NULL; int svcount=make_sphere(&sph);
    GLuint svao,svbo;
    glGenVertexArrays(1,&svao); glGenBuffers(1,&svbo);
    glBindVertexArray(svao); glBindBuffer(GL_ARRAY_BUFFER,svbo);
    glBufferData(GL_ARRAY_BUFFER,svcount*3*sizeof(float),sph,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    free(sph);

    /* crosshair lines */
    float ch[]={ -0.02f,0, 0.02f,0,  0,-0.035f, 0,0.035f };
    GLuint hvao,hvbo;
    glGenVertexArrays(1,&hvao); glGenBuffers(1,&hvbo);
    glBindVertexArray(hvao); glBindBuffer(GL_ARRAY_BUFFER,hvbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(ch),ch,GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    /* unit quad (0,0)-(1,1) for HUD bars / vignette */
    float uq[]={0,0, 1,0, 1,1, 0,0, 1,1, 0,1};
    GLuint qvao,qvbo;
    glGenVertexArrays(1,&qvao); glGenBuffers(1,&qvbo);
    glBindVertexArray(qvao); glBindBuffer(GL_ARRAY_BUFFER,qvbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(uq),uq,GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    /* uniform locations */
    GLint uw_vp=glGetUniformLocation(prog_world,"uVP");
    LightU wl=light_locs(prog_world);

    GLint ul_mvp  =glGetUniformLocation(prog_lit,"uMVP");
    GLint ul_model=glGetUniformLocation(prog_lit,"uModel");
    GLint ul_nrm  =glGetUniformLocation(prog_lit,"uNormalMat");
    GLint ul_base =glGetUniformLocation(prog_lit,"uBase");
    GLint ul_emis =glGetUniformLocation(prog_lit,"uEmissive");
    LightU ll=light_locs(prog_lit);

    GLint ue_mvp=glGetUniformLocation(prog_emit,"uMVP");
    GLint ue_col=glGetUniformLocation(prog_emit,"uColor");
    GLint ue_pul=glGetUniformLocation(prog_emit,"uPulse");

    GLint uh_off=glGetUniformLocation(prog_hud,"uOffset");
    GLint uh_scl=glGetUniformLocation(prog_hud,"uScale");
    GLint uh_col=glGetUniformLocation(prog_hud,"uColor");
    GLint uh_alp=glGetUniformLocation(prog_hud,"uAlpha");

    int prev_mouse_down=0;
    double prev=glfwGetTime(), fps_t=prev; int fps_n=0; char title[200];

    while(!glfwWindowShouldClose(win)){
        double now=glfwGetTime();
        float dt=(float)(now-prev); prev=now; if(dt>0.05f) dt=0.05f;

        if(g_want_regen){
            g_want_regen=0; new_dungeon();
            float *nv=NULL; int nc=build_world_geometry(&nv);
            glBindVertexArray(wvao); glBindBuffer(GL_ARRAY_BUFFER,wvbo);
            glBufferData(GL_ARRAY_BUFFER,nc*9*sizeof(float),nv,GL_STATIC_DRAW);
            free(nv); vcount=nc;
        }

        /* decay timers */
        if(g_hurt>0)    g_hurt   -= dt*1.5f;
        if(g_recoil>0){ g_recoil -= dt*6.0f; if(g_recoil<0) g_recoil=0; }
        if(g_muzzle>0){ g_muzzle -= dt*8.0f; if(g_muzzle<0) g_muzzle=0; }
        if(g_fire_cd>0) g_fire_cd-= dt;
        g_energy += dt*ENERGY_REGEN; if(g_energy>ENERGY_MAX) g_energy=ENERGY_MAX;

        /* movement */
        float fwd_x=cosf(g_yaw), fwd_z=sinf(g_yaw);
        float rgt_x=cosf(g_yaw+PI*0.5f), rgt_z=sinf(g_yaw+PI*0.5f);
        float speed=(glfwGetKey(win,GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS?7.5f:4.2f);
        float mx=0,mz=0;
        if(glfwGetKey(win,GLFW_KEY_W)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_UP)==GLFW_PRESS){    mx+=fwd_x; mz+=fwd_z; }
        if(glfwGetKey(win,GLFW_KEY_S)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_DOWN)==GLFW_PRESS){  mx-=fwd_x; mz-=fwd_z; }
        if(glfwGetKey(win,GLFW_KEY_D)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_RIGHT)==GLFW_PRESS){ mx+=rgt_x; mz+=rgt_z; }
        if(glfwGetKey(win,GLFW_KEY_A)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_LEFT)==GLFW_PRESS){  mx-=rgt_x; mz-=rgt_z; }
        float ml=sqrtf(mx*mx+mz*mz);
        if(ml>1e-4f && !g_won){
            mx/=ml; mz/=ml;
            float pr=PLAYER_R*CELL;
            float nx=g_px+mx*speed*dt, nz=g_pz+mz*speed*dt;
            if(!blocked_r(nx,g_pz,pr)) g_px=nx;
            if(!blocked_r(g_px,nz,pr)) g_pz=nz;
            g_bob += speed*dt*1.6f;
        }

        float eye_y=g_eye_y + sinf(g_bob)*0.06f;
        v3 eye=v3v(g_px,eye_y,g_pz);
        v3 dir=v3v(cosf(g_pitch)*cosf(g_yaw), sinf(g_pitch), cosf(g_pitch)*sinf(g_yaw));

        /* shooting (edge-triggered + cooldown) */
        int mdown=glfwGetMouseButton(win,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
        if(mdown && !prev_mouse_down && g_fire_cd<=0 && g_energy>=SHOT_COST && !g_won){
            fire_bolt(eye,dir); g_fire_cd=0.22f;
            particles_burst(v3add(eye,v3scale(dir,0.7f)), v3v(0.5f,0.8f,1.6f), 10, 3.5f);
        }
        prev_mouse_down=mdown;

        /* update bolts */
        for(int i=0;i<MAX_BOLTS;i++){
            if(!g_bolt[i].alive) continue;
            g_bolt[i].pos=v3add(g_bolt[i].pos,v3scale(g_bolt[i].vel,dt));
            g_bolt[i].life-=dt;
            /* glowing trail */
            particles_spawn(g_bolt[i].pos, v3v((frand()-0.5f)*0.6f,(frand()-0.5f)*0.6f,(frand()-0.5f)*0.6f),
                            v3v(0.4f,0.7f,1.5f), 0.28f, 5.0f);
            if(g_bolt[i].life<=0 || solid_at_world(g_bolt[i].pos.x,g_bolt[i].pos.z)){
                particles_burst(g_bolt[i].pos, v3v(0.4f,0.7f,1.6f), 12, 4.0f);
                g_bolt[i].alive=0; continue;
            }
            for(int m=0;m<g_num_mon;m++){
                if(!g_mon[m].alive) continue;
                float dx=g_mon[m].pos.x-g_bolt[i].pos.x, dy=g_mon[m].pos.y-g_bolt[i].pos.y, dz=g_mon[m].pos.z-g_bolt[i].pos.z;
                float hr=0.9f*g_mon[m].scale;
                if(dx*dx+dy*dy+dz*dz < hr*hr){
                    g_mon[m].hp-=16.0f; g_mon[m].hurt=1.0f; g_bolt[i].alive=0;
                    particles_burst(g_bolt[i].pos, v3v(1.0f,0.4f,0.6f), 10, 3.0f);
                    if(g_mon[m].hp<=0){
                        g_mon[m].alive=0; g_kills++;
                        particles_burst(g_mon[m].pos, v3v(1.2f,0.35f,0.1f), 28, 5.0f);
                    }
                    break;
                }
            }
        }

        /* update monsters: chase player on LOS/proximity, melee on contact */
        for(int m=0;m<g_num_mon;m++){
            Monster *mo=&g_mon[m];
            if(!mo->alive) continue;
            mo->anim+=dt*3.0f;
            if(mo->hurt>0) mo->hurt-=dt*3.0f;
            if(mo->atk_cd>0) mo->atk_cd-=dt;
            v3 tp=v3v(g_px,mo->pos.y,g_pz);
            v3 to=v3sub(tp,mo->pos); float dist=v3len(to);
            int aware = dist<18.0f && (dist<6.0f || los(mo->pos,eye));
            if(aware && dist>1.1f && !g_won){
                v3 step=v3scale(v3norm(to),2.7f*dt);
                float mr=MON_R*CELL*mo->scale;
                if(!blocked_r(mo->pos.x+step.x,mo->pos.z,mr)) mo->pos.x+=step.x;
                if(!blocked_r(mo->pos.x,mo->pos.z+step.z,mr)) mo->pos.z+=step.z;
            }
            if(dist<1.4f && mo->atk_cd<=0 && !g_won){
                g_php-=12.0f; mo->atk_cd=1.0f; g_hurt=0.7f;
                if(g_php<=0){ g_dead_flash=1; reset_player_to_spawn(); g_hurt=1.0f; }
            }
        }

        /* healing shrines: recharge, then heal on contact if hurt */
        for(int i=0;i<g_num_shrines;i++){
            if(g_shrines[i].cd>0){ g_shrines[i].cd-=dt; continue; }
            float dx=g_shrines[i].pos.x-g_px, dz=g_shrines[i].pos.z-g_pz;
            if(dx*dx+dz*dz<1.8f*1.8f && g_php<100.0f && !g_won){
                g_php+=SHRINE_HEAL; if(g_php>100.0f) g_php=100.0f;
                g_shrines[i].cd=SHRINE_CD;
                particles_burst(v3v(g_shrines[i].pos.x,1.2f,g_shrines[i].pos.z), v3v(1.2f,0.3f,0.6f), 34, 4.0f);
            }
        }

        /* ambient particle emitters: torch embers + relic sparkles + shrine motes */
        for(int i=0;i<g_num_shrines;i++){
            if(g_shrines[i].cd>0) continue; /* only active shrines shimmer */
            float dx=g_shrines[i].pos.x-g_px, dz=g_shrines[i].pos.z-g_pz;
            if(dx*dx+dz*dz > 18.0f*18.0f) continue;
            if(frand() < dt*9.0f)
                particles_spawn(v3v(g_shrines[i].pos.x+(frand()-0.5f)*0.5f, 0.2f, g_shrines[i].pos.z+(frand()-0.5f)*0.5f),
                                v3v((frand()-0.5f)*0.2f, 0.6f+0.4f*frand(), (frand()-0.5f)*0.2f),
                                v3v(1.1f,0.35f,0.6f), 1.1f, 5.0f);
        }
        for(int i=0;i<g_num_torches;i++){
            float dx=g_torches[i].pos.x-g_px, dz=g_torches[i].pos.z-g_pz;
            if(dx*dx+dz*dz > 20.0f*20.0f) continue;
            if(frand() < dt*10.0f)
                particles_spawn(v3add(g_torches[i].pos,v3v((frand()-0.5f)*0.4f,0,(frand()-0.5f)*0.4f)),
                                v3v((frand()-0.5f)*0.3f, 0.5f+0.4f*frand(), (frand()-0.5f)*0.3f),
                                v3v(1.0f,0.5f,0.15f), 1.2f, 5.0f);
        }
        for(int i=0;i<g_num_relics;i++){
            if(g_relics[i].taken) continue;
            float dx=g_relics[i].pos.x-g_px, dz=g_relics[i].pos.z-g_pz;
            if(dx*dx+dz*dz > 18.0f*18.0f) continue;
            if(frand() < dt*14.0f){
                float ang=frand()*6.28f, rad=0.5f;
                v3 off=v3v(cosf(ang)*rad, (frand()-0.3f)*0.4f, sinf(ang)*rad);
                v3 vel=v3v(-sinf(ang)*0.6f, 0.2f, cosf(ang)*0.6f); /* orbiting drift */
                particles_spawn(v3add(g_relics[i].pos,off), vel, v3v(0.3f,0.8f,1.3f), 0.7f, 4.0f);
            }
        }

        /* camera matrices */
        glfwGetFramebufferSize(win,&WW,&WH);
        glViewport(0,0,WW,WH);
        mat4 proj=mat4_perspective(70.0f*PI/180.0f,(float)WW/(WH>0?WH:1),0.05f,120.0f);
        mat4 view=mat4_lookat(eye,v3add(eye,dir),v3v(0,1,0));
        mat4 vp=mat4_mul(proj,view);

        /* assemble lights: player torch, muzzle flash, bolts, nearest torches */
        float flick=0.82f+0.18f*sinf((float)now*11.0f)+0.06f*sinf((float)now*23.0f);
        v3 lp[MAX_LIGHTS], lc[MAX_LIGHTS]; float lr[MAX_LIGHTS]; int nl=0;
        lp[nl]=eye; lc[nl]= g_won? v3v(0.4f,1.6f,0.5f):v3scale(v3v(1.3f,0.9f,0.55f),flick); lr[nl]=9.0f; nl++;
        if(g_muzzle>0.01f && nl<MAX_LIGHTS){ lp[nl]=v3add(eye,v3scale(dir,0.8f)); lc[nl]=v3scale(v3v(0.6f,0.8f,1.6f),g_muzzle*2.0f); lr[nl]=6.0f; nl++; }
        for(int i=0;i<MAX_BOLTS && nl<MAX_LIGHTS;i++) if(g_bolt[i].alive){
            lp[nl]=g_bolt[i].pos; lc[nl]=v3v(0.3f,0.6f,1.5f); lr[nl]=4.0f; nl++;
        }
        for(int i=0;i<g_num_torches && nl<MAX_LIGHTS;i++){
            float dx=g_torches[i].pos.x-g_px, dz=g_torches[i].pos.z-g_pz;
            if(dx*dx+dz*dz > 22.0f*22.0f) continue;
            float tf=0.75f+0.25f*sinf((float)now*7.0f+i*1.7f);
            lp[nl]=g_torches[i].pos; lc[nl]=v3scale(g_torches[i].color,tf); lr[nl]=6.5f; nl++;
        }
        for(int i=0;i<g_num_shrines && nl<MAX_LIGHTS;i++){
            if(g_shrines[i].cd>0) continue; /* dark while recharging */
            float dx=g_shrines[i].pos.x-g_px, dz=g_shrines[i].pos.z-g_pz;
            if(dx*dx+dz*dz > 20.0f*20.0f) continue;
            float pf=0.7f+0.3f*sinf((float)now*3.0f+i);
            lp[nl]=v3v(g_shrines[i].pos.x,WALL_H*0.5f,g_shrines[i].pos.z);
            lc[nl]=v3scale(v3v(1.0f,0.3f,0.55f),pf); lr[nl]=6.5f; nl++;
        }

        /* -------- draw world -------- */
        glClearColor(0.01f,0.01f,0.02f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glUseProgram(prog_world);
        glUniformMatrix4fv(uw_vp,1,GL_FALSE,vp.m);
        upload_lights(wl,nl,lp,lc,lr,eye);
        glBindVertexArray(wvao);
        glDrawArrays(GL_TRIANGLES,0,vcount);

        /* -------- monsters (lit bodies) -------- */
        glUseProgram(prog_lit);
        upload_lights(ll,nl,lp,lc,lr,eye);
        glBindVertexArray(cvao);
        glDisable(GL_CULL_FACE);
        for(int m=0;m<g_num_mon;m++){
            Monster *mo=&g_mon[m];
            if(!mo->alive) continue;
            float yaw=atan2f(g_pz-mo->pos.z, g_px-mo->pos.x);
            float sc=mo->scale;
            float hoverY=mo->pos.y + 0.12f*sinf(mo->anim);
            mat4 model=mat4_mul(mat4_translate(v3v(mo->pos.x,hoverY,mo->pos.z)),
                       mat4_mul(mat4_rotY(-yaw), mat4_scale(v3v(0.7f*sc,1.0f*sc,0.55f*sc))));
            mat4 mvp=mat4_mul(vp,model);
            glUniformMatrix4fv(ul_mvp,1,GL_FALSE,mvp.m);
            glUniformMatrix4fv(ul_model,1,GL_FALSE,model.m);
            float nm[9]={0.7f,0,0, 0,1,0, 0,0,0.55f};
            glUniformMatrix3fv(ul_nrm,1,GL_FALSE,nm);
            v3 emis=v3scale(v3v(0.6f,0.1f,0.1f), mo->hurt>0?1.5f:0.15f);
            glUniform3f(ul_base,0.18f,0.05f,0.22f);
            glUniform3f(ul_emis,emis.x,emis.y,emis.z);
            glDrawArrays(GL_TRIANGLES,0,36);
        }
        glEnable(GL_CULL_FACE);

        /* -------- emissive props: round orbs (spheres) + exit pillar (cube) -------- */
        glUseProgram(prog_emit);
        glDisable(GL_CULL_FACE);
        glBindVertexArray(svao);

        for(int m=0;m<g_num_mon;m++){
            Monster *mo=&g_mon[m];
            if(!mo->alive) continue;
            float yaw=atan2f(g_pz-mo->pos.z, g_px-mo->pos.x);
            float sc=mo->scale;
            float hoverY=mo->pos.y + 0.12f*sinf(mo->anim);
            v3 fw=v3v(cosf(yaw),0,sinf(yaw)), rt=v3v(-sinf(yaw),0,cosf(yaw)), up=v3v(0,1,0);
            /* eye layout by tier: 2 in a row / 3 in a line / 4 in a square */
            float ex[4], ey[4]; int ne;
            if(mo->tier==0){      ne=2; ex[0]=-1;ey[0]=0;  ex[1]=1;ey[1]=0; }
            else if(mo->tier==1){ ne=3; ex[0]=-1;ey[0]=0;  ex[1]=0;ey[1]=0;  ex[2]=1;ey[2]=0; }
            else {                ne=4; ex[0]=-1;ey[0]=-1; ex[1]=1;ey[1]=-1; ex[2]=-1;ey[2]=1; ex[3]=1;ey[3]=1; }
            float spacing=0.14f*sc, esz=0.13f*sc;
            /* body half-extent toward the player is ~0.35*sc, so push eyes past
               the front face (0.44*sc) or they get buried inside the cube */
            v3 center=v3add(v3v(mo->pos.x,hoverY+0.25f*sc,mo->pos.z), v3scale(fw,0.44f*sc));
            for(int k=0;k<ne;k++){
                v3 ep=v3add(center, v3add(v3scale(rt,ex[k]*spacing), v3scale(up,ey[k]*spacing)));
                mat4 model=mat4_mul(mat4_translate(ep),mat4_scale(v3v(esz,esz,esz)));
                mat4 mvp=mat4_mul(vp,model);
                glUniformMatrix4fv(ue_mvp,1,GL_FALSE,mvp.m);
                glUniform3f(ue_col,1.4f,0.15f,0.1f);
                glUniform1f(ue_pul,0.6f+0.4f*sinf(mo->anim*2.0f));
                glDrawArrays(GL_TRIANGLES,0,svcount);
            }
        }
        for(int i=0;i<g_num_relics;i++){
            if(g_relics[i].taken) continue;
            float bobY=g_relics[i].pos.y+0.15f*sinf((float)now*2.0f+i);
            mat4 model=mat4_mul(mat4_translate(v3v(g_relics[i].pos.x,bobY,g_relics[i].pos.z)),
                       mat4_scale(v3v(0.5f,0.5f,0.5f)));
            mat4 mvp=mat4_mul(vp,model);
            glUniformMatrix4fv(ue_mvp,1,GL_FALSE,mvp.m);
            glUniform3f(ue_col,0.3f,0.85f,1.1f);
            glUniform1f(ue_pul,0.5f+0.5f*sinf((float)now*3.0f+i));
            glDrawArrays(GL_TRIANGLES,0,svcount);
        }
        for(int i=0;i<MAX_BOLTS;i++){
            if(!g_bolt[i].alive) continue;
            mat4 model=mat4_mul(mat4_translate(g_bolt[i].pos),mat4_scale(v3v(0.22f,0.22f,0.22f)));
            mat4 mvp=mat4_mul(vp,model);
            glUniformMatrix4fv(ue_mvp,1,GL_FALSE,mvp.m);
            glUniform3f(ue_col,0.4f,0.8f,1.6f);
            glUniform1f(ue_pul,1.0f);
            glDrawArrays(GL_TRIANGLES,0,svcount);
        }
        {   /* exit portal — tall pillar (cube) */
            int ready=(g_collected==g_num_relics);
            glBindVertexArray(cvao);
            mat4 model=mat4_mul(mat4_translate(g_exit_pos),
                       mat4_mul(mat4_rotY((float)now*0.6f), mat4_scale(v3v(0.8f,WALL_H*0.95f,0.8f))));
            mat4 mvp=mat4_mul(vp,model);
            glUniformMatrix4fv(ue_mvp,1,GL_FALSE,mvp.m);
            if(ready) glUniform3f(ue_col,0.2f,1.2f,0.4f); else glUniform3f(ue_col,0.5f,0.15f,0.15f);
            glUniform1f(ue_pul,0.5f+0.5f*sinf((float)now*4.0f));
            glDrawArrays(GL_TRIANGLES,0,36);
        }
        /* healing shrines — shorter pink pillars (cube VAO still bound) */
        for(int i=0;i<g_num_shrines;i++){
            int active=(g_shrines[i].cd<=0);
            float H=WALL_H*0.55f;
            mat4 model=mat4_mul(mat4_translate(v3v(g_shrines[i].pos.x,H*0.5f,g_shrines[i].pos.z)),
                       mat4_mul(mat4_rotY((float)now*0.8f), mat4_scale(v3v(0.45f,H,0.45f))));
            mat4 mvp=mat4_mul(vp,model);
            glUniformMatrix4fv(ue_mvp,1,GL_FALSE,mvp.m);
            if(active) glUniform3f(ue_col,1.2f,0.3f,0.55f); else glUniform3f(ue_col,0.22f,0.10f,0.16f);
            glUniform1f(ue_pul, active?0.5f+0.5f*sinf((float)now*3.0f+i):0.05f);
            glDrawArrays(GL_TRIANGLES,0,36);
        }
        glEnable(GL_CULL_FACE);

        /* relic pickup + exit check */
        for(int i=0;i<g_num_relics;i++){
            if(g_relics[i].taken) continue;
            float dx=g_relics[i].pos.x-g_px, dz=g_relics[i].pos.z-g_pz;
            if(dx*dx+dz*dz<1.4f*1.4f){
                g_relics[i].taken=1; g_collected++; g_energy=ENERGY_MAX;
                particles_burst(g_relics[i].pos, v3v(0.35f,0.9f,1.4f), 30, 4.5f);
            }
        }
        if(g_collected==g_num_relics && !g_won){
            float dx=g_exit_pos.x-g_px, dz=g_exit_pos.z-g_pz;
            if(dx*dx+dz*dz<2.0f*2.0f) g_won=1;
        }

        /* -------- particles (world-space, additive) -------- */
        particles_update(dt);
        particles_render(vp);

        /* -------- weapon viewmodel (on top) -------- */
        glClear(GL_DEPTH_BUFFER_BIT);
        {
            v3 f=v3norm(dir);
            v3 rt=v3norm(v3cross(f,v3v(0,1,0)));
            v3 up=v3cross(rt,f);
            float kick=g_recoil*0.12f + sinf(g_bob)*0.01f;
            v3 anchor=v3add(eye, v3add(v3scale(f,0.55f-kick),
                        v3add(v3scale(rt,0.26f), v3scale(up,-0.22f))));
            mat4 rot=mat4_basis(rt,up,v3scale(f,-1.0f));
            /* staff shaft (lit) */
            glUseProgram(prog_lit);
            upload_lights(ll,nl,lp,lc,lr,eye);
            glBindVertexArray(cvao); glDisable(GL_CULL_FACE);
            mat4 model=mat4_mul(mat4_translate(anchor),mat4_mul(rot,mat4_scale(v3v(0.06f,0.06f,0.5f))));
            mat4 mvp=mat4_mul(vp,model);
            glUniformMatrix4fv(ul_mvp,1,GL_FALSE,mvp.m);
            glUniformMatrix4fv(ul_model,1,GL_FALSE,model.m);
            float nm[9]={1,0,0,0,1,0,0,0,1};
            glUniformMatrix3fv(ul_nrm,1,GL_FALSE,nm);
            glUniform3f(ul_base,0.15f,0.13f,0.18f);
            glUniform3f(ul_emis,0.0f,0.0f,0.0f);
            glDrawArrays(GL_TRIANGLES,0,36);
            /* glowing round tip */
            glUseProgram(prog_emit);
            glBindVertexArray(svao);
            v3 tip=v3add(anchor,v3scale(f,0.30f));
            model=mat4_mul(mat4_translate(tip),mat4_scale(v3v(0.12f,0.12f,0.12f)));
            mvp=mat4_mul(vp,model);
            glUniformMatrix4fv(ue_mvp,1,GL_FALSE,mvp.m);
            glUniform3f(ue_col,0.4f,0.8f,1.6f);
            glUniform1f(ue_pul,0.6f+g_recoil*1.2f);
            glDrawArrays(GL_TRIANGLES,0,svcount);
            /* idle sparkle drifting off the staff tip */
            if(frand()<dt*12.0f)
                particles_spawn(tip, v3v((frand()-0.5f)*0.4f,0.4f+0.3f*frand(),(frand()-0.5f)*0.4f),
                                v3v(0.4f,0.7f,1.5f), 0.5f, 4.0f);
            glEnable(GL_CULL_FACE);
        }

        /* -------- HUD -------- */
        glUseProgram(prog_hud);
        glDisable(GL_DEPTH_TEST);

        if(g_hurt>0.01f){
            glUniform2f(uh_off,-1.0f,-1.0f); glUniform2f(uh_scl,2.0f,2.0f);
            glUniform3f(uh_col,0.7f,0.0f,0.0f); glUniform1f(uh_alp,g_hurt*0.45f);
            glBindVertexArray(qvao); glDrawArrays(GL_TRIANGLES,0,6);
        }
        if(g_won){
            glUniform2f(uh_off,-1.0f,-1.0f); glUniform2f(uh_scl,2.0f,2.0f);
            glUniform3f(uh_col,0.1f,0.6f,0.2f); glUniform1f(uh_alp,0.18f);
            glBindVertexArray(qvao); glDrawArrays(GL_TRIANGLES,0,6);
        }
        {   /* health bar bottom-left */
            float hp=g_php<0?0:g_php/100.0f;
            glBindVertexArray(qvao);
            glUniform2f(uh_off,-0.95f,-0.92f); glUniform2f(uh_scl,0.5f,0.05f);
            glUniform3f(uh_col,0.15f,0.03f,0.03f); glUniform1f(uh_alp,0.8f);
            glDrawArrays(GL_TRIANGLES,0,6);
            glUniform2f(uh_scl,0.5f*hp,0.05f);
            glUniform3f(uh_col,0.85f,0.15f,0.12f); glUniform1f(uh_alp,0.95f);
            glDrawArrays(GL_TRIANGLES,0,6);
        }
        {   /* energy bar just above the health bar */
            float en=g_energy/ENERGY_MAX;
            int ready=g_energy>=SHOT_COST;
            glBindVertexArray(qvao);
            glUniform2f(uh_off,-0.95f,-0.85f); glUniform2f(uh_scl,0.5f,0.04f);
            glUniform3f(uh_col,0.03f,0.06f,0.15f); glUniform1f(uh_alp,0.8f);
            glDrawArrays(GL_TRIANGLES,0,6);
            glUniform2f(uh_scl,0.5f*en,0.04f);
            if(ready) glUniform3f(uh_col,0.30f,0.65f,1.0f); else glUniform3f(uh_col,0.25f,0.30f,0.45f);
            glUniform1f(uh_alp,0.95f);
            glDrawArrays(GL_TRIANGLES,0,6);
        }
        /* crosshair (aspect-corrected) */
        glUniform2f(uh_off,0,0);
        glUniform2f(uh_scl,(float)WH/(float)WW,1.0f);
        glUniform3f(uh_col,0.9f,0.9f,0.9f); glUniform1f(uh_alp,0.85f);
        glBindVertexArray(hvao); glLineWidth(2.0f);
        glDrawArrays(GL_LINES,0,4);

        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(win);
        glfwPollEvents();

        /* title HUD */
        fps_n++;
        if(now-fps_t>=0.5){
            double fps=fps_n/(now-fps_t); fps_t=now; fps_n=0;
            if(g_won)
                snprintf(title,sizeof title,"kos3d — YOU ESCAPED!  kills:%d  (R = new dungeon)  %.0f fps",g_kills,fps);
            else if(g_collected==g_num_relics)
                snprintf(title,sizeof title,"kos3d — all relics! reach the GREEN portal  HP:%d  kills:%d  %.0f fps",(int)g_php,g_kills,fps);
            else
                snprintf(title,sizeof title,"kos3d — relics %d/%d  HP:%d  EN:%d  kills:%d  |  LMB shoot, R new, Esc quit  |  %.0f fps",
                         g_collected,g_num_relics,(int)g_php,(int)g_energy,g_kills,fps);
            glfwSetWindowTitle(win,title);
        }
    }
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
