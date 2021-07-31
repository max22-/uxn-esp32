#!/usr/bin/env bash

echo "Cleaning.."
rm -f ./bin/uxnasm
rm -f ./bin/uxnemu
rm -f ./bin/uxncli
rm -f ./bin/boot.rom

# When clang-format is present

if command -v clang-format &> /dev/null
then
	echo "Formatting.."
	clang-format -i src/uxn.h
	clang-format -i src/uxn.c
	clang-format -i src/devices/ppu.h
	clang-format -i src/devices/ppu.c
	clang-format -i src/devices/apu.h
	clang-format -i src/devices/apu.c
	clang-format -i src/uxnasm.c
	clang-format -i src/uxnemu.c
	clang-format -i src/uxncli.c
fi

mkdir -p bin
CFLAGS="-std=c89 -Wall -Wno-unknown-pragmas"
if [ -n "${MSYSTEM}" ]; then
	UXNEMU_LDFLAGS="-static $(sdl2-config --cflags --static-libs)"
else
	UXNEMU_LDFLAGS="-L/usr/local/lib $(sdl2-config --cflags --libs)"
fi

if [ "${1}" = '--debug' ]; 
then
	echo "[debug]"
	CFLAGS="${CFLAGS} -DDEBUG -Wpedantic -Wshadow -Wextra -Werror=implicit-int -Werror=incompatible-pointer-types -Werror=int-conversion -Wvla -g -Og -fsanitize=address -fsanitize=undefined"
	CORE='src/uxn.c'
else
	CFLAGS="${CFLAGS} -DNDEBUG -Os -g0 -s"
	CORE='src/uxn-fast.c'
fi

echo "Building.."
cc ${CFLAGS} src/uxnasm.c -o bin/uxnasm
cc ${CFLAGS} ${CORE} src/devices/ppu.c src/devices/apu.c src/uxnemu.c ${UXNEMU_LDFLAGS} -o bin/uxnemu
cc ${CFLAGS} ${CORE} src/uxncli.c -o bin/uxncli

if [ -d "$HOME/bin" ] && [ -e ./bin/uxnemu ] && [ -e ./bin/uxnasm ]
then
	echo "Installing in $HOME/bin"
	cp ./bin/uxnemu $HOME/bin
	cp ./bin/uxnasm $HOME/bin
	cp ./bin/uxncli $HOME/bin
fi

echo "Assembling.."
./bin/uxnasm projects/examples/demos/piano.tal bin/piano.rom

echo "Running.."
./bin/uxnemu bin/piano.rom

echo "Done."
