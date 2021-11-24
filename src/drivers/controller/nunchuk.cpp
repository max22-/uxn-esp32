#include <Arduino.h>
extern "C" {
#include <uxn.h>
}

#ifdef USE_NUNCHUCK
#include <WiiChuck.h>

static Accessory nunchuck;

int 
devctrl_init()
{
    nunchuck.begin();
    if(nunchuck.type == Unknown)
        nunchuck.type = NUNCHUCK;
    return 1;
}

int
devctrl_handle(Uxn *u)
{
    static int flag_joy = 0, flag_z = 0;
    static int minx = 1000, maxx = 1, minz = 1000, maxz = 1;

    int x, z;

    nunchuck.readData();
    x = nunchuck.getAccelX();
    z = nunchuck.getAccelZ();

    if(flag_joy == 0) {
        if(nunchuck.getJoyX() == 0) {
            u->dev[0x8].dat[0x2] = 0x40;
            uxn_eval(u, u->dev[0x8].vector);
            flag_joy = 1;
        }
        else if(nunchuck.getJoyX() == 255) {
            u->dev[0x8].dat[0x2] = 0x80;
            uxn_eval(u, u->dev[0x8].vector);
            flag_joy = 1;
        }
        else if(nunchuck.getJoyY() == 0) {
            u->dev[0x8].dat[0x2] = 0x20;
            uxn_eval(u, u->dev[0x8].vector);
            flag_joy = 1;
        }
        else if(nunchuck.getJoyY() == 255) {
            u->dev[0x8].dat[0x2] = 0x10;
            uxn_eval(u, u->dev[0x8].vector);
            flag_joy = 1;
        }
    }
    else if(nunchuck.getJoyX() == 128 && nunchuck.getJoyY() == 128) {
        u->dev[0x8].dat[0x2] = 0;
        uxn_eval(u, u->dev[0x8].vector);
        flag_joy = 0;
    }

    if(!flag_z && nunchuck.getButtonZ()) {
        u->dev[0x8].dat[0x3] = 0xd;
        uxn_eval(u, u->dev[0x8].vector);
        flag_z = 1;
    }

    if(flag_z && !nunchuck.getButtonZ()) {
        u->dev[0x8].dat[0x3] = 0;
        uxn_eval(u, u->dev[0x8].vector);
        flag_z = 0;
    }

    minx = min(minx, x);
    maxx = max(maxx, x);
    minz = min(minz, z);
    maxz = max(maxz, z);

    printf("x=%d z=%d minx=%d maxx=%d minz=%d maxz=%d\n",
        x, z, minx, maxx, minz, maxz);

    poke16((Uint8*)&(u->dev[9].dat), 0x2, (320*(x - minx))/(maxx - minx + 1));
    poke16((Uint8*)&(u->dev[9].dat), 0x4, (240*(z - minz))/(maxz - minz + 1));
    uxn_eval(u, u->dev[0x9].vector);

    return 1;
}

#endif