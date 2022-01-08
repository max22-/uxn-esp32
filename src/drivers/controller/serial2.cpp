#include <Arduino.h>
#include "config.h"
extern "C" {
#include <uxn.h>
#include <devices/controller.h>
}

#ifdef USE_SERIAL2

int 
devctrl_init()
{
    Serial2.begin(115200);
    return 1;
}

int
devctrl_handle(Device *d)
{
    char c;
    if(Serial2.available() > 0) {
        Serial2.readBytes(&c, 1);
        controller_key(d, c);
    }
    return 1;
}

#endif