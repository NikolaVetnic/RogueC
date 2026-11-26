# RogueC

A roguelike in C. Go down ten levels, find the Sunstone and carry it back up to the daylight. A procedurally generated dungeon (rooms and corridors at the top, caves below), monsters that hunt you, unidentified potions and scrolls, weapons, armour and hunger, ray-cast line of sight with fog of war, permadeath, and a choice of turn-based or real-time play. Plays in the terminal, in a desktop window, and in the browser (WebAssembly).

**Play in the browser: https://nikolavetnic.github.io/RogueC/**

![RogueC, 2022 version](gif/line-of-sight.gif)

*The 2022 version. Visible cells are now drawn bright and remembered cells dim.*

## Build and play

| Version | Command | Needs |
|---|---|---|
| Terminal | `make run` | C17 compiler, ncurses (`libncursesw` on Linux) |
| Desktop window | `make run-gui` | raylib 6.2 (`brew install raylib`) |
| Browser | `make serve`, then open http://localhost:8000 | Emscripten (`brew install emscripten`); the first build fetches and compiles raylib 6.2 |

```
./build/release/rogue          # resume the saved game, or start a new one
./build/release/rogue 42       # a new game from a specific seed (same seed, same dungeon)
./build/release/rogue-gui 42
make test                      # unit tests with AddressSanitizer + UndefinedBehaviorSanitizer
make help                      # all targets
```

Any terminal or window size works; the view follows the player. `make web` writes a static site to `build/www/` (index.html, .js, .wasm, .data) that any web server can host. `make pages` commits that site to the `gh-pages` branch; `git push origin gh-pages` deploys it to GitHub Pages.

## Controls

| Action | Keys |
|---|---|
| Move | WASD, arrow keys, `hjkl`, numpad |
| Move diagonally | `y u b n`, numpad `7 9 1 3` |
| Run | Shift + a move key or arrow (`W A S D`, `H J K L`, `Y U B N`) |
| Attack | move into a monster |
| Wait a turn | `.`, numpad `5` |
| Rest until healed | `Z` |
| Pick up | `,` or `g` (gold is picked up by walking over it) |
| Use an item (eat, drink, read, wield, wear, take off) | `i`, then its letter |
| Drop an item | `x`, then its letter |
| Stairs down / up | `>` / `<`, standing on them (`<` on depth 1 is the way out) |
| Message history | `m`, Ctrl-P |
| Real time on / off | `T` |
| Options | `o` |
| Help | `?` |
| Save and quit | `q` (in the browser: save; closing the tab saves too) |
| Give up the game (no save, no score) | `Q`, then `y` |

The game saves itself on every new depth and when the window or tab closes (or the terminal is closed, or Ctrl-C), and starting again resumes it. There is one save; dying or winning deletes it. A run stops when a monster comes into view, something happens, or at a fork, doorway or item; in real time it takes one step per turn and any key stops it.

The options (`o`) are kept for later games: start new games in real time, real-time speed (a turn every 450, 300 or 200 ms), 2–4 message lines above the map, and in the terminal the colour mode (what the terminal reports, 256 colours, 8 colours or monochrome).

When the game ends, the end screen shows your score and the high scores; Enter starts a new game and `q` quits. On the help and history screens, any key goes back (in the history, the arrows, PgUp/PgDn and Home/End scroll).

## Layout

```
src/core/            game logic in pure C: no I/O, no globals, deterministic per seed
  rng                PCG32 random numbers
  map                tiles, visibility, explored memory
  gen_cave           cellular-automaton caves, trimmed to one connected region
  gen_rooms          rectangular rooms joined by L-shaped corridors
  level              one level: generator by depth, stairs placement
  path               distance maps (breadth-first search)
  combat             stats, hit chance, damage
  monster            the monster table and monster instances
  ai                 what a monster does on its turn
  item               item kinds, unidentified looks and titles, names
  inventory          the pack, picking up, dropping, and what items do
  score              score, high-score table, its text form
  save               saved games: the whole game as checked base64 text
  settings           the options and their text form
  line               Bresenham line walker
  fov                field of view by ray casting
  game               game state, actions, rules
  msglog             message log (ring buffer, repeats counted)
  render             game state -> grid of cells (glyph + style); the colour palette
  ui                 keymap and screens (map, help, history, inventory, options, end), shared by all frontends
src/platform/term/   ncurses frontend
src/platform/raylib/ raylib frontend: desktop window and browser (Emscripten)
src/platform/common/ shared by the frontends: storing scores, options and the save (files or localStorage)
web/shell.html       page around the browser build
assets/fonts/        JetBrains Mono (SIL Open Font License, see OFL.txt)
tests/               unit tests of the core (no ncurses or raylib needed)
```

## How it works

- **Dungeon:** ten levels, each generated from the seed and its depth alone (its own random-number stream), all at the start, and kept as you left them. Depths 1–3 are rooms and corridors, then the chance of caves rises 25 % a level, so from depth 7 on every level is a cave.
- **Rooms:** up to 12 rectangles placed at random without touching, sorted left to right and joined each to the next by an L-shaped corridor (so all are connected), plus a few extra corridors for loops.
- **Stairs:** the up stairs (on depth 1 the entrance) go on open floor, inside a room or cave rather than a corridor. A distance map from them gives every cell's walking distance, and the down stairs go at least half the longest walk away.
- **Monsters:** ten kinds, from rats and bats near the top to trolls and wraiths at the bottom; each level starts with 3 + depth of them, away from the stairs you arrive by, and more wander in over time. They sleep until they see you, then chase you along a distance map from you (recomputed every tick), keep hunting for 20 moves after losing sight of you, and attack when next to you. Bats flutter at random.
- **Combat:** chance to hit = 60 % + 5 % × (attack − defence), between 5 and 95 %; damage is a roll in the attacker's range. You heal 1 HP every 10 turns.
- **Goal:** the Sunstone (`*`) lies on depth 10, far from the stairs. Picking it up stirs the dungeon: monsters wander in three times as often. Climb back to depth 1 and take its `<` to win.
- **Experience:** kills give experience (a rat 1, a troll 40). Each of 12 levels adds 6 max HP and +1 to hit, every second level +1 damage, and healing gets faster.
- **Score:** gold + 10 × experience + 100 × deepest depth, + 2000 for a win. The ten best are kept in `~/.roguec_scores` (terminal and desktop) or the browser's localStorage.
- **Items:** gold, food rations, six potions (healing, extra healing, poison, speed, confusion, blindness; healing is a third of all potions, and poison hurts but always leaves you 1 HP), six scrolls (identify, magic mapping, teleportation, enchant weapon, enchant armour, aggravate monsters), five weapons and five armours, better ones deeper. Potions look like a colour and scrolls carry a made-up title, shuffled every game; using one tells you what it was, and a scroll of identify names everything you carry. A weapon's or armour's bonus shows once you wield or wear it. You start with a short sword, leather armour and a ration.
- **Hunger:** you get hungrier every turn: Hungry, then Weak (no healing), then starving (losing HP). A ration feeds you for 900 turns.
- **Time:** an energy scheduler. Every tick each creature gains its speed in energy (100 = normal) and acts on reaching 100, so a speed-150 bat acts three times in two ticks. Turn-based, ticks run until it's your move again; in real time, one tick runs every 300 ms, and a key pressed before you're ready is kept until you are. Help and history pause real time.
- **Caves:** random noise (15 % wall) smoothed by 5 steps of a cellular automaton (Game-of-Life-like birth/death rules). A flood fill keeps only the largest connected region, so every floor cell is reachable.
- **Field of view:** one Bresenham ray from the player to every cell on the square of radius 10, cut to a circle. A ray lights each cell it passes, including the first wall it hits, then stops. Casting to the whole perimeter (instead of at fixed angles) leaves no holes at any distance. A last pass lights walls whose ray was blocked by another wall although the floor in front of them is in view (about one wall in five otherwise stays dark).
- **Saving:** every field of the game (all ten levels, monsters, items, the pack, what's identified, the message log, the random-number state) is written in a fixed order as little-endian integers, followed by a checksum, and stored as base64 text: about 70 KB. Loading checks the checksum and every count, index and position, so a damaged or edited save is refused rather than read out of bounds. A loaded game continues exactly as it would have.
- **Running:** in a corridor the run turns with it while there is exactly one way on; in the open it goes straight and stops where the walls beside it change (a doorway) or between corridor and room.
- **Fog of war:** cells seen once stay on the map, drawn in cool blue; what is in view is drawn in warm light.
- **One palette, one keymap:** the core defines the colour of every style and handles keys and screens; the frontends only translate key codes and draw cells. The terminal uses the nearest xterm-256 colours (or 8 basic ones).

## Links

- [Generate Random Cave Levels Using Cellular Automata](https://gamedevelopment.tutsplus.com/tutorials/generate-random-cave-levels-using-cellular-automata--gamedev-9664)
- [Bresenham's line algorithm](https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm)
- [PCG random number generators](https://www.pcg-random.org/)
