#ifndef CONFIG_H
#define CONFIG_H

#define USE_WIFI

/********* devscreen *********/
#define USE_TFT_ESPI
/*****************************/

/********** devctrl **********/
//#define USE_NUNCHUCK
#define USE_SERIAL2
/*****************************/

/********** devmouse *********/
#define USE_TOUCH_SCREEN
#define CAL_DATA { 336, 3549, 262, 3517, 1 }; /* Use Touch_calibrate example of TFT_eSPI lib to calibrate the touch screen */
/*****************************/

#endif