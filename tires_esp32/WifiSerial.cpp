#include "WifiSerial.h"

extern HWCDC USBSerial;

#define MAX_BUFFER_LENGTH 1000

WifiSerial::WifiSerial()
    : server(80), port(80) {}

bool WifiSerial::begin(const char* ssid, const char* password, uint16_t port) {
    this->port = port;
    server = WiFiServer(port);

    // Start WiFi as an Access Point
    WiFi.softAP(ssid, password);
    USBSerial.println("WiFi AP started");

    // Start the server
    server.begin();
    USBSerial.println("WiFi server started");
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
            USBSerial.println("New client connected");
        }
    }

    // If a client is connected, send buffered data
    if (client && client.connected()) {
        if (!buffer.isEmpty()) {
            client.print(buffer);
            buffer = "";  // Clear the buffer after sending
        }
    }
    else if (buffer.length() > MAX_BUFFER_LENGTH)
        buffer= ""; // Clear the buffer to prevent out of memory crash
}
