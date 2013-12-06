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

#include "test.h"

extern "C" {
#include "config/config.h"
#include "util/util.h"
#include "comms/comms.h"
}

TEST(SerializeState, NoBuffers) {
    EXPECT_DEATH(
        { fcs_comms_serialize_state(NULL, NULL); }, "Assertion.*failed");

    EXPECT_DEATH(
        { uint8_t x; fcs_comms_serialize_state(&x, NULL); },
        "Assertion.*failed"
    );

    EXPECT_DEATH(
        { struct fcs_packet_state_t x; fcs_comms_serialize_state(NULL, &x); },
        "Assertion.*failed"
    );
}

TEST(SerializeState, InvalidSolutionTime) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_state_t y;
        memset(&y, 0, sizeof(y));
        y.solution_time = -1;
        fcs_comms_serialize_state(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeState, InvalidLat) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_state_t y;
        memset(&y, 0, sizeof(y));
        y.lat = -95.0;
        fcs_comms_serialize_state(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeState, InvalidLon) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_state_t y;
        memset(&y, 0, sizeof(y));
        y.lon = -190.0;
        fcs_comms_serialize_state(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeState, InvalidAlt) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_state_t y;
        memset(&y, 0, sizeof(y));
        y.alt = -1000.0;
        fcs_comms_serialize_state(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeState, InvalidVelocityN) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_state_t y;
        memset(&y, 0, sizeof(y));
        y.velocity[0] = 1000.0;
        fcs_comms_serialize_state(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeState, InvalidVelocityE) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_state_t y;
        memset(&y, 0, sizeof(y));
        y.velocity[1] = 1000.0;
        fcs_comms_serialize_state(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeState, InvalidVelocityD) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_state_t y;
        memset(&y, 0, sizeof(y));
        y.velocity[2] = 1000.0;
        fcs_comms_serialize_state(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeState, InvalidWindN) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_state_t y;
        memset(&y, 0, sizeof(y));
        y.wind_velocity[0] = 1000.0;
        fcs_comms_serialize_state(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeState, InvalidWindE) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_state_t y;
        memset(&y, 0, sizeof(y));
        y.wind_velocity[1] = 1000.0;
        fcs_comms_serialize_state(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeState, InvalidWindD) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_state_t y;
        memset(&y, 0, sizeof(y));
        y.wind_velocity[2] = 1000.0;
        fcs_comms_serialize_state(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeState, InvalidYaw) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_state_t y;
        memset(&y, 0, sizeof(y));
        y.yaw = 400.0;
        fcs_comms_serialize_state(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeState, InvalidPitch) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_state_t y;
        memset(&y, 0, sizeof(y));
        y.pitch = 100.0;
        fcs_comms_serialize_state(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeState, InvalidRoll) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_state_t y;
        memset(&y, 0, sizeof(y));
        y.roll = 200.0;
        fcs_comms_serialize_state(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeState, InvalidAngularVelocityX) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_state_t y;
        memset(&y, 0, sizeof(y));
        y.angular_velocity[0] = 500.0;
        fcs_comms_serialize_state(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeState, InvalidAngularVelocityY) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_state_t y;
        memset(&y, 0, sizeof(y));
        y.angular_velocity[1] = 500.0;
        fcs_comms_serialize_state(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeState, InvalidAngularVelocityZ) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_state_t y;
        memset(&y, 0, sizeof(y));
        y.angular_velocity[2] = 500.0;
        fcs_comms_serialize_state(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeState, AllOK) {
    struct fcs_packet_state_t state = {
        .solution_time = 200501234,
        .next_waypoint_id = { 't', 'e', 's', 't'},
        .lat = -30.0987654321,
        .lon = 145.0123456789,
        .alt = 100.5,
        .velocity = { 1.0, -2.0, 3.0 },
        .wind_velocity = { -10.0, 20.0, -30.0 },
        .yaw = 45.6789,
        .pitch = 80.1234,
        .roll = -175.0987,
        .angular_velocity = { 0.0, 0.0, 0.0 },
        .lat_lon_uncertainty = 99.9,
        .alt_uncertainty = 0.1,
        .velocity_uncertainty = { 0.0, 2.0, 5.0 },
        .wind_velocity_uncertainty = { 0.1, 2.1, 9.9 },
        .yaw_uncertainty = 12.34,
        .pitch_uncertainty = 2.345,
        .roll_uncertainty = 3.456,
        .angular_velocity_uncertainty = { 0.0, 0.5, 1.0 },
        .mode_indicator = 'A',
        .flags = { 't', 'e', 's', 't' }
    };
    uint8_t result[256];
    size_t result_len;

    result_len = fcs_comms_serialize_state(result, &state);
    ASSERT_TRUE(result_len > 0);
    ASSERT_TRUE(result_len < 256u);

    result[result_len] = 0;

    EXPECT_STREQ(
        "$PSFWAS,200501234,test,-30.0987654,145.0123457,100.50,"
        "1.00,-2.00,3.00,-10.00,20.00,-30.00,"
        "45.68,80.12,-175.10,.00,.00,.00,"
        "100,.1,0,2,5,0,2,10,12,2,3,0,1,1,"
        "A,test*73\r\n",
        (char*)result
    );
}

TEST(DeserializeState, AllOK) {
    struct fcs_packet_state_t state;
    uint8_t input[] =
        "$PSFWAS,200501234,test,-30.0987654,145.0123457,100.50,"
        "1.00,-2.00,3.00,-10.00,20.00,-30.00,"
        "45.68,80.12,-175.10,.00,.00,.00,"
        "100,.1,0,2,5,0,2,10,12,2,3,0,1,1,"
        "A,test*73\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_state(&state, input, sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_OK, status);

    EXPECT_EQ(200501234, state.solution_time);
}

TEST(DeserializeState, InvalidChecksum) {
    struct fcs_packet_state_t state;
    uint8_t input[] =
        "$PSFWAS,200501234,test,-30.0987654,145.0123457,100.50,"
        "1.00,-2.00,3.00,-10.00,20.00,-30.00,"
        "45.68,80.12,-175.10,.00,.00,.00,"
        "100,.1,0,2,5,0,2,10,12,2,3,0,1,1,"
        "A,test*03\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_state(&state, input, sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_ERROR, status);
}

TEST(DeserializeState, InvalidTalker) {
    struct fcs_packet_state_t state;
    uint8_t input[] =
        "$PSFWAs,200501234,test,-30.0987654,145.0123457,100.50,"
        "1.00,-2.00,3.00,-10.00,20.00,-30.00,"
        "45.68,80.12,-175.10,.00,.00,.00,"
        "100,.1,0,2,5,0,2,10,12,2,3,0,1,1,"
        "A,test*53\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_state(&state, input, sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_ERROR, status);
}

TEST(DeserializeState, InvalidFieldData) {
    struct fcs_packet_state_t state;
    uint8_t input[] =
        "$PSFWAS,200501234,test,-3q.0987654,145.0123457,100.50,"
        "1.00,-2.00,3.00,-10.00,20.00,-30.00,"
        "45.68,80.12,-175.10,.00,.00,.00,"
        "100,.1,0,2,5,0,2,10,12,2,3,0,1,1,"
        "A,test*32\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_state(&state, input, sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_ERROR, status);
}

TEST(SerializeWaypoint, AllOK) {
    struct fcs_packet_waypoint_t waypoint = {
        .waypoint_id = { 't', 'e', 's', 't'},
        .waypoint_role = 'q',
        .target_lat = -30.0987654321,
        .target_lon = 145.0123456789,
        .target_alt = 100.5,
        .target_yaw = 45.6789,
        .target_pitch = 80.1234,
        .target_roll = -175.0987,
        .target_airspeed = 54.321,
        .flags = { 't', 'e', 's', 't', '1' }
    };
    uint8_t result[256];
    size_t result_len;

    result_len = fcs_comms_serialize_waypoint(result, &waypoint);
    ASSERT_TRUE(result_len > 0);
    ASSERT_TRUE(result_len <= 87u);

    result[result_len] = 0;

    EXPECT_STREQ(
        "$PSFWAP,test,q,-30.0987654,145.0123457,100.50,"
        "45.68,80.12,-175.10,54.32,test1*5B\r\n",
        (char*)result
    );
}

TEST(SerializeWaypoint, InvalidLat) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_waypoint_t y;
        memset(&y, 0, sizeof(y));
        y.target_lat = -135.0;
        fcs_comms_serialize_waypoint(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeWaypoint, InvalidLon) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_waypoint_t y;
        memset(&y, 0, sizeof(y));
        y.target_lon = 235.0;
        fcs_comms_serialize_waypoint(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeWaypoint, InvalidAlt) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_waypoint_t y;
        memset(&y, 0, sizeof(y));
        y.target_alt = -3500.0;
        fcs_comms_serialize_waypoint(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeWaypoint, InvalidYaw) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_waypoint_t y;
        memset(&y, 0, sizeof(y));
        y.target_yaw = -5.0;
        fcs_comms_serialize_waypoint(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeWaypoint, InvalidPitch) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_waypoint_t y;
        memset(&y, 0, sizeof(y));
        y.target_pitch = -135.0;
        fcs_comms_serialize_waypoint(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeWaypoint, InvalidRoll) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_waypoint_t y;
        memset(&y, 0, sizeof(y));
        y.target_roll = -235.0;
        fcs_comms_serialize_waypoint(x, &y);
    }, "Assertion.*failed");
}

TEST(SerializeWaypoint, InvalidAirspeed) {
    EXPECT_DEATH({
        uint8_t x[256];
        struct fcs_packet_waypoint_t y;
        memset(&y, 0, sizeof(y));
        y.target_airspeed = 3000.0;
        fcs_comms_serialize_waypoint(x, &y);
    }, "Assertion.*failed");
}

TEST(DeserializeWaypoint, AllOK) {
    struct fcs_packet_waypoint_t waypoint;
    uint8_t input[] =
        "$PSFWAP,test,q,-30.0987654,145.0123457,100.50,"
        "45.68,80.12,-175.10,54.32,test1*5B\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_waypoint(&waypoint, input,
                                            sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_OK, status);

    EXPECT_NEAR(-30.0987654, waypoint.target_lat, 1e-9);
    EXPECT_NEAR(145.0123457, waypoint.target_lon, 1e-9);
    EXPECT_NEAR(100.50, waypoint.target_alt, 1e-9);
    EXPECT_NEAR(45.68, waypoint.target_yaw, 1e-9);
    EXPECT_NEAR(80.12, waypoint.target_pitch, 1e-9);
    EXPECT_NEAR(-175.10, waypoint.target_roll, 1e-9);
    EXPECT_NEAR(54.32, waypoint.target_airspeed, 1e-9);
}

TEST(DeserializeWaypoint, InvalidChecksum) {
    struct fcs_packet_waypoint_t waypoint;
    uint8_t input[] =
        "$PSFWAP,test,q,-30.0987654,145.0123457,100.50,"
        "45.68,80.12,-175.10,54.32,test1*5A\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_waypoint(&waypoint, input,
                                            sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_ERROR, status);
}

TEST(DeserializeWaypoint, InvalidTalker) {
    struct fcs_packet_waypoint_t waypoint;
    uint8_t input[] =
        "$PSFWAS,test,q,-30.0987654,145.0123457,100.50,"
        "45.68,80.12,-175.10,54.32,test1*58\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_waypoint(&waypoint, input,
                                            sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_ERROR, status);
}

TEST(DeserializeWaypoint, InvalidFieldData) {
    struct fcs_packet_waypoint_t waypoint;
    uint8_t input[] =
        "$PSFWAP,test,q,-30.0987654,145.0123457,100.50,"
        "45.q8,80.12,-175.10,54.32,test1*1C\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_waypoint(&waypoint, input,
                                            sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_ERROR, status);
}

TEST(DeserializeWaypoint, InvalidLat) {
    struct fcs_packet_waypoint_t waypoint;
    uint8_t input[] =
        "$PSFWAP,test,q,-92.0987654,145.0123457,100.50,"
        "45.68,80.12,-175.10,54.32,test1*53\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_waypoint(&waypoint, input,
                                            sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_ERROR, status);
}

TEST(DeserializeWaypoint, InvalidLon) {
    struct fcs_packet_waypoint_t waypoint;
    uint8_t input[] =
        "$PSFWAP,test,q,-30.0987654,185.0123457,100.50,"
        "45.68,80.12,-175.10,54.32,test1*57\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_waypoint(&waypoint, input,
                                            sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_ERROR, status);
}

TEST(DeserializeWaypoint, InvalidAlt) {
    struct fcs_packet_waypoint_t waypoint;
    uint8_t input[] =
        "$PSFWAP,test,q,-30.0987654,145.0123457,100000.50,"
        "45.68,80.12,-175.10,54.32,test1*6B\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_waypoint(&waypoint, input,
                                            sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_ERROR, status);
}

TEST(DeserializeWaypoint, InvalidYaw) {
    struct fcs_packet_waypoint_t waypoint;
    uint8_t input[] =
        "$PSFWAP,test,q,-30.0987654,145.0123457,100.50,"
        "-45.68,80.12,-175.10,54.32,test1*76\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_waypoint(&waypoint, input,
                                            sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_ERROR, status);
}

TEST(DeserializeWaypoint, InvalidPitch) {
    struct fcs_packet_waypoint_t waypoint;
    uint8_t input[] =
        "$PSFWAP,test,q,-30.0987654,145.0123457,100.50,"
        "45.68,90.12,-175.10,54.32,test1*5A\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_waypoint(&waypoint, input,
                                            sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_ERROR, status);
}

TEST(DeserializeWaypoint, InvalidRoll) {
    struct fcs_packet_waypoint_t waypoint;
    uint8_t input[] =
        "$PSFWAP,test,q,-30.0987654,145.0123457,100.50,"
        "45.68,80.12,-185.10,54.32,test1*54\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_waypoint(&waypoint, input,
                                            sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_ERROR, status);
}

TEST(DeserializeWaypoint, InvalidAirspeed) {
    struct fcs_packet_waypoint_t waypoint;
    uint8_t input[] =
        "$PSFWAP,test,q,-30.0987654,145.0123457,100.50,"
        "45.68,80.12,-175.10,-54.32,test1*76\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_waypoint(&waypoint, input,
                                            sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_ERROR, status);
}

TEST(DeserializeGCS, AllOK) {
    struct fcs_packet_gcs_t gcs;
    uint8_t input[] =
        "$PSFWAG,12345678,-30.0987654,145.0123457,100.50,"
        "1034.68*15\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_gcs(&gcs, input, sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_OK, status);

    EXPECT_EQ(12345678, gcs.solution_time);
    EXPECT_NEAR(-30.0987654, gcs.lat, 1e-9);
    EXPECT_NEAR(145.0123457, gcs.lon, 1e-9);
    EXPECT_NEAR(100.50, gcs.alt, 1e-9);
    EXPECT_NEAR(1034.68, gcs.pressure, 1e-9);
}

TEST(DeserializeGCS, InvalidChecksum) {
    struct fcs_packet_gcs_t gcs;
    uint8_t input[] =
        "$PSFWAG,12345678,-30.0987654,145.0123457,100.50,"
        "1034.68*16\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_gcs(&gcs, input, sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_ERROR, status);
}

TEST(DeserializeGCS, InvalidTalker) {
    struct fcs_packet_gcs_t gcs;
    uint8_t input[] =
        "$PSFWAg,12345678,-30.0987654,145.0123457,100.50,"
        "1034.68*35\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_gcs(&gcs, input, sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_ERROR, status);
}

TEST(DeserializeGCS, InvalidSolutionTime) {
    struct fcs_packet_gcs_t gcs;
    uint8_t input[] =
        "$PSFWAG,-50,-30.0987654,145.0123457,100.50,"
        "1034.68*35\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_gcs(&gcs, input, sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_ERROR, status);
}

TEST(DeserializeGCS, InvalidLat) {
    struct fcs_packet_gcs_t gcs;
    uint8_t input[] =
        "$PSFWAG,12345678,130.0987654,145.0123457,100.50,"
        "1034.68*09\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_gcs(&gcs, input, sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_ERROR, status);
}

TEST(DeserializeGCS, InvalidLon) {
    struct fcs_packet_gcs_t gcs;
    uint8_t input[] =
        "$PSFWAG,12345678,-30.0987654,245.0123457,100.50,"
        "1034.68*16\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_gcs(&gcs, input, sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_ERROR, status);
}

TEST(DeserializeGCS, InvalidAlt) {
    struct fcs_packet_gcs_t gcs;
    uint8_t input[] =
        "$PSFWAG,12345678,-30.0987654,145.0123457,-600,"
        "1034.68*14\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_gcs(&gcs, input, sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_ERROR, status);
}

TEST(DeserializeGCS, InvalidPressure) {
    struct fcs_packet_gcs_t gcs;
    uint8_t input[] =
        "$PSFWAG,12345678,-30.0987654,145.0123457,100.50,"
        "50.00*18\r\n";
    enum fcs_deserialization_result_t status;

    status = fcs_comms_deserialize_gcs(&gcs, input, sizeof(input) - 1);
    EXPECT_EQ(FCS_DESERIALIZATION_ERROR, status);
}
