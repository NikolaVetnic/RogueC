# RogueC

A roguelike in C: procedurally generated caves, ray-cast line of sight with fog of war, permadeath. C practice project, being completed into a full game that also runs in the browser.

![RogueC, 2022 version](gif/line-of-sight.gif)

*The 2022 version. Visible cells are now drawn bright and remembered cells dim.*

## Build and play

```
make          # release build: build/release/rogue
make run      # build and play
./build/release/rogue 42    # play a specific seed (same seed, same cave)
make test     # unit tests with AddressSanitizer + UndefinedBehaviorSanitizer
make help     # all targets
```

Needs a C17 compiler and ncurses (`libncursesw` on Linux). Any terminal size works; the view follows the player.

## Controls

| Action | Keys |
|---|---|
| Move | WASD, arrow keys, `hjkl`, numpad |
| Move diagonally | `y u b n`, numpad `7 9 1 3` |
| Wait a turn | `.`, numpad `5` |
| Quit | `q` |

## Layout

```
src/core/            game logic in pure C: no I/O, no globals, deterministic per seed
  rng                PCG32 random numbers
  map                tiles, visibility, explored memory
  gen_cave           cellular-automaton caves, trimmed to one connected region
  line               Bresenham line walker
  fov                field of view by ray casting
  game               game state, actions, rules
  render             game state -> grid of cells (glyph + style) for any frontend
src/platform/term/   ncurses frontend
tests/               unit tests of the core (no ncurses needed)
```

## How it works

- **Caves:** random noise (15 % wall) smoothed by 5 steps of a cellular automaton (Game-of-Life-like birth/death rules). A flood fill keeps only the largest connected region, so every floor cell is reachable.
- **Field of view:** one Bresenham ray from the player to every cell on the square of radius 10, cut to a circle. A ray lights each cell it passes, including the first wall it hits, then stops. Casting to the whole perimeter (instead of at fixed angles) leaves no holes at any distance.
- **Fog of war:** cells seen once stay on the map, drawn dim.

## Links

- [Generate Random Cave Levels Using Cellular Automata](https://gamedevelopment.tutsplus.com/tutorials/generate-random-cave-levels-using-cellular-automata--gamedev-9664)
- [Bresenham's line algorithm](https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm)
- [PCG random number generators](https://www.pcg-random.org/)
