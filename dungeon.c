/* dungeon.c - random rooms + corridors, and the queries the game runs on them */
#include "dungeon.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

unsigned char g_tiles[MAP_H][MAP_W];
Room g_rooms[MAX_ROOMS];
int  g_num_rooms;

int   irand(int lo, int hi){ return lo + rand() % (hi-lo+1); }
float frand(void){ return (float)rand()/(float)RAND_MAX; }

static void open_cell(int x, int y){
    if(x>0 && x<MAP_W-1 && y>0 && y<MAP_H-1) g_tiles[y][x]=1;
}
/* horizontal corridor, CORRIDOR_W cells thick */
static void carve_h(int x0, int x1, int y){
    if(x0>x1){ int t=x0; x0=x1; x1=t; }
    for(int x=x0;x<=x1;x++)
        for(int w=0;w<CORRIDOR_W;w++) open_cell(x,y+w);
}
/* vertical corridor, CORRIDOR_W cells thick */
static void carve_v(int y0, int y1, int x){
    if(y0>y1){ int t=y0; y0=y1; y1=t; }
    for(int y=y0;y<=y1;y++)
        for(int w=0;w<CORRIDOR_W;w++) open_cell(x+w,y);
}

void gen_dungeon(void){
    memset(g_tiles,0,sizeof(g_tiles));
    g_num_rooms=0;
    for(int attempt=0; attempt<200 && g_num_rooms<MAX_ROOMS; attempt++){
        int w = irand(4,9), h = irand(4,9);
        int x = irand(1, MAP_W-w-2);
        int y = irand(1, MAP_H-h-2);
        int overlap=0;
        for(int i=0;i<g_num_rooms;i++){
            Room r=g_rooms[i];
            if(x <= r.x+r.w && x+w >= r.x-1 && y <= r.y+r.h && y+h >= r.y-1){ overlap=1; break; }
        }
        if(overlap) continue;
        Room nr={x,y,w,h};
        g_rooms[g_num_rooms++]=nr;
        for(int yy=y; yy<y+h; yy++)
            for(int xx=x; xx<x+w; xx++)
                g_tiles[yy][xx]=1;
        if(g_num_rooms>1){
            Room p=g_rooms[g_num_rooms-2];
            int cx=x+w/2, cy=y+h/2, px=p.x+p.w/2, py=p.y+p.h/2;
            if(rand()&1){ carve_h(px,cx,py); carve_v(py,cy,cx); }
            else        { carve_v(py,cy,px); carve_h(px,cx,cy); }
        }
    }
}

int cell_open(int cx, int cy){
    if(cx<0||cy<0||cx>=MAP_W||cy>=MAP_H) return 0;
    return g_tiles[cy][cx];
}
int solid_at_world(float wx, float wz){
    return !cell_open((int)floorf(wx/CELL),(int)floorf(wz/CELL));
}
int blocked_r(float wx, float wz, float r){
    return solid_at_world(wx-r,wz-r) || solid_at_world(wx+r,wz-r) ||
           solid_at_world(wx-r,wz+r) || solid_at_world(wx+r,wz+r);
}
int los(v3 a, v3 b){
    v3 d=v3sub(b,a); float len=v3len(d); if(len<1e-4f) return 1;
    int steps=(int)(len/(CELL*0.4f))+1;
    for(int i=1;i<steps;i++){
        float t=(float)i/steps;
        if(solid_at_world(a.x+d.x*t, a.z+d.z*t)) return 0;
    }
    return 1;
}
v3 room_center_world(Room r){
    return v3v((r.x+r.w*0.5f)*CELL, WALL_H*0.5f, (r.y+r.h*0.5f)*CELL);
}
