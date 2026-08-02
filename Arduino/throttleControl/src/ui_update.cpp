#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "globals.h"
#include "ui/ui.h"
#include "ui/actions.h"
#include "ui_update.h"

extern uint8_t menuState;

// ---------------------------------------------------------------------------
// Interne Helper
// ---------------------------------------------------------------------------

// Aktive Seite (0..8) aus menuState ableiten:
//   0..9   -> 0 (main)            40..49 -> 4 (pas_battery_par)
//   10..19 -> 1 (torque_par)      50..59 -> 5 (div_parameters)
//   20..29 -> 2 (cadence_par)     60..69 -> 6 (stopwatch)
//   30..39 -> 3 (throttle_curve)  70..79 -> 7 (rear_light)
//                                 80..89 -> 8 (front_light)
static inline uint8_t current_page(void) {
    return menuState / 10;
}

// Zeitformatierung: Sekunden -> "hh:mm:ss"
static void format_hms(uint32_t seconds, char *buf, size_t buflen) {
    uint32_t h = seconds / 3600;
    uint32_t m = (seconds % 3600) / 60;
    uint32_t sec = seconds % 60;
    snprintf(buf, buflen, "%02lu:%02lu:%02lu",
             (unsigned long)h, (unsigned long)m, (unsigned long)sec);
}

// Zeitformatierung: Centisekunden -> "hh:mm:ss.cc"
static void format_hms_cs(uint32_t cs_total, char *buf, size_t buflen) {
    uint32_t cs  = cs_total % 100;
    uint32_t sec = (cs_total / 100) % 60;
    uint32_t m   = (cs_total / 6000) % 60;
    uint32_t h   = cs_total / 360000;
    snprintf(buf, buflen, "%02lu:%02lu:%02lu.%02lu",
             (unsigned long)h, (unsigned long)m, (unsigned long)sec, (unsigned long)cs);
}

// Makros: aktualisieren nur wenn sich Wert geändert hat oder force gesetzt ist.
// Jedes Widget hat eine EIGENE prev-Variable -> mehrere Widgets pro Quelle ok.

#define UPD_LBL_INT(prev, cur, obj, fmt) do {                                  \
        if (force || (prev) != (cur)) {                                        \
            char _b[24];                                                       \
            snprintf(_b, sizeof(_b), (fmt), (int)(cur));                       \
            lv_label_set_text((obj), _b);                                      \
            (prev) = (cur);                                                    \
        }                                                                      \
    } while (0)

#define UPD_LBL_UINT(prev, cur, obj, fmt) do {                                 \
        if (force || (prev) != (cur)) {                                        \
            char _b[24];                                                       \
            snprintf(_b, sizeof(_b), (fmt), (unsigned int)(cur));              \
            lv_label_set_text((obj), _b);                                      \
            (prev) = (cur);                                                    \
        }                                                                      \
    } while (0)

#define UPD_LBL_ULONG(prev, cur, obj, fmt) do {                                \
        if (force || (prev) != (cur)) {                                        \
            char _b[24];                                                       \
            snprintf(_b, sizeof(_b), (fmt), (unsigned long)(cur));             \
            lv_label_set_text((obj), _b);                                      \
            (prev) = (cur);                                                    \
        }                                                                      \
    } while (0)

#define UPD_LBL_FLOAT(prev, cur, obj, fmt) do {                                \
        if (force || (prev) != (cur)) {                                        \
            char _b[24];                                                       \
            snprintf(_b, sizeof(_b), (fmt), (double)(cur));                    \
            lv_label_set_text((obj), _b);                                      \
            (prev) = (cur);                                                    \
        }                                                                      \
    } while (0)

#define UPD_LBL_TIME(prev, cur, obj) do {                                      \
        if (force || (prev) != (cur)) {                                        \
            char _b[16];                                                       \
            format_hms((uint32_t)(cur), _b, sizeof(_b));                       \
            lv_label_set_text((obj), _b);                                      \
            (prev) = (cur);                                                    \
        }                                                                      \
    } while (0)

#define UPD_LBL_TIME_CS(prev, cur, obj) do {                                   \
        if (force || (prev) != (cur)) {                                        \
            char _b[20];                                                       \
            format_hms_cs((uint32_t)(cur), _b, sizeof(_b));                    \
            lv_label_set_text((obj), _b);                                      \
            (prev) = (cur);                                                    \
        }                                                                      \
    } while (0)

#define UPD_BAR(prev, cur, obj) do {                                           \
        if (force || (prev) != (cur)) {                                        \
            lv_bar_set_value((obj), (int32_t)(cur), LV_ANIM_OFF);              \
            (prev) = (cur);                                                    \
        }                                                                      \
    } while (0)

#define UPD_LED(prev, cur, obj) do {                                           \
        if (force || (prev) != (cur)) {                                        \
            if ((cur)) lv_led_on((obj)); else lv_led_off((obj));               \
            (prev) = (cur);                                                    \
        }                                                                      \
    } while (0)

#define UPD_LBL_TEXT(prev, cur, obj, txt0, txt1) do {                          \
        if (force || (prev) != (cur)) {                                        \
            lv_label_set_text((obj), (cur) ? (txt1) : (txt0));                 \
            (prev) = (cur);                                                    \
        }                                                                      \
    } while (0)

#define SET_VISIBLE(obj, show) do {                                             \
        if (show) lv_obj_clear_flag((obj), LV_OBJ_FLAG_HIDDEN);                \
        else      lv_obj_add_flag  ((obj), LV_OBJ_FLAG_HIDDEN);                \
    } while (0)

// Zeigt special_txt wenn cur == special_val, sonst numerisch (fmt mit %u)
#define UPD_LBL_SPECIAL(prev, cur, obj, fmt, special_val, special_txt) do {    \
        if (force || (prev) != (cur)) {                                        \
            if ((cur) == (special_val)) {                                      \
                lv_label_set_text((obj), (special_txt));                       \
            } else {                                                           \
                char _b[24];                                                   \
                snprintf(_b, sizeof(_b), (fmt), (unsigned int)(cur));          \
                lv_label_set_text((obj), _b);                                  \
            }                                                                  \
            (prev) = (cur);                                                    \
        }                                                                      \
    } while (0)

// ---------------------------------------------------------------------------
// Kapazitäts-Korrektur: echter % -> kalibrierter Bar-Wert (lineare Interpolation)
// ---------------------------------------------------------------------------
static uint8_t capacity_to_bar(uint8_t cap) {
    return cap; // SOC-Kurve wird in calc.cpp berechnet, 1:1 ans Display
}

// ---------------------------------------------------------------------------
// Page 0: main
// ---------------------------------------------------------------------------
static void update_page_main(bool force) {
    static float    pv_speed   = -1;
    static uint16_t pv_torque  = 0xFFFF;
    static float    pv_cadence = -1;
    static uint16_t pv_power   = 0xFFFF;
    static uint32_t pv_time    = 0xFFFFFFFFUL;
    static float    pv_temp    = -1000;
    static float    pv_trip    = -1;
    static uint32_t pv_onTime  = 0xFFFFFFFFUL;
    static float    pv_thrV    = -1;
    static float    pv_current = -1000;
    static float    pv_voltage = -1;
    static uint16_t pv_range   = 0xFFFF;
    static uint8_t  pv_support = 0xFF;
    static uint8_t  pv_capL    = 0xFF;  // Label
    static uint8_t  pv_capB    = 0xFF;  // Bar
    static uint8_t  pv_supB    = 0xFF;  // support_bar
    static float    pv_whKm    = -1;
    static float    pv_speedAvg = -1;
    static uint8_t  pv_pwRace  = 0xFF;
    static uint8_t  pv_cruise_sp = 0xFF;

    if (force) lv_label_set_recolor(objects.speed_ui, true);

    if (force || pv_speed != s.speed || pv_cruise_sp != s.cruise) {
        char _b[20];
        if (s.speed > 99.9f) {
            snprintf(_b, sizeof(_b), "%03d", (int)s.speed);
        } else {
            int v = (int)(s.speed * 10.0f + 0.5f);
            if (s.cruise) snprintf(_b, sizeof(_b), "%d#ff0000 .#%d", v/10, v%10);
            else          snprintf(_b, sizeof(_b), "%d.%d",           v/10, v%10);
        }
        lv_label_set_text(objects.speed_ui, _b);
        pv_speed    = s.speed;
        pv_cruise_sp = s.cruise;
    }
    UPD_LBL_INT  (pv_torque,  s.torque,       objects.torque_ui,  "%d");
    UPD_LBL_FLOAT(pv_cadence, s.cadenceRpm,   objects.cadence_ui, "%.0f");
    UPD_LBL_INT  (pv_power,   s.power,        objects.power_ui,   "%d");
    UPD_LBL_TIME (pv_time,    s.time,         objects.time_ui);
    UPD_LBL_FLOAT(pv_temp,    s.temp,         objects.temp_ui,    "%.1f");
    UPD_LBL_FLOAT(pv_trip,  (p.tripKm > 9999.9f ? 9999.9f : p.tripKm), objects.trip_ui,  "%.1f");
    UPD_LBL_TIME (pv_onTime,  s.onTime,       objects.on_time_ui);
    UPD_LBL_FLOAT(pv_thrV,    s.throttleVolt, objects.throttle_ui,"%3.1f");
    UPD_LBL_FLOAT(pv_current, s.current,      objects.current_ui, "%.1f");
    UPD_LBL_FLOAT(pv_voltage, s.voltage,      objects.voltage_ui, "%.1f");
    UPD_LBL_UINT (pv_range,   s.range,        objects.range_ui,   "%u");
    UPD_LBL_UINT (pv_support, p.supportLevel, objects.support_ui, "%u");
    UPD_LBL_UINT (pv_capL,    s.capacity,     objects.capacity_ui,"%u");
    UPD_LBL_FLOAT(pv_whKm,    s.whKm,         objects.wh_km_ui,   "%.1f");

    UPD_BAR(pv_capB, capacity_to_bar(s.capacity), objects.capacity_bar);
    UPD_BAR(pv_supB, p.supportLevel, objects.support_bar);
    UPD_LBL_FLOAT(pv_speedAvg, s.speedAvg, objects.speed_avg_ui, "%.1f");

    if (force || pv_pwRace != p.pwRace) {
        lv_obj_set_style_text_color(objects.kmh_ui,
            lv_color_hex(p.pwRace ? 0xFF0000 : 0xFFFFFF),
            LV_PART_MAIN | LV_STATE_DEFAULT);
        pv_pwRace = p.pwRace;
    }
}

// ---------------------------------------------------------------------------
// Page 1: torque_par
// ---------------------------------------------------------------------------
static void update_page_torque(bool force) {
    static float    pv_speed   = -1;
    static uint16_t pv_power   = 0xFFFF;
    static float    pv_thrV    = -1;
    static float    pv_cadence = -1;
    static uint16_t pv_tZero   = 0xFFFF;
    static uint16_t pv_torque  = 0xFFFF;
    static float    pv_current = -1000;
    static uint16_t pv_tMax    = 0xFFFF;
    static uint16_t pv_tDead   = 0xFFFF;
    static float    pv_tRise   = -1;
    static float    pv_tFall   = -1;
    static uint16_t pv_tIdle   = 0xFFFF;

    if (force || pv_speed != s.speed) {
        char _b[8];
        if (s.speed > 99.9f) snprintf(_b, sizeof(_b), "%03d",   (int)s.speed);
        else                  snprintf(_b, sizeof(_b), "%.1f",   (double)s.speed);
        lv_label_set_text(objects.speed1_ui, _b);
        pv_speed = s.speed;
    }
    UPD_LBL_INT  (pv_power,   s.power,              objects.power1_ui,             "%d");
    UPD_LBL_FLOAT(pv_thrV,    s.throttleVolt,       objects.throttle1_ui,          "%3.1f");
    UPD_LBL_FLOAT(pv_cadence, s.cadenceRpm,          objects.cadence1_ui,           "%.0f");
    UPD_LBL_UINT (pv_tZero,   p.torqueZero,         objects.torque_zero_ui,        "%u");
    UPD_LBL_INT  (pv_torque,  s.torque,             objects.torque1_ui,            "%d");
    UPD_LBL_FLOAT(pv_current, s.current,            objects.current1_ui,           "%.1f");
    UPD_LBL_UINT (pv_tMax,    p.torqueMax,          objects.torque_max_ui,         "%u");
    UPD_LBL_UINT (pv_tDead,   p.torqueDeadband,     objects.torque_deadband_ui,    "%u");
    UPD_LBL_FLOAT(pv_tRise,   p.torqueFilterRise,   objects.torque_filter_rise_ui, "%.3f");
    UPD_LBL_FLOAT(pv_tFall,   p.torqueFilterFall,   objects.torque_filter_fall_ui, "%.3f");
    UPD_LBL_UINT (pv_tIdle,   p.torqueIdleMs,       objects.torque_idle_ui,        "%u");
}

// ---------------------------------------------------------------------------
// Page 2: cadence_par
// ---------------------------------------------------------------------------
static void update_page_cadence(bool force) {
    static float    pv_speed   = -1;
    static uint16_t pv_power   = 0xFFFF;
    static float    pv_thrV    = -1;
    static float    pv_cadence = -1;
    static uint8_t  pv_ppr     = 0xFF;
    static uint16_t pv_torque  = 0xFFFF;
    static float    pv_current = -1000;
    static uint32_t pv_cTout   = 0xFFFFFFFFUL;
    static float    pv_cMax    = -1;
    static float    pv_cFilt   = -1;
    static float    pv_cLim    = -1;
    static uint8_t  pv_cGateP  = 0xFF;
    static uint16_t pv_cGateT  = 0xFFFF;

    if (force || pv_speed != s.speed) {
        char _b[8];
        if (s.speed > 99.9f) snprintf(_b, sizeof(_b), "%03d",   (int)s.speed);
        else                  snprintf(_b, sizeof(_b), "%.1f",   (double)s.speed);
        lv_label_set_text(objects.speed2_ui, _b);
        pv_speed = s.speed;
    }
    UPD_LBL_INT  (pv_power,   s.power,          objects.power2_ui,          "%d");
    UPD_LBL_FLOAT(pv_thrV,    s.throttleVolt,   objects.throttle2_ui,       "%3.1f");
    UPD_LBL_FLOAT(pv_cadence, s.cadenceRpm,      objects.cadence2_ui,        "%.0f");
    UPD_LBL_UINT (pv_ppr,     p.pulsesPerRev,   objects.pulses_per_rev_ui,  "%u");
    UPD_LBL_INT  (pv_torque,  s.torque,         objects.torque2_ui,         "%d");
    UPD_LBL_FLOAT(pv_current, s.current,        objects.current2_ui,        "%.1f");
    UPD_LBL_ULONG(pv_cTout,   p.cadenceTimeoutMs, objects.cadence_timeout_ui,"%lu");
    UPD_LBL_FLOAT(pv_cMax,    p.cadenceMaxRpm,    objects.cadence_max_ui,    "%.1f");
    UPD_LBL_FLOAT(pv_cFilt,   p.cadenceFilterAlpha,objects.cadence_filter_ui,"%.2f");
    UPD_LBL_FLOAT(pv_cLim,    p.cadenceMaxLimit,  objects.cadence_limit_ui,  "%.1f");
    UPD_LBL_UINT (pv_cGateP,  p.cadenceGatePulses,objects.cadence_gate_pulses_ui,"%u");
    UPD_LBL_UINT (pv_cGateT,  p.cadenceGateMs,    objects.cadence_gate_time_ui,  "%u");
}

// ---------------------------------------------------------------------------
// Page 3: throttle_curve
// ---------------------------------------------------------------------------
static void update_page_curve(bool force) {
    static float    pv_speed   = -1;
    static uint16_t pv_power   = 0xFFFF;
    static float    pv_thrV    = -1;
    static float    pv_cadence = -1;
    static float    pv_y25     = -1;
    static uint16_t pv_torque  = 0xFFFF;
    static float    pv_current = -1000;
    static float    pv_y50     = -1;
    static float    pv_y75     = -1;
    static float    pv_rUpLow  = -1;
    static float    pv_rThr    = -1;
    static float    pv_rUpHi   = -1;
    static float    pv_rDown   = -1;
    static float    pv_offset  = -1;

    if (force || pv_speed != s.speed) {
        char _b[8];
        if (s.speed > 99.9f) snprintf(_b, sizeof(_b), "%03d",   (int)s.speed);
        else                  snprintf(_b, sizeof(_b), "%.1f",   (double)s.speed);
        lv_label_set_text(objects.speed3_ui, _b);
        pv_speed = s.speed;
    }
    UPD_LBL_INT  (pv_power,   s.power,         objects.power3_ui,         "%d");
    UPD_LBL_FLOAT(pv_thrV,    s.throttleVolt,  objects.throttle3_ui,      "%3.1f");
    UPD_LBL_FLOAT(pv_cadence, s.cadenceRpm,    objects.cadence3_ui,       "%.0f");
    UPD_LBL_FLOAT(pv_y25,     p.curveY25,      objects.curve_y25_ui,      "%.2f");
    UPD_LBL_INT  (pv_torque,  s.torque,        objects.torque3_ui,        "%d");
    UPD_LBL_FLOAT(pv_current, s.current,       objects.current3_ui,       "%.1f");
    UPD_LBL_FLOAT(pv_y50,     p.curveY50,      objects.curve_y50_ui,      "%.2f");
    UPD_LBL_FLOAT(pv_y75,     p.curveY75,      objects.curve75_ui,        "%.2f");
    UPD_LBL_FLOAT(pv_rUpLow,  p.rampUpLow,     objects.ramp_up_low,       "%.3f");
    UPD_LBL_FLOAT(pv_rThr,    p.rampThreshold, objects.ramp_threshold_ui, "%.2f");
    UPD_LBL_FLOAT(pv_rUpHi,   p.rampUpHigh,    objects.ramp_up_high,      "%.3f");
    UPD_LBL_FLOAT(pv_rDown,   p.rampDown,      objects.ramp_down_ui,      "%.3f");
    UPD_LBL_FLOAT(pv_offset,  p.curveOffset,   objects.offset_ui,         "%.2f");
}

// ---------------------------------------------------------------------------
// Page 4: pas_battery_par
// ---------------------------------------------------------------------------
static void update_page_pas_batt(bool force) {
    static float    pv_speed   = -1;
    static uint16_t pv_power   = 0xFFFF;
    static float    pv_thrV    = -1;
    static float    pv_cadence = -1;
    static uint8_t  pv_aPow    = 0xFF;
    static uint16_t pv_torque  = 0xFFFF;
    static float    pv_current = -1000;
    static float    pv_aRamp   = -1;
    static uint8_t  pv_aMode   = 0xFF;
    static float    pv_vMin    = -1;
    static float    pv_vMax    = -1;
    static uint8_t  pv_cruise  = 0xFF;
    static float    pv_iMax    = -1;
    static float    pv_capMax  = -1;
    static float    pv_aRampD  = -1;
    static float    pv_aDead   = -1;

    if (force || pv_speed != s.speed) {
        char _b[8];
        if (s.speed > 99.9f) snprintf(_b, sizeof(_b), "%03d",   (int)s.speed);
        else                  snprintf(_b, sizeof(_b), "%.1f",   (double)s.speed);
        lv_label_set_text(objects.speed4_ui, _b);
        pv_speed = s.speed;
    }
    UPD_LBL_INT  (pv_power,   s.power,         objects.power4_ui,        "%d");
    UPD_LBL_FLOAT(pv_thrV,    s.throttleVolt,  objects.throttle4_ui,     "%3.1f");
    UPD_LBL_FLOAT(pv_cadence, s.cadenceRpm,    objects.cadence4_ui,      "%.0f");
    UPD_LBL_UINT (pv_aPow,    p.cruisePower,   objects.cruise_power_ui,  "%u");
    UPD_LBL_INT  (pv_torque,  s.torque,        objects.torque4_ui,       "%d");
    UPD_LBL_FLOAT(pv_current, s.current,       objects.current4_ui,      "%.1f");
    UPD_LBL_FLOAT(pv_aRamp,   p.cruiseRampUp,  objects.cruise_ramp_up_ui,"%.3f");
    UPD_LBL_FLOAT(pv_aRampD,  p.cruiseRampDown,objects.cruise_ramp_down_ui,"%.3f");
    UPD_LBL_FLOAT(pv_aDead,   p.cruiseDeadband,objects.cruise_deadband_ui,"%.2f");

    UPD_LBL_UINT (pv_aMode,   p.pasMode,       objects.pas_mode_ui, "%u");

    UPD_LBL_FLOAT(pv_vMin,    p.voltageMin,    objects.voltage_min_ui,  "%.1f");
    UPD_LBL_FLOAT(pv_vMax,    p.voltageMax,    objects.voltage_max_ui,  "%.1f");
    UPD_LBL_FLOAT(pv_iMax,    p.currentMax,    objects.current_max_ui,  "%.1f");
    UPD_LBL_FLOAT(pv_capMax,  p.capacityMax,   objects.capacity_max_ui, "%.1f");

    // Indicator-LED: rot wenn Cruise-Taste aktiv
    if (force) lv_led_set_color(objects.cruise_indicator_ui, lv_color_hex(0xFF0000));
    UPD_LED(pv_cruise, s.cruise, objects.cruise_indicator_ui);
}

// ---------------------------------------------------------------------------
// Page 5: div_parameters
// ---------------------------------------------------------------------------
static void update_page_div(bool force) {
    static float    pv_spStr   = -1;
    static float    pv_asStr   = -1;
    static uint16_t pv_pwStr   = 0xFFFF;
    static float    pv_spRac   = -1;
    static float    pv_asRac   = -1;
    static uint16_t pv_pwRac   = 0xFFFF;
    static uint16_t pv_wheel   = 0xFFFF;
    static float    pv_trip    = -1;
    static uint32_t pv_odo     = 0xFFFFFFFFUL;
    static uint8_t  pv_pwRace  = 0xFF;

    if (force || pv_pwRace != p.pwRace) {
        bool show = (p.pwRace != 0);
        SET_VISIBLE(objects.wheel_circle,           show);
        SET_VISIBLE(objects.wheel_circ_ui,          show);
        SET_VISIBLE(objects.wheel_mm,               show);
        SET_VISIBLE(objects.race_line,              show);
        SET_VISIBLE(objects.speed_street,           show);
        SET_VISIBLE(objects.speed_limit_street_ui,  show);
        SET_VISIBLE(objects.speed_kmh,              show);
        SET_VISIBLE(objects.cruise_street,          show);
        SET_VISIBLE(objects.cruise_limit_street_ui, show);
        SET_VISIBLE(objects.cruise_kmh,             show);
        SET_VISIBLE(objects.power_street,           show);
        SET_VISIBLE(objects.power_limit_street_ui,  show);
        SET_VISIBLE(objects.power_w,                show);
        SET_VISIBLE(objects.speed_race,             show);
        SET_VISIBLE(objects.speed_limit_race_ui,    show);
        SET_VISIBLE(objects.speed_race_kmh,         show);
        SET_VISIBLE(objects.cruise_race,            show);
        SET_VISIBLE(objects.cruise_limit_race_ui,   show);
        SET_VISIBLE(objects.cruise_race_kmh,        show);
        SET_VISIBLE(objects.power_race,             show);
        SET_VISIBLE(objects.power_limit_race_ui,    show);
        SET_VISIBLE(objects.power_race_kmh,         show);
        pv_pwRace = p.pwRace;
    }

    UPD_LBL_FLOAT(pv_spStr, p.speedLimitStreet,  objects.speed_limit_street_ui,  "%.1f");
    UPD_LBL_FLOAT(pv_asStr, p.cruiseLimitStreet, objects.cruise_limit_street_ui, "%.1f");
    UPD_LBL_UINT (pv_pwStr, p.powerLimitStreet,  objects.power_limit_street_ui,  "%u");
    UPD_LBL_FLOAT(pv_spRac, p.speedLimitRace,    objects.speed_limit_race_ui,    "%.1f");
    UPD_LBL_FLOAT(pv_asRac, p.cruiseLimitRace,   objects.cruise_limit_race_ui,   "%.1f");
    UPD_LBL_UINT (pv_pwRac, p.powerLimitRace,    objects.power_limit_race_ui,    "%u");
    UPD_LBL_UINT (pv_wheel, p.wheelCircle,       objects.wheel_circ_ui,          "%u");
    UPD_LBL_FLOAT(pv_trip,  (p.tripKm > 9999.9f ? 9999.9f : p.tripKm), objects.trip1_ui, "%.1f");
    UPD_LBL_ULONG(pv_odo,   (unsigned long)(p.odoKm > 999999.0f ? 999999.0f : p.odoKm), objects.odo_ui, "%lu");
}

// ---------------------------------------------------------------------------
// Page 6: stopwatch
// ---------------------------------------------------------------------------
static void update_page_stopwatch(bool force) {
    static uint32_t pv_stop = 0xFFFFFFFFUL;
    static uint32_t pv_time = 0xFFFFFFFFUL;

    // stopwatchTicks wird als hh:mm:ss angezeigt (bei Bedarf auf Sekunden umrechnen)
    UPD_LBL_TIME_CS(pv_stop, p.stopwatchTicks, objects.stoptime_ui);
    UPD_LBL_TIME(pv_time, s.time,           objects.time1_ui);
}

// ---------------------------------------------------------------------------
// Page 7: rear_light
// ---------------------------------------------------------------------------
static void update_page_rear(bool force) {
    static uint8_t  pv_onoff         = 0xFF;
    static uint16_t pv_dim           = 0xFFFF;
    static uint8_t  pv_braking       = 0xFF;
    static uint8_t  pv_streetRestart = 0xFF;
    static uint8_t  pv_pwRace        = 0xFF;

    if (force) lv_led_set_color(objects.rear_led_ui, lv_color_hex(0xFF0000)); // rot wenn an

    if (force || pv_pwRace != p.pwRace) {
        bool show = (p.pwRace != 0);
        SET_VISIBLE(objects.on_restart_line,  show);
        SET_VISIBLE(objects.on_restart,       show);
        SET_VISIBLE(objects.street_par,       show);
        SET_VISIBLE(objects.street_on_restart,show);
        pv_pwRace = p.pwRace;
    }

    if (force || pv_onoff != p.rearLightOnOff || pv_dim != p.rearLightDim || pv_braking != (uint8_t)s.braking) {
        bool rearOn = p.rearLightOnOff && p.rearLightDim >= 1;
        if (!rearOn) {
            lv_led_off(objects.rear_led_ui);
        } else if (s.braking) {
            lv_led_set_brightness(objects.rear_led_ui, 255); // Bremslicht: voll hell
        } else {
            // 1–100% dim → 89–255 brightness (35%–100%), immer sichtbar vs. off-grau
            uint8_t bright = (p.rearLightDim >= 100) ? 255 : (uint8_t)(89 + (uint32_t)(255 - 89) * p.rearLightDim / 100);
            lv_led_set_brightness(objects.rear_led_ui, bright);
        }
        lv_label_set_text(objects.on_off_rear_ui, rearOn ? "ON" : "OFF");
        lv_label_set_text_fmt(objects.rear_dim_ui, "%u", p.rearLightDim);
        pv_onoff   = p.rearLightOnOff;
        pv_dim     = p.rearLightDim;
        pv_braking = (uint8_t)s.braking;
    }
    UPD_LBL_TEXT (pv_streetRestart, p.streetOnRestart, objects.street_on_restart, "NO", "YES");
}

// ---------------------------------------------------------------------------
// Page 8: front_light
// ---------------------------------------------------------------------------
static void update_page_front(bool force) {
    static uint8_t  pv_onoff  = 0xFF;
    static uint16_t pv_dim    = 0xFFFF;
    static uint16_t pv_bkDim  = 0xFFFF;
    static uint16_t pv_bkOn   = 0xFFFF;

    if (force) lv_led_set_color(objects.front_led_ui, lv_color_hex(0xFFFFFF)); // weiss wenn an

    if (force || pv_onoff != p.frontLightOnOff || pv_dim != p.frontLightDim) {
        bool frontOn = p.frontLightOnOff && p.frontLightDim >= 1;
        if (!frontOn) {
            lv_led_off(objects.front_led_ui);
        } else {
            // 1–100% dim → 89–255 brightness (35%–100%), immer sichtbar vs. off-grau
            uint8_t bright = (p.frontLightDim >= 100) ? 255 : (uint8_t)(89 + (uint32_t)(255 - 89) * p.frontLightDim / 100);
            lv_led_set_brightness(objects.front_led_ui, bright);
        }
        lv_label_set_text(objects.on_off_front_ui, frontOn ? "ON" : "OFF");
        lv_label_set_text_fmt(objects.front_dim_ui, "%u", p.frontLightDim);
        pv_onoff = p.frontLightOnOff;
        pv_dim   = p.frontLightDim;
    }

    if (force || pv_bkDim != p.backlightDim) {
        if (p.backlightDim == 101) lv_obj_add_flag  (objects.bkldim, LV_OBJ_FLAG_HIDDEN);
        else                       lv_obj_clear_flag(objects.bkldim, LV_OBJ_FLAG_HIDDEN);
    }
    UPD_LBL_SPECIAL(pv_bkDim, p.backlightDim,    objects.dim_backlight_ui,    "%u", 101,  "AUTO");

    if (force || pv_bkOn != p.onTimeBacklight) {
        if (p.onTimeBacklight == 1000) lv_obj_add_flag  (objects.sec, LV_OBJ_FLAG_HIDDEN);
        else                           lv_obj_clear_flag(objects.sec, LV_OBJ_FLAG_HIDDEN);
    }
    UPD_LBL_SPECIAL(pv_bkOn,  p.onTimeBacklight, objects.on_time_backlight_ui,"%u", 1000, "ON");
}

// ---------------------------------------------------------------------------
// Edit-Highlight: aktives Parameter-Label rot, alle anderen weiss
// ---------------------------------------------------------------------------
static lv_obj_t* get_edit_label(uint8_t state) {
    switch (state) {
        case  1: return objects.support_ui;
        case 11: return objects.torque_zero_ui;
        case 12: return objects.torque_max_ui;
        case 13: return objects.torque_deadband_ui;
        case 14: return objects.torque_filter_rise_ui;
        case 15: return objects.torque_filter_fall_ui;
        case 16: return objects.torque_idle_ui;
        case 21: return objects.pulses_per_rev_ui;
        case 22: return objects.cadence_timeout_ui;
        case 23: return objects.cadence_max_ui;
        case 24: return objects.cadence_filter_ui;
        case 25: return objects.cadence_limit_ui;
        case 26: return objects.cadence_gate_pulses_ui;
        case 27: return objects.cadence_gate_time_ui;
        case 31: return objects.curve_y25_ui;
        case 32: return objects.curve_y50_ui;
        case 33: return objects.curve75_ui;
        case 34: return objects.offset_ui;
        case 35: return objects.ramp_up_low;
        case 36: return objects.ramp_up_high;
        case 37: return objects.ramp_threshold_ui;
        case 38: return objects.ramp_down_ui;
        case 41: return objects.cruise_power_ui;
        case 42: return objects.cruise_ramp_up_ui;
        case 43: return objects.cruise_ramp_down_ui;
        case 44: return objects.cruise_deadband_ui;
        case 45: return objects.pas_mode_ui;
        case 46: return objects.voltage_min_ui;
        case 47: return objects.voltage_max_ui;
        case 48: return objects.current_max_ui;
        case 49: return objects.capacity_max_ui;
        case 51: return objects.wheel_circ_ui;
        case 52: return objects.speed_limit_street_ui;
        case 53: return objects.cruise_limit_street_ui;
        case 54: return objects.power_limit_street_ui;
        case 55: return objects.speed_limit_race_ui;
        case 56: return objects.cruise_limit_race_ui;
        case 57: return objects.power_limit_race_ui;
        case 61: return objects.time1_ui;
        case 71: return objects.rear_dim_ui;
        case 72: return objects.street_on_restart;
        case 81: return objects.front_dim_ui;
        case 82: return objects.dim_backlight_ui;
        case 83: return objects.on_time_backlight_ui;
        default: return NULL;
    }
}

static void update_edit_highlight(bool page_changed) {
    static uint8_t prev_state = 0xFF;
    if (menuState == prev_state) return;

    // Vorheriges Label zurück auf Weiss — nur wenn kein Seitenwechsel
    // (bei Seitenwechsel sind alte Objekte bereits gelöscht)
    if (!page_changed) {
        lv_obj_t *prev_lbl = get_edit_label(prev_state);
        if (prev_lbl) {
            lv_obj_set_style_text_color(prev_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    // Neues Label rot markieren
    lv_obj_t *curr_lbl = get_edit_label(menuState);
    if (curr_lbl) {
        lv_obj_set_style_text_color(curr_lbl, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    prev_state = menuState;
}

// ---------------------------------------------------------------------------
// Öffentliche Funktion
// ---------------------------------------------------------------------------
void ui_update(void) {
    static int8_t prevPage = -1;

    uint8_t page = current_page();
    bool force = (page != prevPage);
    prevPage = page;

    switch (page) {
        case 0: update_page_main(force);        break;
        case 1: update_page_torque(force);      break;
        case 2: update_page_cadence(force);     break;
        case 3: update_page_curve(force);       break;
        case 4: update_page_pas_batt(force); break;
        case 5: update_page_div(force);         break;
        case 6: update_page_stopwatch(force);   break;
        case 7: update_page_rear(force);        break;
        case 8: update_page_front(force);       break;
        default: /* unbekannte Page -> nichts tun */ break;
    }

    update_edit_highlight(force);
}