#include "NBPProtocol.h"
#include "Wheels.h"
#include "SessionManager.h"

// Constructor: Initialize with the provided Stream object
NBPProtocol::NBPProtocol(Stream &serial) 
    : serial(serial), lastTime(0) {}

// Sends an UPDATEALL packet containing all channels
void NBPProtocol::sendUpdateAll() {
    sendPacketHeader("UPDATEALL");
    sendData();
    sendPacketFooter();
}

// Sends metadata information (e.g., device name, version)
void NBPProtocol::sendMetadata(const char* type, const char* value) {
    serial.print("@");
    serial.print(type);
    serial.print(":");
    serial.println(value);
}

// Adds a data channel with name, unit, and value
void NBPProtocol::addChannel(ChannelType channel, Unit unit, float value) {
    if (data.length() > 0) data += "\n";
    
    const char* channelName = getChannelName(channel);
    const char* unitName = getUnitName(unit);
    
    if (unit != Unit::None) {
        data += "\"" + String(channelName) + "\",\"" + String(unitName) + "\":" + String(value, 2);
    } else {
        data += "\"" + String(channelName) + "\":" + String(value, 2);
    }
}

void NBPProtocol::setAllTireTemps(const Wheels::TireTemps &fl,
                      const Wheels::TireTemps &fr,
                      const Wheels::TireTemps &rl,
                      const Wheels::TireTemps &rr, bool farenheit){
    clearChannels();
    Unit tempUnit = (farenheit)? Unit::DegreesF:Unit::DegreesC;
    // Use the explicit section count to decide single- vs three-channel emission.
    // Previously a value of 0 in the middle band was overloaded as "single sensor",
    // so a legitimate 0 (or a startup-rejected band) silently changed the channel set
    // mid-file — a parsing hazard for the logged dumps.
    if (fl.count < 3)
        addChannel(ChannelType::FrontLeftTire, tempUnit, fl.values[0]);
    else{
        addChannel(ChannelType::FrontLeftTireO, tempUnit, fl.values[0]);
        addChannel(ChannelType::FrontLeftTireC, tempUnit, fl.values[1]);
        addChannel(ChannelType::FrontLeftTireI, tempUnit, fl.values[2]);
    }
    if (fr.count < 3)
        addChannel(ChannelType::FrontRightTire, tempUnit, fr.values[0]);
    else{
        addChannel(ChannelType::FrontRightTireI, tempUnit, fr.values[0]);
        addChannel(ChannelType::FrontRightTireC, tempUnit, fr.values[1]);
        addChannel(ChannelType::FrontRightTireO, tempUnit, fr.values[2]);
    }
    if (rl.count < 3)
        addChannel(ChannelType::RearLeftTire, tempUnit, rl.values[0]);
    else{
        addChannel(ChannelType::RearLeftTireO, tempUnit, rl.values[0]);
        addChannel(ChannelType::RearLeftTireC, tempUnit, rl.values[1]);
        addChannel(ChannelType::RearLeftTireI, tempUnit, rl.values[2]);
    }
    if (rr.count < 3)
        addChannel(ChannelType::RearRightTire, tempUnit, rr.values[0]);
    else{
        addChannel(ChannelType::RearRightTireI, tempUnit, rr.values[0]);
        addChannel(ChannelType::RearRightTireC, tempUnit, rr.values[1]);
        addChannel(ChannelType::RearRightTireO, tempUnit, rr.values[2]);
    }
    
    sendUpdateAll();
}

void NBPProtocol::setRawTireTemps(const Wheels::TireTemps &fl,
                      const Wheels::TireTemps &fr,
                      const Wheels::TireTemps &rl,
                      const Wheels::TireTemps &rr, bool farenheit){
    clearChannels();
    Unit tempUnit = (farenheit)? Unit::DegreesF:Unit::DegreesC;
    // Same single- vs three-channel decision and O/C/I ordering as the active set,
    // just under the distinct "... Raw" labels so both coexist in one log.
    if (fl.count < 3)
        addChannel(ChannelType::FrontLeftTireRaw, tempUnit, fl.values[0]);
    else{
        addChannel(ChannelType::FrontLeftTireRawO, tempUnit, fl.values[0]);
        addChannel(ChannelType::FrontLeftTireRawC, tempUnit, fl.values[1]);
        addChannel(ChannelType::FrontLeftTireRawI, tempUnit, fl.values[2]);
    }
    if (fr.count < 3)
        addChannel(ChannelType::FrontRightTireRaw, tempUnit, fr.values[0]);
    else{
        addChannel(ChannelType::FrontRightTireRawI, tempUnit, fr.values[0]);
        addChannel(ChannelType::FrontRightTireRawC, tempUnit, fr.values[1]);
        addChannel(ChannelType::FrontRightTireRawO, tempUnit, fr.values[2]);
    }
    if (rl.count < 3)
        addChannel(ChannelType::RearLeftTireRaw, tempUnit, rl.values[0]);
    else{
        addChannel(ChannelType::RearLeftTireRawO, tempUnit, rl.values[0]);
        addChannel(ChannelType::RearLeftTireRawC, tempUnit, rl.values[1]);
        addChannel(ChannelType::RearLeftTireRawI, tempUnit, rl.values[2]);
    }
    if (rr.count < 3)
        addChannel(ChannelType::RearRightTireRaw, tempUnit, rr.values[0]);
    else{
        addChannel(ChannelType::RearRightTireRawI, tempUnit, rr.values[0]);
        addChannel(ChannelType::RearRightTireRawC, tempUnit, rr.values[1]);
        addChannel(ChannelType::RearRightTireRawO, tempUnit, rr.values[2]);
    }

    sendUpdateAll();
}

void NBPProtocol::sendIMU(float ax, float ay, float az,
                          float gx, float gy, float gz, float lateralG) {
    clearChannels();
    addChannel(ChannelType::AccelX, Unit::G, ax);
    addChannel(ChannelType::AccelY, Unit::G, ay);
    addChannel(ChannelType::AccelZ, Unit::G, az);
    addChannel(ChannelType::GyroX, Unit::DegPerSec, gx);
    addChannel(ChannelType::GyroY, Unit::DegPerSec, gy);
    addChannel(ChannelType::GyroZ, Unit::DegPerSec, gz);
    addChannel(ChannelType::LateralG, Unit::G, lateralG);
    sendUpdateAll();
}

void NBPProtocol::sendSessionSummary(const SessionSummary& s) {
    if (!s.valid) return;
    clearChannels();
    Unit tempUnit = (s.unit == 'C') ? Unit::DegreesC : Unit::DegreesF;

    addChannel(ChannelType::SumFLPeak, tempUnit, (float)s.peak[0]);
    addChannel(ChannelType::SumFRPeak, tempUnit, (float)s.peak[1]);
    addChannel(ChannelType::SumRLPeak, tempUnit, (float)s.peak[2]);
    addChannel(ChannelType::SumRRPeak, tempUnit, (float)s.peak[3]);

    addChannel(ChannelType::SumFLAvg, tempUnit, (float)s.avg[0]);
    addChannel(ChannelType::SumFRAvg, tempUnit, (float)s.avg[1]);
    addChannel(ChannelType::SumRLAvg, tempUnit, (float)s.avg[2]);
    addChannel(ChannelType::SumRRAvg, tempUnit, (float)s.avg[3]);

    addChannel(ChannelType::SumFLWindow, Unit::Percent, (float)s.inWindowPct[0]);
    addChannel(ChannelType::SumFRWindow, Unit::Percent, (float)s.inWindowPct[1]);
    addChannel(ChannelType::SumRLWindow, Unit::Percent, (float)s.inWindowPct[2]);
    addChannel(ChannelType::SumRRWindow, Unit::Percent, (float)s.inWindowPct[3]);

    addChannel(ChannelType::SumFLOver, Unit::None, (float)s.overheatSec[0]);
    addChannel(ChannelType::SumFROver, Unit::None, (float)s.overheatSec[1]);
    addChannel(ChannelType::SumRLOver, Unit::None, (float)s.overheatSec[2]);
    addChannel(ChannelType::SumRROver, Unit::None, (float)s.overheatSec[3]);

    addChannel(ChannelType::SumFrontRear, tempUnit, (float)s.frontRearDelta);
    addChannel(ChannelType::SumLeftRight, tempUnit, (float)s.leftRightDelta);
    // Warm-up: emit -1 when undefined (a seen corner never warmed).
    addChannel(ChannelType::SumWarmup, Unit::None,
               (s.warmupSec == 0xFFFF) ? -1.0f : (float)s.warmupSec);
    addChannel(ChannelType::SumLength, Unit::None, (float)s.durationSec);

    sendUpdateAll();
}

// Append a raw "name":value line, matching addChannel's newline separator handling.
void NBPProtocol::addNamedChannel(const char* name, const String& value) {
    if (data.length() > 0) data += "\n";
    data += "\"" + String(name) + "\":" + value;
}

// Convert an RGB565 color to a quoted "#RRGGBB" hex string channel. The 5/6/5 fields are
// expanded to 8 bits (replicate the high bits) so full-scale reads as FF, not F8/FC.
void NBPProtocol::addHexChannel(const char* name, uint16_t rgb565) {
    uint8_t r5 = (rgb565 >> 11) & 0x1F;
    uint8_t g6 = (rgb565 >> 5)  & 0x3F;
    uint8_t b5 =  rgb565        & 0x1F;
    uint8_t r = (r5 << 3) | (r5 >> 2);
    uint8_t g = (g6 << 2) | (g6 >> 4);
    uint8_t b = (b5 << 3) | (b5 >> 2);
    char buf[10];
    snprintf(buf, sizeof(buf), "\"#%02X%02X%02X\"", r, g, b);
    addNamedChannel(name, String(buf));
}

void NBPProtocol::sendInstrumentation(const float delta[4], const float threshold[4],
                                      const int8_t verdict[4], int8_t overall,
                                      const bool cornerIsCamera[4],
                                      const uint16_t fillColors[4][3],
                                      const uint16_t deltaColors[4][3],
                                      bool farenheit) {
    clearChannels();
    Unit tempUnit = (farenheit) ? Unit::DegreesF : Unit::DegreesC;

    // Per-corner numeric verdict channels, enum-ordered FL/FR/RL/RR.
    static const ChannelType dCh[4] = { ChannelType::FLDelta, ChannelType::FRDelta,
                                        ChannelType::RLDelta, ChannelType::RRDelta };
    static const ChannelType tCh[4] = { ChannelType::FLThreshold, ChannelType::FRThreshold,
                                        ChannelType::RLThreshold, ChannelType::RRThreshold };
    static const ChannelType vCh[4] = { ChannelType::FLVerdict, ChannelType::FRVerdict,
                                        ChannelType::RLVerdict, ChannelType::RRVerdict };
    // Stable band suffixes for the color channels. Outer/Center/Inner in array order.
    static const char* fillNames[4][3] = {
        { "FL Fill O Color", "FL Fill C Color", "FL Fill I Color" },
        { "FR Fill O Color", "FR Fill C Color", "FR Fill I Color" },
        { "RL Fill O Color", "RL Fill C Color", "RL Fill I Color" },
        { "RR Fill O Color", "RR Fill C Color", "RR Fill I Color" },
    };
    static const char* deltaNames[4][3] = {
        { "FL Delta O Color", "FL Delta C Color", "FL Delta I Color" },
        { "FR Delta O Color", "FR Delta C Color", "FR Delta I Color" },
        { "RL Delta O Color", "RL Delta C Color", "RL Delta I Color" },
        { "RR Delta O Color", "RR Delta C Color", "RR Delta I Color" },
    };

    for (int c = 0; c < 4; c++) {
        if (!cornerIsCamera[c]) continue;        // single-sensor corner: no band data
        addChannel(dCh[c], tempUnit, delta[c]);
        addChannel(tCh[c], tempUnit, threshold[c]);
        addChannel(vCh[c], Unit::None, (float)verdict[c]);
        for (int i = 0; i < 3; i++) {
            addHexChannel(fillNames[c][i], fillColors[c][i]);
            addHexChannel(deltaNames[c][i], deltaColors[c][i]);
        }
    }

    addChannel(ChannelType::OverallVerdict, Unit::None, (float)overall);
    sendUpdateAll();
}

void NBPProtocol::sendBootMetadata(const BootMetadata& m) {
    sendMetadata("FIRMWARE_SHA", m.firmwareSha);
    sendMetadata("MODE", m.modeName);
    sendMetadata("TIRE_PROFILE", m.profileName);

    char buf[64];
    const char* u = (m.unit == 'C') ? "degC" : "degF";
    snprintf(buf, sizeof(buf), "%d/%d/%d %s", m.windowMin, m.windowIdeal, m.windowMax, u);
    sendMetadata("WINDOW", buf);
    snprintf(buf, sizeof(buf), "%d degF", m.offsetK);
    sendMetadata("OFFSET_K", buf);
    snprintf(buf, sizeof(buf), "%d s", m.tauSeconds);
    sendMetadata("TAU", buf);

    static const char* cornerTag[4] = { "CROP_FL", "CROP_FR", "CROP_RL", "CROP_RR" };
    for (int c = 0; c < 4; c++) {
        snprintf(buf, sizeof(buf), "L%u/R%u",
                 (unsigned)m.leftOffset[c], (unsigned)m.rightOffset[c]);
        sendMetadata(cornerTag[c], buf);
    }

    sendMetadata("AMBIENT_SOURCE", m.ambientSource);
}

void NBPProtocol::setTireTemps(float frontLeftTemp, float frontRightTemp, float rearLeftTemp, float rearRightTemp, bool farenheit) {
    
    clearChannels();
    Unit tempUnit = (farenheit)? Unit::DegreesF:Unit::DegreesC;
    addChannel(ChannelType::FrontLeftTire, tempUnit, frontLeftTemp);
    addChannel(ChannelType::FrontRightTire, tempUnit, frontRightTemp);
    addChannel(ChannelType::RearLeftTire, tempUnit, rearLeftTemp);
    addChannel(ChannelType::RearRightTire, tempUnit, rearRightTemp);
    sendUpdateAll();
}

// Clears all added data channels
void NBPProtocol::clearChannels() {
    data = "";
}

// Sends the packet header
void NBPProtocol::sendPacketHeader(const char* packetType) {
    unsigned long curTime = millis();
    float timestamp = curTime / 1000.0;
    serial.print("*NBP1,");
    serial.print(packetType);
    serial.print(",");
    serial.println(timestamp, 3);
}

// Sends the data
void NBPProtocol::sendData() {
    serial.println(data);
}

// Sends the packet footer
void NBPProtocol::sendPacketFooter() {
    serial.println("#");
}

// Converts ChannelType to corresponding string
const char* NBPProtocol::getChannelName(ChannelType channel) {
    switch (channel) {
        case ChannelType::Battery:       return "Battery";
        case ChannelType::BrakePedal:    return "Brake Pedal";
        case ChannelType::SteeringWheel: return "Steering Wheel";
        case ChannelType::Gear:          return "Gear";
        case ChannelType::FrontLeftTire: return "Front Left Tire";
        case ChannelType::FrontRightTire: return "Front Right Tire";
        case ChannelType::RearLeftTire: return "Rear Left Tire";
        case ChannelType::RearRightTire: return "Rear Right Tire";
        case ChannelType::FrontLeftTireO: return "Front Left Tire O";
        case ChannelType::FrontRightTireO: return "Front Right Tire O";
        case ChannelType::RearLeftTireO: return "Rear Left Tire O";
        case ChannelType::RearRightTireO: return "Rear Right Tire O";
        case ChannelType::FrontLeftTireC: return "Front Left Tire C";
        case ChannelType::FrontRightTireC: return "Front Right Tire C";
        case ChannelType::RearLeftTireC: return "Rear Left Tire C";
        case ChannelType::RearRightTireC: return "Rear Right Tire C";        
        case ChannelType::FrontLeftTireI: return "Front Left Tire I";
        case ChannelType::FrontRightTireI: return "Front Right Tire I";
        case ChannelType::RearLeftTireI: return "Rear Left Tire I";
        case ChannelType::RearRightTireI: return "Rear Right Tire I";
        case ChannelType::FrontLeftTireRaw:  return "Front Left Tire Raw";
        case ChannelType::FrontRightTireRaw: return "Front Right Tire Raw";
        case ChannelType::RearLeftTireRaw:   return "Rear Left Tire Raw";
        case ChannelType::RearRightTireRaw:  return "Rear Right Tire Raw";
        case ChannelType::FrontLeftTireRawO:  return "Front Left Tire Raw O";
        case ChannelType::FrontRightTireRawO: return "Front Right Tire Raw O";
        case ChannelType::RearLeftTireRawO:   return "Rear Left Tire Raw O";
        case ChannelType::RearRightTireRawO:  return "Rear Right Tire Raw O";
        case ChannelType::FrontLeftTireRawC:  return "Front Left Tire Raw C";
        case ChannelType::FrontRightTireRawC: return "Front Right Tire Raw C";
        case ChannelType::RearLeftTireRawC:   return "Rear Left Tire Raw C";
        case ChannelType::RearRightTireRawC:  return "Rear Right Tire Raw C";
        case ChannelType::FrontLeftTireRawI:  return "Front Left Tire Raw I";
        case ChannelType::FrontRightTireRawI: return "Front Right Tire Raw I";
        case ChannelType::RearLeftTireRawI:   return "Rear Left Tire Raw I";
        case ChannelType::RearRightTireRawI:  return "Rear Right Tire Raw I";
        case ChannelType::AccelX:        return "Accel X";
        case ChannelType::AccelY:        return "Accel Y";
        case ChannelType::AccelZ:        return "Accel Z";
        case ChannelType::GyroX:         return "Gyro X";
        case ChannelType::GyroY:         return "Gyro Y";
        case ChannelType::GyroZ:         return "Gyro Z";
        case ChannelType::LateralG:      return "Lateral G";
        case ChannelType::SumFLPeak:     return "Summary FL Peak";
        case ChannelType::SumFRPeak:     return "Summary FR Peak";
        case ChannelType::SumRLPeak:     return "Summary RL Peak";
        case ChannelType::SumRRPeak:     return "Summary RR Peak";
        case ChannelType::SumFLAvg:      return "Summary FL Avg";
        case ChannelType::SumFRAvg:      return "Summary FR Avg";
        case ChannelType::SumRLAvg:      return "Summary RL Avg";
        case ChannelType::SumRRAvg:      return "Summary RR Avg";
        case ChannelType::SumFLWindow:   return "Summary FL Window";
        case ChannelType::SumFRWindow:   return "Summary FR Window";
        case ChannelType::SumRLWindow:   return "Summary RL Window";
        case ChannelType::SumRRWindow:   return "Summary RR Window";
        case ChannelType::SumFLOver:     return "Summary FL Overheat";
        case ChannelType::SumFROver:     return "Summary FR Overheat";
        case ChannelType::SumRLOver:     return "Summary RL Overheat";
        case ChannelType::SumRROver:     return "Summary RR Overheat";
        case ChannelType::SumFrontRear:  return "Summary Front Rear";
        case ChannelType::SumLeftRight:  return "Summary Left Right";
        case ChannelType::SumWarmup:     return "Summary Warmup";
        case ChannelType::SumLength:     return "Summary Length";
        case ChannelType::FLDelta:       return "FL Delta";
        case ChannelType::FRDelta:       return "FR Delta";
        case ChannelType::RLDelta:       return "RL Delta";
        case ChannelType::RRDelta:       return "RR Delta";
        case ChannelType::FLThreshold:   return "FL Threshold";
        case ChannelType::FRThreshold:   return "FR Threshold";
        case ChannelType::RLThreshold:   return "RL Threshold";
        case ChannelType::RRThreshold:   return "RR Threshold";
        case ChannelType::FLVerdict:     return "FL Verdict";
        case ChannelType::FRVerdict:     return "FR Verdict";
        case ChannelType::RLVerdict:     return "RL Verdict";
        case ChannelType::RRVerdict:     return "RR Verdict";
        case ChannelType::OverallVerdict: return "Overall Verdict";
        default:                         return "";
    }
}

// Converts Unit to corresponding string
const char* NBPProtocol::getUnitName(Unit unit) {
    switch (unit) {
        case Unit::V:        return "V";
        case Unit::Percent:  return "%";
        case Unit::Degrees:  return "deg";
        case Unit::DegreesF:  return "degF";
        case Unit::DegreesC:  return "degC";
        case Unit::G:         return "G";
        case Unit::DegPerSec: return "deg/s";
        case Unit::None:     return "";
        default:             return "";
    }
}
