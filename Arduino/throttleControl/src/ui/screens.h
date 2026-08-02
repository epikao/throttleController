#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *torque_par;
    lv_obj_t *cadence_par;
    lv_obj_t *throttle_curve;
    lv_obj_t *pas_battery_par;
    lv_obj_t *div_parameters;
    lv_obj_t *stopwatch;
    lv_obj_t *rear_light;
    lv_obj_t *front_light;
    lv_obj_t *speed_ui;
    lv_obj_t *kmh_ui;
    lv_obj_t *obj0;
    lv_obj_t *obj1;
    lv_obj_t *obj2;
    lv_obj_t *torque_ui;
    lv_obj_t *obj3;
    lv_obj_t *cadence_ui;
    lv_obj_t *obj4;
    lv_obj_t *power_ui;
    lv_obj_t *obj5;
    lv_obj_t *obj6;
    lv_obj_t *obj7;
    lv_obj_t *obj8;
    lv_obj_t *obj9;
    lv_obj_t *time_ui;
    lv_obj_t *obj10;
    lv_obj_t *obj11;
    lv_obj_t *temp_ui;
    lv_obj_t *trip_ui;
    lv_obj_t *obj12;
    lv_obj_t *obj13;
    lv_obj_t *on_time_ui;
    lv_obj_t *obj14;
    lv_obj_t *throttle_ui;
    lv_obj_t *current_ui;
    lv_obj_t *voltage_ui;
    lv_obj_t *range_ui;
    lv_obj_t *obj15;
    lv_obj_t *obj16;
    lv_obj_t *support_ui;
    lv_obj_t *capacity_bar;
    lv_obj_t *wh_km_ui;
    lv_obj_t *obj17;
    lv_obj_t *capacity_ui;
    lv_obj_t *obj18;
    lv_obj_t *support_bar;
    lv_obj_t *obj19;
    lv_obj_t *obj20;
    lv_obj_t *speed_avg_ui;
    lv_obj_t *obj21;
    lv_obj_t *speed1_ui;
    lv_obj_t *obj22;
    lv_obj_t *power1_ui;
    lv_obj_t *obj23;
    lv_obj_t *obj24;
    lv_obj_t *obj25;
    lv_obj_t *obj26;
    lv_obj_t *throttle1_ui;
    lv_obj_t *obj27;
    lv_obj_t *cadence1_ui;
    lv_obj_t *torque_zero_ui;
    lv_obj_t *torque1_ui;
    lv_obj_t *obj28;
    lv_obj_t *obj29;
    lv_obj_t *obj30;
    lv_obj_t *obj31;
    lv_obj_t *obj32;
    lv_obj_t *current1_ui;
    lv_obj_t *obj33;
    lv_obj_t *obj34;
    lv_obj_t *obj35;
    lv_obj_t *obj36;
    lv_obj_t *obj37;
    lv_obj_t *torque_max_ui;
    lv_obj_t *obj38;
    lv_obj_t *obj39;
    lv_obj_t *torque_deadband_ui;
    lv_obj_t *obj40;
    lv_obj_t *obj41;
    lv_obj_t *torque_filter_rise_ui;
    lv_obj_t *obj42;
    lv_obj_t *obj43;
    lv_obj_t *torque_filter_fall_ui;
    lv_obj_t *obj44;
    lv_obj_t *obj45;
    lv_obj_t *torque_idle_ui;
    lv_obj_t *obj46;
    lv_obj_t *obj47;
    lv_obj_t *obj48;
    lv_obj_t *speed2_ui;
    lv_obj_t *obj49;
    lv_obj_t *power2_ui;
    lv_obj_t *obj50;
    lv_obj_t *obj51;
    lv_obj_t *obj52;
    lv_obj_t *obj53;
    lv_obj_t *throttle2_ui;
    lv_obj_t *obj54;
    lv_obj_t *cadence2_ui;
    lv_obj_t *pulses_per_rev_ui;
    lv_obj_t *torque2_ui;
    lv_obj_t *obj55;
    lv_obj_t *obj56;
    lv_obj_t *obj57;
    lv_obj_t *obj58;
    lv_obj_t *obj59;
    lv_obj_t *current2_ui;
    lv_obj_t *obj60;
    lv_obj_t *obj61;
    lv_obj_t *obj62;
    lv_obj_t *obj63;
    lv_obj_t *obj64;
    lv_obj_t *cadence_timeout_ui;
    lv_obj_t *obj65;
    lv_obj_t *obj66;
    lv_obj_t *cadence_max_ui;
    lv_obj_t *obj67;
    lv_obj_t *obj68;
    lv_obj_t *cadence_filter_ui;
    lv_obj_t *obj69;
    lv_obj_t *obj70;
    lv_obj_t *cadence_limit_ui;
    lv_obj_t *obj71;
    lv_obj_t *obj72;
    lv_obj_t *cadence_gate_pulses_ui;
    lv_obj_t *obj73;
    lv_obj_t *obj74;
    lv_obj_t *cadence_gate_time_ui;
    lv_obj_t *obj75;
    lv_obj_t *obj76;
    lv_obj_t *obj77;
    lv_obj_t *speed3_ui;
    lv_obj_t *obj78;
    lv_obj_t *power3_ui;
    lv_obj_t *obj79;
    lv_obj_t *obj80;
    lv_obj_t *obj81;
    lv_obj_t *obj82;
    lv_obj_t *throttle3_ui;
    lv_obj_t *obj83;
    lv_obj_t *cadence3_ui;
    lv_obj_t *curve_y25_ui;
    lv_obj_t *torque3_ui;
    lv_obj_t *obj84;
    lv_obj_t *obj85;
    lv_obj_t *obj86;
    lv_obj_t *obj87;
    lv_obj_t *obj88;
    lv_obj_t *current3_ui;
    lv_obj_t *obj89;
    lv_obj_t *obj90;
    lv_obj_t *obj91;
    lv_obj_t *obj92;
    lv_obj_t *obj93;
    lv_obj_t *curve_y50_ui;
    lv_obj_t *obj94;
    lv_obj_t *obj95;
    lv_obj_t *curve75_ui;
    lv_obj_t *obj96;
    lv_obj_t *obj97;
    lv_obj_t *ramp_up_low;
    lv_obj_t *obj98;
    lv_obj_t *obj99;
    lv_obj_t *ramp_threshold_ui;
    lv_obj_t *obj100;
    lv_obj_t *obj101;
    lv_obj_t *ramp_up_high;
    lv_obj_t *obj102;
    lv_obj_t *obj103;
    lv_obj_t *ramp_down_ui;
    lv_obj_t *obj104;
    lv_obj_t *obj105;
    lv_obj_t *offset_ui;
    lv_obj_t *obj106;
    lv_obj_t *obj107;
    lv_obj_t *obj108;
    lv_obj_t *speed4_ui;
    lv_obj_t *obj109;
    lv_obj_t *power4_ui;
    lv_obj_t *obj110;
    lv_obj_t *obj111;
    lv_obj_t *obj112;
    lv_obj_t *obj113;
    lv_obj_t *throttle4_ui;
    lv_obj_t *obj114;
    lv_obj_t *cadence4_ui;
    lv_obj_t *cruise_power_ui;
    lv_obj_t *torque4_ui;
    lv_obj_t *obj115;
    lv_obj_t *obj116;
    lv_obj_t *obj117;
    lv_obj_t *obj118;
    lv_obj_t *obj119;
    lv_obj_t *current4_ui;
    lv_obj_t *obj120;
    lv_obj_t *obj121;
    lv_obj_t *obj122;
    lv_obj_t *obj123;
    lv_obj_t *obj124;
    lv_obj_t *cruise_ramp_up_ui;
    lv_obj_t *obj125;
    lv_obj_t *obj126;
    lv_obj_t *pas_mode_ui;
    lv_obj_t *obj127;
    lv_obj_t *voltage_min_ui;
    lv_obj_t *obj128;
    lv_obj_t *obj129;
    lv_obj_t *voltage_max_ui;
    lv_obj_t *obj130;
    lv_obj_t *obj131;
    lv_obj_t *cruise_indicator_ui;
    lv_obj_t *current_max_ui;
    lv_obj_t *obj132;
    lv_obj_t *obj133;
    lv_obj_t *capacity_max_ui;
    lv_obj_t *obj134;
    lv_obj_t *obj135;
    lv_obj_t *cruise_ramp_down_ui;
    lv_obj_t *obj136;
    lv_obj_t *obj137;
    lv_obj_t *cruise_deadband_ui;
    lv_obj_t *obj138;
    lv_obj_t *obj139;
    lv_obj_t *obj140;
    lv_obj_t *obj141;
    lv_obj_t *speed_street;
    lv_obj_t *speed_limit_street_ui;
    lv_obj_t *speed_kmh;
    lv_obj_t *cruise_street;
    lv_obj_t *cruise_limit_street_ui;
    lv_obj_t *cruise_kmh;
    lv_obj_t *power_street;
    lv_obj_t *power_limit_street_ui;
    lv_obj_t *power_w;
    lv_obj_t *speed_race;
    lv_obj_t *speed_limit_race_ui;
    lv_obj_t *speed_race_kmh;
    lv_obj_t *cruise_race;
    lv_obj_t *cruise_limit_race_ui;
    lv_obj_t *cruise_race_kmh;
    lv_obj_t *power_race;
    lv_obj_t *power_limit_race_ui;
    lv_obj_t *power_race_kmh;
    lv_obj_t *wheel_circle;
    lv_obj_t *wheel_circ_ui;
    lv_obj_t *wheel_mm;
    lv_obj_t *obj142;
    lv_obj_t *trip1_ui;
    lv_obj_t *obj143;
    lv_obj_t *obj144;
    lv_obj_t *odo_ui;
    lv_obj_t *obj145;
    lv_obj_t *race_line;
    lv_obj_t *stoptime_ui;
    lv_obj_t *obj146;
    lv_obj_t *obj147;
    lv_obj_t *obj148;
    lv_obj_t *obj149;
    lv_obj_t *time1_ui;
    lv_obj_t *obj150;
    lv_obj_t *obj151;
    lv_obj_t *rear_led_ui;
    lv_obj_t *on_off_rear_ui;
    lv_obj_t *obj152;
    lv_obj_t *rear_dim_ui;
    lv_obj_t *obj153;
    lv_obj_t *on_restart_line;
    lv_obj_t *on_restart;
    lv_obj_t *street_par;
    lv_obj_t *street_on_restart;
    lv_obj_t *obj154;
    lv_obj_t *obj155;
    lv_obj_t *front_led_ui;
    lv_obj_t *on_off_front_ui;
    lv_obj_t *obj156;
    lv_obj_t *front_dim_ui;
    lv_obj_t *obj157;
    lv_obj_t *obj158;
    lv_obj_t *obj159;
    lv_obj_t *obj160;
    lv_obj_t *dim_backlight_ui;
    lv_obj_t *bkldim;
    lv_obj_t *obj161;
    lv_obj_t *on_time_backlight_ui;
    lv_obj_t *sec;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_TORQUE_PAR = 2,
    SCREEN_ID_CADENCE_PAR = 3,
    SCREEN_ID_THROTTLE_CURVE = 4,
    SCREEN_ID_PAS_BATTERY_PAR = 5,
    SCREEN_ID_DIV_PARAMETERS = 6,
    SCREEN_ID_STOPWATCH = 7,
    SCREEN_ID_REAR_LIGHT = 8,
    SCREEN_ID_FRONT_LIGHT = 9,
};

void create_screen_main();
void tick_screen_main();

void create_screen_torque_par();
void tick_screen_torque_par();

void create_screen_cadence_par();
void tick_screen_cadence_par();

void create_screen_throttle_curve();
void tick_screen_throttle_curve();

void create_screen_pas_battery_par();
void tick_screen_pas_battery_par();

void create_screen_div_parameters();
void tick_screen_div_parameters();

void create_screen_stopwatch();
void tick_screen_stopwatch();

void create_screen_rear_light();
void tick_screen_rear_light();

void create_screen_front_light();
void tick_screen_front_light();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/