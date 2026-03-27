#include "NBPProtocol.h"
#include "Wheels.h"

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
    if (fl.values[1] == 0)
        addChannel(ChannelType::FrontLeftTire, tempUnit, fl.values[0]);
    else{
        addChannel(ChannelType::FrontLeftTireO, tempUnit, fl.values[0]);
        addChannel(ChannelType::FrontLeftTireC, tempUnit, fl.values[1]);
        addChannel(ChannelType::FrontLeftTireI, tempUnit, fl.values[2]);
    }
    if (fr.values[1] == 0)
        addChannel(ChannelType::FrontRightTire, tempUnit, fr.values[0]);
    else{
        addChannel(ChannelType::FrontRightTireI, tempUnit, fr.values[0]);
        addChannel(ChannelType::FrontRightTireC, tempUnit, fr.values[1]);
        addChannel(ChannelType::FrontRightTireO, tempUnit, fr.values[2]);
    }
    if (rl.values[1] == 0)
        addChannel(ChannelType::RearLeftTire, tempUnit, rl.values[0]);
    else{
        addChannel(ChannelType::RearLeftTireO, tempUnit, rl.values[0]);
        addChannel(ChannelType::RearLeftTireC, tempUnit, rl.values[1]);
        addChannel(ChannelType::RearLeftTireI, tempUnit, rl.values[2]);
    }
    if (rr.values[1] == 0)
        addChannel(ChannelType::RearRightTire, tempUnit, rr.values[0]);
    else{
        addChannel(ChannelType::RearRightTireI, tempUnit, rr.values[0]);
        addChannel(ChannelType::RearRightTireC, tempUnit, rr.values[1]);
        addChannel(ChannelType::RearRightTireO, tempUnit, rr.values[2]);
    }
    
    sendUpdateAll();
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
        case Unit::None:     return "";
        default:             return "";
    }
}
