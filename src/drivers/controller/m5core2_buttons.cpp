#include "config.h"
#ifdef USE_M5CORE2_BUTTONS

#include <M5Core2.h>
extern "C" {
#include <uxn.h>
#include <devices/controller.h>
}

int 
devctrl_init()
{
    return 1;
}

int
devctrl_handle(Device *d)
{
    M5.update();
    if(M5.BtnA.wasPressed())
        controller_down(d, 0x02);
    if(M5.BtnA.wasReleased())
        controller_up(d, 0x02);

    if(M5.BtnB.wasPressed())
        controller_down(d, 0x01);
    if(M5.BtnB.wasReleased())
        controller_up(d, 0x01);

    if(M5.BtnC.wasPressed())
        controller_down(d, 0x04);
    if(M5.BtnC.wasReleased())
        controller_up(d, 0x04);

    return 1;
}

#endif