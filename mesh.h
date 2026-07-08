/* mesh.h - static geometry: world triangles + a unit cube */
#ifndef MESH_H
#define MESH_H

/* unit cube, positions only (36 verts x 3 floats) */
extern const float CUBE[108];

/* Build the dungeon's floor/ceiling/wall triangles into a freshly malloc'd
 * interleaved buffer: pos(3) normal(3) uv(2) type(1) = 9 floats per vertex.
 * Caller owns *out and must free() it. Returns the vertex count. */
int build_world_geometry(float **out);

/* Build a unit-radius UV sphere (positions only, radius 0.5 to match the cube's
 * half-extent). Caller owns *out and must free() it. Returns the vertex count.
 * For a sphere centered at the origin the position doubles as the normal. */
int make_sphere(float **out);

#endif /* MESH_H */
