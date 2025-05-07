#ifndef BTMSG_H_
#define BTMSG_H_

#define BT_MSG_MAX_LEN 128
#define BT_DEVICE_NAME "ESP32-BT-Vario"

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
