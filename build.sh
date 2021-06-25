#!/usr/bin/env bash

echo "Formatting.."
clang-format -i src/uxn.h
clang-format -i src/uxn.c
clang-format -i src/devices/ppu.h
clang-format -i src/devices/ppu.c
clang-format -i src/devices/apu.h
clang-format -i src/devices/apu.c
clang-format -i src/devices/mpu.h
clang-format -i src/devices/mpu.c
clang-format -i src/uxnasm.c
clang-format -i src/uxnemu.c
clang-format -i src/uxncli.c

echo "Cleaning.."
rm -f ./bin/uxnasm
rm -f ./bin/uxnemu
rm -f ./bin/uxncli
rm -f ./bin/boot.rom

echo "Building.."
mkdir -p bin
CFLAGS="-std=c89 -Wall -Wno-unknown-pragmas"
UXNEMU_LDFLAGS="-L/usr/local/lib $(sdl2-config --cflags --libs)"
if [ "${1}" = '--debug' ]; 
then
	echo "[debug]"
	CFLAGS="${CFLAGS} -DDEBUG -Wpedantic -Wshadow -Wextra -Werror=implicit-int -Werror=incompatible-pointer-types -Werror=int-conversion -Wvla -g -Og -fsanitize=address -fsanitize=undefined"
    CORE='src/uxn.c'
else
	CFLAGS="${CFLAGS} -DNDEBUG -Os -g0 -s"
    CORE='src/uxn-fast.c'
fi
cc ${CFLAGS} src/uxnasm.c -o bin/uxnasm
cc ${CFLAGS} ${CORE} src/devices/ppu.c src/devices/apu.c src/devices/mpu.c src/uxnemu.c ${UXNEMU_LDFLAGS} -o bin/uxnemu
cc ${CFLAGS} ${CORE} src/uxncli.c -o bin/uxncli

echo "Installing.."
if [ -d "$HOME/bin" ] && [ -e ./bin/uxnemu ] && [ -e ./bin/uxnasm ]
then
	cp ./bin/uxnemu $HOME/bin
	cp ./bin/uxnasm $HOME/bin
	cp ./bin/uxncli $HOME/bin
	echo "Installed in $HOME/bin" 
fi

echo "Assembling.."
./bin/uxnasm projects/examples/demos/piano.tal bin/piano.rom

echo "Running.."
./bin/uxnemu bin/piano.rom

echo "Done."
