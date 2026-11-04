# libautuni

libautuni is the drawing code shared by the programs of the [AutumnBSD](https://github.com/Serusion/AutumnBSD) desktop: the login screen of [Akture](https://github.com/Serusion/Akture), and the panel and launcher to come. It draws with pixman into images in memory, shapes with antialiased edges and text with FreeType, and puts the result on an X window through MIT-SHM. It was split off from the greeter of Akture.

## Status

Early development. The interface may still change; it is a static library, so programs pick up a new version when they are built again.

## Contents

- Shapes: filled and outlined rounded rectangles, circles and rings, lines with round ends, the arc of a power symbol, solid fills and opaque images, all drawn through a canvas clipped to the damage
- Text: font files opened by path with FreeType, variable fonts drawn at any weight, and a fallback face for missing glyphs
- A frame buffer for a window in MIT-SHM shared memory, of which only the damaged parts are sent
- Backgrounds in the `.akbg` format, mapped from the file as they are
- A theme: the background, accent and text colours and the scale of a user interface
- Parsers of `key = value` configuration lines, colours, booleans and scales

The interface is [autuni.h](autuni.h), where each function has a line on what it does. The `.akbg` format is in [akbg.h](akbg.h) and the parsers in [akparse.h](akparse.h); they keep the names they had in Akture, whose master uses them.

## Requirements

- BSD make (NetBSD or FreeBSD; FreeBSD is untested) and pkg-config
- pixman, FreeType and libxcb with the shm extension

## Building and installing

```sh
make
make install
```

This installs `libautuni.a` to `/usr/local/lib`, the headers to `/usr/local/include/autuni` and `autuni.pc` to `/usr/local/lib/pkgconfig`. `AUTUNI_PREFIX` sets another prefix:

```sh
make AUTUNI_PREFIX=/usr/pkg install
```

On AutumnBSD, install the package `libautuni` of [Serusion/autumn-pkgsrc](https://github.com/Serusion/autumn-pkgsrc) instead.

## Using it

```sh
cc -o prog prog.c $(pkg-config --cflags --libs autuni)
```

```c
#include "autuni.h"
```

A program that keeps its own copy of the library builds it without installing it: [Akture](https://github.com/Serusion/Akture) has this repository as it is in `libautuni/`, and sets `AUTUNI_INTERNAL` in its `Makefile.inc`.

## Tests

The atf-c tests in `tests/` need the library built first:

```sh
make
cd tests && make && kyua test
```

The test of text draws with the font named by `AUTUNI_TEST_FONT`, and is skipped without it.

## License

BSD-2-Clause; see [LICENSE](LICENSE).
