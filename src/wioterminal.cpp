#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Seeed_Arduino_FS.h>

extern "C" {
#include "uxn.h"

Uxn u;

int uxn_setup(Uxn* u, char* rom);
void uxn_run(Uxn *u);

void
console_printf(const char* format, ...)
{
	va_list args;
	va_start(args, format);   

	char buf[50];
    vsnprintf(buf, 50, format, args);
    Serial.print(buf);
	va_end(args);   
}

#define MODE_SHORT 0x20
#define MODE_RETURN 0x40
#define MODE_KEEP 0x80

void
diss(Uxn* u, uint16_t addr)
{
    uint16_t end = addr + 40;
    uint8_t instr;
    uint8_t val8;
    uint16_t val16;
    
    static char code[6];
    char ops[][4] = {
        /* Stack */      "LIT", "INC", "POP", "DUP", "NIP", "SWP", "OVR", "ROT",
        /* Logic */      "EQU", "NEQ", "GTH", "LTH", "JMP", "JNZ", "JSR", "STH",
        /* Memory */     "LDZ", "STZ", "LDR", "STR", "LDA", "STA", "DEI", "DEO",
        /* Arithmetic */ "ADD", "SUB", "MUL", "DIV", "AND", "ORA", "EOR", "SFT",
    };

    while (addr < end) {
        instr = u->ram.dat[addr];
        if (instr == 0) {
            strcpy(code, "BRK");
        }
        else {
            strcpy(code, ops[instr & 0x1f]);
            if (instr & MODE_SHORT) {
                strcat(code, "2");
            }
            if (instr & MODE_KEEP) {
                strcat(code, "k");
            }
            if (instr & MODE_RETURN) {
                strcat(code, "r");
            }
        }
        console_printf("%04x %s", addr, code);
        if (instr == 0x20) {
            val16 = (u->ram.dat[addr + 1] << 8) + u->ram.dat[addr + 2];
            console_printf(" %04x", val16);
            addr += 3;
        }
        else
        if (instr == 0x40 || instr == 0x80) {
            val8 = u->ram.dat[addr + 1];
            console_printf(" %02x", val8);
            addr += 2;
        }
        else {
            addr += 1;
        }
        console_printf("\n");
    }
}

void
dump(Uxn* u, int addr)
{
    for (int y = 0; y < 4; y++) {
        console_printf("\n%04x", addr + y * 8);
        for (int x = 0; x < 8; x++) {
            console_printf(" %02x", u->ram.dat[addr + y * 8 + x]);
        }
    }
    console_printf("\n\n");
}

int
uxn_load(Uxn *u, char *filepath)
{
	File f;
	if (!(f = SD.open(filepath))) {
        console_printf("Error loading \"%s\"\n", filepath);
		return 0;
	}
	f.read(u->ram.dat + PAGE_PROGRAM, sizeof(u->ram.dat) - PAGE_PROGRAM);
	f.close();
	console_printf("Loaded %s\n", filepath);
    dump(u, PAGE_PROGRAM);
	return 1;
}
}

char rom[] = "/uxn/bin/hello.rom";
#define LINE_LENGTH 40
char line[LINE_LENGTH];
int cursor = 0;

void
print_line()
{
    Serial.print("\ruxn> ");
    for (int i = 0; i < LINE_LENGTH; i++) {
        Serial.print(' ');
    }
    Serial.print("\ruxn> ");
    Serial.print(line);
}

void
setup()
{
    Serial.begin(115200);
	while (!Serial);

    if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI, 4000000UL)) {
		console_printf("SDcard initialization failed!");
  	}

    if (!uxn_setup(&u, rom)) {
        return;
    }

    print_line();
	//uxn_run(&u);
}

void
monitor(char* line)
{
    short start;

    switch (line[0]) {
        case 'l':
            uxn_load(&u, line+1);
            break;
        case 'r':
            uxn_run(&u);
            break;
        case 'd':
            start = 0xffff & strtoul(line+1, NULL, 16);
            dump(&u, start);
            break;
        case 'a':
            start = 0xffff & strtoul(line+1, NULL, 16);
            diss(&u, start);
            break;
        default:
            console_printf("Unknown '%c'. Try (l)oad, (r)un, (d)ump, dis(a)\n", line[0]);
    }
}

void
loop()
{
    char c;
    if (Serial.available() > 0) {
        c = Serial.read();
        if (cursor > 0 && c == 0x08) {
            line[--cursor] = '\0';
        }
        else
        if (c == 0x0d) {
            // discard
        }
        else
        if (c == 0x0a) {
            Serial.println();
            cursor = 0;
            monitor(line);
            line[0] = '\0';
        }
        else
        if (cursor < LINE_LENGTH && ((c >= '0' && c <= 'z') || c == '/' || c == '.')) {
            line[cursor++] = c;
            line[cursor] = '\0';
        }
        print_line();
    }
}