#Copyright (C) 2013 Ben Dyer
#
#Permission is hereby granted, free of charge, to any person obtaining a copy
#of this software and associated documentation files (the "Software"), to deal
#in the Software without restriction, including without limitation the rights
#to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#copies of the Software, and to permit persons to whom the Software is
#furnished to do so, subject to the following conditions:
#
#The above copyright notice and this permission notice shall be included in
#all copies or substantial portions of the Software.
#
#THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#SOFTWARE.

import os
import sys
import copy
import math
import time
import socket
import vectors
import datetime
from ctypes import *


_fcs = None


FCS_STREAM_UART_INT0 = 0
FCS_STREAM_UART_INT1 = 1
FCS_STREAM_UART_EXT0 = 2
FCS_STREAM_UART_EXT1 = 3
FCS_STREAM_USB = 4
FCS_STREAM_NUM_DEVICES = 5


FCS_STREAM_OK = 0
FCS_STREAM_ERROR = 1


FCS_MODE_INITIALIZING = ord("I")
FCS_MODE_CALIBRATING = ord("C")
FCS_MODE_SAFE = ord("S")
FCS_MODE_ARMED = ord("R")
FCS_MODE_ACTIVE = ord("A")
FCS_MODE_HOLDING = ord("H")
FCS_MODE_ABORT = ord("F")


FCS_PATH_LINE = ord("L")
FCS_PATH_DUBINS_CURVE = ord("D")
FCS_PATH_FIGURE_EIGHT = ord("8")
FCS_PATH_INVALID = 0


FCS_CONTROL_INVALID_PATH_ID = 0xFFFF
FCS_CONTROL_HOLD_PATH_ID = 500 - 1
FCS_CONTROL_INTERPOLATE_PATH_ID = 500 - 2
FCS_CONTROL_RESUME_PATH_ID = 500 - 3
FCS_CONTROL_HOLD_WAYPOINT_ID = 1000 - 1
FCS_CONTROL_INTERPOLATE_WAYPOINT_ID = 1000 - 2
FCS_CONTROL_RESUME_WAYPOINT_ID = 1000 - 3


ahrs_state = None
control_state = None
nav_state = None
ahrs_tick = 0


def socket_readlines(socket):
    buf = socket.recv(4096)
    done = False
    while 1:
        if "\n" in buf:
            (line, buf) = buf.split("\n", 1)
            yield line + "\n"
        else:
            break


def euler_to_q(yaw, pitch, roll):
    return (vectors.Q.rotate("X", -roll) *
            vectors.Q.rotate("Y", -pitch) *
            vectors.Q.rotate("Z", -yaw))


class TRICALInstance(Structure):
    _fields_ = [
        ("field_norm", c_float),
        ("measurement_noise", c_float),
        ("state", c_float * 12),
        ("state_covariance", c_float * 12 * 12),
        ("measurement_count", c_uint)
    ]


class AHRSCalibrationEntry(Structure):
    _fields_ = [
        ("header", c_ubyte),
        ("sensor", c_ubyte),
        ("type", c_ubyte),
        ("reserved", c_ubyte),
        ("error", c_float),
        ("params", c_float * 12),
        ("orientation", c_float * 4),
        ("offset", c_float * 3),
        ("scale_factor", c_float)
    ]


class AHRSCalibrationMap(Structure):
    _fields_ = [
        ("sensor_calibration", AHRSCalibrationEntry * 128)
    ]


class AHRSMeasurementLog(Structure):
    _fields_ = [
        ("data", c_ubyte * 256),
        ("length", c_uint)
    ]


class AHRSState(Structure):
    _fields_ = [
        ("solution_time", c_ulonglong),
        ("measurements", AHRSMeasurementLog),

        # UKF state + control input
        ("lat", c_double),
        ("lon", c_double),
        ("alt", c_double),
        ("velocity", c_double * 3),
        ("acceleration", c_double * 3),
        ("attitude", c_double * 4),
        ("angular_velocity", c_double * 3),
        ("angular_acceleration", c_double * 3),
        ("wind_velocity", c_double * 3),
        ("gyro_bias", c_double * 3),
        ("control_pos", c_double * 4),

        # UKF error estimate
        ("lat_error", c_double),
        ("lon_error", c_double),
        ("alt_error", c_double),
        ("velocity_error", c_double * 3),
        ("acceleration_error", c_double * 3),
        ("attitude_error", c_double * 3),
        ("angular_velocity_error", c_double * 3),
        ("angular_acceleration_error", c_double * 3),
        ("wind_velocity_error", c_double * 3),
        ("gyro_bias_error", c_double * 3),

        # Configuration
        ("wmm_field_dir", c_double * 3),
        ("wmm_field_norm", c_double),
        ("ukf_process_noise", c_double * 24),
        ("ukf_dynamics_model", c_uint),
        ("calibration", AHRSCalibrationMap),
        ("trical_instances", TRICALInstance * 4),
        ("trical_update_attitude", c_double * 4 * 4),

        # Aerodynamics
        ("reference_alt", c_double),
        ("reference_pressure", c_double),
        ("aero_static_pressure", c_double),
        ("aero_static_temp", c_double),
        ("aero_dynamic_pressure", c_double),

        # Sensor health
        ("last_accelerometer_time", c_ulonglong),
        ("last_gyroscope_time", c_ulonglong),
        ("last_magnetometer_time", c_ulonglong),
        ("last_barometer_time", c_ulonglong),
        ("last_pitot_time", c_ulonglong),
        ("last_gps_time", c_ulonglong),
        ("gps_num_svs", c_uint * 2),
        ("gps_pdop", c_double * 2),

        # Mode
        ("mode_start_time", c_ulonglong),
        ("mode", c_uint),

        # Payload
        ("payload_present", c_ubyte)
    ]


class ControlChannel(Structure):
    _fields_ = [
        ("setpoint", c_float),
        ("min", c_float),
        ("max", c_float),
        ("rate", c_float)
    ]


class ControlState(Structure):
    _fields_ = [
        ("controls", ControlChannel * 4),
        ("gpio_state", c_ubyte)
    ]


class NavPath(Structure):
    _fields_ = [
        ("start_waypoint_id", c_ushort),
        ("end_waypoint_id", c_ushort),
        ("type", c_uint),
        ("flags", c_ushort),
        ("next_path_id", c_ushort)
    ]


class NavWaypoint(Structure):
    _fields_ = [
        ("lat", c_double),
        ("lon", c_double),
        ("alt", c_float),
        ("airspeed", c_float),
        ("yaw", c_float),
        ("pitch", c_float),
        ("roll", c_float),
        ("flags", c_uint)
    ]

    def __repr__(self):
        return ("Waypoint(lat=%12.9f, lon=%12.9f, alt=%6.2f, airspeed=%4.1f, "
                + "yaw=%5.2f, pitch=%5.2f, roll=%5.2f, flags=%x)") % (
                self.lat, self.lon, self.alt, self.airspeed,
                math.degrees(self.yaw), math.degrees(self.pitch),
                math.degrees(self.roll), self.flags)


class NavBoundary(Structure):
    _fields_ = [
        ("num_waypoint_ids", c_ushort),
        ("waypoint_ids", c_ushort * 64),
        ("flags", c_ubyte)
    ]


class NavState(Structure):
    _fields_ = [
        ("paths", NavPath * 500),
        ("waypoints", NavWaypoint * 1000),
        ("boundary", NavBoundary),
        ("reference_trajectory", NavWaypoint * 101),
        ("reference_path_id", c_ushort * 101)
    ]


def reset():
    """
    (Re-)initializes all FCS modules.
    """
    if not _fcs:
        raise RuntimeError("Please call init()")

    _fcs.fcs_board_init_platform()
    _fcs.fcs_util_init()
    _fcs.fcs_comms_init()
    _fcs.fcs_ahrs_init()
    _fcs.fcs_control_init()

    ahrs_state.mode = FCS_MODE_ACTIVE


def tick():
    """
    Runs all periodic FCS tasks.
    """
    if not _fcs:
        raise RuntimeError("Please call init()")

    global ahrs_tick, ahrs_state
    _fcs.fcs_measurement_log_init(ahrs_state.measurements, ahrs_tick)
    ahrs_tick += 1

    _fcs.fcs_board_tick()
    _fcs.fcs_ahrs_tick()
    _fcs.fcs_control_tick()
    _fcs.fcs_comms_tick()


def write(stream_id, value):
    """
    Writes the character array `value` (up to 255 bytes) to the stream
    identified by `stream_id`. Returns the number of bytes written.
    """
    if not _fcs:
        raise RuntimeError("Please call init()")
    if stream_id < 0 or stream_id > FCS_STREAM_NUM_DEVICES:
        raise ValueError("Invalid stream ID")
    if len(value) >= 256:
        raise ValueError(
            "Input value is too long (got %d bytes, max is 255)" % len(value))

    bytes_written = _fcs._fcs_stream_write_to_rx_buffer(
        stream_id, value, len(value))

    return bytes_written


def read(stream_id, max_len):
    """
    Reads up to `max_len` bytes (which must be equal to or smaller than 255)
    from the stream identified by `stream_id`.
    """
    if not _fcs:
        raise RuntimeError("Please call init()")
    if stream_id < 0 or stream_id > FCS_STREAM_NUM_DEVICES:
        raise ValueError("Invalid stream ID")
    if max_len >= 256:
        raise ValueError(
            "Too many bytes requested (got %d, max is 255)" % max_len)
    elif max_len < 1:
        raise ValueError(
            "Can't request fewer than 1 bytes (got %d)" % max_len)

    buf = create_string_buffer(256)
    bytes_read = _fcs._fcs_stream_read_from_tx_buffer(
        stream_id, buf, max_len)

    return buf[0:bytes_read]


def init(dll_path):
    """
    Loads the FCS dynamic library at `dll_path` and sets up the ctypes
    interface. Must be called before any other functions from this module.
    """
    global _fcs, ahrs_state, nav_state, control_state
    # Load the library
    _fcs = cdll.LoadLibrary(dll_path)

    # Get a reference to the required globals
    ahrs_state = AHRSState.in_dll(_fcs, "fcs_global_ahrs_state")
    nav_state = NavState.in_dll(_fcs, "fcs_global_nav_state")
    control_state = ControlState.in_dll(_fcs, "fcs_global_control_state")

    # From ahrs/ahrs.h
    _fcs.fcs_ahrs_init.argtypes = []
    _fcs.fcs_ahrs_init.restype = None

    _fcs.fcs_ahrs_tick.argtypes = []
    _fcs.fcs_ahrs_tick.restype = None

    # From ahrs/measurement.h
    _fcs.fcs_measurement_log_init.argtypes = [POINTER(AHRSMeasurementLog),
                                              c_ushort]
    _fcs.fcs_measurement_log_init.restype = None

    # From comms/comms.h
    _fcs.fcs_comms_init.argtypes = []
    _fcs.fcs_comms_init.restype = None

    _fcs.fcs_comms_tick.argtypes = []
    _fcs.fcs_comms_tick.restype = None

    # From drivers/stream.c
    _fcs._fcs_stream_write_to_rx_buffer.argtypes = [c_ubyte, c_char_p,
                                                    c_ulong]
    _fcs._fcs_stream_write_to_rx_buffer.restype = c_ulong

    _fcs._fcs_stream_read_from_tx_buffer.argtypes = [c_ubyte, c_char_p,
                                                     c_ulong]
    _fcs._fcs_stream_read_from_tx_buffer.restype = c_ulong

    # From hardware/platform/cpuv1-ioboardv1.c
    _fcs.fcs_board_init_platform.argtypes = []
    _fcs.fcs_board_init_platform.restype = None

    _fcs.fcs_board_tick.argtypes = []
    _fcs.fcs_board_tick.restype = None

    # From control/control.h
    _fcs.fcs_control_init.argtypes = []
    _fcs.fcs_control_init.restype = None

    _fcs.fcs_control_tick.argtypes = []
    _fcs.fcs_control_tick.restype = None

    # From util/util.h
    _fcs.fcs_util_init.argtypes = []
    _fcs.fcs_util_init.restype = None

    reset()


if __name__ == "__main__":
    init(sys.argv[1])
    if len(sys.argv) > 2:
        logfile = open(sys.argv[2], "rb")
    else:
        logfile = sys.stdin

    t = 0
    mlog = ""
    mbytes = logfile.read(1024)
    while mbytes:
        mlog, _, mbytes = mbytes.partition("\x00\x00")
        if len(mlog) and len(mlog) < 255:
            write(FCS_STREAM_UART_EXT0, "\x00" + mlog + "\x00")
            tick()

            # Write output for this timestep
            stream_data = [
                read(i, 255) for i in xrange(FCS_STREAM_NUM_DEVICES)]
            if any(stream_data):
                sys.stdout.write(
                    ("%9d," % t) + ",".join(("%r" % s) for s in stream_data) +
                    "\n"
                )
                sys.stdout.flush()

            t += 1

        mbytes = mbytes + logfile.read(1024)
