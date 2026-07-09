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
#include <string.h>
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
static int    g_paused=1;                 /* start in the menu, paused */
static int    g_dead=0;                   /* death screen active */
static int    g_click=0;                  /* one-shot: set on each LMB press event */
static double g_cursor_x, g_cursor_y;     /* raw cursor position (menu) */

/* pause/resume: swap cursor mode and avoid a look-jump on resume */
static void set_paused(GLFWwindow *w, int p){
    g_paused=p;
    glfwSetInputMode(w,GLFW_CURSOR, p?GLFW_CURSOR_NORMAL:GLFW_CURSOR_DISABLED);
    if(!p) g_first_mouse=1;
}

static void key_cb(GLFWwindow *w, int key, int sc, int action, int mods){
    (void)sc;(void)mods;
    if(action==GLFW_PRESS){
        if(key==GLFW_KEY_ESCAPE && !g_dead && !g_won) set_paused(w, !g_paused); /* toggle menu */
    }
}
static void mouse_cb(GLFWwindow *w, double mx, double my){
    (void)w;
    g_cursor_x=mx; g_cursor_y=my;
    if(g_paused||g_dead||g_won) return;     /* no camera look in menu / death / win screen */
    if(g_first_mouse){ g_last_mx=mx; g_last_my=my; g_first_mouse=0; }
    float dx=(float)(mx-g_last_mx), dy=(float)(my-g_last_my);
    g_last_mx=mx; g_last_my=my;
    const float sens=0.0022f;
    g_yaw+=dx*sens; g_pitch-=dy*sens;
    float lim=PI*0.5f-0.05f;
    if(g_pitch>lim) g_pitch=lim;
    if(g_pitch<-lim) g_pitch=-lim;
}
/* one discrete event per physical press -> reliable single-click semantics */
static void mousebtn_cb(GLFWwindow *w, int button, int action, int mods){
    (void)w;(void)mods;
    if(button==GLFW_MOUSE_BUTTON_LEFT && action==GLFW_PRESS) g_click=1;
}

/* ---- minimal icon + pixel-digit HUD helpers (no fonts / no text labels) ---- */
static GLint H_off, H_scl, H_rot, H_col, H_alp; /* HUD uniform locations */
static float H_asp = 1.0f;                        /* WH/WW, refreshed per frame */

/* 3x5 pixel font, digits 0-9. Each byte is one row, bit2=left .. bit0=right. */
static const unsigned char DIGIT[10][5] = {
    {7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7}, {5,5,7,1,1},
    {7,4,7,1,7}, {7,4,7,5,7}, {7,1,1,2,2}, {7,5,7,5,7}, {7,5,7,1,7},
};

/* solid axis-aligned rect; expects prog_hud active and the unit-quad VAO bound */
static void hud_rect(float x,float y,float w,float h,float r,float g,float b,float a){
    glUniform1f(H_rot,0.0f);
    glUniform2f(H_off,x,y); glUniform2f(H_scl,w,h);
    glUniform3f(H_col,r,g,b); glUniform1f(H_alp,a);
    glDrawArrays(GL_TRIANGLES,0,6);
}
/* gem/diamond centered at (cx,cy), half-height hh (aspect-corrected to a square) */
static void hud_diamond(float cx,float cy,float hh,float r,float g,float b,float a){
    float hw=hh*H_asp;
    glUniform1f(H_rot,0.78539816f);   /* 45 degrees */
    glUniform2f(H_off,cx-hw,cy-hh); glUniform2f(H_scl,2.0f*hw,2.0f*hh);
    glUniform3f(H_col,r,g,b); glUniform1f(H_alp,a);
    glDrawArrays(GL_TRIANGLES,0,6);
}
/* one digit with its top-left at (x,y); pixel cell is pw x ph */
static void hud_digit(int d,float x,float y,float pw,float ph,float r,float g,float b,float a){
    if(d<0||d>9) return;
    for(int row=0;row<5;row++)
        for(int col=0;col<3;col++)
            if(DIGIT[d][row] & (4>>col))
                hud_rect(x+col*pw, y-(row+1)*ph, pw*0.82f, ph*0.82f, r,g,b,a);
}
/* left-aligned non-negative integer starting at top-left (x,y) */
static void hud_number(int val,float x,float y,float pw,float ph,float r,float g,float b,float a){
    char buf[16]; int n=snprintf(buf,sizeof buf,"%d",val<0?0:val);
    float dx=x;
    for(int i=0;i<n;i++){ hud_digit(buf[i]-'0',dx,y,pw,ph,r,g,b,a); dx+=pw*3.6f; }
}

/* 3x5 glyph for the few uppercase letters the menu needs (else blank) */
static void glyph5(char c, unsigned char g[5]){
    if(c>='0'&&c<='9'){ for(int i=0;i<5;i++) g[i]=DIGIT[c-'0'][i]; return; }
    const unsigned char *p=0;
    switch(c){
        case 'A':{static const unsigned char t[5]={2,5,7,5,5}; p=t;} break;
        case 'D':{static const unsigned char t[5]={6,5,5,5,6}; p=t;} break;
        case 'E':{static const unsigned char t[5]={7,4,6,4,7}; p=t;} break;
        case 'I':{static const unsigned char t[5]={7,2,2,2,7}; p=t;} break;
        case 'K':{static const unsigned char t[5]={5,6,4,6,5}; p=t;} break;
        case 'O':{static const unsigned char t[5]={7,5,5,5,7}; p=t;} break;
        case 'R':{static const unsigned char t[5]={6,5,6,5,5}; p=t;} break;
        case 'S':{static const unsigned char t[5]={7,4,7,1,7}; p=t;} break;
        case 'T':{static const unsigned char t[5]={7,2,2,2,2}; p=t;} break;
        case 'X':{static const unsigned char t[5]={5,5,2,5,5}; p=t;} break;
        case 'Y':{static const unsigned char t[5]={5,5,2,2,2}; p=t;} break;
        case 'U':{static const unsigned char t[5]={5,5,5,5,7}; p=t;} break;
        case 'C':{static const unsigned char t[5]={7,4,4,4,7}; p=t;} break;
        case 'L':{static const unsigned char t[5]={4,4,4,4,7}; p=t;} break;
        case 'P':{static const unsigned char t[5]={6,5,6,4,4}; p=t;} break;
        case 'M':{static const unsigned char t[5]={5,7,7,5,5}; p=t;} break;
        case '/':{static const unsigned char t[5]={1,1,2,4,4}; p=t;} break;
        default: for(int i=0;i<5;i++) g[i]=0; return;
    }
    for(int i=0;i<5;i++) g[i]=p[i];
}
/* left-aligned text; pw/ph are the pixel-cell size in NDC */
static void hud_text(const char *s,float x,float y,float pw,float ph,float r,float g,float b,float a){
    float dx=x;
    for(const char *c=s;*c;c++){
        unsigned char gl[5]; glyph5(*c,gl);
        for(int row=0;row<5;row++)
            for(int col=0;col<3;col++)
                if(gl[row] & (4>>col))
                    hud_rect(dx+col*pw, y-(row+1)*ph, pw*0.82f, ph*0.82f, r,g,b,a);
        dx+=pw*3.6f;
    }
}
/* text centered horizontally on cx, vertically on cy */
static void hud_text_centered(const char *s,float cx,float cy,float ph,float r,float g,float b,float a){
    float pw=ph*H_asp;
    float w=(float)strlen(s)*pw*3.6f - pw*0.6f;
    hud_text(s, cx-w*0.5f, cy+2.5f*ph, pw, ph, r,g,b,a);
}

int main(void){
    srand((unsigned)time(NULL));
    if(!glfwInit()){ fprintf(stderr,"glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES,4);

    /* fullscreen on the primary monitor at its current (native) resolution */
    GLFWmonitor *mon=glfwGetPrimaryMonitor();
    const GLFWvidmode *mode=glfwGetVideoMode(mon);
    glfwWindowHint(GLFW_RED_BITS,   mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS,  mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    int WW=mode->width, WH=mode->height;
    GLFWwindow *win=glfwCreateWindow(WW,WH,"kos3d — dungeon crawler",mon,NULL);
    if(!win){ fprintf(stderr,"window failed\n"); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    glfwSetInputMode(win,GLFW_CURSOR,GLFW_CURSOR_NORMAL); /* start in the menu */
    if(glfwRawMouseMotionSupported()) glfwSetInputMode(win,GLFW_RAW_MOUSE_MOTION,GLFW_TRUE);
    glfwSetKeyCallback(win,key_cb);
    glfwSetCursorPosCallback(win,mouse_cb);
    glfwSetMouseButtonCallback(win,mousebtn_cb);

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

    /* cylinder VAO/VBO (round pillars: shrines) */
    float *cyl=NULL; int cycount=make_cylinder(&cyl);
    GLuint cyvao,cyvbo;
    glGenVertexArrays(1,&cyvao); glGenBuffers(1,&cyvbo);
    glBindVertexArray(cyvao); glBindBuffer(GL_ARRAY_BUFFER,cyvbo);
    glBufferData(GL_ARRAY_BUFFER,cycount*3*sizeof(float),cyl,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    free(cyl);

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
    GLint ul_fade =glGetUniformLocation(prog_lit,"uFade");
    LightU ll=light_locs(prog_lit);

    GLint ue_mvp=glGetUniformLocation(prog_emit,"uMVP");
    GLint ue_model=glGetUniformLocation(prog_emit,"uModel");
    GLint ue_col=glGetUniformLocation(prog_emit,"uColor");
    GLint ue_pul=glGetUniformLocation(prog_emit,"uPulse");
    GLint ue_alp=glGetUniformLocation(prog_emit,"uAlpha");
    GLint ue_view=glGetUniformLocation(prog_emit,"uViewPos");

    GLint uh_off=glGetUniformLocation(prog_hud,"uOffset");
    GLint uh_scl=glGetUniformLocation(prog_hud,"uScale");
    GLint uh_rot=glGetUniformLocation(prog_hud,"uRot");
    GLint uh_col=glGetUniformLocation(prog_hud,"uColor");
    GLint uh_alp=glGetUniformLocation(prog_hud,"uAlpha");
    H_off=uh_off; H_scl=uh_scl; H_rot=uh_rot; H_col=uh_col; H_alp=uh_alp;

    int sprint_ready=1;   /* stamina gate: locks at empty, re-arms once recharged */
    float g_level_time=0.0f;  /* seconds spent in the current run */
    double prev=glfwGetTime();

    while(!glfwWindowShouldClose(win)){
        double now=glfwGetTime();
        float dt=(float)(now-prev); prev=now; if(dt>0.05f) dt=0.05f;

        if(g_want_regen){
            g_want_regen=0; new_dungeon();
            float *nv=NULL; int nc=build_world_geometry(&nv);
            glBindVertexArray(wvao); glBindBuffer(GL_ARRAY_BUFFER,wvbo);
            glBufferData(GL_ARRAY_BUFFER,nc*9*sizeof(float),nv,GL_STATIC_DRAW);
            free(nv); vcount=nc;
            g_level_time=0.0f;             /* fresh run -> reset the clock */
        }

        /* level timer runs only while actively playing */
        if(!g_paused && !g_dead && !g_won) g_level_time += dt;

        /* decay timers */
        if(g_hurt>0)    g_hurt   -= dt*1.5f;
        if(g_recoil>0){ g_recoil -= dt*6.0f; if(g_recoil<0) g_recoil=0; }
        if(g_muzzle>0){ g_muzzle -= dt*8.0f; if(g_muzzle<0) g_muzzle=0; }
        if(g_fire_cd>0) g_fire_cd-= dt;
        if(!g_paused && !g_dead && !g_won){   /* energy only recharges while playing */
            g_energy += dt*ENERGY_REGEN; if(g_energy>ENERGY_MAX) g_energy=ENERGY_MAX;
        }

        /* movement */
        float fwd_x=cosf(g_yaw), fwd_z=sinf(g_yaw);
        float rgt_x=cosf(g_yaw+PI*0.5f), rgt_z=sinf(g_yaw+PI*0.5f);
        float mx=0,mz=0;
        if(glfwGetKey(win,GLFW_KEY_W)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_UP)==GLFW_PRESS){    mx+=fwd_x; mz+=fwd_z; }
        if(glfwGetKey(win,GLFW_KEY_S)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_DOWN)==GLFW_PRESS){  mx-=fwd_x; mz-=fwd_z; }
        if(glfwGetKey(win,GLFW_KEY_D)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_RIGHT)==GLFW_PRESS){ mx+=rgt_x; mz+=rgt_z; }
        if(glfwGetKey(win,GLFW_KEY_A)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_LEFT)==GLFW_PRESS){  mx-=rgt_x; mz-=rgt_z; }
        float ml=sqrtf(mx*mx+mz*mz);
        /* sprint only while there is energy, and it drains the same blue bar as
           shooting so you can't run forever */
        /* stamina hysteresis: empty the bar and sprint locks until it recharges */
        if(g_energy<=0.5f)       sprint_ready=0;
        if(g_energy>=SHOT_COST)  sprint_ready=1;  /* re-arm once a shot is affordable */
        int sprint = glfwGetKey(win,GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS && ml>1e-4f && sprint_ready && !g_won && !g_paused && !g_dead;
        float speed = sprint?7.5f:4.2f;
        if(ml>1e-4f && !g_won && !g_paused && !g_dead){
            mx/=ml; mz/=ml;
            float pr=PLAYER_R*CELL;
            float nx=g_px+mx*speed*dt, nz=g_pz+mz*speed*dt;
            if(!blocked_r(nx,g_pz,pr)) g_px=nx;
            if(!blocked_r(g_px,nz,pr)) g_pz=nz;
            g_bob += speed*dt*1.6f;
            if(sprint){ g_energy -= dt*SPRINT_DRAIN; if(g_energy<0) g_energy=0; }
        }

        float eye_y=g_eye_y + sinf(g_bob)*0.06f;
        v3 eye=v3v(g_px,eye_y,g_pz);
        v3 dir=v3v(cosf(g_pitch)*cosf(g_yaw), sinf(g_pitch), cosf(g_pitch)*sinf(g_yaw));

        /* one click == one shot: consume the pending press event (set by the
           mouse-button callback). Used for firing OR menu/screen selection. */
        int click=g_click; g_click=0;
        if(!g_paused && !g_dead && !g_won && click && g_fire_cd<=0 && g_energy>=SHOT_COST){
            fire_bolt(eye,dir); g_fire_cd=0.22f;
            particles_burst(v3add(eye,v3scale(dir,0.7f)), v3v(0.5f,0.8f,1.6f), 10, 3.5f);
        }

      if(!g_paused && !g_dead && !g_won){
        /* update bolts */
        for(int i=0;i<MAX_BOLTS;i++){
            if(!g_bolt[i].alive) continue;
            g_bolt[i].vel.y -= BOLT_GRAVITY*dt;   /* bullet drop */
            g_bolt[i].pos=v3add(g_bolt[i].pos,v3scale(g_bolt[i].vel,dt));
            g_bolt[i].life-=dt;
            /* glowing trail */
            particles_spawn(g_bolt[i].pos, v3v((frand()-0.5f)*0.6f,(frand()-0.5f)*0.6f,(frand()-0.5f)*0.6f),
                            v3v(0.4f,0.7f,1.5f), 0.28f, 5.0f);
            if(g_bolt[i].life<=0 || solid_at_world(g_bolt[i].pos.x,g_bolt[i].pos.z)
               || g_bolt[i].pos.y<=0.0f || g_bolt[i].pos.y>=WALL_H){ /* wall, floor or ceiling */
                particles_burst(g_bolt[i].pos, v3v(0.4f,0.7f,1.6f), 12, 4.0f);
                g_bolt[i].alive=0; continue;
            }
            for(int m=0;m<g_num_mon;m++){
                if(!g_mon[m].alive) continue;
                float dx=g_mon[m].pos.x-g_bolt[i].pos.x, dy=g_mon[m].pos.y-g_bolt[i].pos.y, dz=g_mon[m].pos.z-g_bolt[i].pos.z;
                float hr=0.9f*g_mon[m].scale;
                if(dx*dx+dy*dy+dz*dz < hr*hr){
                    g_mon[m].hp-=16.0f; g_mon[m].hurt=1.0f; g_mon[m].aggro=1; g_bolt[i].alive=0;
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
            /* aggro'd wraiths (ones you've shot) hunt you from any distance */
            int aware = mo->aggro || (dist<18.0f && (dist<6.0f || los(mo->pos,eye)));
            /* separation: shove away from other nearby wraiths so bodies never merge */
            v3 sep=v3v(0,0,0); float minsep=1.15f*CELL;
            for(int j=0;j<g_num_mon;j++){
                if(j==m || !g_mon[j].alive) continue;
                float dx=mo->pos.x-g_mon[j].pos.x, dz=mo->pos.z-g_mon[j].pos.z;
                float d2=dx*dx+dz*dz;
                if(d2>1e-4f && d2<minsep*minsep){
                    float d=sqrtf(d2), push=(minsep-d)/minsep;
                    sep.x+=dx/d*push; sep.z+=dz/d*push;
                }
            }
            if(!g_won){
                /* collision radius stays fixed (not scaled by visual size) so big
                   monsters can still fit through corridors instead of getting stuck */
                float mr=MON_R*CELL;
                v3 vel=v3v(0,0,0);
                if(aware && dist>1.1f) vel=v3scale(v3norm(to),4.8f); /* faster than walk */
                vel.x+=sep.x*3.5f; vel.z+=sep.z*3.5f;
                float sx=vel.x*dt, sz=vel.z*dt;
                if(!blocked_r(mo->pos.x+sx,mo->pos.z,mr)) mo->pos.x+=sx;
                if(!blocked_r(mo->pos.x,mo->pos.z+sz,mr)) mo->pos.z+=sz;
            }
            if(dist<1.4f && mo->atk_cd<=0 && !g_won){
                g_php-=12.0f; mo->atk_cd=1.0f; g_hurt=0.7f;
                if(g_php<=0){                 /* death: freeze and show the death screen */
                    g_php=0; g_dead=1; g_hurt=1.0f;
                    glfwSetInputMode(win,GLFW_CURSOR,GLFW_CURSOR_NORMAL);
                }
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
      } /* end if(!g_paused) game update */

        /* camera matrices */
        glfwGetFramebufferSize(win,&WW,&WH);
        glViewport(0,0,WW,WH);
        mat4 proj=mat4_perspective(70.0f*PI/180.0f,(float)WW/(WH>0?WH:1),0.05f,120.0f);
        mat4 view=mat4_lookat(eye,v3add(eye,dir),v3v(0,1,0));
        mat4 vp=mat4_mul(proj,view);

        /* assemble lights: player torch, muzzle flash, bolts, nearest torches */
        v3 lp[MAX_LIGHTS], lc[MAX_LIGHTS]; float lr[MAX_LIGHTS]; int nl=0;
        /* player torch is steady (no flicker) to avoid eye discomfort */
        lp[nl]=eye; lc[nl]= g_won? v3v(0.4f,1.6f,0.5f):v3v(1.15f,0.80f,0.50f); lr[nl]=9.0f; nl++;
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

        /* -------- monsters: capsule wraiths -- cylinder body + hemisphere top ---- */
        glUseProgram(prog_lit);
        upload_lights(ll,nl,lp,lc,lr,eye);
        glDisable(GL_CULL_FACE);
        float nmI[9]={1,0,0, 0,1,0, 0,0,1};
        glUniformMatrix3fv(ul_nrm,1,GL_FALSE,nmI);
        for(int m=0;m<g_num_mon;m++){
            Monster *mo=&g_mon[m];
            if(!mo->alive) continue;
            float sc=mo->scale;
            float R=0.62f*sc;                       /* slimmer cylinder & dome scale */
            float hoverY=mo->pos.y + 0.12f*sinf(mo->anim);
            float topY=hoverY+0.65f*sc;             /* cylinder top / dome centre */
            v3 emis = mo->hurt>0 ? v3v(0.9f,0.15f,0.15f) : v3v(0.10f,0.07f,0.18f);
            glUniform3f(ul_base,0.24f,0.16f,0.34f); /* pale spectral tone */
            glUniform3f(ul_emis,emis.x,emis.y,emis.z);
            /* body: cylinder, bottom fades to a wispy tail */
            glUniform1f(ul_fade,1.0f);              /* dome uses the same fade so */
            glBindVertexArray(cyvao);               /* the two share alpha (no seam) */
            mat4 body=mat4_mul(mat4_translate(v3v(mo->pos.x,hoverY,mo->pos.z)),
                      mat4_scale(v3v(R,1.30f*sc,R)));
            glUniformMatrix4fv(ul_mvp,1,GL_FALSE,mat4_mul(vp,body).m);
            glUniformMatrix4fv(ul_model,1,GL_FALSE,body.m);
            glDrawArrays(GL_TRIANGLES,0,cycount);
            /* rounded top: hemisphere (sphere centred on the cylinder's top rim);
               its lower half fades out inside the body so nothing shows through */
            glBindVertexArray(svao);
            mat4 dome=mat4_mul(mat4_translate(v3v(mo->pos.x,topY,mo->pos.z)),
                      mat4_scale(v3v(R,R,R)));
            glUniformMatrix4fv(ul_mvp,1,GL_FALSE,mat4_mul(vp,dome).m);
            glUniformMatrix4fv(ul_model,1,GL_FALSE,dome.m);
            glDrawArrays(GL_TRIANGLES,0,svcount);
        }
        glEnable(GL_CULL_FACE);

        /* -------- emissive props: glassy glowing orbs -------- */
        glUseProgram(prog_emit);
        glUniform3f(ue_view,eye.x,eye.y,eye.z);
        glDisable(GL_CULL_FACE);
        glBindVertexArray(svao);

        glUniform1f(ue_alp,1.0f);   /* eyes stay solid */
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
            float spacing=0.10f*sc, esz=0.07f*sc;
            /* eyes sit on the front of the dome/upper body (radius ~0.31*sc) */
            v3 center=v3add(v3v(mo->pos.x,hoverY+0.62f*sc,mo->pos.z), v3scale(fw,0.30f*sc));
            for(int k=0;k<ne;k++){
                v3 ep=v3add(center, v3add(v3scale(rt,ex[k]*spacing), v3scale(up,ey[k]*spacing)));
                mat4 model=mat4_mul(mat4_translate(ep),mat4_scale(v3v(esz,esz,esz)));
                glUniformMatrix4fv(ue_mvp,1,GL_FALSE,mat4_mul(vp,model).m);
                glUniformMatrix4fv(ue_model,1,GL_FALSE,model.m);
                glUniform3f(ue_col,1.4f,0.15f,0.1f);
                glUniform1f(ue_pul,0.6f+0.4f*sinf(mo->anim*2.0f));
                glDrawArrays(GL_TRIANGLES,0,svcount);
            }
        }
        glUniform1f(ue_alp,0.55f);   /* relics: translucent glass */
        for(int i=0;i<g_num_relics;i++){
            if(g_relics[i].taken) continue;
            float bobY=g_relics[i].pos.y+0.15f*sinf((float)now*2.0f+i);
            mat4 model=mat4_mul(mat4_translate(v3v(g_relics[i].pos.x,bobY,g_relics[i].pos.z)),
                       mat4_scale(v3v(0.5f,0.5f,0.5f)));
            glUniformMatrix4fv(ue_mvp,1,GL_FALSE,mat4_mul(vp,model).m);
            glUniformMatrix4fv(ue_model,1,GL_FALSE,model.m);
            glUniform3f(ue_col,0.3f,0.85f,1.1f);
            glUniform1f(ue_pul,0.5f+0.5f*sinf((float)now*3.0f+i));
            glDrawArrays(GL_TRIANGLES,0,svcount);
        }
        glUniform1f(ue_alp,0.8f);    /* bolts: mostly solid so they read fast */
        for(int i=0;i<MAX_BOLTS;i++){
            if(!g_bolt[i].alive) continue;
            mat4 model=mat4_mul(mat4_translate(g_bolt[i].pos),mat4_scale(v3v(0.22f,0.22f,0.22f)));
            glUniformMatrix4fv(ue_mvp,1,GL_FALSE,mat4_mul(vp,model).m);
            glUniformMatrix4fv(ue_model,1,GL_FALSE,model.m);
            glUniform3f(ue_col,0.4f,0.8f,1.6f);
            glUniform1f(ue_pul,1.0f);
            glDrawArrays(GL_TRIANGLES,0,svcount);
        }
        {   /* exit portal — tall pillar (cube) */
            int ready=(g_collected==g_num_relics);
            glUniform1f(ue_alp,0.85f);
            glBindVertexArray(cvao);
            mat4 model=mat4_mul(mat4_translate(g_exit_pos),
                       mat4_mul(mat4_rotY((float)now*0.6f), mat4_scale(v3v(0.8f,WALL_H*0.95f,0.8f))));
            glUniformMatrix4fv(ue_mvp,1,GL_FALSE,mat4_mul(vp,model).m);
            glUniformMatrix4fv(ue_model,1,GL_FALSE,model.m);
            if(ready) glUniform3f(ue_col,0.2f,1.2f,0.4f);  /* unlocked: green */
            else      glUniform3f(ue_col,0.95f,0.97f,1.05f); /* locked: white (distinct from shrines) */
            glUniform1f(ue_pul,0.5f+0.5f*sinf((float)now*4.0f));
            glDrawArrays(GL_TRIANGLES,0,36);
        }
        /* healing shrines — shorter round translucent pink pillars (cylinder) */
        glUniform1f(ue_alp,0.5f);
        glBindVertexArray(cyvao);
        for(int i=0;i<g_num_shrines;i++){
            int active=(g_shrines[i].cd<=0);
            float H=WALL_H*0.55f;
            mat4 model=mat4_mul(mat4_translate(v3v(g_shrines[i].pos.x,H*0.5f,g_shrines[i].pos.z)),
                       mat4_mul(mat4_rotY((float)now*0.8f), mat4_scale(v3v(0.5f,H,0.5f))));
            glUniformMatrix4fv(ue_mvp,1,GL_FALSE,mat4_mul(vp,model).m);
            glUniformMatrix4fv(ue_model,1,GL_FALSE,model.m);
            if(active) glUniform3f(ue_col,1.2f,0.3f,0.55f); else glUniform3f(ue_col,0.22f,0.10f,0.16f);
            glUniform1f(ue_pul, active?0.5f+0.5f*sinf((float)now*3.0f+i):0.05f);
            glDrawArrays(GL_TRIANGLES,0,cycount);
        }
        glEnable(GL_CULL_FACE);

        /* relic pickup + exit check (not while paused / dead) */
        for(int i=0;i<g_num_relics && !g_paused && !g_dead;i++){
            if(g_relics[i].taken) continue;
            float dx=g_relics[i].pos.x-g_px, dz=g_relics[i].pos.z-g_pz;
            if(dx*dx+dz*dz<1.4f*1.4f){
                g_relics[i].taken=1; g_collected++; g_energy=ENERGY_MAX;
                particles_burst(g_relics[i].pos, v3v(0.35f,0.9f,1.4f), 30, 4.5f);
            }
        }
        if(g_collected==g_num_relics && !g_paused && !g_dead && !g_won){
            float dx=g_exit_pos.x-g_px, dz=g_exit_pos.z-g_pz;
            if(dx*dx+dz*dz<2.0f*2.0f){
                /* escaped -> show the level-cleared screen */
                g_won=1;
                particles_burst(v3v(g_exit_pos.x,1.2f,g_exit_pos.z), v3v(0.3f,1.4f,0.5f), 80, 7.0f);
                glfwSetInputMode(win,GLFW_CURSOR,GLFW_CURSOR_NORMAL);
            }
        }

        /* -------- particles (world-space, additive) -------- */
        if(!g_paused && !g_dead) particles_update(dt);
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
            /* basis whose local +Y (the cylinder's axis) points forward along the aim */
            mat4 rot=mat4_basis(rt,f,up);
            /* staff shaft (lit, fully opaque -> no ghost fade) */
            glUseProgram(prog_lit);
            upload_lights(ll,nl,lp,lc,lr,eye);
            glUniform1f(ul_fade,0.0f);
            glBindVertexArray(cyvao); glDisable(GL_CULL_FACE); /* cylindrical shaft */
            mat4 model=mat4_mul(mat4_translate(anchor),mat4_mul(rot,mat4_scale(v3v(0.05f,0.55f,0.05f))));
            mat4 mvp=mat4_mul(vp,model);
            glUniformMatrix4fv(ul_mvp,1,GL_FALSE,mvp.m);
            glUniformMatrix4fv(ul_model,1,GL_FALSE,model.m);
            float nm[9]={1,0,0,0,1,0,0,0,1};
            glUniformMatrix3fv(ul_nrm,1,GL_FALSE,nm);
            glUniform3f(ul_base,0.15f,0.13f,0.18f);
            glUniform3f(ul_emis,0.0f,0.0f,0.0f);
            glDrawArrays(GL_TRIANGLES,0,cycount);
            /* glowing round glassy tip */
            glUseProgram(prog_emit);
            glUniform3f(ue_view,eye.x,eye.y,eye.z);
            glUniform1f(ue_alp,0.6f);
            glBindVertexArray(svao);
            v3 tip=v3add(anchor,v3scale(f,0.30f));
            model=mat4_mul(mat4_translate(tip),mat4_scale(v3v(0.12f,0.12f,0.12f)));
            glUniformMatrix4fv(ue_mvp,1,GL_FALSE,mat4_mul(vp,model).m);
            glUniformMatrix4fv(ue_model,1,GL_FALSE,model.m);
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
        glUniform1f(uh_rot,0.0f);
        H_asp=(float)WH/(float)WW;

        if(g_hurt>0.01f){
            glUniform2f(uh_off,-1.0f,-1.0f); glUniform2f(uh_scl,2.0f,2.0f);
            glUniform3f(uh_col,0.7f,0.0f,0.0f); glUniform1f(uh_alp,g_hurt*0.45f);
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
        {   /* relics — gem icon + "collected/total" number, top-right corner */
            char rb[32]; snprintf(rb,sizeof rb,"%d/%d",g_collected,g_num_relics);
            float ph=0.026f, pw=0.014f*H_asp, cy=0.90f, endx=0.95f;
            float w=(float)strlen(rb)*3.6f*pw - 0.6f*pw;
            float startx=endx-w;
            hud_diamond(startx-0.05f, cy, 0.028f, 0.40f,0.95f,1.20f, 0.95f);
            hud_text(rb, startx, cy+2.5f*ph, pw, ph, 0.55f,0.92f,1.25f, 0.95f);
        }
        {   /* kills — a red wraith-eye gem plus a pixel-digit count, top-left */
            float cy=0.90f;
            hud_diamond(-0.93f, cy, 0.028f, 1.40f,0.25f,0.20f, 0.95f);
            float pw=0.014f*H_asp, ph=0.026f;
            hud_number(g_kills, -0.88f, cy+2.5f*ph, pw, ph, 1.35f,0.45f,0.35f, 0.95f);
        }
        /* crosshair (aspect-corrected) */
        glUniform1f(uh_rot,0.0f);
        glUniform2f(uh_off,0,0);
        glUniform2f(uh_scl,(float)WH/(float)WW,1.0f);
        glUniform3f(uh_col,0.9f,0.9f,0.9f); glUniform1f(uh_alp,0.85f);
        glBindVertexArray(hvao); glLineWidth(2.0f);
        glDrawArrays(GL_LINES,0,4);

        /* -------- pause / start menu -------- */
        if(g_paused){
            glBindVertexArray(qvao);
            hud_rect(-1.0f,-1.0f,2.0f,2.0f, 0.0f,0.0f,0.0f, 0.62f); /* dim overlay */
            hud_text_centered("KOS3D", 0.0f, 0.60f, 0.060f, 0.45f,0.80f,1.20f, 1.0f);

            float nx=(float)(g_cursor_x/(double)WW)*2.0-1.0;
            float ny=1.0-(float)(g_cursor_y/(double)WH)*2.0;
            const char *labels[3]={"START","RESTART","EXIT"};
            float cys[3]={0.24f,0.0f,-0.24f};
            float bw=0.30f, bh=0.08f;
            for(int i=0;i<3;i++){
                float cy=cys[i];
                int hover=(nx>-bw&&nx<bw&&ny>cy-bh&&ny<cy+bh);
                if(hover) hud_rect(-bw,cy-bh,2*bw,2*bh, 0.22f,0.42f,0.68f, 0.92f);
                else      hud_rect(-bw,cy-bh,2*bw,2*bh, 0.09f,0.11f,0.17f, 0.85f);
                hud_text_centered(labels[i], 0.0f, cy, 0.030f, 0.96f,0.98f,1.0f, 1.0f);
                if(hover && click){
                    if(i==0)      set_paused(win,0);                    /* START = continue */
                    else if(i==1){ g_want_regen=1; set_paused(win,0); } /* RESTART = new journey */
                    else           glfwSetWindowShouldClose(win,1);     /* EXIT = quit */
                }
            }
        }

        /* -------- death screen -------- */
        if(g_dead){
            glBindVertexArray(qvao);
            hud_rect(-1.0f,-1.0f,2.0f,2.0f, 0.35f,0.0f,0.0f, 0.55f); /* blood overlay */
            hud_text_centered("YOU DIED", 0.0f, 0.28f, 0.075f, 1.0f,0.20f,0.20f, 1.0f);
            hud_text_centered("KILLS", -0.14f, -0.02f, 0.030f, 0.9f,0.9f,0.9f, 1.0f);
            hud_number(g_kills, 0.08f, -0.02f+2.5f*0.030f, 0.030f*H_asp, 0.030f, 1.0f,0.9f,0.5f, 1.0f);
            hud_text_centered("CLICK TO RESTART", 0.0f, -0.30f, 0.028f, 0.85f,0.90f,1.0f, 1.0f);
            if(click){                       /* restart a fresh journey */
                g_want_regen=1; g_dead=0; set_paused(win,0);
            }
        }

        /* -------- level-cleared / victory screen -------- */
        if(g_won){
            /* rain celebration confetti around the player (rendered next frame) */
            for(int k=0;k<4;k++){
                v3 p=v3add(eye, v3v((frand()-0.5f)*4.0f, 2.0f+frand()*1.5f, (frand()-0.5f)*4.0f));
                v3 vel=v3v((frand()-0.5f)*1.5f, 0.5f+frand()*1.5f, (frand()-0.5f)*1.5f);
                v3 col=v3v(0.4f+frand()*0.9f, 0.4f+frand()*0.9f, 0.4f+frand()*0.9f);
                particles_spawn(p, vel, col, 1.6f, 6.0f);
            }
            glBindVertexArray(qvao);
            hud_rect(-1.0f,-1.0f,2.0f,2.0f, 0.0f,0.20f,0.05f, 0.55f); /* green overlay */
            hud_text_centered("ESCAPED", 0.0f, 0.42f, 0.065f, 0.4f,1.2f,0.5f, 1.0f);
            /* TIME <seconds> */
            hud_text_centered("TIME", -0.16f, 0.08f, 0.032f, 0.9f,0.95f,0.9f, 1.0f);
            hud_number((int)g_level_time, 0.06f, 0.08f+2.5f*0.032f, 0.032f*H_asp, 0.032f, 1.0f,1.0f,0.6f, 1.0f);
            /* KILLS <n> */
            hud_text_centered("KILLS", -0.16f, -0.06f, 0.032f, 0.9f,0.95f,0.9f, 1.0f);
            hud_number(g_kills, 0.06f, -0.06f+2.5f*0.032f, 0.032f*H_asp, 0.032f, 1.0f,0.7f,0.5f, 1.0f);
            hud_text_centered("CLICK TO RESTART", 0.0f, -0.34f, 0.028f, 0.85f,1.0f,0.9f, 1.0f);
            if(click){                       /* next journey */
                g_want_regen=1; g_won=0; set_paused(win,0);
            }
        }

        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(win);
        glfwPollEvents();

    }
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
