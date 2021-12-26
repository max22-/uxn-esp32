# Uxn-Esp32

A port of the [Uxn](https://wiki.xxiivv.com/site/uxn.html) virtual machine and [Varvara](https://wiki.xxiivv.com/site/varvara.html) environment to the esp32 platform.

# How to build it

Install [PlatforimIO Core](https://platformio.org/install/cli) if you want to use the command line only. It is also available as a plugin for several IDEs/editors (Emacs, vim, VSCode, Atom, etc).


```
git clone https://github.com/max22-/uxn-esp32
cd uxn-esp32
pio run
```

Then edit [User_Setup.h](https://github.com/Bodmer/TFT_eSPI/blob/master/User_Setup.h) in the TFT_eSPI lib directory which should have appeared in .pio/libdeps/esp32dev/TFT_eSPI after the build is complete.

Rename wifi_credentials.sample.h to wifi_credentials.h and modify its contents. Idem for the configuration at the top of main.cpp. Wifi is used only to get time from an NTP server for the datetime device. You can comment out USE_WIFI if you dan't want to use it.

Then it is time to upload everything on the device :

```
pio run -t uploadfs
pio run -t upload
```

I will try to make it buildable with Arduino IDE as well in the future.

The roms must be in the "data" folder. (they are uploaded with "pio run -t uploadfs").

# Devices available

Console, screen, file, datetime, mouse (touch screen). More to come later !