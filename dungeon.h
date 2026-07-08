/* dungeon.h - procedural map generation, tile queries, collision & LOS */
#ifndef DUNGEON_H
#define DUNGEON_H

#include "math3d.h"

#define MAP_W       48
#define MAP_H       48
#define MAX_ROOMS   16
#define CELL        2.6f   /* world size of one grid cell   */
#define WALL_H      4.3f   /* wall / ceiling height (~4 brick rows) */
#define CORRIDOR_W  2      /* corridor width in cells       */

typedef struct { int x, y, w, h; } Room;

extern unsigned char g_tiles[MAP_H][MAP_W]; /* 1 = open floor, 0 = solid */
extern Room g_rooms[MAX_ROOMS];
extern int  g_num_rooms;

/* small rng helpers shared across the game */
int   irand(int lo, int hi);   /* inclusive */
float frand(void);             /* [0,1) */

void  gen_dungeon(void);
int   cell_open(int cx, int cy);
int   solid_at_world(float wx, float wz);
int   blocked_r(float wx, float wz, float r);  /* bounding-box vs solid cells */
int   los(v3 a, v3 b);                          /* line-of-sight, sampled */
v3    room_center_world(Room r);

#endif /* DUNGEON_H */
