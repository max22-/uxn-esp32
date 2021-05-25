# Uxn

An [8-bit stack-based computer](https://wiki.xxiivv.com/site/uxn.html), written in ANSI C. 

## Build

### Linux 

To build the Uxn emulator, you must have [SDL2](https://wiki.libsdl.org/).

```sh
./build.sh 
	--debug # Add debug flags to compiler
	--cli # Run rom without graphics
```

### Plan 9 

To build the Uxn emulator on [9front](http://9front.org/), via [npe](https://git.sr.ht/~ft/npe):

```rc
mk
```

If the build fails on 9front because of missing headers or functions,
try again after `rm -r /sys/include/npe`.

## Getting Started

Begin by building the assembler and emulator by running the build script. The assembler(`uxnasm`) and emulator(`uxnemu`) are created in the `bin` folder.

```
./build.sh
```

This example will create the `life.rom` from the `life.usm` uxambly file, point to a different usm file to assemble a different rom. You can find additional roms [here](https://sr.ht/~rabbits/uxn/sources). To create a rom, from a [usm file](https://wiki.xxiivv.com/site/uxambly.html), use the following command:

```
bin/uxnasm projects/demos/life.usm bin/life.rom
```

To launch the rom:

```
bin/uxnemu bin/life.rom
```

## Emulator Controls

- `ctrl+h` toggle debugger
- `alt+h` toggle zoom

## Need a hand?

Find us in `#uxn`, on irc.esper.net
