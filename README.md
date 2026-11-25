# RogueC

A roguelike in C: a procedurally generated dungeon ten levels deep (rooms and corridors at the top, caves below), ray-cast line of sight with fog of war, permadeath. Plays in the terminal, in a desktop window, and in the browser (WebAssembly).

![RogueC, 2022 version](gif/line-of-sight.gif)

*The 2022 version. Visible cells are now drawn bright and remembered cells dim.*

## Build and play

| Version | Command | Needs |
|---|---|---|
| Terminal | `make run` | C17 compiler, ncurses (`libncursesw` on Linux) |
| Desktop window | `make run-gui` | raylib 6.2 (`brew install raylib`) |
| Browser | `make serve`, then open http://localhost:8000 | Emscripten (`brew install emscripten`); the first build fetches and compiles raylib 6.2 |

```
./build/release/rogue 42       # play a specific seed (same seed, same dungeon)
./build/release/rogue-gui 42
make test                      # unit tests with AddressSanitizer + UndefinedBehaviorSanitizer
make help                      # all targets
```

Any terminal or window size works; the view follows the player. `make web` writes a static site to `build/www/` (index.html, .js, .wasm, .data) that any web server can host.

## Controls

| Action | Keys |
|---|---|
| Move | WASD, arrow keys, `hjkl`, numpad |
| Move diagonally | `y u b n`, numpad `7 9 1 3` |
| Wait a turn | `.`, numpad `5` |
| Stairs down / up | `>` / `<`, standing on them |
| Message history | `m`, Ctrl-P |
| Help | `?` |
| Quit | `q` |

On the help and history screens, any key goes back (in the history, the arrows, PgUp/PgDn and Home/End scroll).

## Layout

```
src/core/            game logic in pure C: no I/O, no globals, deterministic per seed
  rng                PCG32 random numbers
  map                tiles, visibility, explored memory
  gen_cave           cellular-automaton caves, trimmed to one connected region
  gen_rooms          rectangular rooms joined by L-shaped corridors
  level              one level: generator by depth, stairs placement
  path               distance maps (breadth-first search)
  line               Bresenham line walker
  fov                field of view by ray casting
  game               game state, actions, rules
  msglog             message log (ring buffer, repeats counted)
  render             game state -> grid of cells (glyph + style); the colour palette
  ui                 keymap and screens (map, help, message history), shared by all frontends
src/platform/term/   ncurses frontend
src/platform/raylib/ raylib frontend: desktop window and browser (Emscripten)
web/shell.html       page around the browser build
assets/fonts/        JetBrains Mono (SIL Open Font License, see OFL.txt)
tests/               unit tests of the core (no ncurses or raylib needed)
```

## How it works

- **Dungeon:** ten levels, each generated from the seed and its depth alone (its own random-number stream), all at the start, and kept as you left them. Depths 1–3 are rooms and corridors, then the chance of caves rises 25 % a level, so from depth 7 on every level is a cave. Depth 1 has no up stairs yet.
- **Rooms:** up to 12 rectangles placed at random without touching, sorted left to right and joined each to the next by an L-shaped corridor (so all are connected), plus a few extra corridors for loops.
- **Stairs:** the up stairs (on depth 1 the entrance) go on open floor, inside a room or cave rather than a corridor. A distance map from them gives every cell's walking distance, and the down stairs go at least half the longest walk away.
- **Caves:** random noise (15 % wall) smoothed by 5 steps of a cellular automaton (Game-of-Life-like birth/death rules). A flood fill keeps only the largest connected region, so every floor cell is reachable.
- **Field of view:** one Bresenham ray from the player to every cell on the square of radius 10, cut to a circle. A ray lights each cell it passes, including the first wall it hits, then stops. Casting to the whole perimeter (instead of at fixed angles) leaves no holes at any distance. A last pass lights walls whose ray was blocked by another wall although the floor in front of them is in view (about one wall in five otherwise stays dark).
- **Fog of war:** cells seen once stay on the map, drawn in cool blue; what is in view is drawn in warm light.
- **One palette, one keymap:** the core defines the colour of every style and handles keys and screens; the frontends only translate key codes and draw cells. The terminal uses the nearest xterm-256 colours (or 8 basic ones).

## Links

- [Generate Random Cave Levels Using Cellular Automata](https://gamedevelopment.tutsplus.com/tutorials/generate-random-cave-levels-using-cellular-automata--gamedev-9664)
- [Bresenham's line algorithm](https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm)
- [PCG random number generators](https://www.pcg-random.org/)
