#ifndef BLE_SERIAL_H
#define BLE_SERIAL_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>

class BLESerial : public Stream {
public:
    BLESerial();
    bool begin(const char* deviceName);  // Initialize BLE with device name
    void println(const String& message);
    void print(const String& message);

    // Stream overrides
    virtual int available() override { return 0; }
    virtual int read() override { return -1; }
    virtual int peek() override { return -1; }
    virtual void flush() override {}
    virtual size_t write(uint8_t c) override;
    using Print::write;  // Pull in write(str) and write(buf, size) from Print

    void loop();  // Handle BLE events (optional)

private:
    BLEServer* pServer;           // BLE Server
    BLECharacteristic* pTxChar;   // Transmit characteristic
    bool connected;               // Connection status
    static void onConnect(BLEServer* pServer);
    static void onDisconnect(BLEServer* pServer);
};

#endif  // BLE_SERIAL_H
