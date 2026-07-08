# kos3d — procedural 3D FPS dungeon crawler

A first-person dungeon crawler in modern OpenGL (3.3 core). Every run carves a
fresh maze of rooms and corridors patrolled by glowing-eyed wraiths. Blast them
with your bolt-staff, collect all the relics, then step into the green exit
portal to escape.

No asset files — geometry, bump-mapped brick/stone materials, torch lighting,
monsters, particles and the dungeon layout are all generated procedurally.
Rendering uses per-fragment Phong (diffuse + specular), fbm-noise materials with
bump mapping, additive glow particles, HDR tonemapping, gamma correction and
distance fog.

## Source layout

The game is split into small modules:

| File          | Responsibility                                          |
|---------------|---------------------------------------------------------|
| `math3d.h`    | vector / matrix math (header-only, `static inline`)     |
| `dungeon.*`   | map generation, tile queries, collision & line-of-sight |
| `mesh.*`      | world triangles, unit cube, UV sphere                   |
| `shader.*`    | GLSL sources + compile/link + light-uniform plumbing    |
| `entities.*`  | relics, torches, monsters, bolts, energy, player state  |
| `particles.*` | additive point-sprite particle pool                     |
| `main.c`      | window, input, and the render / update loop             |

## Build & run

Dependencies: `glfw3`, `glew`, OpenGL (already present on this machine).

```sh
make
make run      # or ./kos3d
```

## Controls

| Input            | Action                         |
|------------------|--------------------------------|
| Mouse            | Look around                    |
| Left Mouse       | Shoot a magic bolt             |
| W A S D / arrows | Move                           |
| Left Shift       | Sprint                         |
| R                | Regenerate a brand-new dungeon |
| Esc              | Quit                           |

The HUD shows a red **health** bar and a blue **energy** bar (bottom-left) plus a
crosshair; a red vignette flashes when a wraith hits you. Each shot drains
energy — it recharges slowly on its own, and a **relic pickup refills it fully**,
so the blue orbs double as your ammo supply. Relic count, HP, energy, kills and
FPS are shown in the window title bar. Take too much damage and you respawn at
the entrance.

## How it works

- **Dungeon** — random non-overlapping rooms connected by L-shaped corridors on a
  48×48 grid (`gen_dungeon`).
- **Geometry** — each open cell emits a floor + ceiling quad; walls are emitted
  only on edges facing solid cells (`build_world_geometry`), packed into one VBO.
- **Materials** — walls/floor/ceiling use fbm value-noise for brick, flagstone and
  rough-stone patterns computed per-fragment, with **bump mapping** (normals
  perturbed from a per-material height field) and per-material specular.
- **Lighting** — a flickering torch follows the player, warm static torches sit in
  nearby rooms, and every magic bolt and muzzle flash is a real dynamic light.
  Phong diffuse + specular, HDR tonemapping, gamma correction and distance fog.
- **Monsters** — wraiths spawn across the rooms, chase you on line-of-sight, and melee
  you on contact. Lit bodies with glowing round eyes that track the player; they
  flash white when hit and burst into embers when killed.
- **Combat & energy** — left-click fires a bolt (`fire_bolt`); bolts leave a glowing
  trail, collide with walls/monsters, and light the room as they fly. Firing drains
  the energy bar; relics refill it.
- **Round props** — relics, bolts, staff tip and monster eyes are real UV spheres
  (`make_sphere`) shaded to read as glowing orbs; the exit pillar turns green once
  every relic is collected.
- **Particles** — an additive point-sprite pool (`particles.*`): torch embers,
  orbiting relic sparkles, bolt trails, muzzle sparks, and impact/death bursts.
- **Movement** — per-axis bounding-box collision so you slide along walls, with subtle
  head-bob while walking.
- **HUD** — health bar, energy bar, damage vignette, win tint and crosshair, all drawn
  as screen-space quads/lines.
