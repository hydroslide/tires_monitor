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
    // Instrumentation channels (story 08 / issue #9): per-corner inflation Delta
    // (edge-vs-center, in the active temp unit; #18 dropped the baseline term), the
    // Threshold it is
    // judged against, and the signed Verdict (+1 over / 0 / -1 under). OverallVerdict is
    // the device's latched over/under state so the renderer consumes the decision
    // directly instead of re-deriving it.
    FLDelta, FRDelta, RLDelta, RRDelta,
    FLThreshold, FRThreshold, RLThreshold, RRThreshold,
    FLVerdict, FRVerdict, RLVerdict, RRVerdict,
    OverallVerdict,
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

    // Emit the device's own verdict + per-segment colors as its own UPDATEALL packet
    // (story 08 / issue #9), so the downstream renderer applies them directly with no
    // re-derivation. Per camera corner: Delta/Threshold/Verdict numeric channels plus
    // three fill-color and three delta-color hex channels (#RRGGBB); overall = the
    // latched over/under state. cornerIsCamera[] gates the per-band color emission (a
    // single-sensor corner has no bands). Track-mode only (the caller gates it).
    void sendInstrumentation(const float delta[4], const float threshold[4],
                             const int8_t verdict[4], int8_t overall,
                             const bool cornerIsCamera[4],
                             const uint16_t fillColors[4][3],
                             const uint16_t deltaColors[4][3],
                             bool farenheit);

    // Emit self-describing boot metadata as @-metadata lines, once per session (story
    // 08 / issue #9): firmware git SHA, active tire profile + window, offset K / tau,
    // per-corner crop offsets, temperature unit, mode, and ambient source. Lets a log
    // captured months later be interpreted without external notes.
    struct BootMetadata {
        const char* firmwareSha;
        const char* profileName;
        const char* modeName;      // "Track" / "Street"
        char        unit;          // 'F' / 'C'
        int         windowMin, windowIdeal, windowMax;
        int         offsetK;       // degrees F
        int         tauSeconds;
        uint8_t     leftOffset[4];
        uint8_t     rightOffset[4];
        const char* ambientSource; // e.g. "none" (no dedicated ambient sensor)
    };
    void sendBootMetadata(const BootMetadata& m);

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

    // Append a raw "name":value channel line (respecting the newline separator). Used
    // for the hex color channels, whose stable names don't fit the numeric enum model.
    void addNamedChannel(const char* name, const String& value);
    // Append a per-band color channel as a "#RRGGBB" hex string (RGB565 -> RGB888).
    void addHexChannel(const char* name, uint16_t rgb565);
};

#endif // NBP_PROTOCOL_H
