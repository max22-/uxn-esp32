</$objtype/mkfile

TARG=bin/uxncli bin/uxnasm bin/uxnemu
USM=`{walk -f projects/ | grep '\.tal$' | grep -v blank.tal | grep -v /assets/ | grep -v /library/}
ROM=${USM:%.tal=%.rom}
CFLAGS=$CFLAGS -D__plan9__ -I/sys/include/npe -I/sys/include/npe/SDL2
HFILES=\
	/sys/include/npe/stdio.h\
	src/devices/audio.h\
	src/devices/controller.h\
	src/devices/datetime.h\
	src/devices/file.h\
	src/devices/mouse.h\
	src/devices/screen.h\
	src/devices/system.h\
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

bin/uxncli: file.$O datetime.$O system.$O uxncli.$O uxn.$O
	$LD $LDFLAGS -o $target $prereq

bin/uxnasm: uxnasm.$O
	$LD $LDFLAGS -o $target $prereq

bin/uxnemu: audio.$O controller.$O datetime.$O file.$O mouse.$O screen.$O system.$O uxn.$O uxnemu.$O
	$LD $LDFLAGS -o $target $prereq

(uxnasm|uxncli|uxnemu|uxn)\.$O:R: src/\1.c
	$CC $CFLAGS -Isrc -o $target src/$stem1.c

(audio|controller|datetime|file|mouse|screen|system)\.$O:R: src/devices/\1.c
	$CC $CFLAGS -Isrc -o $target src/devices/$stem1.c

nuke:V: clean

clean:V:
	rm -f *.[$OS] [$OS].??* $TARG $CLEANFILES

%.clean:V:
	rm -f $stem.[$OS] [$OS].$stem $stem

install:QV: all
	exit 'Sorry, there is no install rule yet'

#LDFLAGS=-p
