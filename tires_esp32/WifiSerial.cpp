#include "WifiSerial.h"

WifiSerial::WifiSerial()
    : server(80), port(80) {}

bool WifiSerial::begin(const char* ssid, const char* password, uint16_t port) {
    this->port = port;
    server = WiFiServer(port);

    // Start WiFi as an Access Point
    WiFi.softAP(ssid, password);
    Serial.println("WiFi AP started");

    // Start the server
    server.begin();
    Serial.println("WiFi server started");
    return true;
}

void WifiSerial::print(const String& message) {
    buffer += message;
}

void WifiSerial::println(const String& message) {
    buffer += message + "\n";
}

size_t WifiSerial::write(uint8_t c) {
    buffer += static_cast<char>(c);
    return 1;
}

void WifiSerial::loop() {
    // Check for new client connections
    if (!client || !client.connected()) {
        client = server.available();
        if (client) {
            Serial.println("New client connected");
        }
    }

    // If a client is connected, send buffered data
    if (client && client.connected()) {
        if (!buffer.isEmpty()) {
            client.print(buffer);
            buffer = "";  // Clear the buffer after sending
        }
    }
}
