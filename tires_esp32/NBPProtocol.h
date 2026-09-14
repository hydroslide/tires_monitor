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
    LongitudinalG,
    LateralG,
    YawRate,
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
//
// One packet, every channel, always the same shape. NBP clients (TrackAddict) fix
// their channel list on the FIRST ALL/UPDATEALL packet they see, and the spec says an
// UPDATEALL "must include all supported data channels". This class therefore keeps a
// registry of every channel the device has ever set, and publish() emits one UPDATEALL
// carrying all of them. The set*() calls only update the registry; nothing is sent
// until publish(). (Before this, each set emitted its own partial UPDATEALL, so which
// channels TrackAddict logged depended on which packet arrived first after connecting.)
class NBPProtocol {
public:
    // Constructor: Accepts a Stream object for serial communication
    NBPProtocol(Stream &serial);

    // Emit one UPDATEALL packet with every registered channel, at most once per
    // minIntervalMs (rate limit; the caller may invoke it from several places).
    void publish(unsigned long minIntervalMs = 100);

    // Sends metadata information (e.g., device name, version)
    void sendMetadata(const char* type, const char* value);

    // Register or update a data channel (name from the enum) with its unit and value.
    void addChannel(ChannelType channel, Unit unit, float value);

    // Drop every registered channel. Only for a deliberate re-shape; a client that
    // already captured the channel list will not follow.
    void clearChannels();

    void setTireTemps(float frontLeftTemp, float frontRightTemp, float rearLeftTemp, float rearRightTemp, bool farenheit);

    // Active (calculated when enabled) temps under the original channel labels.
    void setAllTireTemps(const Wheels::TireTemps &fl,
                      const Wheels::TireTemps &fr,
                      const Wheels::TireTemps &rl,
                      const Wheels::TireTemps &rr, bool farenheit);

    // Raw surface reading under the "... Raw" channel labels (story 03). Mirrors
    // setAllTireTemps' O/C/I ordering per corner.
    void setRawTireTemps(const Wheels::TireTemps &fl,
                      const Wheels::TireTemps &fr,
                      const Wheels::TireTemps &rl,
                      const Wheels::TireTemps &rr, bool farenheit);

    // Orientation-calibrated IMU sample: the raw sensor axes (accel in g, gyro in deg/s)
    // plus the vehicle-frame longitudinal / lateral acceleration and yaw rate derived
    // from the same sample. All land in the shared UPDATEALL with one timestamp.
    void setIMU(float ax, float ay, float az,
                float gx, float gy, float gz,
                float longitudinalG, float lateralG, float yawRateDps);

    // Sealed session summary (story 01). Overheat is seconds-over; window is percent;
    // balance/warm-up are their stored values. Skipped when the summary is not valid.
    void setSessionSummary(const SessionSummary& s);
    // Register the summary channels at -1 before any seal, so they are part of the
    // packet shape from the first publish (a client will not add columns mid-log).
    void presetSessionSummary(bool farenheit);

    // The device's own verdict + per-segment colors (story 08 / issue #9), so the
    // downstream renderer applies them directly with no re-derivation. Per camera
    // corner: Delta/Threshold/Verdict numeric channels plus three fill-color and three
    // delta-color channels, each an integer 0xRRGGBB (NBP values must be numeric);
    // overall = the latched over/under state. cornerIsCamera[] gates the per-band
    // emission (a single-sensor corner has no bands). Track-mode only (the caller gates it).
    void setInstrumentation(const float delta[4], const float threshold[4],
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
        bool        imuPresent;
        uint8_t     imuRateHz;
        char        imuLongitudinalAxis;
        char        imuLateralAxis;
        char        imuYawAxis;
    };
    void sendBootMetadata(const BootMetadata& m);

    // Registered channels (for tests / diagnostics).
    int channelCount() const { return nChannels; }

private:
    // Reference to the Stream object for communication
    Stream &serial;

    // Last timestamp used for packets
    unsigned long lastTime;
    unsigned long lastPublishMs;

    // Channel registry, in first-seen order. Names and units are string literals owned
    // by getChannelName()/getUnitName() or the static color-name tables.
    static const int MAX_CHANNELS = 96;
    struct Channel { const char* name; const char* unit; float value; uint8_t decimals; };
    Channel channels[MAX_CHANNELS];
    int nChannels;

    int  findChannel(const char* name) const;
    void upsert(const char* name, const char* unit, float value, uint8_t decimals = 2);
    // Per-band color channel as an integer 0xRRGGBB (RGB565 -> RGB888).
    void addColorChannel(const char* name, uint16_t rgb565);

    // Write one UPDATEALL packet with every registered channel.
    void sendUpdateAll();
    void sendPacketHeader(const char* packetType);
    void sendPacketFooter();

    // Converts enum values to corresponding strings
    const char* getChannelName(ChannelType channel);
    const char* getUnitName(Unit unit);
};

#endif // NBP_PROTOCOL_H
