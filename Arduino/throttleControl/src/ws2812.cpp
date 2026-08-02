#include <Arduino.h>
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.h"

// ---------------------------------------------------------------------------
// WS2812-PIO-Programm (Byte-fuer-Byte identisch zum offiziellen pico-examples
// "ws2812.pio", 800 kHz, T1=2 / T2=5 / T3=3 -> 10 PIO-Zyklen pro Bit).
// Side-set steuert die Datenleitung, die Timings stecken in den [n]-Delays.
//
//   bitloop:
//     out x, 1        side 0 [T3-1]   ; Bit aus OSR holen, Leitung low halten
//     jmp !x do_zero  side 1 [T1-1]   ; Leitung high; ist das Bit 0 -> frueh runter
//   do_one:
//     jmp  bitloop    side 1 [T2-1]   ; 1-Bit: lange high
//   do_zero:
//     nop             side 0 [T2-1]   ; 0-Bit: frueh low
// ---------------------------------------------------------------------------
static const uint16_t ws2812_program_instructions[] = {
    0x6221, //  0: out    x, 1            side 0 [2]
    0x1123, //  1: jmp    !x, 3           side 1 [1]
    0x1400, //  2: jmp    0               side 1 [4]
    0xa442, //  3: nop                    side 0 [4]
};

static const struct pio_program ws2812_program = {
    .instructions = ws2812_program_instructions,
    .length       = 4,
    .origin       = -1,
};

static PIO  s_pio = pio0;
static uint s_sm  = 0;

void ws2812_begin(uint8_t pin) {
    uint offset = pio_add_program(s_pio, &ws2812_program);

    pio_gpio_init(s_pio, pin);
    pio_sm_set_consecutive_pindirs(s_pio, s_sm, pin, 1, true);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + 3);
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_sideset_pins(&c, pin);
    // 24 Bit pro LED (GRB), MSB zuerst, Autopull nach 24 Bit.
    sm_config_set_out_shift(&c, false /*shift_left*/, true /*autopull*/, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // Bitrate 800 kHz: das Programm braucht 10 PIO-Zyklen pro Bit. Teiler aus der
    // tatsaechlichen Systemtaktfrequenz berechnen -> unabhaengig vom CPU-Takt.
    float div = (float)clock_get_hz(clk_sys) / (800000.0f * 10.0f);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(s_pio, s_sm, offset, &c);
    pio_sm_set_enabled(s_pio, s_sm, true);
}

void ws2812_set(uint8_t r, uint8_t g, uint8_t b) {
    // WS2812 erwartet die Reihenfolge GRB. Autopull zieht die oberen 24 Bit des
    // 32-Bit-Worts -> Farbwert um 8 nach links schieben (linksbuendig).
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;
    pio_sm_put_blocking(s_pio, s_sm, grb << 8u);
}
