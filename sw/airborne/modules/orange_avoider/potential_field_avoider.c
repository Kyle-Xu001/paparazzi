/*
 * Copyright (C) Kirk Scheper <kirkscheper@gmail.com>
 *
 * This file is part of paparazzi
 *
 */
/**
 * @file "modules/orange_avoider/potential_field_avoider.c"
 * @author Kirk Scheper
 * This module is an example module for the course AE4317 Autonomous Flight of Micro Air Vehicles at
 * the TU Delft. This module is used in combination with a color filter (cv_detect_color_object) and
 * the guided mode of the autopilot. The avoidance strategy is to simply count the total number of
 * orange pixels. When above a certain percentage threshold, (given by color_count_frac) we assume
 * that there is an obstacle and we turn.
 *
 * The color filter settings are set using the cv_detect_color_object. This module can run multiple
 * filters simultaneously so you have to define which filter to use with the
 * POTENTIAL_FIELD_AVOIDER_VISUAL_DETECTION_ID setting. This module differs from the simpler
 * orange_avoider.xml in that this is flown in guided mode. This flight mode is less dependent on a
 * global positioning estimate as with the navigation mode. This module can be used with a simple
 * speed estimate rather than a global position.
 *
 * Here we also need to use our onboard sensors to stay inside of the cyberzoo and not collide with
 * the nets. For this we employ a simple color detector, similar to the orange poles but for green
 * to detect the floor. When the total amount of green drops below a given threshold (given by
 * floor_count_frac) we assume we are near the edge of the zoo and turn around. The color detection
 * is done by the cv_detect_color_object module, use the FLOOR_VISUAL_DETECTION_ID setting to define
 * which filter to use.
 */

#include "modules/orange_avoider/potential_field_avoider.h"

#include <stdio.h>
#include <time.h>
#include "modules/core/abi.h"
#include "firmwares/rotorcraft/autopilot_guided.h"
#include "firmwares/rotorcraft/guidance/guidance_h.h"
#include "firmwares/rotorcraft/navigation.h"
#include "generated/airframe.h"
#include "math/pprz_algebra.h"
#include "math/pprz_algebra_float.h"
#include "modules/core/abi.h"
#include "state.h"
#define POTENTIAL_FIELD_AVOIDER_VERBOSE TRUE

#define PRINT(string, ...) \
  fprintf(stderr, "[potential_field_avoider->%s()] " string, __FUNCTION__, ##__VA_ARGS__)
#define DEBUG_PRINT(string, ...) fprintf(stderr, " " string, ##__VA_ARGS__)
#if POTENTIAL_FIELD_AVOIDER_VERBOSE
#define VERBOSE_PRINT PRINT
#else
#define VERBOSE_PRINT(...)
#endif

uint8_t chooseRandomIncrementAvoidance(void);

float computeDistance(float obs_x, float obs_y);
/**
 * @brief attraction function
 *
 * @param goal
 * @param current
 * @return struct FloatVect2
 */

struct FloatVect2 attractive(struct FloatVect2 goal, struct FloatVect2 current);

/**
 * @brief repulsion function
 *
 * @param obs
 * @param current
 * @return struct FloatVect2
 */
struct FloatVect2 repulsion(struct FloatVect2* obs, struct FloatVect2 current);

/**
 * @brief get update as position
 *
 * @param obs
 * @param goal
 * @param curpt
 * @return struct FloatVect2
 */
struct FloatVect2 potentialFieldPosUpdate(struct FloatVect2* obs, struct FloatVect2 goal,
                                          struct FloatVect2 curpt);

/**
 * @brief get update as velocity
 *
 * @param obs
 * @param goal
 * @param curpt
 * @return struct FloatVect2
 */
struct FloatVect2 potentialFieldVelUpdate(struct FloatVect2* obs, struct FloatVect2 goal,
                                          struct FloatVect2 curpt);

/**
 * @brief convert 2D points in global frame to 2D points in body frame
 *
 * @param pos_global
 * @return struct FloatVect2
 */
struct FloatVect2 globalToBodyPosition(struct FloatVect2* global);

/**
 * @brief state machine
 */
enum navigation_state_t { SAFE, PLANNING, WAIT_TARGET, EMERGENCY, OUT_OF_BOUNDS, REENTER_ARENA };

// define settings
float K_ATTRACTION        = 10;   // strength of attraction force
float K_REPULSION         = 10;   // strength of repulsion force
float PF_GOAL_THRES       = 0.2;  // threshold near the goal
float PF_MAX_ITER         = 10;   // max iteration of potential field iterations
float PF_STEP_SIZE        = 1.0;  // step size between current states and new goal
float PF_INFLUENCE_RADIUS = 3.0;  // distance where repulsion can take effect
float PF_MAX_VELOCITY     = 1.0;  // maximum velocity
float PF_FORWARD_WEIGHT   = 1.0;  // weight for moving forward

// define and initialise global variables
enum navigation_state_t navigation_state = WAIT_TARGET;  // current state in state machine
// int32_t color_count    = 0;  // orange color count from color filter for obstacle detection
// int32_t floor_count    = 0;  // green color count from color filter for floor detection
// int32_t floor_centroid = 0;  // floor detector centroid in y direction (along the horizon)
float   avoidance_heading_direction = 0.3;  // heading change direction for avoidance [rad/s]
int16_t obstacle_free_confidence    = 0;    // certainty that the way ahead if safe.

float oag_color_count_frac = 0.18f;  // obstacle detection threshold as a fraction of total of image
float oag_floor_count_frac = 0.05f;  // floor detection threshold as a fraction of total of image
int32_t color_count        = 0;
int32_t floor_count        = 0;
int32_t floor_centroid     = 0;  // floor detector centroid in y direction (along the horizon)

// Define obstacle position
#define NUM_OBS 5
#define NUM_WPS 10
// array of obstacles

// array of all waypoints
struct FloatVect2 _obs[NUM_OBS] = {
    {0.6f, 0.7f}, {2.5f, 2.8f}, {-2.5f, 1.5f}, {-1.8f, -3.4f}, {0.5f, -1.8f}};

uint8_t _goal_flag = 0;

// array of all goals
struct FloatVect2 _goals[4] = {{2.0f, 2.0f}, {-2.0f, 2.0f}, {-2.0f, -2.0f}, {2.0f, -2.0f}};
struct FloatVect2 _goal;  // current goal

// This call back will be used to receive the color count from the orange detector
#ifndef POTENTIAL_FIELD_AVOIDER_VISUAL_DETECTION_ID
#error This module requires two color filters, as such you have to define POTENTIAL_FIELD_AVOIDER_VISUAL_DETECTION_ID to the orange filter
#error Please define POTENTIAL_FIELD_AVOIDER_VISUAL_DETECTION_ID to be COLOR_OBJECT_DETECTION1_ID or
// COLOR_OBJECT_DETECTION2_ID in your airframe
#endif
static abi_event color_detection_ev;
static void      color_detection_cb(uint8_t __attribute__((unused)) sender_id,
                                    int16_t __attribute__((unused)) pixel_x,
                                    int16_t __attribute__((unused)) pixel_y,
                                    int16_t __attribute__((unused)) pixel_width,
                                    int16_t __attribute__((unused)) pixel_height, int32_t quality,
                                    int16_t __attribute__((unused)) extra) {
  color_count = quality;
}

#ifndef FLOOR_VISUAL_DETECTION_ID
#error This module requires two color filters, as such you have to define FLOOR_VISUAL_DETECTION_ID to the orange filter
#error Please define FLOOR_VISUAL_DETECTION_ID to be COLOR_OBJECT_DETECTION1_ID or COLOR_OBJECT_DETECTION2_ID in your airframe
#endif
static abi_event floor_detection_ev;
static void      floor_detection_cb(uint8_t __attribute__((unused)) sender_id,
                                    int16_t __attribute__((unused)) pixel_x, int16_t pixel_y,
                                    int16_t __attribute__((unused)) pixel_width,
                                    int16_t __attribute__((unused)) pixel_height, int32_t quality,
                                    int16_t __attribute__((unused)) extra) {
  floor_count    = quality;
  floor_centroid = pixel_y;
}

// needed to receive output from a separate module running on a parallel process
int32_t x_flow=-1, y_flow=-1;
#ifndef FLOW_OPTICFLOW_CAM1_ID
#define FLOW_OPTICFLOW_CAM1_ID ABI_BROADCAST
#endif
static abi_event opticflow_ev;
static void opticflow_cb(uint8_t __attribute__((unused)) sender_id,
                         uint32_t __attribute__((unused)) stamp, 
                         int32_t flow_x, 
                         int32_t flow_y,
                         int32_t flow_der_x, 
                         int32_t flow_der_y,
                         float __attribute__((unused)) quality, 
                         float size_divergence) {
  x_flow = flow_x;
  y_flow = flow_y;
}

/*
 * Initialisation function
 */
void potential_field_avoider_init(void) {
  // Initialise random values
  srand(time(NULL));
  _goal      = _goals[0];
  _goal_flag = 0;
  VERBOSE_PRINT("[goal] Set goal at (%.2f, %.2f)\n", _goal.x, _goal.y);
  // bind our colorfilter callbacks to receive the color filter outputs
  // AbiBindMsgVISUAL_DETECTION(POTENTIAL_FIELD_AVOIDER_VISUAL_DETECTION_ID, &color_detection_ev,
  //                            color_detection_cb);
  // AbiBindMsgVISUAL_DETECTION(FLOOR_VISUAL_DETECTION_ID, &floor_detection_ev, floor_detection_cb);
  AbiBindMsgOPTICAL_FLOW(FLOW_OPTICFLOW_ID, &opticflow_ev, opticflow_cb);
}

void potential_field_avoider_periodic(void) {
  if (guidance_h.mode != GUIDANCE_H_MODE_GUIDED) {
    navigation_state = SAFE;
    VERBOSE_PRINT("[GUIDE] guidance_h.mode is %i \n", guidance_h.mode);
    return;
  }

  // compute current color thresholds
  int32_t color_count_threshold =
      oag_color_count_frac * front_camera.output_size.w * front_camera.output_size.h;
  int32_t floor_count_threshold =
      oag_floor_count_frac * front_camera.output_size.w * front_camera.output_size.h;
  float floor_centroid_frac = floor_centroid / (float)front_camera.output_size.h / 2.f;

  VERBOSE_PRINT("Color_count: %d  threshold: %d state: %d \n", color_count, color_count_threshold,
                navigation_state);
  VERBOSE_PRINT("Floor count: %d, threshold: %d\n", floor_count, floor_count_threshold);
  VERBOSE_PRINT("Floor centroid: %f\n", floor_centroid_frac);

  // update our safe confidence using color threshold
  if (color_count < color_count_threshold) {
    obstacle_free_confidence++;
  } else {
    obstacle_free_confidence -= 2;  // be more cautious with positive obstacle detections
  }

  // bound obstacle_free_confidence
  Bound(obstacle_free_confidence, 0, 5);

  switch (navigation_state) {
    case SAFE:
      VERBOSE_PRINT("======== SAFE ========\n");
      struct FloatVect2 state = {stateGetPositionNed_f()->x, stateGetPositionNed_f()->y};
      VERBOSE_PRINT("[STATE] (%.2f, %.2f)\n", state.x, state.y);

      // TODO: use bottom camera to detect out of bound
      if (ABS(state.x) >= 2.5f || ABS(state.y) >= 2.5f) {
        navigation_state = OUT_OF_BOUNDS;
      }

      struct FloatVect2 zero = {0.0f, 0.0f};
      struct FloatVect2 obs_local[NUM_OBS];

      for (uint8_t idx = 0; idx < NUM_OBS; idx++) {
        DEBUG_PRINT("[OBS] ");
        DEBUG_PRINT(" Global: %i (%.2f, %.2f) ", idx, _obs[idx].x, _obs[idx].y);
        obs_local[idx] = globalToBodyPosition(&_obs[idx]);
        DEBUG_PRINT("\tLocal: [%.2f, %.2f] \n", obs_local[idx].x, obs_local[idx].y);
      }
      DEBUG_PRINT("\n");
      // VERBOSE_PRINT("[GOAL] (%.2f, %.2f) ", _goal.x, _goal.y);
      // struct FloatVect2 goal_local = globalToBodyPosition(&_goal);
      // DEBUG_PRINT("\n");
      struct FloatVect2 goal_local = {2.0f, 1.0f};
      VERBOSE_PRINT("[GOAL] (%.2f, %.2f)\n", goal_local.x, goal_local.y);

      struct FloatVect2 wpt = potentialFieldVelUpdate(&obs_local, goal_local, zero);

      /* distance to the goal */
      struct FloatVect2 diff, cur;
      cur.x = stateGetPositionNed_f()->x;
      cur.y = stateGetPositionNed_f()->y;
      VECT2_DIFF(diff, _goal, cur);
      float distance = VECT2_NORM2(diff);

      if (distance > PF_GOAL_THRES) {
        wpt.x += 1.0f * PF_FORWARD_WEIGHT;  // make velocity towards forward
        float agl = atan2f(wpt.y, wpt.x);

        /* if send waypoints in body frame */
        // guided_pos_body_relative(wpt.x, wpt.y, agl);

        /* if send velocity */
        guided_vel_body_relative(wpt.x, wpt.y, agl);

        VERBOSE_PRINT("[state] current heading is %.3f \n", stateGetNedToBodyEulers_f()->psi);
        VERBOSE_PRINT("[state] current atan2f is %.3f \n", agl);
      } else {
        navigation_state = PLANNING;
      }

      // TODO(@siyuan): understand how controller controls to set points
      // TODO(@siyuan): understand the execution time
      break;

    case PLANNING:
      VERBOSE_PRINT("======== PLANNING ========\n");
      _goal_flag++;
      if (_goal_flag == 4) {
        _goal_flag = 0;
      }
      _goal = _goals[_goal_flag];
      VERBOSE_PRINT("[goal] Set goal at (%.2f, %.2f)\n", _goal.x, _goal.y);
      navigation_state = SAFE;
      break;

    case EMERGENCY:
      VERBOSE_PRINT("FSM: ======== EMERGENCY ========\n");
      // step back if closed to obstacles
      guided_pos_body_relative(-0.5, 0, 0);
      navigation_state = SAFE;
      break;

    case WAIT_TARGET:
      VERBOSE_PRINT("FSM: ======== WAIT_TARGET ========\n");

      // turning slowly
      guidance_h_set_guided_heading_rate(RadOfDeg(5));

      break;

    case OUT_OF_BOUNDS:
      VERBOSE_PRINT("FSM: ======== OUT_OF_BOUNDS ========\n");
      // stop
      guidance_h_set_guided_body_vel(0, 0);

      // start turn back into arena
      // TODO: We use global position in this part
      float ox      = stateGetPositionNed_f()->x;
      float oy      = stateGetPositionNed_f()->y;
      float yaw_rad = stateGetNedToBodyEulers_f()->psi;
      yaw_rad       = (yaw_rad < M_PI) ? (yaw_rad + 2 * M_PI) : yaw_rad;
      float oyaw    = DegOfRad(yaw_rad);
      oyaw          = (oyaw < -180) ? (oyaw + 360) : oyaw;
      oyaw          = (oyaw > 180) ? (oyaw - 360) : oyaw;
      VERBOSE_PRINT("[STATE] (%.2f, %.2f, %.2f) ", state.x, state.y, oyaw);

      if (ox > 2.8f && oyaw < -90) {
        DEBUG_PRINT("HEADING SOUTH \n");
        guidance_h_set_guided_body_vel(0.5, 0);
        navigation_state = SAFE;
      } else if (ox < -2.8f && oyaw < 90 && oyaw > 0) {
        DEBUG_PRINT("HEADING NORTH \n");
        guidance_h_set_guided_body_vel(0.5, 0);
        navigation_state = SAFE;
      } else if (oy > 2.8f && oyaw > 90) {
        DEBUG_PRINT("HEADING EAST \n");
        guidance_h_set_guided_body_vel(0.5, 0);
        navigation_state = SAFE;
      } else if (oy < 2.8f && oyaw < 0 && oyaw > -90) {
        DEBUG_PRINT("HEADING WEST \n");
        guidance_h_set_guided_body_vel(0.5, 0);
        navigation_state = SAFE;
      } else {
        DEBUG_PRINT("ROTATING \n");
        guidance_h_set_guided_body_vel(0, 0);
        guidance_h_set_guided_heading_rate(RadOfDeg(30));
        navigation_state = OUT_OF_BOUNDS;
      }
      break;

    case REENTER_ARENA:
      VERBOSE_PRINT("FSM: ======== REENTER_ARENA ========\n");
      guidance_h_set_guided_heading_rate(RadOfDeg(15));
      // force floor center to opposite side of turn to head back into arena
      if (floor_count >= floor_count_threshold &&
          avoidance_heading_direction * floor_centroid_frac >= 0.f) {
        // guidance_h_set_guided_heading_rate(RadOfDeg(180));

        // return to heading mode
        guidance_h_set_guided_heading(stateGetNedToBodyEulers_f()->psi);

        // reset safe counter
        obstacle_free_confidence = 0;

        // ensure direction is safe before continuing
        navigation_state = SAFE;
      } else {
      }
      break;
    default:
      break;
  }
}

void guided_pos_ned(float x, float y, float heading) {
  guidance_h_set_guided_pos(x, y);
  guidance_h_set_guided_heading(heading);
}

void guided_pos_ned_relative(float dx, float dy, float dyaw) {
  float x       = stateGetPositionNed_f()->x + dx;
  float y       = stateGetPositionNed_f()->y + dy;
  float heading = stateGetNedToBodyEulers_f()->psi + dyaw;
  guided_pos_ned(x, y, heading);
}

void guided_pos_body_relative(float dx, float dy, float dyaw) {
  float psi     = stateGetNedToBodyEulers_f()->psi;
  float x       = stateGetPositionNed_f()->x + cosf(-psi) * dx + sinf(-psi) * dy;
  float y       = stateGetPositionNed_f()->y - sinf(-psi) * dx + cosf(-psi) * dy;
  float heading = psi + dyaw;
  guided_pos_ned(x, y, heading);
}

void guided_vel_body_relative(float vx, float vy, float dyaw) {
  DEBUG_PRINT("[VEL] %.2f m/s\n",
              sqrtf(SQUARE(PF_MAX_VELOCITY * vx) + SQUARE(PF_MAX_VELOCITY * vy)));
  guidance_h_set_guided_body_vel(PF_MAX_VELOCITY * vx, PF_MAX_VELOCITY * vy);
  guidance_h_set_guided_heading_rate(dyaw);
}

void guided_move_ned(float vx, float vy, float heading) {
  guidance_h_set_guided_vel(vx, vy);
  guidance_h_set_guided_heading(heading);
}

float computeDistance(float obs_x, float obs_y) {
  float x0 = stateGetPositionNed_f()->x;
  float y0 = stateGetPositionNed_f()->y;
  return sqrtf(SQUARE(x0 - obs_x) + SQUARE(y0 - obs_y));
}

struct FloatVect2 attractive(struct FloatVect2 goal, struct FloatVect2 current) {
  struct FloatVect2 att;
  VECT2_DIFF(att, goal, current);
  VECT2_SMUL(att, att, K_ATTRACTION);
  return att;
}

struct FloatVect2 repulsion(struct FloatVect2* obs, struct FloatVect2 current) {
  struct FloatVect2 rep, tmp, dir;
  VECT2_ASSIGN(rep, 0.0f, 0.0f);
  for (int i = 0; i < NUM_OBS; i++) {
    /* debug */
    // DEBUG_PRINT("[REP] obs %i (%.2f, %.2f), ", i, obs[i].x, obs[i].y);

    if (obs[i].x < 0) {
      DEBUG_PRINT("[REP] obstacle invisible \n");
      continue;
    } else {
      VECT2_DIFF(tmp, current, obs[i]);
      float distance = VECT2_NORM2(tmp);

      if (distance > PF_INFLUENCE_RADIUS) {
        DEBUG_PRINT("[REP] obstacle too far\n");
        continue;
      } else {
        VECT2_SDIV(dir, tmp, distance);
        DEBUG_PRINT("[REP] distance is (%.2f)\n", distance);
        float u = K_REPULSION * (1.0f / distance - 1.0f / PF_INFLUENCE_RADIUS) / (SQUARE(distance));
        DEBUG_PRINT("[REP] repulsion gain is (%.2f)\n", u);
        VECT2_SMUL(dir, dir, u);
        VECT2_ADD(rep, dir);
      }
    }
  }
  DEBUG_PRINT("[REP] computed repulsion direction is (%.2f, %.2f)\n", rep.x, rep.y);
  return rep;
}

struct FloatVect2 potentialFieldPosUpdate(struct FloatVect2* obs, struct FloatVect2 goal,
                                          struct FloatVect2 curpt) {
  struct FloatVect2 newpt;  // new position
  struct FloatVect2 force;  // potential force
  struct FloatVect2 diret;  // direction

  struct FloatVect2 att = attractive(goal, curpt);
  struct FloatVect2 rep = repulsion(obs, curpt);
  VECT2_SUM(force, att, rep);
  VECT2_SDIV(force, force, sqrtf(VECT2_NORM2(force)));
  DEBUG_PRINT("[UPDATE] computed force as (%.2f, %.2f)\n", force.x, force.y);
  VECT2_SMUL(diret, force, PF_STEP_SIZE);
  VECT2_SUM(newpt, curpt, diret);
  DEBUG_PRINT("[UPDATE] computed new waypoint as (%.2f, %.2f)\n", newpt.x, newpt.y);
  return newpt;
}

struct FloatVect2 potentialFieldVelUpdate(struct FloatVect2* obs, struct FloatVect2 goal,
                                          struct FloatVect2 curpt) {
  struct FloatVect2 newpt;  // new position
  struct FloatVect2 force;  // potential force
  struct FloatVect2 diret;  // direction

  struct FloatVect2 att = attractive(goal, curpt);
  struct FloatVect2 rep = repulsion(obs, curpt);
  VECT2_SUM(force, att, rep);
  DEBUG_PRINT("[UPDATE] computed force as (%.2f, %.2f)\n", force.x, force.y);
  VECT2_SDIV(force, force, sqrtf(VECT2_NORM2(force)));
  VECT2_SMUL(diret, force, PF_STEP_SIZE);
  DEBUG_PRINT("[UPDATE] computed new speed as (%.2f, %.2f)\n", diret.x, diret.y);
  return diret;
}

struct FloatVect2 globalToBodyPosition(struct FloatVect2* global) {
  float psi = stateGetNedToBodyEulers_f()->psi;
  float x_g = global->x - stateGetPositionNed_f()->x;
  float y_g = global->y - stateGetPositionNed_f()->y;

  /* debug */
  // DEBUG_PRINT("Psi: %.2f; x_g: %.2f = %.2f - %.2f; y_g: %.2f = %.2f - %.2f;", psi, x_g,
  // global->x,
  //            stateGetPositionNed_f()->x, y_g, global->y, stateGetPositionNed_f()->y);

  struct FloatVect2 local;
  local.x = x_g * cosf(-psi) - y_g * sinf(-psi);
  local.y = x_g * sinf(-psi) + y_g * cosf(-psi);
  return local;
}

uint8_t chooseRandomIncrementAvoidance(void) {
  // Randomly choose CW or CCW avoiding direction
  if (rand() % 2 == 0) {
    avoidance_heading_direction = 1.f;
    VERBOSE_PRINT("Set avoidance increment to: %f\n", avoidance_heading_direction * RadOfDeg(20.f));
  } else {
    avoidance_heading_direction = -1.f;
    VERBOSE_PRINT("Set avoidance increment to: %f\n", avoidance_heading_direction * RadOfDeg(20.f));
  }
  return false;
}
