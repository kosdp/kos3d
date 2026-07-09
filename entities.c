/* entities.c - world contents, player state, spawning, and world regeneration */
#include "entities.h"

/* world contents */
Relic   g_relics[MAX_ROOMS];
int     g_num_relics, g_collected;
Torch   g_torches[MAX_LIGHTS];
int     g_num_torches;
Monster g_mon[MAX_MONSTERS];
int     g_num_mon, g_kills;
Bolt    g_bolt[MAX_BOLTS];
Shrine  g_shrines[MAX_SHRINES];
int     g_num_shrines;
int     g_exit_room;
v3      g_exit_pos;

/* player / camera state */
float g_px, g_pz;
float g_yaw=-PI*0.5f, g_pitch=0;
float g_eye_y=WALL_H*0.55f;
float g_bob=0, g_hurt=0, g_recoil=0, g_muzzle=0, g_fire_cd=0, g_php=100.0f, g_energy=ENERGY_MAX;
int   g_won=0, g_dead_flash=0;

void place_props(void){
    g_num_relics=0; g_collected=0; g_num_torches=0; g_num_mon=0; g_kills=0; g_num_shrines=0;
    for(int i=0;i<MAX_BOLTS;i++) g_bolt[i].alive=0;

    /* exit portal: the room farthest from spawn (decided first so relic/shrine
       placement can avoid it) */
    v3 spawn=room_center_world(g_rooms[0]); float best=-1; g_exit_room=0;
    for(int i=1;i<g_num_rooms;i++){
        float d=v3len(v3sub(room_center_world(g_rooms[i]),spawn));
        if(d>best){ best=d; g_exit_room=i; }
    }
    g_exit_pos=room_center_world(g_rooms[g_exit_room]); g_exit_pos.y=WALL_H*0.5f;

    /* every non-spawn room holds EITHER a relic or a healing shrine (both at the
       room centre), never both. The exit room holds only the portal. Roughly
       every third room is a shrine. */
    for(int i=1;i<g_num_rooms;i++){
        if(i==g_exit_room) continue;             /* portal-only room */
        v3 c=room_center_world(g_rooms[i]);
        if((i%3)==0 && g_num_shrines<MAX_SHRINES){
            c.y=WALL_H*0.5f;
            g_shrines[g_num_shrines].pos=c; g_shrines[g_num_shrines].cd=0.0f;
            g_num_shrines++;
        }else{
            c.y=0.9f;
            g_relics[g_num_relics].pos=c; g_relics[g_num_relics].taken=0; g_num_relics++;
        }
    }
    /* warm torch lights near ceilings, index 0 reserved for the player torch */
    for(int i=0;i<g_num_rooms && g_num_torches<MAX_LIGHTS-1;i++){
        v3 c=room_center_world(g_rooms[i]); c.y=WALL_H*0.82f;
        g_torches[g_num_torches].pos=c;
        g_torches[g_num_torches].color=v3v(0.95f,0.55f,0.22f);
        g_num_torches++;
    }
    /* monsters: 1-3 per room (not the spawn room), on random open cells.
       tiers: normal (2 hits), big (3 hits), huge (4 hits) */
    for(int i=1;i<g_num_rooms && g_num_mon<MAX_MONSTERS;i++){
        Room r=g_rooms[i]; int n=irand(1,3);
        for(int k=0;k<n && g_num_mon<MAX_MONSTERS;k++){
            int cx=r.x+irand(0,r.w-1), cy=r.y+irand(0,r.h-1);
            Monster m; m.pos=v3v((cx+0.5f)*CELL,0.95f,(cy+0.5f)*CELL);
            int roll=irand(0,99);
            if(roll<60){ m.tier=0; m.hp=30.0f; m.scale=1.0f; }       /* 2 hits */
            else if(roll<88){ m.tier=1; m.hp=45.0f; m.scale=1.4f; }  /* 3 hits */
            else { m.tier=2; m.hp=60.0f; m.scale=1.8f; }             /* 4 hits */
            m.pos.y=0.75f+0.25f*m.scale; /* keep the base near the floor */
            m.maxhp=m.hp; m.alive=1; m.anim=frand()*6.28f; m.hurt=0; m.atk_cd=0; m.aggro=0;
            g_mon[g_num_mon++]=m;
        }
    }
}

void reset_player_to_spawn(void){
    v3 c=room_center_world(g_rooms[0]);
    g_px=c.x; g_pz=c.z; g_yaw=-PI*0.5f; g_pitch=0; g_won=0; g_php=100.0f; g_energy=ENERGY_MAX;
}

void new_dungeon(void){ gen_dungeon(); place_props(); reset_player_to_spawn(); }

void fire_bolt(v3 eye, v3 dir){
    /* scatter the aim inside a small cone so shots aren't pixel-perfect */
    v3 jit=v3v((frand()-0.5f)*2.0f*BOLT_SPREAD,
               (frand()-0.5f)*2.0f*BOLT_SPREAD,
               (frand()-0.5f)*2.0f*BOLT_SPREAD);
    v3 d=v3norm(v3add(dir,jit));
    for(int i=0;i<MAX_BOLTS;i++) if(!g_bolt[i].alive){
        g_bolt[i].pos=v3add(eye,v3scale(d,0.6f));
        g_bolt[i].vel=v3scale(d,BOLT_SPEED);
        g_bolt[i].life=1.6f; g_bolt[i].alive=1;
        break;
    }
    g_energy-=SHOT_COST; if(g_energy<0) g_energy=0;
    g_recoil=1.0f; g_muzzle=1.0f;
}
