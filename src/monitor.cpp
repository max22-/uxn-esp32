#include <Arduino.h>

extern "C" {
#include "uxn.h"
void console_printf(const char* format, ...);
}

int uxn_load(Uxn *u, char *filepath);
void uxn_run(Uxn *u);

#define MODE_SHORT 0x20
#define MODE_RETURN 0x40
#define MODE_KEEP 0x80

#define LINE_LENGTH 40
static char line[LINE_LENGTH];
static int cursor = 0;

static void
diss(uint8_t* mem, uint16_t addr)
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
        instr = mem[addr];
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
            val16 = (mem[addr + 1] << 8) + mem[addr + 2];
            console_printf(" %04x", val16);
            addr += 3;
        }
        else
        if (instr == 0x40 || instr == 0x80) {
            val8 = mem[addr + 1];
            console_printf(" %02x", val8);
            addr += 2;
        }
        else {
            addr += 1;
        }
        console_printf("\n");
    }
}

static void
dump(uint8_t* mem, uint16_t addr)
{
    for (int y = 0; y < 4; y++) {
        console_printf("\n%04x", addr + y * 8);
        for (int x = 0; x < 8; x++) {
            console_printf(" %02x", mem[addr + y * 8 + x]);
        }
    }
    console_printf("\n\n");
}

static void
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
mon_init()
{
    print_line();
}

static void
monitor(Uxn* u, char* line)
{
    short start;

    switch (line[0]) {
        case 'l':
            uxn_load(u, line+1);
            break;
        case 'r':
            uxn_run(u);
            break;
        case '.':
            start = 0xffff & strtoul(line+1, NULL, 16);
            dump(u->ram.dat, start);
            break;
        case ';':
            start = 0xffff & strtoul(line+1, NULL, 16);
            diss(u->ram.dat, start);
            break;
        default:
            console_printf("Unknown '%c'. Try (l)oad, (r)un, (.)dump, (;)disass.\n", line[0]);
    }
}

void
mon_onechar(Uxn* u, char c)
{
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
        monitor(u, line);
        line[0] = '\0';
    }
    else
    if (cursor < LINE_LENGTH && ((c >= '0' && c <= 'z') || c == '/' || c == '.')) {
        line[cursor++] = c;
        line[cursor] = '\0';
    }
    print_line();
}