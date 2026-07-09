# kos3d — procedural 3D FPS dungeon crawler

A first-person dungeon crawler in modern OpenGL (3.3 core). Every run carves a
fresh maze of rooms and corridors haunted by translucent wraiths. Blast them with
your bolt-staff, gather every relic, then step into the exit portal — once it turns
green — to escape and be dropped into a brand-new dungeon.

No asset files — geometry, bump-mapped brick/stone materials, torch lighting,
monsters, particles, the pixel-font HUD and the dungeon layout are all generated
procedurally. Rendering uses per-fragment Phong (diffuse + specular), fbm-noise
materials with bump mapping, additive glow particles, translucent fresnel-lit
orbs, HDR tonemapping, gamma correction and distance fog. It launches **fullscreen
at your monitor's native resolution**.

## Source layout

The game is split into small modules:

| File          | Responsibility                                             |
|---------------|------------------------------------------------------------|
| `math3d.h`    | vector / matrix math (header-only, `static inline`)        |
| `dungeon.*`   | map generation, tile queries, collision & line-of-sight    |
| `mesh.*`      | world triangles, unit cube, UV sphere, cylinder            |
| `shader.*`    | GLSL sources + compile/link + light-uniform plumbing       |
| `entities.*`  | relics, torches, wraiths, bolts, shrines, energy, player   |
| `particles.*` | additive point-sprite particle pool                        |
| `main.c`      | window, input, HUD/menus, and the render / update loop     |

## Build & run

Dependencies: `glfw3`, `glew`, OpenGL.

```sh
make            # normal dynamic build -> ./kos3d
make run        # build and launch
make static     # best-effort static build -> ./kos3d-static
```

`make static` statically links everything Ubuntu ships as a `.a` (GLEW + the
X11/xcb chain). GLFW and the GL driver stay dynamic on purpose — there is no
static GLFW package, and the GPU driver is loaded at runtime, so a fully static
OpenGL binary isn't possible on Linux.

## Controls

| Input            | Action                                    |
|------------------|-------------------------------------------|
| Mouse            | Look around                               |
| Left Mouse       | Shoot a bolt (also selects menu entries)  |
| W A S D / arrows | Move                                      |
| Left Shift       | Sprint (drains energy)                    |
| Esc              | Open / close the pause menu               |

The game boots into a **menu** (paused). Entries are chosen with the mouse:

- **START** — begin / resume the current run
- **RESTART** — carve a fresh dungeon immediately
- **EXIT** — quit

## HUD & screens

- **Health bar** (red) and **energy bar** (blue) sit bottom-left; a red vignette
  flashes when a wraith hits you.
- **Relic counter** — a cyan gem plus a `collected/total` number, top-right.
- **Kill counter** — a red wraith-eye gem plus a number, top-left.
- Numbers use a tiny built-in 3×5 pixel font (no font files). The window title is
  just the game name.
- **Death screen** — losing all health freezes the run and shows *YOU DIED* with
  your kill count; click to restart a fresh journey.
- **Victory screen** — reaching the green portal shows *ESCAPED* with your run
  **time** and **kills** amid a shower of celebration confetti; click to continue
  into the next dungeon.

## How it works

- **Dungeon** — random non-overlapping rooms joined by 2-wide L-shaped corridors
  on a 48×48 grid (`gen_dungeon`). Each non-spawn room holds either one relic or
  one healing shrine; the farthest room holds only the exit portal.
- **Geometry** — each open cell emits a floor + ceiling quad; walls are emitted
  only on edges facing solid cells (`build_world_geometry`), packed into one VBO.
- **Materials** — walls/floor/ceiling use fbm value-noise for brick, flagstone and
  rough-stone patterns computed per-fragment, with **bump mapping** (normals
  perturbed from a per-material height field) and per-material specular.
- **Lighting** — a steady warm torch follows the player, flickering torches sit in
  nearby rooms, and every bolt, muzzle flash and active shrine is a real dynamic
  light. Phong diffuse + specular, HDR tonemapping, gamma correction, distance fog.
- **Wraiths** — capsule ghosts (cylinder body + hemisphere top) rendered
  translucent with a dissolving, wispy bottom edge. Three tiers signalled by eye
  layout: normal (2 eyes, 2 hits), big (3 eyes in a line, 3 hits), huge (4 eyes in
  a square, 4 hits). They chase on line-of-sight, **hunt you from any distance once
  shot**, push apart from each other so they never merge, and melee on contact.
- **Combat** — left-click fires a bolt (`fire_bolt`) with **bullet drop** (gravity)
  and a small random **spread cone**, so aiming at range takes skill. Bolts leave a
  glowing trail, light the room, and burst on walls, floor, ceiling or monsters.
- **Energy** — a shared resource drained by **both shooting and sprinting**. It
  recharges slowly; a **relic pickup refills it fully**. Emptying it locks
  sprinting until it recovers past one shot's worth (a stamina gate).
- **Healing shrines** — round pink pillars in some rooms restore health on contact,
  then recharge on a cooldown so you can revisit them.
- **Round glassy props** — relics, bolts, staff orb and shrines are real spheres /
  cylinders shaded as **translucent fresnel-lit orbs** (bright opaque rim, see-through
  core). The exit portal is a white pillar that turns green once every relic is
  collected.
- **Particles** — an additive point-sprite pool (`particles.*`): torch embers,
  orbiting relic sparkles, bolt trails, muzzle sparks, impact/death bursts, shrine
  motes, and victory confetti.
- **Movement** — per-axis bounding-box collision so you slide along walls, with
  subtle head-bob while walking.
