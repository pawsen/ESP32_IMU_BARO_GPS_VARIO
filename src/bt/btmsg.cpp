#include "btmsg.h"
#include "NimBLEDevice.h"
#include "common.h"
#include "config.h"
#include "sensor/gps.h"
#include <BluetoothSerial.h>
#if USE_MS5611
#include "sensor/ms5611.h"
#endif
#if USE_BMP388
#include "sensor/bmp388.h"
#endif
#include "ui/ui.h"

static const char *TAG = "btmsg";

// BLE Server objects
BLEServer *pServer = nullptr;
BLEService *pService = nullptr;
BLECharacteristic *pCharacteristic = nullptr;

bool deviceConnected = false;

// Server callbacks
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        deviceConnected = true;
        ESP_LOGI(TAG, "Device connected");
    };

    void onDisconnect(BLEServer *pServer) {
        deviceConnected = false;
        ESP_LOGI(TAG, "Device disconnected");
        // Restart advertising to allow reconnection
        pServer->startAdvertising();
    }
};

bool btmsg_init() {
    // Initialize NimBLE
    BLEDevice::init(BT_DEVICE_NAME);
    BLEDevice::setMTU(512); // Request larger MTU size

    // default power level is +3dB, max +9dB
    // NimBLEDevice::setPower(ESP_PWR_LVL_N3); // -3dB
    // NimBLEDevice::setPower(ESP_PWR_LVL_P6);  // +6db
    NimBLEDevice::setPower(ESP_PWR_LVL_N0); // 0dB

    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityPasskey(123456);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);

    // Create BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create BLE Service
    pService = pServer->createService(BTMSG_SERVICE_UUID);

    // Create BLE Characteristic
    pCharacteristic = pService->createCharacteristic(
        BTMSG_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    // Start the service
    pService->start();

    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BTMSG_SERVICE_UUID);
    pAdvertising->setScanResponseData(BLEAdvertisementData());
    pAdvertising->start();

    ESP_LOGI(TAG, "BLE initialized successfully");
    return true;
}

void btmsg_tx_message(const char *szmsg) {
#ifdef BLE_DEBUG
    dbg_printf(("%s", szmsg));
#endif
    if (deviceConnected) {
        // pCharacteristic->setValue(reinterpret_cast<const uint8_t*>(szmsg), strlen(szmsg));
        pCharacteristic->setValue((const uint8_t *)szmsg, strlen(szmsg));
        pCharacteristic->notify();
    }
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
static void formatNMEAMessage(char *szmsg, size_t maxLen, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(szmsg, maxLen, format, args);
    va_end(args);

    // \r\n adds NMEA message termination
    uint8_t cksum = btmsg_nmeaChecksum(szmsg);
    snprintf(szmsg + strlen(szmsg), maxLen - strlen(szmsg), "%02X\r\n", cksum);
}

// clang-format off
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
// clang-format on
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
