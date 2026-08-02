# -*- coding: utf-8 -*-
"""
Throttle Control Visualisierung - E-Bike Motorcontroller
Physikalisch korrekte Darstellung:
  - Kadenz: diskrete digitale Pulse (Interrupt) -> instantRpm (Treppe) -> LP-Filter
  - Drehmoment: analoges Sinussignal (2x pro Umdrehung, Minimum bei 6/12 Uhr)
"""

import numpy as np
import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec

# === Farbpalette =============================================================
C_TORQUE   = '#E06040'
C_CADENCE  = '#40A0D0'
C_PULSE    = '#80FFFF'
C_INST_RPM = '#80DDFF'
C_CURVE    = '#50C878'
C_THROTTLE = '#F0A020'
C_TARGET   = '#9090FF'
C_LINEAR   = '#777799'
C_DEAD     = '#FFDD44'
DARK_BG    = '#1A1A2E'
PANEL_BG   = '#16213E'

plt.rcParams.update({
    'figure.facecolor': DARK_BG, 'axes.facecolor': PANEL_BG,
    'axes.edgecolor': '#555577', 'axes.labelcolor': '#CCCCEE',
    'axes.titlecolor': '#EEEEFF', 'xtick.color': '#AAAACC',
    'ytick.color': '#AAAACC', 'grid.color': '#333355', 'grid.alpha': 0.5,
    'text.color': '#DDDDFF', 'legend.facecolor': '#1A1A2E',
    'legend.edgecolor': '#444466', 'legend.framealpha': 0.85,
    'font.size': 9, 'axes.titlesize': 10, 'axes.labelsize': 9,
})

# === Default-Parameter (aus globals.h) =======================================
P = dict(
    curveY25           = 0.15,
    curveY50           = 0.40,
    curveY75           = 0.70,
    torqueFilterRise   = 0.40,
    torqueFilterFall   = 0.02,
    cadenceFilterAlpha = 0.05,
    rampUpLow          = 0.008,
    rampUpHigh         = 0.025,
    rampThreshold      = 0.50,
    rampDown           = 0.050,
    supportLevel       = 6,
    cadenceMode        = 1,
    cadenceMaxRpm      = 120.0,
    cadenceTimeoutMs   = 500,
    pulsesPerRev       = 12,
    cadenceMaxLimit    = 200.0,
)

# === Hilfsfunktionen =========================================================

def apply_curve(x, y25, y50, y75):
    x = np.clip(x, 0.0, 1.0)
    return np.where(x <= 0.25, (x / 0.25) * y25,
           np.where(x <= 0.50, y25 + ((x - 0.25) / 0.25) * (y50 - y25),
           np.where(x <= 0.75, y50 + ((x - 0.50) / 0.25) * (y75 - y50),
                               y75 + ((x - 0.75) / 0.25) * (1.0 - y75))))

def add_volt_axis(ax, ymin=0.0, ymax=1.0):
    ax2 = ax.twinx()
    ax2.set_ylim(ymin * 5.0, ymax * 5.0)
    ax2.set_ylabel('Throttle [V]', color='#AAAACC', fontsize=8)
    ax2.tick_params(colors='#AAAACC', labelsize=8)
    ax2.spines['right'].set_color('#555577')
    return ax2

def asymm_filter(raw, alpha_rise, alpha_fall):
    f = np.zeros_like(raw)
    for i in range(1, len(raw)):
        a = alpha_rise if raw[i] > f[i - 1] else alpha_fall
        f[i] = a * raw[i] + (1.0 - a) * f[i - 1]
    return f

def lp_filter(raw, alpha):
    f = np.zeros_like(raw)
    for i in range(1, len(raw)):
        f[i] = alpha * raw[i] + (1.0 - alpha) * f[i - 1]
    return f

def simulate_ramp(targets, rup_low, rup_high, r_thr, rdown):
    thr, out = 0.0, np.zeros(len(targets))
    for i, tg in enumerate(targets):
        if tg == 0.0:
            thr = 0.0
        elif tg > thr:
            thr = min(thr + (rup_high if tg > r_thr else rup_low), 1.0)
        else:
            thr = max(thr - rdown, 0.0)
        out[i] = thr
    return out

def simulate_cadence_pulses(t_arr, rpm_profile, ppr, cadence_max_rpm,
                             cadence_max_limit, alpha, timeout_ms):
    """
    Simuliert echte diskrete Pulse (digitaler Interrupt) des Kadenz-Sensors.
    Gibt zurueck: pulse_arr, instant_rpm_arr, filtered_norm_arr
    """
    n = len(t_arr)
    fs = 1.0 / (t_arr[1] - t_arr[0])
    pulse_spacing_rad = 2.0 * np.pi / ppr
    timeout_steps = int(timeout_ms / 1000.0 * fs)

    pulses       = np.zeros(n)
    instant_rpm  = np.zeros(n)
    filt_cad     = np.zeros(n)

    crank_angle     = 0.0
    prev_pulse_crank = 0.0
    last_pulse_step = -9999
    cur_inst_rpm    = 0.0

    for i in range(1, n):
        dt = 1.0 / fs
        d_angle = 2.0 * np.pi * rpm_profile[i] / 60.0 * dt
        crank_angle += d_angle

        while (crank_angle - prev_pulse_crank) >= pulse_spacing_rad:
            steps_since = i - last_pulse_step
            dt_pulse = steps_since / fs
            if dt_pulse >= 0.005:
                new_rpm = 60.0 / (dt_pulse * ppr)
                if new_rpm < cadence_max_limit:
                    cur_inst_rpm = new_rpm
            pulses[i] = 1
            last_pulse_step = i
            prev_pulse_crank += pulse_spacing_rad

        instant_rpm[i] = cur_inst_rpm

        steps_since_pulse = i - last_pulse_step
        if steps_since_pulse > timeout_steps:
            filt_cad[i] = 0.0
            cur_inst_rpm = 0.0
        else:
            safe = cur_inst_rpm / cadence_max_rpm
            filt_cad[i] = alpha * safe + (1.0 - alpha) * filt_cad[i - 1]

    return pulses, instant_rpm, np.clip(filt_cad, 0.0, 1.0)

# === Figur-Layout ============================================================
# Zeile 0: Kurve + Support (2 Spalten)
# Zeile 1: Torque-Filter (links) + Kadenz-Pulses mit 2 Sub-Zeilen (rechts)
# Zeile 2: Ramp-Dynamik (breit)
# Zeile 3: Vollstaendige Simulation (breit)

fig = plt.figure(figsize=(17, 24))
fig.patch.set_facecolor(DARK_BG)

gs_outer = gridspec.GridSpec(4, 2, figure=fig,
                              height_ratios=[1.1, 1.15, 0.95, 1.5],
                              hspace=0.52, wspace=0.36,
                              left=0.07, right=0.93, top=0.94, bottom=0.065)

ax_curve   = fig.add_subplot(gs_outer[0, 0])
ax_support = fig.add_subplot(gs_outer[0, 1])
ax_tfilt   = fig.add_subplot(gs_outer[1, 0])

# Kadenz-Panel: 2 Sub-Achsen (oben: Pulse; unten: RPM + Filter)
gs_cad = gridspec.GridSpecFromSubplotSpec(2, 1, subplot_spec=gs_outer[1, 1],
                                           height_ratios=[1, 3], hspace=0.08)
ax_pulses = fig.add_subplot(gs_cad[0])
ax_cfilt  = fig.add_subplot(gs_cad[1], sharex=ax_pulses)

ax_ramp = fig.add_subplot(gs_outer[2, :])
ax_sim  = fig.add_subplot(gs_outer[3, :])

fig.suptitle(
    'Throttle Control  --  Parametereinfluss & Signalfluss\n'
    'E-Bike Motorcontroller  |  RP2350  |  MCP4725 DAC (12-bit, 0-5 V)',
    fontsize=13, fontweight='bold', color='#EEEEFF', y=0.975)

x_norm = np.linspace(0, 1, 500)

# =============================================================================
# Panel 1 -- Throttle-Kurve
# =============================================================================
ax = ax_curve
ax.set_title('1  Throttle-Kurve  (piecewise linear, 5 Stuetzpunkte)', pad=7)
ax.set_xlabel('Eingang  (norm. 0..1)')
ax.set_ylabel('Ausgang  (norm. 0..1)')
ax.set_xlim(0, 1); ax.set_ylim(-0.04, 1.08)
ax.grid(True, linestyle='--')

ax.plot([0, 1], [0, 1], '--', color=C_LINEAR, lw=1.2, label='Linear (Referenz)')
y_def = apply_curve(x_norm, P['curveY25'], P['curveY50'], P['curveY75'])
ax.plot(x_norm, y_def, color=C_CURVE, lw=2.5, label='Default-Kurve')

pts_x = [0.00, 0.25, 0.50, 0.75, 1.00]
pts_y = [0.00, P['curveY25'], P['curveY50'], P['curveY75'], 1.00]
ax.scatter(pts_x, pts_y, color='#FFFF80', s=70, zorder=5)
for (px, py), (dx, dy), lbl in [
    ((0.00, 0.00), ( 0.03, -0.09), '(0, 0) fix'),
    ((0.25, P['curveY25']), (-0.14,  0.05), f'curveY25={P["curveY25"]}'),
    ((0.50, P['curveY50']), (-0.14,  0.05), f'curveY50={P["curveY50"]}'),
    ((0.75, P['curveY75']), ( 0.03, -0.09), f'curveY75={P["curveY75"]}'),
    ((1.00, 1.00),          (-0.10, -0.09), '(1, 1) fix'),
]:
    ax.annotate(lbl, xy=(px, py), xytext=(px+dx, py+dy), fontsize=7.5, color='#FFFF80',
                arrowprops=dict(arrowstyle='->', color='#FFFF80', lw=0.8))

for y25, y50, y75, lbl, ls in [
    (0.30, 0.55, 0.78, 'frueher Anstieg (fast linear)', ':'),
    (0.05, 0.22, 0.58, 'progressiv / spaet',            '-.'),
]:
    ax.plot(x_norm, apply_curve(x_norm, y25, y50, y75),
            ls=ls, color='#AAAAFF', lw=1.4, alpha=0.75, label=lbl)

add_volt_axis(ax, -0.04, 1.08)
ax.set_ylim(-0.04, 1.08)
ax.legend(loc='upper left', fontsize=7.5)

# =============================================================================
# Panel 2 -- Support-Level
# =============================================================================
ax = ax_support
ax.set_title('2  Support-Level  (0 ... 12)  --  skaliert den Ausgang', pad=7)
ax.set_xlabel('Eingang  (norm.)'); ax.set_ylabel('Ausgang  (norm.)')
ax.set_xlim(0, 1); ax.set_ylim(-0.04, 1.08)
ax.grid(True, linestyle='--')
cmap = plt.cm.RdYlGn
for level in [0, 2, 4, 6, 8, 10, 12]:
    yv = apply_curve(x_norm, P['curveY25'], P['curveY50'], P['curveY75']) * (level / 12.0)
    ax.plot(x_norm, yv, color=cmap(level / 12.0),
            lw=2.5 if level == P['supportLevel'] else 1.3,
            label=f'Level {level:2d}  ({level/12*100:.0f}%)')
add_volt_axis(ax, -0.04, 1.08)
ax.set_ylim(-0.04, 1.08)
ax.legend(loc='upper left', fontsize=7.5)

# =============================================================================
# Panel 3 -- Torque-Filter: SINUSFOERMIGES Eingangssignal (physikalisch korrekt)
# Drehmoment = |sin(2*pi*f_crank*t)|  (2 Huegel pro Umdrehung: links + rechts)
# Minimum bei 6 Uhr und 12 Uhr (Totpunkte)
# =============================================================================
t3 = np.linspace(0, 4, 401)          # 4 Sekunden @ 60 RPM = 4 Umdrehungen
f_crank3 = 60.0 / 60.0               # 1 Hz = 60 RPM
effort3 = np.where(t3 >= 0.4, 0.70, 0.0)

# |sin(2*pi*f*t)| ergibt 2 Huegel pro Umdrehung (linkes + rechtes Pedal)
# Minimumwert 0.05 (Fussgewicht, kein echter Nulldurchgang)
raw_t3 = effort3 * (0.05 + 0.95 * np.abs(np.sin(2.0 * np.pi * f_crank3 * t3)))

tau_rise_ms = 1000.0 / (P['torqueFilterRise'] * 100)
tau_fall_ms = 1000.0 / (P['torqueFilterFall'] * 100)

ax = ax_tfilt
ax.set_title(
    f'3  Torque-Filter  --  reales Eingangssignal: |sin|  (2x / Umdrehung, Totpunkte bei 6/12 Uhr)\n'
    f'Rise alpha={P["torqueFilterRise"]} (tau~{tau_rise_ms:.0f}ms)   '
    f'Fall alpha={P["torqueFilterFall"]} (tau~{tau_fall_ms:.0f}ms)   @ 60 RPM',
    pad=7)
ax.set_xlabel('Zeit [s]'); ax.set_ylabel('Normierter Wert')
ax.set_xlim(0, 4); ax.set_ylim(-0.05, 1.05)
ax.grid(True, linestyle='--')

# Totpunkte markieren (alle 0.5 s bei 60 RPM ab t=0.4)
for t_dead in np.arange(0.4, 4.1, 0.5):
    ax.axvline(t_dead, color='#555533', lw=0.7, alpha=0.8)
ax.text(0.42, 1.01, '12/6 Uhr (Totpunkt)', fontsize=6.5, color=C_DEAD)

ax.plot(t3, raw_t3, color='#888866', lw=1.0, ls='--', alpha=0.8, label='Rohsignal (ADC 0-5V)')

# Default: asymmetrisch
f_asym = asymm_filter(raw_t3, P['torqueFilterRise'], P['torqueFilterFall'])
ax.plot(t3, f_asym, color=C_TORQUE, lw=2.5,
        label=f'Asymm. Filter  Rise={P["torqueFilterRise"]}  Fall={P["torqueFilterFall"]}  (Default)')

# Symmetrisch langsam: beide alpha=fall -> Motor pulsiert bei jedem Totpunkt!
f_sym_slow = asymm_filter(raw_t3, P['torqueFilterFall'], P['torqueFilterFall'])
ax.plot(t3, f_sym_slow, color='#FF6060', lw=1.4, ls=':',
        label=f'Symmetrisch langsam  alpha={P["torqueFilterFall"]}  -> Totpunkt-Einbruch')

# Symmetrisch schnell: beide alpha=rise -> folgt dem Roh-Signal, viel Ripple
f_sym_fast = asymm_filter(raw_t3, P['torqueFilterRise'], P['torqueFilterRise'])
ax.plot(t3, f_sym_fast, color='#FFAA40', lw=1.4, ls='-.',
        label=f'Symmetrisch schnell  alpha={P["torqueFilterRise"]}  -> viel Ripple')

ax.annotate('Motor pulsiert!\n(Totpunkt-Einbruch)', xy=(1.0, 0.08), xytext=(1.2, 0.30),
            fontsize=7.5, color='#FF6060',
            arrowprops=dict(arrowstyle='->', color='#FF6060', lw=0.9))
ax.annotate('Asymm. Filter\nueberbrueckt Totpunkte', xy=(1.55, 0.66), xytext=(2.0, 0.45),
            fontsize=7.5, color=C_TORQUE,
            arrowprops=dict(arrowstyle='->', color=C_TORQUE, lw=0.9))

ax.legend(loc='lower right', fontsize=7.5)

# =============================================================================
# Panel 4 -- Kadenz: DIGITALE PULSE -> instantRpm (Treppe) -> LP-Filter
# =============================================================================
t4 = np.linspace(0, 5, 501)
rpm_profile4 = np.where(t4 < 0.5,  0.0,
               np.where(t4 < 1.5,  70.0 * (t4 - 0.5),
               np.where(t4 < 3.5,  70.0, 0.0)))

pulses4, inst_rpm4, s_cad4 = simulate_cadence_pulses(
    t4, rpm_profile4,
    P['pulsesPerRev'], P['cadenceMaxRpm'],
    P['cadenceMaxLimit'], P['cadenceFilterAlpha'], P['cadenceTimeoutMs'])

tau_cad_ms = 1000.0 / (P['cadenceFilterAlpha'] * 100)
pulse_period_at70 = 60.0 / (70.0 * P['pulsesPerRev']) * 1000  # ms

# Oben: Pulse-Train
ax = ax_pulses
ax.set_title(
    f'4  Kadenz-Sensor  --  Digital-Puls -> instantRpm -> LP-Filter\n'
    f'{P["pulsesPerRev"]} Magnete  |  @ 70 RPM: Puls alle {pulse_period_at70:.0f} ms  |  '
    f'alpha={P["cadenceFilterAlpha"]} (tau~{tau_cad_ms:.0f}ms)  |  Timeout={P["cadenceTimeoutMs"]}ms',
    pad=7)
ax.set_ylabel('Puls\n(digital)', fontsize=8)
ax.set_ylim(-0.2, 1.5)
ax.set_yticks([0, 1])
ax.grid(True, linestyle='--', axis='x')
ax.tick_params(labelbottom=False)
ax.spines['bottom'].set_visible(False)

pulse_times = t4[pulses4 == 1]
ax.vlines(pulse_times, 0, 1, color=C_PULSE, lw=0.8, alpha=0.9, label='Interrupt-Puls (Magnet)')
ax.text(0.52, 1.25, '<-- Treten beginnt', fontsize=7, color='#AAAACC')
ax.text(3.52, 1.25, '<-- Treten stoppt', fontsize=7, color='#AAAACC')
ax.legend(loc='upper right', fontsize=7.5)

# Unten: RPM Treppe + gefilterte Kadenz
ax = ax_cfilt
ax.set_xlabel('Zeit [s]')
ax.set_ylabel('RPM (norm. 0..1)', fontsize=8)
ax.set_xlim(0, 5); ax.set_ylim(-0.05, 1.25)
ax.grid(True, linestyle='--')

# instantRpm als normierte Treppenstufen
inst_norm4 = np.clip(inst_rpm4 / P['cadenceMaxRpm'], 0, 1)
ax.step(t4, inst_norm4, color=C_INST_RPM, lw=1.2, where='post', alpha=0.7,
        label='instantRpm  (normiert, Treppenstufen)')
ax.plot(t4, s_cad4, color=C_CADENCE, lw=2.5,
        label=f'Gefilterte Kadenz  (alpha={P["cadenceFilterAlpha"]}, tau~{tau_cad_ms:.0f}ms)')
ax.axhline(0.1, color='#FF8040', ls=':', lw=1.2, label='Gate-Schwelle 0.1 (Kadenz > 10% -> Motor aktiv)')
timeout_t4 = 3.5 + P['cadenceTimeoutMs'] / 1000.0
ax.axvline(timeout_t4, color='#FF8040', ls='-.', lw=1.0, alpha=0.8,
           label=f'Timeout  ({P["cadenceTimeoutMs"]} ms nach letztem Puls)')

ax2_cad = ax.twinx()
ax2_cad.set_ylim(-0.05 * P['cadenceMaxRpm'], 1.25 * P['cadenceMaxRpm'])
ax2_cad.set_ylabel('RPM', color='#AAAACC', fontsize=8)
ax2_cad.tick_params(colors='#AAAACC', labelsize=8)
ax2_cad.spines['right'].set_color('#555577')
ax.legend(loc='upper right', fontsize=7.5)

# =============================================================================
# Panel 5 -- Ramp-Dynamik
# =============================================================================
N4 = 401
t5 = np.linspace(0, 4, N4)
ramp_low_s  = 1.0 / (P['rampUpLow']  * 100)
ramp_high_s = 1.0 / (P['rampUpHigh'] * 100)
ramp_dn_s   = 1.0 / (P['rampDown']   * 100)

ax = ax_ramp
ax.set_title(
    f'5  Ramp-Dynamik  --  Anstieg / Abfall des Throttle-Ausgangs\n'
    f'rampUpLow={P["rampUpLow"]} ({ramp_low_s:.2f}s 0->100%)   '
    f'rampUpHigh={P["rampUpHigh"]} ({ramp_high_s:.2f}s 0->100%)   '
    f'rampThreshold={P["rampThreshold"]}   '
    f'rampDown={P["rampDown"]} ({ramp_dn_s:.2f}s 100->0%)', pad=7)
ax.set_xlabel('Zeit [s]'); ax.set_ylabel('Throttle  (norm.)')
ax.set_xlim(0, 4); ax.set_ylim(-0.05, 1.12)
ax.grid(True, linestyle='--')

for tv, col, lbl in [
    (0.35, '#80CCFF',
     f'Ziel=35%  (<Threshold)  ->  rampUpLow  ({ramp_low_s:.2f}s)'),
    (0.70, '#80FFAA',
     f'Ziel=70%  (>Threshold)  ->  rampUpHigh ({ramp_high_s:.2f}s)'),
    (1.00, C_THROTTLE,
     f'Ziel=100%  (>Threshold)  ->  rampDown beim Abfall ({ramp_dn_s:.2f}s)'),
]:
    tg = np.where((t5 > 0.3) & (t5 < 2.5), tv, 0.0)
    ax.plot(t5, tg, color=col, lw=0.9, ls='--', alpha=0.45)
    ax.plot(t5, simulate_ramp(tg, P['rampUpLow'], P['rampUpHigh'],
                               P['rampThreshold'], P['rampDown']),
            color=col, lw=2.2, label=lbl)

ax.axhline(P['rampThreshold'], color='#FF8040', ls=':', lw=1.2,
           label=f'rampThreshold={P["rampThreshold"]} (Grenze langsam/schnell)')
add_volt_axis(ax, -0.05, 1.12)
ax.set_ylim(-0.05, 1.12)
ax.legend(loc='lower right', fontsize=7.5)

# =============================================================================
# Panel 6 -- Vollstaendige Simulation mit physikalisch korrekten Signalen
# Kadenz: echte Pulse -> instantRpm -> LP-Filter
# Drehmoment: sinusfoermig mit Pedalfrequenz (Totpunkte bei 6/12 Uhr)
# =============================================================================
FSIM = 100; TSIM = 10.0; N_SIM = int(FSIM * TSIM)
t_sim = np.linspace(0, TSIM, N_SIM)

# RPM-Profil: Anlauf -> Plateau -> Einbruch -> Stopp
def rpm_envelope(t):
    return np.where(t < 1.0, 0.0,
           np.where(t < 2.2, 70.0 * (t - 1.0) / 1.2,
           np.where(t < 6.5, 70.0,
           np.where(t < 7.0, 70.0 * (7.0 - t) / 0.5,
           np.where(t < 7.5, 35.0,
           np.where(t < 8.5, 70.0, 0.0))))))

rpm_sim = rpm_envelope(t_sim)

# Kadenz: diskrete Pulse simulieren
pulses_sim, inst_rpm_sim, s_cadence = simulate_cadence_pulses(
    t_sim, rpm_sim,
    P['pulsesPerRev'], P['cadenceMaxRpm'],
    P['cadenceMaxLimit'], P['cadenceFilterAlpha'], P['cadenceTimeoutMs'])

# Drehmoment: integrierter Kurbelwinkel -> sinusfoermiges Rohsignal
# |sin(2*pi*f_crank*t)| mit Anstrengungsverlauf
crank_angle_sim = np.zeros(N_SIM)
for i in range(1, N_SIM):
    crank_angle_sim[i] = crank_angle_sim[i-1] + 2.0 * np.pi * rpm_sim[i] / 60.0 / FSIM

def effort_envelope(t):
    return np.where(t < 1.5, 0.0,
           np.where(t < 2.5, 0.7 * (t - 1.5),
           np.where(t < 5.0, 0.7,
           np.where(t < 6.0, 0.7 * (6.0 - t),
           np.where(t < 6.5, 0.0,
           np.where(t < 7.5, 0.8,
           np.where(t < 8.5, 0.5, 0.0)))))))

effort_sim = effort_envelope(t_sim)
# 2 Huegel pro Umdrehung (links + rechts), Minimum ~0.05 an Totpunkten
raw_torque_sim = effort_sim * (0.05 + 0.95 * np.abs(np.sin(crank_angle_sim)))

# Asymmetrischer Torque-Filter
filt_torque_sim = asymm_filter(raw_torque_sim, P['torqueFilterRise'], P['torqueFilterFall'])

# Kombination (Gate-Modus: Kadenz > 0.1 aktiviert Drehmoment)
if P['cadenceMode'] == 1:
    factor = np.where(s_cadence > 0.1, filt_torque_sim, 0.0)
else:
    factor = filt_torque_sim * s_cadence

target = apply_curve(factor, P['curveY25'], P['curveY50'], P['curveY75']) * (P['supportLevel'] / 12.0)

# Ramp
throttle_sim = np.zeros(N_SIM)
thr = 0.0
for i in range(1, N_SIM):
    tg = target[i]
    if tg == 0.0 or s_cadence[i] < 0.05:
        thr = 0.0
    elif tg > thr:
        thr = min(thr + (P['rampUpHigh'] if tg > P['rampThreshold'] else P['rampUpLow']), 1.0)
    else:
        thr = max(thr - P['rampDown'], 0.0)
    throttle_sim[i] = thr

mode_txt = 'Gate (Kadenz>10% -> Motor aktiv)' if P['cadenceMode'] == 1 else 'Multiplikativ'
ax = ax_sim
ax.set_title(
    f'6  Vollstaendige Simulation  --  Kadenz (Digital-Puls) + Drehmoment (analog, Sinus)  ->  Throttle Output\n'
    f'cadenceMode={mode_txt}  |  supportLevel={P["supportLevel"]}/12  '
    f'|  Rohsignal Drehmoment: |sin(Kurbelwinkel)|, min ~5% an Totpunkten',
    pad=7)
ax.set_xlabel('Zeit [s]'); ax.set_ylabel('Normierter Wert  /  Throttle')
ax.set_xlim(0, TSIM); ax.set_ylim(-0.05, 1.20)
ax.grid(True, linestyle='--')

ax.fill_between(t_sim, 0, raw_torque_sim,  alpha=0.08, color=C_TORQUE)
ax.fill_between(t_sim, 0, s_cadence,       alpha=0.08, color=C_CADENCE)
ax.fill_between(t_sim, 0, throttle_sim,    alpha=0.22, color=C_THROTTLE)

ax.plot(t_sim, raw_torque_sim,  color=C_TORQUE,   lw=0.8, ls='--', alpha=0.5,
        label='Drehmoment roh  (analog, |sin(Kurbelwinkel)|, Totpunkte sichtbar)')
ax.plot(t_sim, filt_torque_sim, color=C_TORQUE,   lw=2.0,
        label=f'Drehmoment gefiltert  (asymm. Rise={P["torqueFilterRise"]}, Fall={P["torqueFilterFall"]})')
ax.plot(t_sim, np.clip(inst_rpm_sim / P['cadenceMaxRpm'], 0, 1),
        color=C_INST_RPM, lw=0.9, ls=':', alpha=0.6, label='instantRpm normiert (Treppenstufen)')
ax.plot(t_sim, s_cadence, color=C_CADENCE, lw=2.0,
        label=f'Kadenz gefiltert (alpha={P["cadenceFilterAlpha"]})')
ax.plot(t_sim, target, color=C_TARGET, lw=1.2, ls=':',
        label=f'Ziel (nach Kurve x Level {P["supportLevel"]}/12)')
ax.plot(t_sim, throttle_sim, color=C_THROTTLE, lw=2.8,
        label='Throttle Output  (an DAC, 0-5 V)')

ax.axhline(0.1, color=C_CADENCE, ls=':', lw=0.8, alpha=0.6)
ax.text(0.1, 0.115, 'Gate-Schwelle', fontsize=7, color=C_CADENCE, alpha=0.8)

for xv, lbl in [(1.0,'Treten\nbeginn'),(1.5,'Kraft\nanstieg'),(5.0,'Kraft\nabfall'),(8.5,'Treten\nstopp')]:
    ax.axvline(xv, color='#555577', ls=':', lw=0.8)
    ax.text(xv + 0.08, 1.10, lbl, fontsize=7, color='#AAAACC', va='top')

ax2_sim = ax.twinx()
ax2_sim.set_ylim(-0.05 * 5.0, 1.20 * 5.0)
ax2_sim.set_ylabel('Throttle [V]', color='#AAAACC', fontsize=8)
ax2_sim.tick_params(colors='#AAAACC', labelsize=8)
ax2_sim.spines['right'].set_color('#555577')
ax.legend(loc='lower right', fontsize=7.5, ncol=2)

# === Hinweis-Box =============================================================
notes = (
    "PARAMETER-HINWEISE:\n"
    "  torqueZero=0 / torqueMax=0  (Standardwerte) -> Division durch 0 moeglich! Vor Betrieb kalibrieren.\n"
    f"  cadenceMaxLimit={P['cadenceMaxLimit']:.0f} RPM -> Spike-Filter (Mensch max ~130-150 RPM; Wert kann gesenkt werden)\n"
    f"  rampDown=0.050 -> 0.05 (trailing zero, kosmetisch)   |   "
    f"rampUpLow={P['rampUpLow']}: {ramp_low_s:.2f}s  rampUpHigh={P['rampUpHigh']}: {ramp_high_s:.2f}s  rampDown={P['rampDown']}: {ramp_dn_s:.2f}s  (je 0->100% @ 100Hz)"
)
fig.text(0.015, 0.005, notes, fontsize=8, color='#FFDD80', fontfamily='monospace',
         va='bottom',
         bbox=dict(boxstyle='round,pad=0.5', facecolor='#080818', alpha=0.9, edgecolor='#444466'))

# === Speichern ===============================================================
out_path = r'c:\Technical\throttlecontrol\software\throttle_viz.png'
fig.savefig(out_path, dpi=140, bbox_inches='tight', facecolor=DARK_BG)
print(f"Gespeichert: {out_path}")
plt.show()
