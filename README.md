# RogueC

![RogueC](gif/line-of-sight.gif)

A roguelike in C. C practice project. The map generation algorithm used is based on Conway's Game of Life automaton. Field of view implementation relies on ray casting procedure using Bresenham's line algorithm to detect visible ground tiles.

Movement with WASD.

## Build

```
make          # release build: build/release/rogue
make run      # build and play
make test     # unit tests with AddressSanitizer + UBSan
make help     # all targets
```

Needs a C17 compiler and ncurses (`libncursesw` on Linux).

## Tips

### Printing UTF-8 in C

You need to initialize the locale's encoding by calling `setlocale` at your program's start, like so:

```
    setlocale(LC_CTYPE, "")
```

This is normally carried out for you if you use some initialization function.

## Links

[ASCII Table](https://theasciicode.com.ar/extended-ascii-code/block-graphic-character-ascii-code-219.html)

[Generate Random Cave Levels Using Cellular Automata](https://gamedevelopment.tutsplus.com/tutorials/generate-random-cave-levels-using-cellular-automata--gamedev-9664)

[Bresenham's Line Algorithm in Eight Directions](http://tech-algorithm.com/articles/drawing-line-using-bresenham-algorithm/)
