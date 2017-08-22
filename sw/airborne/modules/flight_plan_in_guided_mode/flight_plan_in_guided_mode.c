/*
 * Copyright (C) Shuo Li
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
 * @file "modules/flight_plan_in_guided_mode/flight_plan_in_guided_mode.c"
 * @author Shuo Li
 * This module is used to generate flight plan in guided mode
 */

#include <stdio.h>
#include <stdlib.h>
#include "flight_plan_in_guided_mode.h"
#include "firmwares/rotorcraft/autopilot.h"
#include "modules/flight_plan_in_guided_mode/flight_plan_in_guided_mode.h"
#include "firmwares/rotorcraft/guidance/guidance_h.h"
#include "firmwares/rotorcraft/guidance/guidance_v.h"
#include "state.h"
#include "modules/guidance_loop_velocity_autonomous_race/guidance_loop_velocity_autonomous_race.h"
#include <math.h>
#include "firmwares/rotorcraft/stabilization/stabilization_attitude.h"
#include "modules/state_autonomous_race/state_autonomous_race.h"
#include "modules/computer_vision/snake_gate_detection.h"
#include "subsystems/ins.h"
#include "modules/kalman_filter/kalman_filter.h"
#include "subsystems/imu.h"
#include "math/pprz_algebra_float.h"



#define KP_Y 0.4 
#define KI_Y 0.0
#define KD_Y 0.3
#define MAX_PHI  30.0/180*3.14


float psi0;//
float v_x_f;
float psi1;
float psi_startup;
float z0;
float x_start;
float y_start;
float heading;
float body_velocity_x;
float vx_earth;
float vy_earth;
float psi;
float velocity_body_x;
float velocity_body_y;
float velocity_earth_x;
float velocity_earth_y;
float init_heading;
float previous_error_y = 0;
float sum_y_error = 0;
double g = 9.81;

int primitive_in_use; // This variable is used for showing which primitive is used now;

void flight_plan_in_guided_mode_init() {
    primitive_in_use = NO_PRIMITIVE;
}


void display_information()
{
}

bool prepare_before_take_off(double prepare_time)
{
    if(primitive_in_use != PREPARE_BEFORE_TAKE_OFF){
        primitive_in_use = PREPARE_BEFORE_TAKE_OFF;
        counter_primitive = 0;
        time_primitive = 0;
        guidance_h_mode_changed(GUIDANCE_H_MODE_HOVER);
        guidance_v_mode_changed(GUIDANCE_V_MODE_HOVER);
    }
	if (time_primitive > prepare_time)
	{
			return 1;
	}
	else
	{
			return 0;
	}
}


void hover()
{
    if(primitive_in_use != HOVER){
        primitive_in_use = HOVER;
        counter_primitive = 0;
        time_primitive = 0;
        guidance_h_mode_changed(GUIDANCE_H_MODE_HOVER);
        guidance_v_mode_changed(GUIDANCE_V_MODE_HOVER);
    }
}

bool go_straight(float theta,float distance,double ref_y){
    if(primitive_in_use != GO_STRAIGHT){
        primitive_in_use = GO_STRAIGHT;
        counter_primitive = 0;
        time_primitive = 0;
        guidance_h_mode_changed(GUIDANCE_H_MODE_MODULE);
        guidance_v_mode_changed(GUIDANCE_V_MODE_GUIDED);
        psi0 = stateGetNedToBodyEulers_f()->psi;
        z0 = stateGetPositionNed_f()->z;
        x_start = stateGetPositionNed_f()->x;
        y_start = stateGetPositionNed_f()->y;
		sum_y_error = 0;
		states_race.attitude_control = TRUE;
		float v_x_e = stateGetSpeedNed_f()->x;
		float v_y_e = stateGetSpeedNed_f()->y;
		v_x_f = cos(psi)*v_x_e +sin(psi)*v_y_e;
		if (arc_counter == 0)
		{

				guidance_loop_set_heading(0.0);
		}
		else
		{
				guidance_loop_set_heading(3.14);
		}
    }

	float current_y = stateGetPositionNed_f()->y;
	float sign = 1;
	if(ref_y > 1.5)sign = -1;
	float error_y = (ref_y - current_y)*sign;
	sum_y_error += error_y/20.0;
	float phi = KP_Y * error_y+ KD_Y *(error_y-previous_error_y)*20.0 + KI_Y*sum_y_error;
	if(phi > MAX_PHI)phi = MAX_PHI;
	if(phi < -MAX_PHI)phi = -MAX_PHI;
	guidance_loop_set_theta(theta);
	guidance_loop_set_phi(phi); 
	/*guidance_loop_set_heading(psi0);*/
	previous_error_y = error_y;
	//if (time_primitive > 1.0)
	if(ref_y < 1.5 && stateGetPositionNed_f()->x > 3)   
	{
			return TRUE;
	}
	else if(ref_y > 1.5 && stateGetPositionNed_f()->x < 0)   
	{
			return TRUE;
	}
	else
	{
			return FALSE;
	}
}


void circle(float radius, float planned_time){
}

void arc(float radius, float planned_time, float desired_angle_change){
}

void set_velocity_test(float vx_earth_t,float vy_earth_t){
}

void go_left_right(float velocity){
}

void go_up_down(float altitude){
}

void adjust_position(float derta_altitude){
}


void search_gate()
{
}

void take_off(float desired_altitude)
{
		if (primitive_in_use != TAKE_OFF)
		{
				psi_startup = stateGetNedToBodyEulers_f()->psi;
				primitive_in_use = TAKE_OFF;
				counter_primitive = 0;
				time_primitive = 0;
				guidance_h_mode_changed(GUIDANCE_H_MODE_MODULE);
				guidance_v_mode_changed(GUIDANCE_V_MODE_MODULE);  // vertical module should be called!
				states_race.attitude_control = FALSE;
				guidance_loop_set_velocity(0,0);
				states_race.altitude_is_achieved = 0;
		}

		if (time_primitive>2)
		{
				states_race.altitude_is_achieved = 1;
		}
}

void land()
{
        guidance_v_mode_changed(GUIDANCE_V_MODE_GUIDED);
		guidance_v_set_guided_z(0.0);
        guidance_h_mode_changed(GUIDANCE_H_MODE_HOVER);
		printf("AAAAAAAAAAAAAAAA!!!!!!!!!!!!!!!!\n");
}

void adjust_heading(float delta_heading) {
}

void left_right_back(float velocity_in_body_x,float velocity_in_body_y)
{
}



void hold_altitude(float desired_altitude)
{
    if (primitive_in_use != HOLD_ALTITUDE)
    {
        primitive_in_use = HOLD_ALTITUDE;
        guidance_h_mode_changed(GUIDANCE_H_MODE_HOVER);
        guidance_v_mode_changed(GUIDANCE_V_MODE_GUIDED);
        counter_primitive = 0;
        time_primitive = 0;
        states_race.altitude_is_achieved = 0;
        guidance_v_set_guided_z(TAKE_OFF_ALTITUDE);
		states_race.attitude_control = FALSE;
		/*guidance_loop_set_velocity(0.0, 0.0);*/
        return;
    }
    if (fabs(stateGetPositionNed_f()->z - desired_altitude)<0.1 && time_primitive > 2)
	{
        // psi1 = stateGetNedToBodyEulers_f()->psi;
        states_race.altitude_is_achieved = 1;
        return;
    }

}

void change_heading_absolute(float psi)
{
}


void set_theta(float desired_theta)
{
}



void set_phi(float desired_phi)
{
}



void set_attitude(float desired_theta,float desired_phi)
{
    if(primitive_in_use != SET_ATTITUDE)
    { 
		primitive_in_use = SET_ATTITUDE;
        psi0 = stateGetNedToBodyEulers_f()->psi;
        z0 = stateGetPositionNed_f()->z;
        counter_primitive = 0;
        time_primitive = 0;
        guidance_h_mode_changed(GUIDANCE_H_MODE_MODULE);
        guidance_v_mode_changed(GUIDANCE_V_MODE_GUIDED);
		states_race.attitude_control = TRUE;
    }
	guidance_loop_set_theta(desired_theta);
	guidance_v_set_guided_z(TAKE_OFF_ALTITUDE);
	guidance_loop_set_phi(desired_phi); 
	guidance_loop_set_heading(psi0);
	printf("It is in attitude control mode!!!!!!!!!!!\n");
}


void calculate_attitude_average(double * p_theta,double *p_phi,struct acceleration* p_accel)
{
}

struct arc_open_loop_status arc_status;
void drone_model(struct arc_open_loop_status * sta);




bool arc_open_loop(double radius,double desired_theta,float delta_psi)
{
    if(primitive_in_use != ARC_OPEN_LOOP)
    { 
		primitive_in_use = ARC_OPEN_LOOP;
        psi = stateGetNedToBodyEulers_f()->psi;
		psi0 = psi;
        z0 = stateGetPositionNed_f()->z;
		arc_status.flag_in_arc = TRUE;
		arc_status.x= stateGetPositionNed_f()->x;
		arc_status.y= stateGetPositionNed_f()->y;
		arc_status.z= stateGetPositionNed_f()->z;
		arc_status.phi_cmd = stateGetNedToBodyEulers_f()->phi;
		arc_status.theta_cmd = desired_theta;
        arc_status.psi_cmd= stateGetNedToBodyEulers_f()->psi;
		arc_status.v_x_f = cos(arc_status.psi_cmd)*stateGetSpeedNed_f()->x + sin(arc_status.psi_cmd)*stateGetSpeedNed_f()->y; 
		arc_status.v_y_f = -sin(arc_status.psi_cmd)*stateGetSpeedNed_f()->x + cos(arc_status.psi_cmd)*stateGetSpeedNed_f()->y; 
		arc_status.v_z_f = stateGetSpeedNed_f()->z;
        counter_primitive = 0;
        time_primitive = 0;
        guidance_h_mode_changed(GUIDANCE_H_MODE_MODULE);
        guidance_v_mode_changed(GUIDANCE_V_MODE_GUIDED);
		states_race.attitude_control = TRUE;
		arc_status.drag_coef_body_x_1 = -0.53;
		arc_status.drag_coef_body_y_1 = -0.47;
		arc_status.drag_coef_body_z_1 = 0;
		arc_status.drag_coef_body_x_0 = 0.0057;
		arc_status.drag_coef_body_y_0 = -0.025;
		arc_status.drag_coef_body_z_0 = 0;
		arc_status.drag_coef_body_x_2 = 0;
		arc_status.drag_coef_body_y_2 = 0;
		arc_status.drag_coef_body_z_2 = 0;
		/*arc_status.drag_coef_body_x_1 = -0.5;*/
		/*arc_status.drag_coef_body_y_1 = 0;*/
		/*arc_status.drag_coef_body_z_1 = -0;*/
		/*arc_status.drag_coef_body_x_0 = -0;*/
		/*arc_status.drag_coef_body_y_0 = 0;*/
		/*arc_status.drag_coef_body_z_0 = -0;*/
		/*arc_status.drag_coef_body_x_2 = -0;*/
		/*arc_status.drag_coef_body_y_2 = 0;*/
		/*arc_status.drag_coef_body_z_2 = 0;*/
    }
// calculate command needed
// transfer velocity from earth coordinate to body and body fixed coordinate
	double phi = arc_status.phi_cmd;
    double theta = arc_status.theta_cmd;
    double psi = arc_status.psi_cmd;
	// calculate body velocity
	arc_status.v_x_b = cos(theta)*arc_status.v_x_f-
			sin(theta)*arc_status.v_z_f;
	arc_status.v_y_b = sin(phi)*sin(theta)*arc_status.v_x_f+
			cos(phi)*arc_status.v_y_f+
		   sin(phi)*cos(theta)*arc_status.v_z_f;
	arc_status.v_z_b = cos(phi)*sin(theta)*arc_status.v_x_f-
		sin(phi)*arc_status.v_y_f+
		cos(phi)*cos(theta)*arc_status.v_z_f;	

    /*arc_status.v_x_b = cos(theta)*arc_status.v_x_f;*/
    /*arc_status.v_y_b = 0;*/
	/*arc_status.v_z_b = cos(phi)*sin(theta)*arc_status.v_x_f;*/

//  calculate drag in body velocity
	arc_status.drag_x_b = arc_status.drag_coef_body_x_1*arc_status.v_x_b;
	arc_status.drag_y_b = arc_status.drag_coef_body_y_1*arc_status.v_y_b;
	arc_status.drag_z_b = arc_status.drag_coef_body_z_1*arc_status.v_z_b;

//  transfer drag from body coordinate to body fixed coordinate
	arc_status.drag_x_f = cos(theta)*arc_status.drag_x_b+sin(phi)*sin(theta)*arc_status.drag_y_b
			+cos(phi)*sin(theta)*arc_status.drag_z_b;
	arc_status.drag_y_f = cos(phi)*arc_status.drag_y_b-sin(phi)*arc_status.drag_z_b;
	/*arc_status.drag_y_f = 0;*/
	arc_status.drag_z_f = -sin(theta)*arc_status.drag_x_b+sin(phi)*cos(theta)*arc_status.drag_y_b+
			cos(phi)*cos(theta)*arc_status.drag_z_b;


//  calculate command
	double d_psi = arc_status.v_x_f/radius;
	arc_status.psi_cmd += d_psi/20.0;
	arc_status.theta_cmd = desired_theta;
	arc_status.phi_cmd= atan((arc_status.v_x_f*arc_status.v_x_f/radius-arc_status.drag_y_f)*cos(arc_status.theta_cmd)/
			(9.8+arc_status.drag_z_f));
	/*arc_status.phi_cmd = -atan(-arc_status.v_x_f*d_psi*cos(arc_status.theta_cmd)/9.8);*/
	arc_status.thrust_cmd= (-9.8-arc_status.drag_z_f)/cos(arc_status.phi_cmd)/cos(arc_status.theta_cmd);
	// euler method to predict
	drone_model(&arc_status);

	arc_status.x += arc_status.dx/20.0;
	arc_status.y += arc_status.dy/20.0;
	arc_status.z += arc_status.dz/20.0;
	arc_status.v_x_f += arc_status.dv_x_f/20.0;
	arc_status.v_y_f += arc_status.dv_y_f/20.0;
	arc_status.v_z_f += arc_status.dv_z_f/20.0;


/*printf("v_x_f is %f\n",arc_status.v_x_f);*/


	guidance_loop_set_theta(arc_status.theta_cmd);
	guidance_loop_set_phi(arc_status.phi_cmd); 
	guidance_loop_set_heading(arc_status.psi_cmd);
	guidance_v_set_guided_z(TAKE_OFF_ALTITUDE);


	if (arc_status.psi_cmd >(psi0+delta_psi))
	{
			arc_status.flag_in_arc = FALSE;
			return 1;
	}
	else
	{
			return 0;
	}
}

void drone_model(struct arc_open_loop_status* sta)
{
		double phi = sta->phi_cmd;
		double theta = sta->theta_cmd;
		double psi = sta->psi_cmd;

		sta->dx = cos(psi)*sta->v_x_f-sin(psi)*sta->v_y_f;
		sta->dy = sin(psi)*sta->v_x_f+cos(psi)*sta->v_y_f;
		sta->dz = sta->v_z_f;
		
		double T = arc_status.thrust_cmd;
		/*double T = -9.8/cos(theta)/cos(phi);*/
		sta->dv_x_f = cos(phi)*sin(theta)*T+sta->drag_x_f+sta->v_x_f*sta->v_y_f/1.5;
		/*sta->dv_x_f = cos(phi)*sin(theta)*T+sta->drag_x_f;*/
		sta->dv_y_f = -sin(phi)*T+sta->drag_y_f-sta->v_x_f*sta->v_x_f/1.5; 	
		sta->dv_z_f = 9.8+cos(phi)*cos(theta)*T+sta->drag_z_f; 	
}


double sum_phi;
double sum_theta;
int trim_att_counter;
double trim_phi;
double trim_theta;
bool hover_at_origin()
{
    if(primitive_in_use != HOVER_AT_ORIGIN)
    { 
		primitive_in_use = HOVER_AT_ORIGIN;
        psi0 = stateGetNedToBodyEulers_f()->psi;
        z0 = stateGetPositionNed_f()->z;
        counter_primitive = 0;
        time_primitive = 0;
        guidance_h_mode_changed(GUIDANCE_H_MODE_GUIDED);
        guidance_v_mode_changed(GUIDANCE_V_MODE_GUIDED);
		guidance_h_set_guided_pos(0.0,0.0);
		guidance_h_set_guided_heading(0.0);
		guidance_v_set_guided_z(TAKE_OFF_ALTITUDE);
		sum_phi = 0;
		sum_theta = 0;
		trim_att_counter = 0;
    }
 float current_x = stateGetPositionNed_f()->x;
 float current_y = stateGetPositionNed_f()->y;
 if (time_primitive >10.0)
 {
		 sum_phi += stateGetNedToBodyEulers_f()->phi;
		 sum_theta += stateGetNedToBodyEulers_f()->theta;
		 trim_att_counter ++;
		 trim_phi = sum_phi /trim_att_counter;
		 trim_theta = sum_theta /trim_att_counter;
 }
 if (sqrt(current_x*current_x + current_y*current_y)<0.2&&time_primitive > 20)
 {
		 return 1;
 }
 else
 {
		 printf("Trim phi = %f,\n",trim_phi/3.14*180);
		 printf("Trim theta= %f,\n",trim_theta/3.14*180);
		 return 0;
 }
}


struct zigzag_open_loop_status zigzag_status;

bool zigzag_open_loop(double desired_y,double desired_theta,float max_roll,float break_angle,float break_time)
{
    if(primitive_in_use != ZIGZAG_OPEN_LOOP)
    { 
		primitive_in_use = ZIGZAG_OPEN_LOOP;
        psi = stateGetNedToBodyEulers_f()->psi;
		psi0 = psi;
        z0 = stateGetPositionNed_f()->z;
		zigzag_status.flag_in_zigzag = TRUE;
		zigzag_status.position.x= stateGetPositionNed_f()->x;
		zigzag_status.position.y= stateGetPositionNed_f()->y;
		zigzag_status.position.z= stateGetPositionNed_f()->z;
		zigzag_status.phi_cmd = stateGetNedToBodyEulers_f()->phi;
		zigzag_status.theta_cmd = desired_theta;
        zigzag_status.psi_cmd= stateGetNedToBodyEulers_f()->psi;
        counter_primitive = 0;
        time_primitive = 0;
        guidance_h_mode_changed(GUIDANCE_H_MODE_MODULE);
        guidance_v_mode_changed(GUIDANCE_V_MODE_GUIDED);
		states_race.attitude_control = TRUE;
		zigzag_status.drag_coef.x = -0.55;
		zigzag_status.drag_coef.y = -0.56;
		zigzag_status.drag_coef.z = 0;
		zigzag_status.drag_coef_angular_rate.x = -0.08;
		zigzag_status.drag_coef_angular_rate.y = 0.1;
        zigzag_status.velocity.x = stateGetSpeedNed_f()->x ;
        zigzag_status.velocity.y = stateGetSpeedNed_f()->y ;
        zigzag_status.velocity.z = stateGetSpeedNed_f()->z ;
		zigzag_status.flag_break = TRUE;
		counter_temp3 = 0;
		time_temp3 = 0;
    }

	if (zigzag_status.flag_break == TRUE)
	{
			guidance_loop_set_theta(break_angle);
			guidance_loop_set_phi(0.0);
			guidance_loop_set_heading(zigzag_status.psi_cmd);
			guidance_v_set_guided_z(TAKE_OFF_ALTITUDE);
			if(time_temp3 > break_time)
			{
					zigzag_status.flag_break = FALSE;
			}
			return 0;
	}
// calculate command needed
// transfer velocity from earth coordinate to body and body fixed coordinate
	// calculate body velocity
    zigzag_status.eulers.phi = stateGetNedToBodyEulers_f()->phi;
    zigzag_status.eulers.theta = stateGetNedToBodyEulers_f()->theta;
    zigzag_status.eulers.psi = stateGetNedToBodyEulers_f()->psi;
    float_rmat_of_eulers_321(&zigzag_status.R_E_B,&zigzag_status.eulers); 
    float_rmat_vmult(&zigzag_status.body_velocity,&zigzag_status.R_E_B,&zigzag_status.velocity);

//  calculate drag in body velocity

	/*zigzag_status.drag_b.x = zigzag_status.drag_coef.x*zigzag_status.body_velocity.x+*/
			/*zigzag_status.drag_coef_angular_rate.x*;*/
	/*zigzag_status.drag_b.y = zigzag_status.drag_coef.y*zigzag_status.body_velocity.y;*/
	/*[>z<]igzag_status.drag_b.z = zigzag_status.drag_coef.z*zigzag_status.body_velocity.z;*/

//  transfer drag from body coordinate to earth coordinate

	float_rmat_transp_vmult(&zigzag_status.drag_e,&zigzag_status.R_E_B,&zigzag_status.drag_b);

//  predict trajectory

	struct FloatVect3 thrust_b; 
	struct FloatVect3 thrust_e; 
	struct FloatVect3 g;
	thrust_b.x = 0;
	thrust_b.y = 0;
	thrust_b.z = -9.8/cos(zigzag_status.eulers.phi)/cos(zigzag_status.eulers.theta);

	g.x = 0;
	g.y = 0;
	g.z = 9.8;

    float_rmat_transp_vmult(&thrust_e,&zigzag_status.R_E_B,&thrust_b);

	float_vect_sum(&zigzag_status.d_velocity.x,&g.x,&thrust_e.x,3);
	float_vect_add(&zigzag_status.d_velocity.x,&zigzag_status.drag_e.x,3);
	float_vect_copy(&zigzag_status.d_position.x,&zigzag_status.velocity.x,3);

//  intergrate position and velocity
    float_vect3_integrate_fi(&zigzag_status.position,&zigzag_status.d_position,1.0/20.0);
    float_vect3_integrate_fi(&zigzag_status.velocity,&zigzag_status.d_velocity,1.0/20.0);

	// calculte angle command
	guidance_loop_set_theta(desired_theta);
	if (zigzag_status.position.y < desired_y/2.0)
	{
			guidance_loop_set_phi(max_roll); 
			printf("predicted position y == %f\n",zigzag_status.position.y);
	}
	else
	{
			guidance_loop_set_phi(-max_roll); 
			printf("predicted position y == %f\n",zigzag_status.position.y);
	}
	guidance_loop_set_heading(zigzag_status.psi_cmd);
	guidance_v_set_guided_z(TAKE_OFF_ALTITUDE);


	if (fabs(zigzag_status.position.y-desired_y)<0.2)
	{
			zigzag_status.flag_in_zigzag = FALSE;
			return 1;
	}
	else
	{
			return 0;
	}
}