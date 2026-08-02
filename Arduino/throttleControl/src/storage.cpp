#include <Arduino.h>
#include <string.h>
#include "globals.h"
#include "storage.h"
#include "menu.h"

// ---------------------------------------------------------------------------
// Speicher-Layout
//
//   0                 Magic (1 B)
//   1                 sizeof(EBikeSettings) (2 B)
//   3 .. 3+sizeof(p)  Settings-Block ("cold": nur bei echten Aenderungen geschrieben)
//   192 ..            Hot-Bereich ("hot": Trip/Odo/Wh-pro-km, haeufig geschrieben)
//
// Die Trennung ist fuer EEPROM wichtig: ein Kilometer-Update beschreibt so nur EINE
// Page (1 Zyklus, ~5 ms) statt des kompletten Settings-Blocks (6 Zyklen, ~30 ms).
// ---------------------------------------------------------------------------
#define STORAGE_MAGIC         0xA5
#define STORAGE_ADDR_MAGIC    0        // 1 byte
#define STORAGE_ADDR_SIZE     1        // 2 bytes  (sizeof EBikeSettings)
#define STORAGE_ADDR_SETTINGS 3        // struct data

// ===========================================================================
// EEPROM-Implementierung (24LC64, Pagesize 32 bytes, Write-Cycle 5 ms)
// ===========================================================================
#if STORAGE_TYPE == STORAGE_EEPROM

#define EEPROM_I2C_ADDR  0x50
#define EEPROM_PAGE_SIZE 32
#define EEPROM_WRITE_MS  5

// Schreibt len Bytes page-weise (überschreitet nie eine 32-Byte-Page-Grenze)
static void eeprom_write(uint16_t addr, const uint8_t* data, uint16_t len) {
    while (len > 0) {
        uint8_t pageOff = (uint8_t)(addr % EEPROM_PAGE_SIZE);
        uint8_t chunk   = (uint8_t)min((uint16_t)(EEPROM_PAGE_SIZE - pageOff), len);
        Wire2.beginTransmission(EEPROM_I2C_ADDR);
        Wire2.write((uint8_t)(addr >> 8));
        Wire2.write((uint8_t)(addr & 0xFF));
        for (uint8_t i = 0; i < chunk; i++) Wire2.write(data[i]);
        Wire2.endTransmission();
        delay(EEPROM_WRITE_MS);
        addr += chunk;
        data += chunk;
        len  -= chunk;
    }
}

// Liest len Bytes in 32-Byte-Blöcken (Wire-Buffer-Limit)
static void eeprom_read(uint16_t addr, uint8_t* buf, uint16_t len) {
    while (len > 0) {
        uint8_t chunk = (uint8_t)min(len, (uint16_t)32);
        Wire2.beginTransmission(EEPROM_I2C_ADDR);
        Wire2.write((uint8_t)(addr >> 8));
        Wire2.write((uint8_t)(addr & 0xFF));
        Wire2.endTransmission(false);
        Wire2.requestFrom((uint8_t)EEPROM_I2C_ADDR, chunk);
        for (uint8_t i = 0; i < chunk && Wire2.available(); i++) buf[i] = Wire2.read();
        addr += chunk;
        buf  += chunk;
        len  -= chunk;
    }
}

#endif // STORAGE_EEPROM

// ===========================================================================
// FRAM-Implementierung (MB85RC..., I2C, 16-Bit-Adresse) -- direkte Wire2-Transfers,
// ohne externe Library. FRAM ist ein I2C-Speicher wie ein EEPROM, aber OHNE Write-Delay
// und OHNE Page-Grenzen -> nur die Wire-Puffergroesse (256) begrenzt einen Block.
// ===========================================================================
#if STORAGE_TYPE == STORAGE_FRAM

#define FRAM_I2C_ADDR  0x50
#define FRAM_ID_ADDR   0x7C   // Reserved Slave ID (0xF8>>1): Hersteller-/Produkt-ID
#define FRAM_CHUNK     128    // <= Wire-Puffer (256) inkl. der 2 Adressbytes

static void fram_write(uint16_t addr, const uint8_t* data, uint16_t len) {
    while (len > 0) {
        uint8_t chunk = (len < FRAM_CHUNK) ? (uint8_t)len : FRAM_CHUNK;
        Wire2.beginTransmission(FRAM_I2C_ADDR);
        Wire2.write((uint8_t)(addr >> 8));
        Wire2.write((uint8_t)(addr & 0xFF));
        for (uint8_t i = 0; i < chunk; i++) Wire2.write(data[i]);
        Wire2.endTransmission();
        addr += chunk; data += chunk; len -= chunk;
    }
}

static void fram_read(uint16_t addr, uint8_t* buf, uint16_t len) {
    while (len > 0) {
        uint8_t chunk = (len < FRAM_CHUNK) ? (uint8_t)len : FRAM_CHUNK;
        Wire2.beginTransmission(FRAM_I2C_ADDR);
        Wire2.write((uint8_t)(addr >> 8));
        Wire2.write((uint8_t)(addr & 0xFF));
        Wire2.endTransmission(false);   // repeated start, Adresspointer gesetzt lassen
        Wire2.requestFrom((uint8_t)FRAM_I2C_ADDR, chunk);
        for (uint8_t i = 0; i < chunk && Wire2.available(); i++) buf[i] = Wire2.read();
        addr += chunk; buf += chunk; len -= chunk;
    }
}

static uint8_t fram_read8(uint16_t addr) {
    uint8_t v = 0;
    fram_read(addr, &v, 1);
    return v;
}
static void fram_write8(uint16_t addr, uint8_t v) {
    fram_write(addr, &v, 1);
}

#endif // STORAGE_FRAM

// ===========================================================================
// Gemeinsame Zugriffsschicht (kapselt FRAM vs. EEPROM)
// ===========================================================================
static inline void store_write(uint16_t addr, const uint8_t* data, uint16_t len) {
#if STORAGE_TYPE == STORAGE_FRAM
    fram_write(addr, data, len);
#else
    eeprom_write(addr, data, len);
#endif
}

static inline void store_read(uint16_t addr, uint8_t* buf, uint16_t len) {
#if STORAGE_TYPE == STORAGE_FRAM
    fram_read(addr, buf, len);
#else
    eeprom_read(addr, buf, len);
#endif
}

// ===========================================================================
// Hot-Datensatz: Trip/Odo/Wh-pro-km
//
// Eigener Bereich, damit ein Kilometer-Update nur eine Page kostet. Zusaetzlich als
// RING ueber HOT_SLOTS Pages mit laufender Sequenznummer:
//   - verteilt den Verschleiss auf HOT_SLOTS Pages (EEPROM: HOT_SLOTS-fache Lebensdauer)
//   - ein durch Stromausfall zerrissener Slot faellt per CRC auf den vorherigen zurueck
// Jeder Slot belegt genau eine EEPROM-Page (Verschleiss zaehlt pro Page).
// ===========================================================================
#define HOT_MAGIC   0x5B
#define HOT_BASE    192   // Page-Grenze oberhalb des Settings-Blocks
#define HOT_SLOT_SZ 32    // genau eine EEPROM-Page pro Slot
#define HOT_SLOTS   8

struct HotRecord {
    uint8_t  magic;
    uint8_t  chk;      // XOR-Pruefsumme ueber alle Bytes ab Offset 2 (kein echtes CRC;
                       // reicht, um einen zerrissenen Schreibvorgang zu erkennen)
    uint16_t _pad;
    uint32_t seq;      // laufende Nummer; hoechste gueltige = aktueller Datensatz
    float    tripKm;
    float    odoKm;
    float    lastWhKm;
};

static_assert(sizeof(HotRecord) <= HOT_SLOT_SZ, "HotRecord passt nicht in eine Page");
// Waechst EBikeSettings ueber 189 Byte, ueberlappt der Settings-Block den Hot-Bereich.
// Dann HOT_BASE auf die naechste 32-Byte-Grenze erhoehen (Hot-Daten gehen dabei einmalig
// verloren, die Settings bleiben erhalten).
static_assert(STORAGE_ADDR_SETTINGS + sizeof(EBikeSettings) <= HOT_BASE,
              "Settings-Block ueberlappt den Hot-Bereich -> HOT_BASE erhoehen");

static uint8_t  hotSlot = HOT_SLOTS - 1;  // zuletzt beschriebener Slot
static uint32_t hotSeq  = 0;              // dessen Sequenznummer

static uint8_t hot_chk(const HotRecord& r) {
    const uint8_t* b = (const uint8_t*)&r;
    uint8_t c = 0;
    for (size_t i = 2; i < sizeof(HotRecord); i++) c ^= b[i];  // magic + chk auslassen
    return c;
}

// Sucht den neuesten gueltigen Slot und uebernimmt dessen Werte nach p.
static void hot_load() {
    HotRecord best;
    bool found = false;
    for (uint8_t i = 0; i < HOT_SLOTS; i++) {
        HotRecord r;
        store_read(HOT_BASE + (uint16_t)i * HOT_SLOT_SZ, (uint8_t*)&r, sizeof(r));
        if (r.magic != HOT_MAGIC || r.chk != hot_chk(r)) continue;  // leer oder zerrissen
        // Vorzeichenbehafteter Vergleich -> korrekt auch bei Ueberlauf der Sequenznummer
        if (!found || (int32_t)(r.seq - best.seq) > 0) {
            best    = r;
            hotSlot = i;
            found   = true;
        }
    }
    // Kein Hot-Datensatz vorhanden (erster Start nach dem Update): Werte aus dem
    // Settings-Block behalten -> nahtlose Migration, kein Verlust von Trip/Odo.
    if (!found) return;

    hotSeq     = best.seq;
    p.tripKm   = best.tripKm;
    p.odoKm    = best.odoKm;
    p.lastWhKm = best.lastWhKm;
}

// ===========================================================================
// Öffentliche Funktionen
// ===========================================================================

// Schreibt NUR den Hot-Datensatz (eine Page) - fuer Kilometer-Updates.
void storage_save_hot() {
    HotRecord r;
    memset(&r, 0, sizeof(r));
    r.magic    = HOT_MAGIC;
    r.seq      = ++hotSeq;
    r.tripKm   = p.tripKm;
    r.odoKm    = p.odoKm;
    r.lastWhKm = p.lastWhKm;
    r.chk      = hot_chk(r);

    hotSlot = (uint8_t)((hotSlot + 1) % HOT_SLOTS);   // Ring: Verschleiss verteilen
    store_write(HOT_BASE + (uint16_t)hotSlot * HOT_SLOT_SZ, (const uint8_t*)&r, sizeof(r));
    storageHotDirty = false;
}

// Schreibt Settings UND Hot-Datensatz. Der Hot-Satz wird immer mitgeschrieben, damit er
// nie aelter ist als der Settings-Block (beim Laden hat der Hot-Satz Vorrang).
void storage_save() {
    uint16_t sz    = (uint16_t)sizeof(p);
    uint8_t  magic = STORAGE_MAGIC;

    store_write(STORAGE_ADDR_MAGIC,    &magic,              sizeof(magic));
    store_write(STORAGE_ADDR_SIZE,     (const uint8_t*)&sz, sizeof(sz));
    store_write(STORAGE_ADDR_SETTINGS, (const uint8_t*)&p,  sizeof(p));
    storageDirty = false;

    storage_save_hot();
}

void storage_load() {
    uint16_t sz    = 0;
    uint8_t  magic = 0;

    store_read(STORAGE_ADDR_MAGIC, &magic, sizeof(magic));
    if (magic == STORAGE_MAGIC) {
        store_read(STORAGE_ADDR_SIZE, (uint8_t*)&sz, sizeof(sz));
        if (sz == (uint16_t)sizeof(p)) {
            store_read(STORAGE_ADDR_SETTINGS, (uint8_t*)&p, sizeof(p));
        }
    }
    hot_load();   // ueberschreibt Trip/Odo/lastWhKm mit dem frischeren Hot-Datensatz
}

void storage_handler() {
    static uint32_t lastSaveMs = 0;

    // Hot-Datensatz kostet nur eine Page -> darf jederzeit sofort geschrieben werden,
    // auch mitten im Editieren eines Parameters.
    if (storageHotDirty) storage_save_hot();

    if (!storageDirty) return;
    // Normalfall: speichern auf einer Page-Root (menuState % 10 == 0), also nicht
    // mitten im Editieren eines Parameters.
    // Fallback: laeuft man laenger auf einer Param-Subseite, nach 60s trotzdem sichern.
    if (menuState % 10 == 0 || (millis() - lastSaveMs) > 60000) {
        storage_save();
        lastSaveMs = millis();
    }
}

// ===========================================================================
// Test-Funktion (nur FRAM)
// ===========================================================================
#if STORAGE_TYPE == STORAGE_FRAM
void storage_test() {
    // Basis-R/W-Test an Adresse 0 (Wire2 muss vorher per Wire2.begin() initialisiert sein)
    uint8_t v = fram_read8(0);
    if (v == 0x44) {
        Serial.print("Fram value already 0x44");
    } else {
        Serial.print("Fram read != 0x44, value = "); Serial.println(v);
        fram_write8(0, 0x44);
        if (fram_read8(0) == 0x44) {
            Serial.print("Fram write successful, value = "); Serial.println(fram_read8(0));
        } else {
            Serial.print("Fram write not successful, value = "); Serial.println(fram_read8(0));
        }
    }
    Serial.println();
    Serial.println(__FUNCTION__);

    // Hersteller-/Produkt-ID ueber die Reserved Slave ID (0x7C) lesen: Geraeteadresse
    // schreiben, dann per Repeated-Start 3 Byte lesen (12 Bit Manuf. + 12 Bit Produkt).
    Wire2.beginTransmission(FRAM_ID_ADDR);
    Wire2.write((uint8_t)(FRAM_I2C_ADDR << 1));
    if (Wire2.endTransmission(false) == 0 && Wire2.requestFrom((uint8_t)FRAM_ID_ADDR, (uint8_t)3) == 3) {
        uint8_t b0 = Wire2.read(), b1 = Wire2.read(), b2 = Wire2.read();
        uint16_t manuf = ((uint16_t)b0 << 4) | (b1 >> 4);
        uint16_t prod  = ((uint16_t)(b1 & 0x0F) << 8) | b2;
        Serial.print("ManufacturerID: 0x"); Serial.println(manuf, HEX);  // Fujitsu = 0x00A
        Serial.print("     ProductID: 0x"); Serial.println(prod, HEX);
    } else {
        Serial.println("FRAM-ID (0x7C) nicht lesbar");
    }
    Serial.println();
}
#endif // STORAGE_FRAM
