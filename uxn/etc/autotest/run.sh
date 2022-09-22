#!/bin/sh -e
cd "$(dirname "${0}")"
if ! which Xvfb 2>/dev/null; then
    echo "error: ${0} depends on Xvfb"
    exit 1
fi
if ! which xdotool 2>/dev/null; then
    echo "error: ${0} depends on xdotool"
    exit 1
fi
if [ ! -e fix_fft.c ]; then
    wget https://gist.githubusercontent.com/Tomwi/3842231/raw/67149b6ec81cfb6ac1056fd23a3bb6ce1f0a5188/fix_fft.c
fi
if which clang-format 2>/dev/null; then
    ( cd ../.. && clang-format -i etc/autotest/main.c )
fi
../../bin/uxnasm autotest.tal autotest.rom
gcc -std=gnu89 -Wall -Wextra -o autotest main.c fix_fft.c -lm
./autotest

