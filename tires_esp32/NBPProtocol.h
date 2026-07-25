#ifndef NBP_PROTOCOL_H
#define NBP_PROTOCOL_H

#include <Arduino.h>
#include "Wheels.h"

// Enumerations for channel types and units
enum class ChannelType {
    Battery,
    BrakePedal,
    SteeringWheel,
    Gear,
    FrontLeftTire,
    FrontRightTire,
    RearLeftTire,
    RearRightTire,
    FrontLeftTireO,
    FrontRightTireO,
    RearLeftTireO,
    RearRightTireO,
    FrontLeftTireC,
    FrontRightTireC,
    RearLeftTireC,
    RearRightTireC,
    FrontLeftTireI,
    FrontRightTireI,
    RearLeftTireI,
    RearRightTireI,
    // Raw surface channels (story 03): the untouched, unsmoothed surface reading,
    // logged as its own channel set so calibration/tau can be re-derived offline even
    // while calculated mode drives the original labels.
    FrontLeftTireRaw,
    FrontRightTireRaw,
    RearLeftTireRaw,
    RearRightTireRaw,
    FrontLeftTireRawO,
    FrontRightTireRawO,
    RearLeftTireRawO,
    RearRightTireRawO,
    FrontLeftTireRawC,
    FrontRightTireRawC,
    RearLeftTireRawC,
    RearRightTireRawC,
    FrontLeftTireRawI,
    FrontRightTireRawI,
    RearLeftTireRawI,
    RearRightTireRawI,
    // IMU (orientation-calibrated) channels
    AccelX,
    AccelY,
    AccelZ,
    GyroX,
    GyroY,
    GyroZ,
    LateralG,
    // Session summary channels (story 01): emitted once on seal so the per-corner recap
    // is captured off-device. Per-corner peak/avg/time-in-window/overheat, plus the
    // session-level balance deltas and warm-up time.
    SumFLPeak, SumFRPeak, SumRLPeak, SumRRPeak,
    SumFLAvg,  SumFRAvg,  SumRLAvg,  SumRRAvg,
    SumFLWindow, SumFRWindow, SumRLWindow, SumRRWindow,
    SumFLOver, SumFROver, SumRLOver, SumRROver,
    SumFrontRear, SumLeftRight, SumWarmup, SumLength,
    // Add additional types as needed
};

struct SessionSummary; // defined in SessionManager.h

enum class Unit {
    V,         // Volts
    Percent,   // Percentage
    Degrees,   // Degrees
    DegreesF,
    DegreesC,
    G,         // g (acceleration)
    DegPerSec, // deg/s (angular rate)
    None       // No unit
};

// NBPProtocol Class Definition
class NBPProtocol {
public:
    // Constructor: Accepts a Stream object for serial communication
    NBPProtocol(Stream &serial);

    // Sends an UPDATEALL packet containing all channels
    void sendUpdateAll();

    // Sends metadata information (e.g., device name, version)
    void sendMetadata(const char* type, const char* value);

    // Adds a data channel with name, unit, and value
    void addChannel(ChannelType channel, Unit unit, float value);

    // Clears all added data channels
    void clearChannels();

    void setTireTemps(float frontLeftTemp, float frontRightTemp, float rearLeftTemp, float rearRightTemp, bool farenheit);

    void setAllTireTemps(const Wheels::TireTemps &fl,
                      const Wheels::TireTemps &fr,
                      const Wheels::TireTemps &rl,
                      const Wheels::TireTemps &rr, bool farenheit);

    // Emit the raw surface reading as its own UPDATEALL packet under the "... Raw"
    // channel labels (story 03). Mirrors setAllTireTemps' O/C/I ordering per corner.
    void setRawTireTemps(const Wheels::TireTemps &fl,
                      const Wheels::TireTemps &fr,
                      const Wheels::TireTemps &rl,
                      const Wheels::TireTemps &rr, bool farenheit);

    // Emit the orientation-calibrated IMU sample (accel in g, gyro in deg/s) plus
    // the gated lateral-g value as its own UPDATEALL packet.
    void sendIMU(float ax, float ay, float az,
                 float gx, float gy, float gz, float lateralG);

    // Emit the sealed session summary as its own UPDATEALL packet (story 01). Overheat
    // is emitted as seconds-over; window as percent; balance/warm-up as their stored
    // values. Skipped when the summary is not valid.
    void sendSessionSummary(const SessionSummary& s);

private:
    // Reference to the Stream object for communication
    Stream &serial;

    // Last timestamp used for packets
    unsigned long lastTime;

    // Internal buffer to store channel data
    String data;

    // Helper functions to send packet components
    void sendPacketHeader(const char* packetType);
    void sendData();
    void sendPacketFooter();

    // Converts enum values to corresponding strings
    const char* getChannelName(ChannelType channel);
    const char* getUnitName(Unit unit);
};

#endif // NBP_PROTOCOL_H
