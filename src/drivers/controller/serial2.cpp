#include <Arduino.h>
#include "config.h"
extern "C" {
#include <uxn.h>
}

#ifdef USE_SERIAL2

int 
devctrl_init()
{
    Serial2.begin(115200);
    return 1;
}

int
devctrl_handle(Uxn *u)
{
    char c;
    if(Serial2.available() > 0) {
        Serial2.readBytes(&c, 1);
        u->dev[0x8].dat[0x3] = c;
        return uxn_eval(u, u->dev[0x8].vector);
    }
    return 1;
}

#endif