/*
Copyright (C) 2013 Ben Dyer

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <float.h>

#include "../config/config.h"
#include "../util/util.h"
#include "../util/3dmath.h"
#include "../nmpc/cnmpc.h"
#include "../exports/exports.h"
#include "control.h"
#include "trajectory.h"


float fcs_trajectory_interpolate_figure_eight(
struct fcs_waypoint_t *new_point, const struct fcs_waypoint_t *last_point,
const float *restrict wind, const struct fcs_waypoint_t *start,
const struct fcs_waypoint_t *end, float t) {
#ifndef __TI_COMPILER_VERSION__
#pragma unused(end)
#endif
    /*
    Figure-eight path with the crossover point (middle of the 8) on the
    start waypoint. Altitude is set by the start waypoint; all other
    values are defined by the curve.

    We construct the figure-eight using two circles touching at the start/end
    point. The first circle involves a clockwise turn (banking to starboard),
    and the second a counter-clockwise turn (banking to port). The turn radius
    is FCS_CONTROL_TURN_RADIUS.

    The yaw value of the starting point determines the orientation of the
    pattern; the centres of the circles are FCS_CONTROL_TURN_RADIUS metres
    directly to the port and starboard of the start pose.

    Each circle takes
        2 * pi * FCS_CONTROL_TURN_RADIUS / airspeed
    seconds to complete, so the yaw rate is just
        FCS_CONTROL_TURN_RADIUS / airspeed.

    The sign of last point's roll value determines whether that value is added
    to or subtracted from last point's yaw value to get the new yaw value;
    every full circle, the roll value is negated.

    The new point's lat and lon are determined from the start lat/lon and the
    new yaw.
    */
    float yaw_rate, offset_n, offset_e, sd, cd, sy, cy, target_airspeed,
          target_yaw, target_roll, tangent_n,
          tangent_e, wind_dot, wind_yaw;

    /* Fly at the start speed. */
    target_airspeed = start->airspeed;

    /*
    Work out the wind correction for yaw rate -- project the wind vector onto
    the tangent of the path at the current point (which is the same direction
    as our velocity/heading) and then add that number of radians to the next
    yaw value.
    */
    tangent_n = (float)cos(last_point->yaw);
    tangent_e = (float)sin(last_point->yaw);
    wind_dot = tangent_n * wind[0] + tangent_e * wind[1];
    wind_yaw = t * wind_dot * (float)(1.0 / FCS_CONTROL_TURN_RADIUS);

    /*
    Roll angle is based on airspeed and turn radius (constant):
    roll_deg = 90 - atan(9.8 * r / v^2)
    */
    target_roll =
        (float)(M_PI * 0.5 - atan2(G_ACCEL * FCS_CONTROL_TURN_RADIUS,
                                   (target_airspeed * target_airspeed)));

    /* If delta yaw > start yaw - last yaw, it's time to change direction. */
    target_yaw = start->yaw - last_point->yaw;
    if (target_yaw > M_PI) {
        target_yaw -= M_PI * 2.0;
    } else if (target_yaw < -M_PI) {
        target_yaw += M_PI * 2.0;
    }

    /*
    Determine yaw rate based on airspeed; whether it's left or right depends
    on the current bank angle.
    */
    yaw_rate = target_airspeed * (float)(1.0 / FCS_CONTROL_TURN_RADIUS);

    if (last_point->roll > 0.0 && target_yaw > 0.0 &&
            target_yaw < yaw_rate * t + wind_yaw) {
        target_roll = -target_roll;
        yaw_rate = -target_airspeed * (float)(1.0 / FCS_CONTROL_TURN_RADIUS);
        wind_yaw = -wind_yaw;
    } else if (last_point->roll < 0.0 && target_yaw < 0.0 &&
               target_yaw > -yaw_rate * t - wind_yaw) {
        yaw_rate = target_airspeed * (float)(1.0 / FCS_CONTROL_TURN_RADIUS);
    } else if (last_point->roll < 0.0) {
        target_roll = -target_roll;
        yaw_rate = -yaw_rate;
        wind_yaw = -wind_yaw;
    }

    new_point->roll = target_roll;

    /* Scale roll angle to resolve discontinuity during direction change */
    if (absval(target_yaw) < 1.0f) {
        new_point->roll *=
            max(0.01f, absval(target_yaw) * absval(target_yaw));
    }

    /* Work out next yaw value, constraining to 0..2*pi. */
    new_point->yaw = mod_2pi_f(last_point->yaw + yaw_rate * t + wind_yaw);

    sy = (float)sin(start->yaw);
    cy = (float)cos(start->yaw);
    sd = (float)sin(new_point->yaw - start->yaw);
    cd = (float)cos(new_point->yaw - start->yaw);

    if (last_point->roll < 0.0) {
        /* Bank to the left, so the circle origin is to port. */
        offset_n = sd * -cy + (cd - 1.0f) * -sy;
        offset_e = sd * -sy - (cd - 1.0f) * -cy;
    } else {
        offset_n = sd * cy + (cd - 1.0f) * sy;
        offset_e = sd * sy - (cd - 1.0f) * cy;
    }

    offset_n *= FCS_CONTROL_TURN_RADIUS;
    offset_e *= FCS_CONTROL_TURN_RADIUS;

    /*
    Don't need to multiply offsets by t here, since it's done in the yaw
    calculation, from which these offsets are derived.
    */
    new_point->lat = start->lat + (1.0/WGS84_A) * offset_n;
    new_point->lon = start->lon + (1.0/WGS84_A) * offset_e / cos(start->lat);

    new_point->alt = start->alt;
    new_point->airspeed = target_airspeed;
    new_point->pitch = 0.0f;

    /* Always returning t means we never advance to the next path. */
    return t;
}