</$objtype/mkfile

TARG=bin/uxncli bin/uxnasm bin/uxnemu bin/chr2img
USM=`{walk -f projects/ | grep '\.tal$' | grep -v blank.tal}
ROM=${USM:%.tal=%.rom}
CFLAGS=$CFLAGS -I/sys/include/npe -I/sys/include/npe/SDL2
HFILES=\
	/sys/include/npe/stdio.h\
	src/devices/apu.h\
	src/devices/mpu.h\
	src/devices/ppu.h\
	src/uxn.h\

CLEANFILES=$TARG $ROM

default:V: all

all:V: bin $TARG $ROM

bin:
	mkdir -p bin

/sys/include/npe/stdio.h:
	hget https://git.sr.ht/~ft/npe/archive/master.tar.gz | tar xz &&
	cd npe-master &&
	mk install &&
	rm -r npe-master

%.rom:Q: %.tal bin/uxnasm
	bin/uxnasm $stem.tal $target >/dev/null

bin/uxncli: uxncli.$O uxn.$O
	$LD $LDFLAGS -o $target $prereq

bin/uxnasm: uxnasm.$O
	$LD $LDFLAGS -o $target $prereq

bin/uxnemu: uxnemu.$O apu.$O mpu.$O ppu.$O uxn.$O
	$LD $LDFLAGS -o $target $prereq

bin/chr2img: chr2img.$O
	$LD $LDFLAGS -o $target $prereq

(uxnasm|uxncli|uxnemu|uxn|chr2img)\.$O:R: src/\1.c
	$CC $CFLAGS -Isrc -o $target src/$stem1.c

(apu|mpu|ppu)\.$O:R: src/devices/\1.c
	$CC $CFLAGS -Isrc -o $target src/devices/$stem1.c

nuke:V: clean

clean:V:
	rm -f *.[$OS] [$OS].??* $TARG $CLEANFILES

%.clean:V:
	rm -f $stem.[$OS] [$OS].$stem $stem

install:QV: all
	exit 'Sorry, there is no install rule yet'

#LDFLAGS=-p
