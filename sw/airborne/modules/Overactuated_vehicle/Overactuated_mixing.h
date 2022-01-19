/*
 * Copyright (C) 2015 C. De Wagter
 *               2015 Freek van Tienen <freek.v.tienen@gmail.com>
 *
 * This file is part of paparazzi
 *
 * paparazzi is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * paparazzi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with paparazzi; see the file COPYING.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
/**
 * @file "modules/Overactuated_vehicle/Overactuated_vehicle.h"
 * @author Alessandro Mancinelli
 * Control laws for Overactuated Vehicle
 */

#ifndef OVERACTUATED_MIXING_H
#define OVERACTUATED_MIXING_H

#include "std.h"
#include "paparazzi.h"
#include "generated/airframe.h"


/**
 * DEFINE VEHICLE PROPERTIES
 */
#define N_ACT 12
#define INDI_INPUTS 6
#define INDI_NUM_ACT 12

/**
 * DEFINE SOME USEFUL STRUCT
 */
struct FloatEulersPosition {
    float phi;
    float theta;
    float psi;
    float x;
    float y;
    float z;
};

struct ActuatorsStruct {
    float motor;
    float elevation;
    float azimuth;
};

struct PID_over_simple {
    float p;
    float i;
    float d;
};

struct PID_over {
    struct FloatEulersPosition p;
    struct FloatEulersPosition i;
    struct FloatEulersPosition d;
};
struct PD_indi_over {
    struct FloatEulersPosition p;
    struct FloatEulersPosition d;
};

/* overactuated mixing structure */
struct overactuated_mixing_t {
    int32_t commands[N_ACT];      ///< The output commands
};

extern struct overactuated_mixing_t overactuated_mixing;
extern struct PID_over pid_gains_over;
extern struct PID_over_simple pid_pos_x_att;
extern struct PID_over_simple pid_pos_y_att;
extern struct PID_over_simple pid_gain_psi_motor;
extern struct PD_indi_over indi_gains_over;
extern struct ActuatorsStruct act_dyn_struct;

// Variables for slider
extern float wind_speed;
extern float test_ale;
extern float stick_gain_yaw;
extern float stick_gain_throttle;
extern bool activate_tilting_az;
extern bool activate_tilting_el;
extern bool yaw_with_tilting;
extern bool yaw_with_motors;
extern bool position_with_attitude;
extern bool soft_PID; //The derivative gain is fed without computing the error derivative (just act on rates or speed).
extern bool manual_motor_stick;

extern bool static_tilt_motor_for_yaw;
extern int static_tilt_angle;

extern bool manual_heading;
extern int manual_heading_value_rad;

//Variables for PID failsafe mode:

extern float P_az_gain;
extern float D_az_gain;
extern float P_el_gain;
extern float D_el_gain;

/* External used functions */
extern void overactuated_mixing_init(void);
extern void overactuated_mixing_run(pprz_t in_cmd[], bool in_flight);

#endif