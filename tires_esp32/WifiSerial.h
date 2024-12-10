#ifndef WIFI_SERIAL_H
#define WIFI_SERIAL_H

#include <Arduino.h>
#include <WiFi.h>

// DEFAULT HOST IP ADDESS IN SoftAP Mode: 192.168.4.1

class WifiSerial : public Stream {
public:
    WifiSerial();
    bool begin(const char* ssid, const char* password, uint16_t port = 80);
    void print(const String& message);
    void println(const String& message);
    void loop();  // Handle client connections and send buffered data

    // Stream overrides
    virtual int available() override { return client ? client.available() : 0; }
    virtual int read() override { return client ? client.read() : -1; }
    virtual int peek() override { return client ? client.peek() : -1; }
    virtual void flush() override { if (client) client.flush(); }
    virtual size_t write(uint8_t c) override;

    using Print::write;  // Pull in write(str) and write(buf, size) from Print

private:
    WiFiServer server;     // WiFiServer instance
    WiFiClient client;     // Current client connection
    String buffer;         // Buffer for outgoing messages
    uint16_t port;         // Server port
};

#endif  // WIFI_SERIAL_H
