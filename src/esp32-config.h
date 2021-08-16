#ifndef ESP32_CONFIG_H
#define ESP32_CONFIG_H

/* Screen config */
/* You have to modify User_Setup.h in TFT_eSPI library folder */
/* this file --> https://github.com/Bodmer/TFT_eSPI/blob/master/User_Setup.h */

/* Sound config */

#define I2S_BCK_PIN 14
#define I2S_WS_PIN 13       /* Labeled LCK on the PCM5102 module */
#define I2S_DATA_OUT_PIN 12 /* data out for the esp32, but data in for the i2s module */

#endif