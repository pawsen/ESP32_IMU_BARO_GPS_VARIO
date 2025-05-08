#ifndef BTMSG_H_
#define BTMSG_H_

#include <string>
#include "NimBLEDevice.h"

// BLE Service UUID
#define BTMSG_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
// Characteristic UUID for NMEA messages
#define BTMSG_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define BT_MSG_MAX_LEN 128
static const std::string BT_DEVICE_NAME = "ESP32-BLE-Vario";

// Replace magic numbers.
// XXX not used. Ideally we check if the reading is available, otherwise send this
#define PRESSURE_UNAVAILABLE 999999
#define ALTITUDE_UNAVAILABLE 99999
#define VARIO_UNAVAILABLE 9999
#define TEMP_UNAVAILABLE 99
#define BATTERY_UNAVAILABLE 999

bool btmsg_init();
void btmsg_genLK8EX1(char* szmsg, int32_t altm, int32_t cps, float batVoltage);
void btmsg_genXCTRC(char* szmsg);
void btmsg_tx_message(const char* szmsg);

#endif
