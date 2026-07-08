/* entities.h - game entities, player state, and world (re)generation */
#ifndef ENTITIES_H
#define ENTITIES_H

#include "math3d.h"
#include "dungeon.h"

#define MAX_LIGHTS   24
#define MAX_MONSTERS 48
#define MAX_BOLTS    48
#define PLAYER_R     0.42f   /* collision radius in cells */
#define MON_R        0.34f
#define ENERGY_MAX   100.0f  /* staff energy pool          */
#define SHOT_COST    15.0f   /* energy drained per bolt     */
#define ENERGY_REGEN 6.0f    /* slow passive recharge /sec  */
#define MAX_SHRINES  10
#define SHRINE_HEAL  45.0f   /* health restored per use     */
#define SHRINE_CD    14.0f   /* seconds until it recharges  */

typedef struct { v3 pos; int taken; } Relic;
typedef struct { v3 pos; v3 color; } Torch;
/* tier 0 = normal (2 eyes, 2 hits), 1 = big (3 eyes in a line, 3 hits),
   2 = huge (4 eyes in a square, 4 hits) */
typedef struct { v3 pos; float hp, maxhp, anim, hurt, atk_cd, scale; int alive, tier; } Monster;
typedef struct { v3 pos, vel; float life; int alive; } Bolt;
typedef struct { v3 pos; float cd; } Shrine; /* healing pillar; cd>0 = recharging */

/* world contents */
extern Relic   g_relics[MAX_ROOMS];
extern int     g_num_relics, g_collected;
extern Torch   g_torches[MAX_LIGHTS];
extern int     g_num_torches;
extern Monster g_mon[MAX_MONSTERS];
extern int     g_num_mon, g_kills;
extern Bolt    g_bolt[MAX_BOLTS];
extern Shrine  g_shrines[MAX_SHRINES];
extern int     g_num_shrines;
extern int     g_exit_room;
extern v3      g_exit_pos;

/* player / camera state */
extern float g_px, g_pz, g_yaw, g_pitch, g_eye_y;
extern float g_bob, g_hurt, g_recoil, g_muzzle, g_fire_cd, g_php, g_energy;
extern int   g_won, g_dead_flash;

void place_props(void);
void reset_player_to_spawn(void);
void new_dungeon(void);
void fire_bolt(v3 eye, v3 dir);

#endif /* ENTITIES_H */
