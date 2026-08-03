#include <Arduino.h>
#include "globals.h"

#define VESC_SERIAL  Serial2
#define VESC_BAUD    115200
#define VESC_TX_PIN  8
#define VESC_RX_PIN  9

#define COMM_GET_VALUES   0x04
#define COMM_GET_IMU_DATA 0x41  // 65 dezimal

// ---------------------------------------------------------------------------
// COMM_GET_VALUES response payload-Offsets (v6.x, inkl. Kommando-Byte)
// Alle Werte big-endian, skalierte Integer (keine IEEE 754 floats)
//
// [0]       = Kommando-Echo (0x04)
// [1..2]    = temp_fet         int16 / 10.0  [°C]
// [3..4]    = temp_motor       int16 / 10.0  [°C]
// [5..8]    = avg_motor_current int32 / 100.0 [A]
// [9..12]   = avg_input_current int32 / 100.0 [A]  ← Batteriestrom
// [13..16]  = avg_id           int32 / 100.0 [A]
// [17..20]  = avg_iq           int32 / 100.0 [A]
// [21..22]  = duty_cycle       int16 / 1000.0
// [23..26]  = rpm              int32 / 1.0
// [27..28]  = v_in             int16 / 10.0  [V]  ← Batteriespannung
// ---------------------------------------------------------------------------
#define OFF_TEMP_FET    1
#define OFF_TEMP_MOTOR  3
#define OFF_I_MOTOR     5   // Phasenstrom -> Drehmoment -> Last am Getriebe
#define OFF_I_IN        9
#define OFF_DUTY       21
#define OFF_RPM        23
#define OFF_V_IN       27
#define MIN_PAYLOAD_LEN 29  // mindestens bis v_in

// ---------------------------------------------------------------------------
// COMM_GET_IMU_DATA response (mask 0x003F = Roll+Pitch+Yaw+AccX+AccY+AccZ)
// VESC echot die Mask als uint16 zurück → alle Felder um +2 verschoben
//
// [0]       = Kommando-Echo (0x41)
// [1..2]    = mask uint16 echo (0x00, 0x3F)
// [3..6]    = roll   [°]
// [7..10]   = pitch  [°]  (positiv = Nase hoch)
// [11..14]  = yaw    [°]
// [15..18]  = acc_x  [m/s²]  ← Vorwärtsachse (negativ = Bremsen)
// [19..22]  = acc_y  [m/s²]
// [23..26]  = acc_z  [m/s²]
//
// Mask-Bits (VESC firmware): Bit0=Roll Bit1=Pitch Bit2=Yaw Bit3=AccX Bit4=AccY Bit5=AccZ
// ---------------------------------------------------------------------------
#define IMU_PAYLOAD_LEN  27

static float accX_cal   = 0.0f;   // Offset bei Stillstand (Montagewinkel + Gravitation)
static float pitch_cal  = 0.0f;   // Pitch bei Kalibrierung [°]
static bool  imu_ready  = false;   // Kalibrierung abgeschlossen

volatile uint32_t lastVescValuesMs = 0; // Zeitstempel letzte gültige VESC-Antwort (Core 0 -> Core 1)

// ---------------------------------------------------------------------------
// CRC16/CCITT (Poly 0x1021, Init 0x0000) — über Payload-Bytes
// ---------------------------------------------------------------------------
static uint16_t crc16(const uint8_t *buf, uint8_t len) {
    uint16_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= (uint16_t)buf[i] << 8;
        for (uint8_t j = 0; j < 8; j++)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
    }
    return crc;
}

// ---------------------------------------------------------------------------
// Requests senden
// ---------------------------------------------------------------------------
static void sendRequest(const uint8_t *payload, uint8_t payloadLen) {
    uint16_t crc = crc16(payload, payloadLen);
    VESC_SERIAL.write((uint8_t)0x02);
    VESC_SERIAL.write(payloadLen);
    VESC_SERIAL.write(payload, payloadLen);
    VESC_SERIAL.write((uint8_t)(crc >> 8));
    VESC_SERIAL.write((uint8_t)(crc & 0xFF));
    VESC_SERIAL.write((uint8_t)0x03);
}

static void sendGetValues() {
    uint8_t payload = COMM_GET_VALUES;
    sendRequest(&payload, 1);
}

static void sendGetImu() {
    // mask 0x003F = Bits 0-5: Roll+Pitch+Yaw+AccX+AccY+AccZ
    uint8_t payload[3] = { COMM_GET_IMU_DATA, 0x00, 0x3F };
    sendRequest(payload, 3);
}

// ---------------------------------------------------------------------------
// Empfangs-State-Machine
// ---------------------------------------------------------------------------
enum RxState : uint8_t { WAIT_START, WAIT_LEN, READ_PAYLOAD, READ_CRC0, READ_CRC1, WAIT_END };

static RxState  rxState = WAIT_START;
static uint8_t  rxBuf[128];
static uint8_t  rxIdx = 0;
static uint8_t  rxPayloadLen = 0;
static uint16_t rxCrcReceived = 0;

static inline float buf_to_float(const uint8_t *buf, uint8_t idx) {
    uint32_t u = ((uint32_t)buf[idx] << 24) | ((uint32_t)buf[idx+1] << 16) |
                 ((uint32_t)buf[idx+2] << 8)  |  (uint32_t)buf[idx+3];
    float f; memcpy(&f, &u, 4); return f;
}

static float cal_accX_sum  = 0.0f;
static float cal_pitch_sum = 0.0f;

static void parseImu(const uint8_t *buf, uint8_t len) {
    s.dbgImuLen = len;  // immer speichern, auch wenn zu kurz
    if (len < IMU_PAYLOAD_LEN) return;
    // [0]=cmd  [1..2]=mask_echo  [3..6]=roll  [7..10]=pitch  [11..14]=yaw
    // [15..18]=accX  [19..22]=accY  [23..26]=accZ
    float pitch = buf_to_float(buf, 7);
    float accX  = buf_to_float(buf, 15);
    float accY  = buf_to_float(buf, 19);
    float accZ  = buf_to_float(buf, 23);

    s.rawAccX = accX;
    s.rawAccY = accY;
    s.rawAccZ = accZ;

    if (!imu_ready) {
        // Auto-Kalibrierung: erste 20 Samples sammeln (egal wann die IMU bereit wird)
        cal_accX_sum  += accX;
        cal_pitch_sum += pitch;
        s.imuCalCount++;
        if (s.imuCalCount >= 20) {
            accX_cal  = cal_accX_sum  / (float)s.imuCalCount;
            pitch_cal = cal_pitch_sum / (float)s.imuCalCount;
            imu_ready = true;
            s.imuReady = true;
        }
        return;
    }

    float accDyn = (accX - accX_cal) - 9.81f * (sinf(pitch * 0.01745329f) - sinf(pitch_cal * 0.01745329f));
    s.accForward = accDyn;

    static uint32_t brakeHoldEnd = 0;
    if (accDyn < p.brakeThreshold) {
        s.braking = true;
        brakeHoldEnd = millis() + p.brakeHoldMs;
    } else if (millis() > brakeHoldEnd) {
        s.braking = false;
    }
}

static inline int32_t buf_i32(const uint8_t *b, uint8_t i) {
    return ((int32_t)b[i] << 24) | ((int32_t)b[i+1] << 16) |
           ((int32_t)b[i+2] <<  8) |  (int32_t)b[i+3];
}

static inline int16_t buf_i16(const uint8_t *b, uint8_t i) {
    return (int16_t)(((uint16_t)b[i] << 8) | (uint16_t)b[i+1]);
}

static void parseValues(const uint8_t *buf, uint8_t len) {
    if (len < MIN_PAYLOAD_LEN) return;
    lastVescValuesMs = millis();

    float i = (float)buf_i32(buf, OFF_I_IN) / 100.0f;
    s.current = (i > 0.0f) ? i : 0.0f; // nur Entladestrom

    s.voltage = (float)buf_i16(buf, OFF_V_IN) / 10.0f;

    // Phasenstrom als Betrag: beim Bremsen/Rekuperieren ist er negativ, fuer die
    // Getriebelast zaehlt aber der Betrag.
    s.motorCurrent = fabsf((float)buf_i32(buf, OFF_I_MOTOR) / 100.0f);
    s.dutyCycle    = (float)buf_i16(buf, OFF_DUTY) / 1000.0f;
    s.erpm         = (float)buf_i32(buf, OFF_RPM);
    s.tempFet      = (float)buf_i16(buf, OFF_TEMP_FET)   / 10.0f;
    s.tempMotor    = (float)buf_i16(buf, OFF_TEMP_MOTOR) / 10.0f;
}

static void receiveBytes() {
    while (VESC_SERIAL.available()) {
        uint8_t b = (uint8_t)VESC_SERIAL.read();
        switch (rxState) {
            case WAIT_START:
                if (b == 0x02) rxState = WAIT_LEN;
                break;
            case WAIT_LEN:
                rxPayloadLen = b;
                rxIdx = 0;
                rxState = (b > 0 && b <= sizeof(rxBuf)) ? READ_PAYLOAD : WAIT_START;
                break;
            case READ_PAYLOAD:
                rxBuf[rxIdx++] = b;
                if (rxIdx == rxPayloadLen) rxState = READ_CRC0;
                break;
            case READ_CRC0:
                rxCrcReceived = (uint16_t)b << 8;
                rxState = READ_CRC1;
                break;
            case READ_CRC1:
                rxCrcReceived |= b;
                rxState = WAIT_END;
                break;
            case WAIT_END:
                if (b == 0x03) {
                    uint16_t calcCrc = crc16(rxBuf, rxPayloadLen);
                    if (rxCrcReceived == calcCrc) {
                        if (rxBuf[0] == COMM_GET_VALUES) {
                            parseValues(rxBuf, rxPayloadLen);
                        } else if (rxBuf[0] == COMM_GET_IMU_DATA) {
                            parseImu(rxBuf, rxPayloadLen);
                        } else {
                            // Unbekannter Frame: für Diagnose speichern
                            s.dbgFrameCmd = rxBuf[0];
                            s.dbgFrameLen = rxPayloadLen;
                            s.dbgFrameCount++;
                            uint8_t n = rxPayloadLen < 8 ? rxPayloadLen : 8;
                            for (uint8_t i = 0; i < n; i++) s.dbgBytes[i] = rxBuf[i];
                        }
                    }
                }
                rxState = WAIT_START;
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// Öffentliche Funktionen
// ---------------------------------------------------------------------------
void vesc_begin() {
    VESC_SERIAL.setTX(VESC_TX_PIN);
    VESC_SERIAL.setRX(VESC_RX_PIN);
    // RX-Puffer vergroessern (Default 32 B): GET_VALUES-Frame ist ~70 B und
    // Core 0 wird durch LVGL/TFT zeitweise blockiert -> sonst Pufferueberlauf,
    // abgeschnittene Frames, CRC-Fehler und vescDataLost-Cutoff. Muss vor begin().
    VESC_SERIAL.setFIFOSize(256);
    VESC_SERIAL.begin(VESC_BAUD);
    lastVescValuesMs = millis(); // Timeout startet ab jetzt
}

void vesc_update() {
    static uint32_t lastValuesMs = 0;
    static uint32_t lastImuMs = 0;
    uint32_t now = millis();
    if (now - lastValuesMs >= 200) { sendGetValues(); lastValuesMs = now; }
    if (now - lastImuMs   >=  50) { sendGetImu();    lastImuMs   = now; }
    receiveBytes();
}

void vesc_imu_calibrate() {
    // Kalibrierung läuft automatisch in parseImu() sobald IMU antwortet
}
