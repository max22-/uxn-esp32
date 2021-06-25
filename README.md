# Uxn

An assembler and emulator for the [Uxn stack-machine](https://wiki.xxiivv.com/site/uxn.html), written in ANSI C. 

## Build

### Linux 

To build the Uxn emulator, you must have [SDL2](https://wiki.libsdl.org/).

```sh
./build.sh 
	--debug # Add debug flags to compiler
```

### Plan 9 

To build the Uxn emulator on [9front](http://9front.org/), via [npe](https://git.sr.ht/~ft/npe):

```rc
mk
```

If the build fails on 9front because of missing headers or functions, try again after `rm -r /sys/include/npe`.

## Getting Started

Begin by building the assembler and emulator by running the build script. The assembler(`uxnasm`) and emulator(`uxnemu`) are created in the `/bin` folder.

```
./build.sh
```

The following command will create an Uxn-compatible rom from an [uxntal file](https://wiki.xxiivv.com/site/uxntal.html), point to a different .tal file in `/projects` to assemble a different rom. 

```
bin/uxnasm projects/examples/demos/life.tal bin/life.rom
```

To start the rom, point the emulator to the newly created rom:

```
bin/uxnemu bin/life.rom
```

You can also use the emulator without graphics by using `uxncli`. You can find additional roms [here](https://sr.ht/~rabbits/uxn/sources).

## Emulator Controls

- `F1` toggle zoom
- `F2` toggle debug
- `F3` capture screen

## Need a hand?

Find us in `#uxn`, on irc.esper.net
