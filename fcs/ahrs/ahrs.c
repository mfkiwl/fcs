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
#include <string.h>
#include <math.h>
#include <assert.h>

#ifdef __TI_COMPILER_VERSION__
#include "../c66x-csl/ti/csl/cslr_device.h"
#include "../c66x-csl/ti/csl/cslr_sem.h"
#endif

#include "../config/config.h"
#include "../util/util.h"
#include "../util/3dmath.h"
#include "../ukf/cukf.h"
#include "../stats/stats.h"
#include "../TRICAL/TRICAL.h"
#include "measurement.h"
#include "wmm.h"
#include "ahrs.h"

#define AHRS_DELTA 0.001

/*
Track the "actual" control position -- NMPC outputs desired position but
we limit the rate at which that can change to the relevant control_rate
value.

The control_scale value is the magnitude of the maximum control input.
*/
static double control_rate[4];
static double control_scale[4];

/* Global FCS state structure */
struct fcs_ahrs_state_t fcs_global_ahrs_state;

/* Macro to limit the absolute value of x to l, but preserve the sign */
#define limitabs(x, l) (x < 0.0 && x < -l ? -l : x > 0.0 && x > l ? l : x)

static void _fcs_ahrs_update_wmm(void);
static void _fcs_ahrs_magnetometer_calibration(void);
static void _fcs_ahrs_accelerometer_calibration(void);
static void _fcs_ahrs_reset_state(void);
static void _fcs_ahrs_update_global_state(double *restrict state,
double *restrict covariance);
static bool _fcs_ahrs_trical_is_valid(TRICAL_instance_t *instance);

void fcs_ahrs_init(void) {
    /* Ensure UKF library is configured correctly */
    assert(ukf_config_get_state_dim() == 24u);
    assert(ukf_config_get_control_dim() == 4u);
    assert(ukf_config_get_measurement_dim() == 20u);
    assert(ukf_config_get_precision() == UKF_PRECISION_DOUBLE);

#ifdef __TI_COMPILER_VERSION__
    /* Release the global state semaphore */
    volatile CSL_SemRegs *const semaphore =
            (CSL_SemRegs*)CSL_SEMAPHORE_REGS;
    semaphore->SEM[FCS_SEMAPHORE_GLOBAL_STATE] = 1u;
#endif

    fcs_wmm_init();

    /* TODO: don't reset attitude if any of the entries are non-zero */
    memset(fcs_global_ahrs_state.attitude, 0, sizeof(double) * 4u);
    fcs_global_ahrs_state.attitude[3] = 1.0;

    /* Reset/init the UKF */
    _fcs_ahrs_reset_state();

    /* Calculate WMM field at current lat/lon/alt/time */
    _fcs_ahrs_update_wmm();

    /* Set the UKF model */
    double default_process_noise[] = {
        1e-15, 1e-15, 1e-5, /* lat, lon, alt */
        7e-5, 7e-5, 7e-5, /* velocity N, E, D */
        2e-4, 2e-4, 2e-4, /* acceleration x, y, z */
        1e-9, 1e-9, 1e-9, /* attitude roll, pitch, yaw */
        3e-3, 3e-3, 3e-3, /* angular velocity roll, pitch, yaw */
        1e-3, 1e-3, 1e-3, /* angular acceleration roll, pitch, yaw */
        1e-5, 1e-5, 1e-5, /* wind velocity N, E, D */
        1.5e-12, 1.5e-12, 1.5e-12 /* gyro bias x, y, z */
    };
    memcpy(fcs_global_ahrs_state.ukf_process_noise, default_process_noise,
           sizeof(default_process_noise));

    fcs_global_ahrs_state.lat = -37.8136 * M_PI / 180.0;
    fcs_global_ahrs_state.lon = 144.9631 * M_PI / 180.0;
    fcs_global_ahrs_state.alt = 70.0;

    /* Initialize AHRS mode */
    fcs_global_ahrs_state.mode = FCS_MODE_INITIALIZING;
    fcs_global_ahrs_state.ukf_dynamics_model = UKF_MODEL_X8;

    /*
    Update the TRICAL instance parameters. Instances 0 and 1 are
    magnetometers; instances 2 and 3 are accelerometers.
    */
    uint8_t i;
    for (i = 0; i < FCS_AHRS_NUM_TRICAL_INSTANCES; i++) {
        TRICAL_init(&fcs_global_ahrs_state.trical_instances[i]);

        /*
        Set the norm to 1.0, because the sensor calibration scale factor is
        set such that TRICAL will always work with (theoretically) unit
        vectors.
        */
        TRICAL_norm_set(&fcs_global_ahrs_state.trical_instances[i], 1.0f);
        TRICAL_noise_set(&fcs_global_ahrs_state.trical_instances[i],
                         i < 2u ? 1e-3f : 1.0f);
    }
}

void fcs_ahrs_tick(void) {
    /* Increment solution time */
    fcs_global_ahrs_state.solution_time++;

    _fcs_ahrs_update_wmm();
    _fcs_ahrs_magnetometer_calibration();

    /*
    While copying measurement data to the UKF, get sensor error and geometry
    information so the sensor model parameters can be updated.
    */
    struct ukf_ioboard_params_t params = {
        {0, 0, 0, 1}, /* accel_orientation */
        {0, 0, 0}, /* accel_offset */
        {0, 0, 0, 1}, /* gyro_orientation */
        {0, 0, 0, 1} /* mag_orientation */
    };

    /* Use the latest WMM field vector (unit length) */
    memcpy(params.mag_field, fcs_global_ahrs_state.wmm_field_dir,
           sizeof(double) * 3u);

    /* Read sensor data from the measurement log, and pass it to the UKF */
    double v[4], err;
    bool got_values;
    struct fcs_measurement_log_t *restrict mlog =
        &fcs_global_ahrs_state.measurements;
    const struct fcs_calibration_map_t *restrict cmap =
        &fcs_global_ahrs_state.calibration;

    ukf_sensor_clear();

    got_values = fcs_measurement_log_get_calibrated_value(
        mlog, cmap, FCS_MEASUREMENT_TYPE_ACCELEROMETER, v, &err,
        params.accel_offset, 1.0);
    if (got_values) {
        /* Accelerometer output is in g, convert to m/s^2 */
        ukf_sensor_set_accelerometer(v[0] * G_ACCEL, v[1] * G_ACCEL,
                                     v[2] * G_ACCEL);
        params.accel_covariance[0] = params.accel_covariance[1] =
            params.accel_covariance[2] = err * err;
    }

    got_values = fcs_measurement_log_get_calibrated_value(
        mlog, cmap, FCS_MEASUREMENT_TYPE_GYROSCOPE, v, &err, NULL, 1.0);
    if (got_values) {
        ukf_sensor_set_gyroscope(v[0], v[1], v[2]);
        params.gyro_covariance[0] = params.gyro_covariance[1] =
            params.gyro_covariance[2] = err * err;
    }

    /*
    We need to pre-scale the sensor reading by the current WMM field magnitude
    to work with the calibration params.
    */
    double field_norm_inv = 1.0 / fcs_global_ahrs_state.wmm_field_norm;
    got_values = fcs_measurement_log_get_calibrated_value(
        mlog, cmap, FCS_MEASUREMENT_TYPE_MAGNETOMETER, v, &err, NULL,
        field_norm_inv);
    if (got_values) {
        /*
        The calibration scales the magnetometer value to unity expectation.
        Scale error by the same amount, so the units of error are Gauss.
        */
        ukf_sensor_set_magnetometer(v[0], v[1], v[2]);
        err *= field_norm_inv;
        params.mag_covariance[0] = params.mag_covariance[1] =
            params.mag_covariance[2] = err * err;
    }

    got_values = fcs_measurement_log_get_calibrated_value(
        mlog, cmap, FCS_MEASUREMENT_TYPE_PITOT, v, &err, NULL, 1.0);
    if (got_values) {
        ukf_sensor_set_pitot_tas(0.0 /* v[0] */);
        params.pitot_covariance = err * err;
    }

    got_values = fcs_measurement_log_get_calibrated_value(
        mlog, cmap, FCS_MEASUREMENT_TYPE_PRESSURE_TEMP, v, &err, NULL, 1.0);
    if (got_values) {
        ukf_sensor_set_barometer_amsl(v[0]);
        params.barometer_amsl_covariance = err * err;
    }

    got_values = fcs_measurement_log_get_calibrated_value(
        mlog, cmap, FCS_MEASUREMENT_TYPE_GPS_POSITION, v, &err, NULL, 1.0);
    if (got_values) {
        ukf_sensor_set_gps_position(v[0], v[1], v[2]);
        params.gps_position_covariance[0] =
            params.gps_position_covariance[1] = err * err;
        params.gps_position_covariance[2] = 225.0;
    }

    got_values = fcs_measurement_log_get_calibrated_value(
        mlog, cmap, FCS_MEASUREMENT_TYPE_GPS_VELOCITY, v, &err, NULL, 1.0);
    if (got_values) {
        ukf_sensor_set_gps_velocity(v[0], v[1], v[2]);
        params.gps_velocity_covariance[0] =
            params.gps_velocity_covariance[1] = err * err;
        params.gps_velocity_covariance[2] = 49.0;
    }

    /* TODO: Read control set values from NMPC */
    double control_set[4] = { 0.0, 0.0, 0.0, 0.0 };

    /*
    Work out the nominal current control position, taking into account the
    control response time configured in control_rates. Log the result.
    */
    struct fcs_measurement_t control_log;
    uint8_t i;

    #pragma MUST_ITERATE(4, 4)
    for (i = 0; i < 4u; i++) {
        double delta = control_set[i] - fcs_global_ahrs_state.control_pos[i],
               limit = control_rate[i] * AHRS_DELTA;
        fcs_global_ahrs_state.control_pos[i] += limitabs(delta, limit);
        control_log.data.i16[i] =
            (int16_t)(fcs_global_ahrs_state.control_pos[i] / control_scale[i]
                      * INT16_MAX);
    }

    fcs_measurement_set_header(&control_log, 16u, 4u);
    fcs_measurement_set_sensor(&control_log, 0,
                               FCS_MEASUREMENT_TYPE_CONTROL_POS);
    fcs_measurement_log_add(mlog, &control_log);

    /*
    Run the UKF, taking sensor readings and current control position into
    account
    */
    ukf_set_params(&params);
    ukf_set_process_noise(fcs_global_ahrs_state.ukf_process_noise);

    /* Don't update the filter during initialization */
    if (fcs_global_ahrs_state.mode != FCS_MODE_INITIALIZING) {
        ukf_iterate(AHRS_DELTA, fcs_global_ahrs_state.control_pos);
    }

    /* Copy the global state out of the UKF */
    double state_values[25], covariance[24];
    ukf_get_state((struct ukf_state_t*)state_values);
    ukf_get_state_covariance_diagonal(covariance);

    /*
    Use different process noise values during calibration to get the gyro
    bias estimate converging more quickly.
    */
    if (fcs_global_ahrs_state.mode == FCS_MODE_CALIBRATING) {
        fcs_global_ahrs_state.ukf_process_noise[9] = 1e-5;
        fcs_global_ahrs_state.ukf_process_noise[10] = 1e-5;
        fcs_global_ahrs_state.ukf_process_noise[11] = 1e-5;

        /* Trust the gyro bias estimate less.  */
        fcs_global_ahrs_state.ukf_process_noise[21] = 1e-7;
        fcs_global_ahrs_state.ukf_process_noise[22] = 1e-7;
        fcs_global_ahrs_state.ukf_process_noise[23] = 1e-7;

        /*
        Run TRICAL on the current accelerometer results.
        */
        _fcs_ahrs_accelerometer_calibration();
    } else {
        fcs_global_ahrs_state.ukf_process_noise[9] = 1e-8;
        fcs_global_ahrs_state.ukf_process_noise[10] = 1e-8;
        fcs_global_ahrs_state.ukf_process_noise[11] = 1e-8;

        fcs_global_ahrs_state.ukf_process_noise[21] = 1e-9;
        fcs_global_ahrs_state.ukf_process_noise[22] = 1e-9;
        fcs_global_ahrs_state.ukf_process_noise[23] = 1e-9;
    }

    /* Validate the UKF state; if it's invalid, reset it */
    bool ukf_valid = true;

    #pragma MUST_ITERATE(24, 24);
    for (i = 0; i < 24u; i++) {
        if (isnan(state_values[i]) || isnan(covariance[i])) {
            ukf_valid = false;
        }
    }
    if (isnan(state_values[24])) {
        ukf_valid = false;
    }

    if (ukf_valid) {
        /* Update the global state structure */
        _fcs_ahrs_update_global_state(state_values, covariance);
    } else {
        _fcs_ahrs_reset_state();
    }

    /* Check the current mode and transition if necessary */
    if (fcs_global_ahrs_state.mode == FCS_MODE_INITIALIZING) {
        fcs_global_ahrs_state.mode = FCS_MODE_CALIBRATING;
        fcs_global_ahrs_state.mode_start_time =
            fcs_global_ahrs_state.solution_time;
    } else if (fcs_global_ahrs_state.mode == FCS_MODE_CALIBRATING) {
        /* Transition out of calibration mode after 30s */
        if (fcs_global_ahrs_state.solution_time -
                fcs_global_ahrs_state.mode_start_time > 30000) {
            fcs_global_ahrs_state.mode = FCS_MODE_SAFE;
        }
    }
}

static void _fcs_ahrs_reset_state(void) {
    size_t i;

    /*
    Reset critical UKF parameters (position, attitude, gyro bias) based on
    their previous values, and clear everything else.
    */
    struct ukf_state_t reset_state;
    memset(&reset_state, 0, sizeof(reset_state));

    /*
    Copy the last position and attitude; if gyro bias is sane, copy that too
    */
    reset_state.position[0] = fcs_global_ahrs_state.lat;
    reset_state.position[1] = fcs_global_ahrs_state.lon;
    reset_state.position[2] = fcs_global_ahrs_state.alt;

    #pragma MUST_ITERATE(3, 3);
    for (i = 0; i < 3u; i++) {
        reset_state.attitude[i] = fcs_global_ahrs_state.attitude[i];

        if (fabs(fcs_global_ahrs_state.gyro_bias[i]) < M_PI / 10.0) {
            reset_state.gyro_bias[i] = fcs_global_ahrs_state.gyro_bias[i];
        }
    }
    reset_state.attitude[3] = fcs_global_ahrs_state.attitude[3];

    ukf_init();
    ukf_set_state(&reset_state);

    double covariance[24];
    ukf_get_state_covariance_diagonal(covariance);

    /* Update the output state and covariance with the latest values */
    _fcs_ahrs_update_global_state((double*)&reset_state, covariance);

    fcs_global_counters.ukf_resets++;
}

static void _fcs_ahrs_update_global_state(double *restrict state,
double *restrict covariance) {
#ifdef __TI_COMPILER_VERSION__
    /*
    Use a semaphore to prevent the NMPC code accessing the state while we're
    updating it.
    */
    volatile CSL_SemRegs *const semaphore =
        (CSL_SemRegs*)CSL_SEMAPHORE_REGS;
    uint32_t sem_val = semaphore->SEM[FCS_SEMAPHORE_GLOBAL_STATE];
    assert(sem_val == 1u);
#endif

    memcpy(&fcs_global_ahrs_state.lat, state, sizeof(double) * 25u);
    memcpy(&fcs_global_ahrs_state.lat_covariance, covariance,
           sizeof(double) * 24u);

#ifdef __TI_COMPILER_VERSION__
    /* Release the semaphore */
    semaphore->SEM[FCS_SEMAPHORE_GLOBAL_STATE] = 1u;
#endif
}

static void _fcs_ahrs_update_wmm(void) {
    /* Calculate WMM field at current lat/lon/alt/time */
    bool result;
    result = fcs_wmm_calculate_field(
        fcs_global_ahrs_state.lat, fcs_global_ahrs_state.lon,
        fcs_global_ahrs_state.alt, 2014.0,
        fcs_global_ahrs_state.wmm_field_dir);
    if (!result) {
        fcs_global_counters.wmm_errors++;
    } else {
        fcs_global_ahrs_state.wmm_field_norm =
            vector3_norm_d(fcs_global_ahrs_state.wmm_field_dir);

        double norm_inv = 1.0 / fcs_global_ahrs_state.wmm_field_norm;
        fcs_global_ahrs_state.wmm_field_dir[0] *= norm_inv;
        fcs_global_ahrs_state.wmm_field_dir[1] *= norm_inv;
        fcs_global_ahrs_state.wmm_field_dir[2] *= norm_inv;

        /*
        WMM returns the field in nT; the magnetometer sensitivity is in Gauss
        (1G = 100 000nT). Convert the field norm to G.
        */
        fcs_global_ahrs_state.wmm_field_norm *= (1.0f / 100000.0f);
    }
}

static void _fcs_ahrs_magnetometer_calibration(void) {
    /*
    Handle magnetometer calibration update based on new readings -- we do this
    regardless of calibration mode
    */
    struct fcs_calibration_t *restrict sensor_calibration_map =
        fcs_global_ahrs_state.calibration.sensor_calibration;
    TRICAL_instance_t *restrict instance;
    struct fcs_measurement_t measurement;

    double mag_value[4], expected_field[3], scale_factor, delta_angle,
           field_norm_inv = 1.0f / fcs_global_ahrs_state.wmm_field_norm;
    float mag_value_f[3], expected_field_f[3];
    size_t i;

    /*
    Rotate the WMM field by the current attitude to get the expected field
    direction for these readings
    */
    quaternion_vector3_multiply_d(
        expected_field, fcs_global_ahrs_state.attitude,
        fcs_global_ahrs_state.wmm_field_dir);
    expected_field_f[0] = expected_field[0];
    expected_field_f[1] = expected_field[1];
    expected_field_f[2] = expected_field[2];

    for (i = 0; i < 2u; i++) {
        if (fcs_measurement_log_find(
                &fcs_global_ahrs_state.measurements,
                FCS_MEASUREMENT_TYPE_MAGNETOMETER, i, &measurement)) {
            /*
            If the current attitude is too close to the attitude at which this
            TRICAL instance was last updated, skip calibration this time
            */
            delta_angle = quaternion_quaternion_angle_d(
                    fcs_global_ahrs_state.attitude,
                    fcs_global_ahrs_state.trical_update_attitude[i]);
            if (delta_angle < 3.0 * M_PI / 180.0) {
                continue;
            }

            /* Get the measurement value */
            fcs_measurement_get_values(&measurement, mag_value);

            uint8_t sensor_key =
                ((i << FCS_MEASUREMENT_SENSOR_ID_OFFSET) &
                 FCS_MEASUREMENT_SENSOR_ID_MASK) |
                ((FCS_MEASUREMENT_TYPE_MAGNETOMETER <<
                  FCS_MEASUREMENT_SENSOR_TYPE_OFFSET) &
                 FCS_MEASUREMENT_SENSOR_TYPE_MASK);

            /*
            Update TRICAL instance parameters with the latest results. Scale
            the magnetometer reading such that the expected magnitude is the
            unit vector, by dividing by the current WMM field strength in
            Gauss.
            */
            instance = &fcs_global_ahrs_state.trical_instances[i];

            /*
            Copy the current sensor calibration to the TRICAL instance state
            so that any external changes to the calibration are captured.
            */
            memcpy(instance->state, sensor_calibration_map[sensor_key].params,
                   9u * sizeof(float));

            scale_factor = field_norm_inv *
                sensor_calibration_map[sensor_key].scale_factor;
            mag_value_f[0] = mag_value[0] * scale_factor;
            mag_value_f[1] = mag_value[1] * scale_factor;
            mag_value_f[2] = mag_value[2] * scale_factor;

            TRICAL_estimate_update(instance, mag_value_f, expected_field_f);

            if (!_fcs_ahrs_trical_is_valid(instance)) {
                fcs_global_counters.trical_resets[i]++;
            }

            /*
            Copy the TRICAL calibration estimate to the magnetometer
            calibration
            */
            memcpy(sensor_calibration_map[sensor_key].params, instance->state,
                   9u * sizeof(float));

            /*
            Record the attitude at which this TRICAL instance was last updated
            so that we can space out calibration updates
            */
            memcpy(fcs_global_ahrs_state.trical_update_attitude[i],
                   fcs_global_ahrs_state.attitude, sizeof(double) * 4);
        }
    }
}

static void _fcs_ahrs_accelerometer_calibration(void) {
    struct fcs_calibration_t *restrict sensor_calibration_map =
        fcs_global_ahrs_state.calibration.sensor_calibration;
    TRICAL_instance_t *restrict instance;
    struct fcs_measurement_t measurement;

    double accel_value[4], scale_factor;
    float accel_value_f[3], g_field[] = { 0.0, 0.0, -1.0 };
    size_t i;

    for (i = 0; i < 2u; i++) {
        if (fcs_measurement_log_find(
                &fcs_global_ahrs_state.measurements,
                FCS_MEASUREMENT_TYPE_ACCELEROMETER, i, &measurement)) {
            /* Get the accelerometer value */
            fcs_measurement_get_values(&measurement, accel_value);

            uint8_t sensor_key =
                ((i << FCS_MEASUREMENT_SENSOR_ID_OFFSET) &
                 FCS_MEASUREMENT_SENSOR_ID_MASK) |
                ((FCS_MEASUREMENT_TYPE_ACCELEROMETER <<
                  FCS_MEASUREMENT_SENSOR_TYPE_OFFSET) &
                 FCS_MEASUREMENT_SENSOR_TYPE_MASK);

            /*
            Update TRICAL instance parameters with the latest results.

            The accelerometer TRICAL instances are 3 and 4.
            */
            instance = &fcs_global_ahrs_state.trical_instances[i + 2u];

            /*
            Copy the current sensor calibration to the TRICAL instance
            state so that any external changes to the calibration are
            captured.
            */
            memcpy(instance->state,
                   sensor_calibration_map[sensor_key].params,
                   9u * sizeof(float));

            scale_factor =
                sensor_calibration_map[sensor_key].scale_factor;
            accel_value_f[0] = accel_value[0] * scale_factor;
            accel_value_f[1] = accel_value[1] * scale_factor;
            accel_value_f[2] = accel_value[2] * scale_factor;

            /*
            If the vehicle is level, we can assume that bias accounts for
            essentially the entire deviation from a reading of (0, 0, -1).

            Strictly that's not quite true, as the Z-axis may have scale
            error, but it'll get us pretty close.

            Since we're level, we can also set the expected field
            direction to straight down.
            */
            if (instance->state[0] == 0.0 || instance->state[1] == 0.0 ||
                    instance->state[2] == 0.0) {
                instance->state[0] = accel_value_f[0];
                instance->state[1] = accel_value_f[1];
                instance->state[2] = accel_value_f[2] + 1.0f;
            }

            TRICAL_estimate_update(instance, accel_value_f, g_field);

            if (!_fcs_ahrs_trical_is_valid(instance)) {
                fcs_global_counters.trical_resets[i + 2u]++;
            }

            /*
            Copy the TRICAL calibration estimate to the accelerometer
            calibration
            */
            memcpy(sensor_calibration_map[sensor_key].params,
                   instance->state, 9u * sizeof(float));
        }
    }
}

static bool _fcs_ahrs_trical_is_valid(TRICAL_instance_t *instance) {
    size_t j;
    for (j = 0; j < 9u; j++) {
        if (isnan(instance->state[j])) {
            /* TRICAL has blown up -- reset this instance. */
            TRICAL_reset(instance);
            return false;
        }
    }

    return true;
}

bool fcs_ahrs_set_mode(enum fcs_mode_t mode) {
    fcs_global_ahrs_state.mode = mode;

    /* Activate the dynamics model in active mode */
    if (mode == FCS_MODE_ACTIVE) {
        ukf_choose_dynamics(0);
    } else {
        ukf_choose_dynamics(fcs_global_ahrs_state.ukf_dynamics_model);
    }

    return true;
}
