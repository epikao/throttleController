#include <Arduino.h>
#include "globals.h"
#include "pico/sha256.h"

#define RACEKEY_SERIAL   Serial1
#define RACEKEY_BAUD     9600
#define RACEKEY_TX_PIN   0
#define RACEKEY_RX_PIN   1

// ---------------------------------------------------------------------------
// Passwort: "heiaaixxso84!"
//
// Hash generieren (Linux/Mac Terminal oder Git Bash unter Windows):
//   echo -n "heiaaixxso84!" | sha256sum
// Ergebnis in Bytes aufteilen (je 2 Hex-Zeichen = 1 Byte), z.B. mit:
//   echo -n "heiaaixxso84!" | sha256sum | awk '{gsub(/../,"0x&, ",$1); print $1}'
//
// Ergebnis: b2ef5bd813000cac28285101dc226095949fc981da2ab0f66861ab83415e7e48
// ---------------------------------------------------------------------------
static const uint8_t PW_HASH[SHA256_RESULT_BYTES] = {
    0xb2, 0xef, 0x5b, 0xd8, 0x13, 0x00, 0x0c, 0xac,
    0x28, 0x28, 0x51, 0x01, 0xdc, 0x22, 0x60, 0x95,
    0x94, 0x9f, 0xc9, 0x81, 0xda, 0x2a, 0xb0, 0xf6,
    0x68, 0x61, 0xab, 0x83, 0x41, 0x5e, 0x7e, 0x48
};

#define PW_LEN           13   // strlen("heiaaixxso84!")
#define RACEKEY_TIMEOUT_MS 1500

static char     rxBuf[PW_LEN];
static uint8_t  rxIdx      = 0;
static uint32_t lastCharMs = 0;

static bool checkHash() {
    pico_sha256_state_t state;
    if (pico_sha256_try_start(&state, SHA256_BIG_ENDIAN, false) != PICO_OK)
        return false;
    pico_sha256_update_blocking(&state, (const uint8_t*)rxBuf, PW_LEN);
    sha256_result_t result;
    pico_sha256_finish(&state, &result);
    return memcmp(result.bytes, PW_HASH, SHA256_RESULT_BYTES) == 0;
}

void racekey_begin() {
    RACEKEY_SERIAL.setTX(RACEKEY_TX_PIN);
    RACEKEY_SERIAL.setRX(RACEKEY_RX_PIN);
    RACEKEY_SERIAL.begin(RACEKEY_BAUD);
}

void racekey_update() {
    if (rxIdx > 0 && (millis() - lastCharMs) > RACEKEY_TIMEOUT_MS) {
        rxIdx = 0;
    }

    while (RACEKEY_SERIAL.available()) {
        char c = (char)RACEKEY_SERIAL.read();
        if (c == '\n') {
            rxIdx = 0;
            lastCharMs = millis();
            continue;
        }
        rxBuf[rxIdx++] = c;
        lastCharMs = millis();

        if (rxIdx == PW_LEN) {
            if (checkHash()) {
                p.pwRace  = 1;
                storageDirty = true;
                RACEKEY_SERIAL.print('S'); // Stick: LED2 ein
            }
            rxIdx = 0;
        }
    }
}
