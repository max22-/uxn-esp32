BIN = build/esp32.esp32.esp32/exu-esp32.ino.bin
FQBN = esp32:esp32:esp32:PartitionScheme=defaultffat 
SRCS = uxn-esp32.ino $(shell find . -iname \*.c -o -iname \*.h -o -iname \*.cpp)
PORT = /dev/ttyUSB0

all: $(BIN)

$(BIN): uxn-esp32.ino $(SRCS)
	arduino-cli compile --fqbn $(FQBN) -v

.PHONY: upload clean

upload: $(BIN)
	arduino-cli upload --fqbn $(FQBN) --port $(PORT)

clean:
	rm -rf build
