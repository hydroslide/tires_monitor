#include "NBPProtocol.h"

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
