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
clang-format -i src/chr2img.c

echo "Cleaning.."
rm -f ./bin/uxnasm
rm -f ./bin/uxnemu
rm -f ./bin/uxncli
rm -f ./bin/chr2img
rm -f ./bin/boot.rom

echo "Building.."
mkdir -p bin
if [ "${1}" = '--debug' ]; 
then
	echo "[debug]"
    cc -std=c89 -DDEBUG -Wall -Wno-unknown-pragmas -Wpedantic -Wshadow -Wextra -Werror=implicit-int -Werror=incompatible-pointer-types -Werror=int-conversion -Wvla -g -Og -fsanitize=address -fsanitize=undefined src/uxnasm.c -o bin/uxnasm
	cc -std=c89 -DDEBUG -Wall -Wno-unknown-pragmas -Wpedantic -Wshadow -Wextra -Werror=implicit-int -Werror=incompatible-pointer-types -Werror=int-conversion -Wvla -g -Og -fsanitize=address -fsanitize=undefined src/uxn.c src/devices/ppu.c src/devices/apu.c src/devices/mpu.c src/uxnemu.c -L/usr/local/lib $(sdl2-config --cflags --libs) -o bin/uxnemu
    cc -std=c89 -DDEBUG -Wall -Wno-unknown-pragmas -Wpedantic -Wshadow -Wextra -Werror=implicit-int -Werror=incompatible-pointer-types -Werror=int-conversion -Wvla -g -Og -fsanitize=address -fsanitize=undefined src/uxn.c src/uxncli.c -o bin/uxncli
    cc -std=c89 -DDEBUG -Wall -Wno-unknown-pragmas -Wpedantic -Wshadow -Wextra -Werror=implicit-int -Werror=incompatible-pointer-types -Werror=int-conversion -Wvla -g -Og -fsanitize=address -fsanitize=undefined src/chr2img.c -o bin/chr2img
else
	cc src/uxnasm.c -std=c89 -Os -DNDEBUG -g0 -s -Wall -Wno-unknown-pragmas -o bin/uxnasm
	cc src/uxn-fast.c src/uxncli.c -std=c89 -Os -DNDEBUG -g0 -s -Wall -Wno-unknown-pragmas -o bin/uxncli
	cc src/uxn-fast.c src/devices/ppu.c src/devices/apu.c src/devices/mpu.c src/uxnemu.c -std=c89 -Os -DNDEBUG -g0 -s -Wall -Wno-unknown-pragmas -L/usr/local/lib $(sdl2-config --cflags --libs) -o bin/uxnemu
	cc src/chr2img.c -std=c89 -Os -DNDEBUG -g0 -s -Wall -Wno-unknown-pragmas -o bin/chr2img
fi

echo "Installing.."
if [ -d "$HOME/bin" ] && [ -e ./bin/uxnemu ] && [ -e ./bin/uxnasm ]
then
	cp ./bin/uxnemu $HOME/bin
	cp ./bin/uxnasm $HOME/bin
	cp ./bin/uxncli $HOME/bin
	cp ./bin/chr2img $HOME/bin
	echo "Installed in $HOME/bin" 
fi

echo "Assembling.."
./bin/uxnasm projects/examples/demos/piano.tal bin/piano.rom

echo "Running.."
./bin/uxnemu bin/piano.rom

echo "Done."
