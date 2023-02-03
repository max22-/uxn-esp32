#include <Arduino.h>
#include <ESP32-USB-Soft-Host.h>
#include "usbkbd.h"

#define PROFILE_NAME "Default Wroom"
#define DP_P0  16
#define DM_P0  17

static usb_pins_config_t USB_Pins_Config =
{
  DP_P0, DM_P0,
  -1, -1,
  -1, -1,
  -1, -1
};

class MyKeyboardReportParser : public KeyboardReportParser {
    void OnControlKeysChanged(uint8_t before, uint8_t after) override {
      Serial.printf("OnControlKeysChanged(before=0x%02x, after=0x%02x)\n", before, after);
    };
    void OnKeyDown(uint8_t mod, uint8_t key) {
      Serial.printf("key down : %c\n", OemToAscii(mod, key));
    };
    void OnKeyUp(uint8_t mod, uint8_t key) {
      Serial.printf("key up : %c\n", OemToAscii(mod, key));
    };
};

MyKeyboardReportParser kbd_report_parser;

static void my_USB_DetectCB( uint8_t usbNum, void * dev )
{
  sDevDesc *device = (sDevDesc*)dev;
  printf("New device detected on USB#%d\n", usbNum);
  printf("desc.bcdUSB             = 0x%04x\n", device->bcdUSB);
  printf("desc.bDeviceClass       = 0x%02x\n", device->bDeviceClass);
  printf("desc.bDeviceSubClass    = 0x%02x\n", device->bDeviceSubClass);
  printf("desc.bDeviceProtocol    = 0x%02x\n", device->bDeviceProtocol);
  printf("desc.bMaxPacketSize0    = 0x%02x\n", device->bMaxPacketSize0);
  printf("desc.idVendor           = 0x%04x\n", device->idVendor);
  printf("desc.idProduct          = 0x%04x\n", device->idProduct);
  printf("desc.bcdDevice          = 0x%04x\n", device->bcdDevice);
  printf("desc.iManufacturer      = 0x%02x\n", device->iManufacturer);
  printf("desc.iProduct           = 0x%02x\n", device->iProduct);
  printf("desc.iSerialNumber      = 0x%02x\n", device->iSerialNumber);
  printf("desc.bNumConfigurations = 0x%02x\n", device->bNumConfigurations);
  // if( device->iProduct == mySupportedIdProduct && device->iManufacturer == mySupportedManufacturer ) {
  //   myListenUSBPort = usbNum;
  // }
}

static void my_USB_PrintCB(uint8_t usbNum, uint8_t byte_depth, uint8_t* data, uint8_t data_len)
{
  // if( myListenUSBPort != usbNum ) return;
  printf("in: ");
  for(int k=0;k<data_len;k++) {
    printf("0x%02x ", data[k] );
  }
  printf("\n");
  kbd_report_parser.Parse(data_len, data);
}

void keyboard_init()
{
  USH.init( USB_Pins_Config, my_USB_DetectCB, my_USB_PrintCB );
}