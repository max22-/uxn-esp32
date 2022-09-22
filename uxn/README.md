# Uxn

An assembler and emulator for the [Uxn stack-machine](https://wiki.xxiivv.com/site/uxn.html), written in ANSI C. 

## Download binaries

Binaries are available for 64-bit x86 computers running [Linux](https://rabbits.srht.site/uxn/uxn-essentials-lin64.tar.gz), [Windows](https://rabbits.srht.site/uxn/uxn-essentials-win64.zip) and [macOS](https://rabbits.srht.site/uxn/uxn-essentials-mac64.tar.gz).

## Build

### Linux/OS X

To build the Uxn emulator, you must install [SDL2](https://wiki.libsdl.org/) for your distro. If you are using a package manager:

```sh
sudo pacman -Sy sdl2             # Arch
sudo apt install libsdl2-dev     # Ubuntu
sudo xbps-install SDL2-devel     # Void Linux
brew install sdl2                # OS X
```

Build the assembler and emulator by running the `build.sh` script. The assembler(`uxnasm`) and emulator(`uxnemu`) are created in the `./bin` folder.

```sh
./build.sh 
	--debug # Add debug flags to compiler
	--format # Format source code
	--install # Copy to ~/bin
```

If you wish to build the emulator without graphics mode:

```sh
cc src/devices/datetime.c src/devices/system.c src/devices/file.c src/uxn.c -DNDEBUG -Os -g0 -s src/uxncli.c -o bin/uxncli
```

### Plan 9 

To build the Uxn emulator on [9front](http://9front.org/), via [npe](https://git.sr.ht/~ft/npe):

```rc
mk
```

If the build fails on 9front because of missing headers or functions, try again after `rm -r /sys/include/npe`.

### Windows

Uxn can be built on Windows with [MSYS2](https://www.msys2.org/). Install by downloading from their website or with Chocolatey with `choco install msys2`. In the MSYS shell, type:

```sh
pacman -S git mingw-w64-x86_64-gcc mingw64/mingw-w64-x86_64-SDL2
export PATH="${PATH}:/mingw64/bin"
git clone https://git.sr.ht/~rabbits/uxn
cd uxn
./build.sh
```

If you'd like to work with the Console device in `uxnemu.exe`, run `./build.sh --console` instead: this will bring up an extra window for console I/O unless you run `uxnemu.exe` in Command Prompt or PowerShell.

## Getting Started

### Emulator

To launch a `.rom` in the emulator, point the emulator to the target rom file:

```sh
bin/uxnemu bin/piano.rom
```

You can also use the emulator without graphics by using `uxncli`. You can find additional roms [here](https://sr.ht/~rabbits/uxn/sources), you can find prebuilt rom files [here](https://itch.io/c/248074/uxn-roms). 

### Assembler 

The following command will create an Uxn-compatible rom from an [uxntal file](https://wiki.xxiivv.com/site/uxntal.html). Point the assembler to a `.tal` file, followed by and the rom name:

```sh
bin/uxnasm projects/examples/demos/life.tal bin/life.rom
```

### I/O

You can send events from Uxn to another application, or another instance of uxn, with the Unix pipe. For a companion application that translates notes data into midi, see the [shim](https://git.sr.ht/~rabbits/shim).

```sh
uxnemu orca.rom | shim
```

## Emulator Options

- `-s 1`, `-s 2` or `-s 3` set zoom (default 1)

## Emulator Controls

- `F1` toggle zoom
- `F2` toggle debug
- `F3` capture screen
- `F4` load launcher.rom

### Buttons

- `LCTRL` A
- `LALT` B
- `LSHIFT` SEL 
- `HOME` START

## Need a hand?

The following resources are a good place to start:

* [XXIIVV — uxntal](https://wiki.xxiivv.com/site/uxntal.html)
* [XXIIVV — uxntal cheatsheet](https://wiki.xxiivv.com/site/uxntal_cheatsheet.html)
* [XXIIVV — uxntal reference](https://wiki.xxiivv.com/site/uxntal_reference.html)
* [compudanzas — uxn tutorial](https://compudanzas.net/uxn_tutorial.html)
* [Fediverse — #uxn tag](https://merveilles.town/tags/uxn)

## Contributing

Submit patches using [`git send-email`](https://git-send-email.io/) to the [~rabbits/public-inbox mailing list](https://lists.sr.ht/~rabbits/public-inbox).
