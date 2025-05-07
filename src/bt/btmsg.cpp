#include "common.h"
#include <BluetoothSerial.h>
#include "config.h"
#include "btmsg.h"
#include "sensor/gps.h"
#if USE_MS5611
#include "sensor/ms5611.h"
#endif
#if USE_BMP388
#include "sensor/bmp388.h"
#endif
#include "ui/ui.h"

static const char* TAG = "btmsg";

BluetoothSerial* pSerialBT = NULL;

void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param){
	if(event == ESP_SPP_SRV_OPEN_EVT){
		ESP_LOGD(TAG, "BT client Connected");
		}
	if(event == ESP_SPP_CLOSE_EVT ){
		ESP_LOGD(TAG, "BT client disconnected");
		}
	}

bool btmsg_init() {
    static BluetoothSerial serialBT;
    pSerialBT = &serialBT;

    if (!pSerialBT->register_callback(callback)) {
        ESP_LOGE(TAG, "Failed to register BT callback");
        return false;
    }

    if (!pSerialBT->begin(BT_DEVICE_NAME)) {
        ESP_LOGE(TAG, "BT initialization failed");
        return false;
    }

    ESP_LOGI(TAG, "BT initialized successfully");
    return true;
}

void btmsg_tx_message(const char* szmsg) {
	pSerialBT->print(szmsg);
	}


static uint8_t btmsg_nmeaChecksum(const char *szNMEA) {
    // The checksum is calculated by XOR-ing all characters between $ and * in the NMEA message
    const char *sz = &szNMEA[1]; // skip leading '$'
    uint8_t cksum = 0;
    while ((*sz) != 0 && (*sz != '*')) {
        cksum ^= (uint8_t)*sz;
        sz++;
    }
    return cksum;
}

// Helper function for common NMEA message formatting
static void formatNMEAMessage(char* szmsg, size_t maxLen, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(szmsg, maxLen, format, args);
    va_end(args);

    // \r\n adds NMEA message termination
    uint8_t cksum = btmsg_nmeaChecksum(szmsg);
    snprintf(szmsg + strlen(szmsg), maxLen - strlen(szmsg), "%02X\r\n", cksum);
}

/*
LK8000 EXTERNAL INSTRUMENT SERIES 1 - NMEA SENTENCE: LK8EX1
VERSION A, 110217

LK8EX1,pressure,altitude,vario,temperature,battery,*checksum

Field 0, raw pressure in hPascal:
	hPA*100 (example for 1013.25 becomes  101325)
	no padding (987.25 becomes 98725, NOT 098725)
	If no pressure available, send 999999 (6 times 9)
	If pressure is available, field 1 altitude will be ignored

Field 1, altitude in meters, relative to QNH 1013.25
	If raw pressure is available, this value will be IGNORED (you can set it to 99999
	but not really needed)!
	(if you want to use this value, set raw pressure to 999999)
	This value is relative to sea level (QNE). We are assuming that
	currently at 0m altitude pressure is standard 1013.25.
	If you cannot send raw altitude, then send what you have but then
	you must NOT adjust it from Basic Setting in LK.
	Altitude can be negative
	If altitude not available, and Pressure not available, set Altitude
	to 99999  (5 times 9)
	LK will say "Baro altitude available" if one of fields 0 and 1 is available.

Field 2, vario in cm/s
	If vario not available, send 9999  (4 times 9)
	Value can also be negative

Field 3, temperature in C , can be also negative
	If not available, send 99

Field 4, battery voltage or charge percentage
	Cannot be negative
	If not available, send 999 (3 times 9)
	Voltage is sent as float value like: 0.1 1.4 2.3  11.2
	To send percentage, add 1000. Example 0% = 1000
	14% = 1014 .  Do not send float values for percentages.
	Percentage should be 0 to 100, with no decimals, added by 1000!
*/
void btmsg_genLK8EX1(char *szmsg, int32_t altm, int32_t cps, float batVoltage) {
    formatNMEAMessage(szmsg, BT_MSG_MAX_LEN, "$LK8EX1,999999,%d,%d,99,%.1f*", altm, cps,
                      batVoltage);
}

/*
(From XCSoar) Native XTRC sentences
$XCTRC,2015,1,5,16,34,33,36,46.947508,7.453117,540.32,12.35,270.4,2.78,,,,964.93,98*67

$XCTRC,year,month,day,hour,minute,second,centisecond,latitude,longitude,altitude,speedoverground,
     course,climbrate,res,res,res,rawpressure,batteryindication*checksum
*/
void btmsg_genXCTRC(char *szmsg) {
    // not really battery voltage, power comes from 5V power bank
    int32_t batteryPercent = (int32_t)((SupplyVoltageV * 100.0f) / 5.0f);
    CLAMP(batteryPercent, 0, 100); // some power banks supply more than 5V
    float latDeg = FLOAT_DEG(NavPvt.nav.latDeg7);
    float lonDeg = FLOAT_DEG(NavPvt.nav.lonDeg7);
    float altM = ((float)NavPvt.nav.heightMSLmm) / 1000.0f;
    // clang-format off
    formatNMEAMessage(szmsg, BT_MSG_MAX_LEN,
        "$XCTRC,%d,%d,%d,%d,%d,%d,%d,%.6f,%.6f,%.2f,%.2f,%.1f,%.2f,,,,%.2f,%d*",
        NavPvt.nav.utcYear, NavPvt.nav.utcMonth, NavPvt.nav.utcDay,
        NavPvt.nav.utcHour, NavPvt.nav.utcMinute, NavPvt.nav.utcSecond,
        NavPvt.nav.nanoSeconds / 10000000,
        latDeg, lonDeg, altM,
        NavPvt.nav.groundSpeedmmps * 0.0036f,
        (float)GpsCourseHeadingDeg,
        KFClimbrateCps * 0.01f,
#if USE_MS5611
        PaSample_MS5611 * 0.01f,
#elif USE_BMP388
        PaSample_BMP388 * 0.01f,
#else
        0.0f,
#endif
        batteryPercent);
    // clang-format on
}
