#include "WifiSerial.h"
#include "NBPProtocol.h"

// WifiSerial instance
WifiSerial wifiSerial;
NBPProtocol nbp(wifiSerial);

void setup() {
    Serial.begin(115200);

    if (!wifiSerial.begin("ESP32_WiFi_NBP", "12345678")) {
        Serial.println("Failed to start WiFi!");
        while (1);
    }
    Serial.println("WiFi initialized!");

    // Send metadata about the device
    nbp.sendMetadata("NAME", "ESP32 WiFi Device");
    nbp.sendMetadata("VERSION", "1.1.2");
}

void loop() {
    // Clear the previous channel data
    nbp.clearChannels();

    // Add data channels with enum-based names and units
    nbp.addChannel(ChannelType::Battery, Unit::V, 13.56);
    nbp.addChannel(ChannelType::BrakePedal, Unit::Percent, 100.0);
    nbp.addChannel(ChannelType::SteeringWheel, Unit::Degrees, -35.31);
    nbp.addChannel(ChannelType::Gear, Unit::None, 3);

    // Send the data as an UPDATEALL packet over WiFi
    nbp.sendUpdateAll();

    // Call the loop function for WifiSerial
    wifiSerial.loop();

    delay(500);
}
