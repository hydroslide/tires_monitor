#ifndef NBP_PROTOCOL_H
#define NBP_PROTOCOL_H

#include <Arduino.h>

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
    // Add additional types as needed
};

enum class Unit {
    V,         // Volts
    Percent,   // Percentage
    Degrees,   // Degrees
    DegreesF,
    DegreesC,
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
