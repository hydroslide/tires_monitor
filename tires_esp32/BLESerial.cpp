#include "BLESerial.h"

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_TX "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLESerial::BLESerial()
    : pServer(nullptr), pTxChar(nullptr), connected(false) {}

bool BLESerial::begin(const char* deviceName) {
    // Initialize BLE
    BLEDevice::init(deviceName);
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new BLEServerCallbacks());  // Explicit callback assignment

    // Set callback functions manually
    pServer->setCallbacks(new BLEServerCallbacks());
    pServer->setCallbacks(new class : public BLEServerCallbacks {
        void onConnect(BLEServer* pServer) override { BLESerial::onConnect(pServer); }
        void onDisconnect(BLEServer* pServer) override { BLESerial::onDisconnect(pServer); }
    });

    // Create BLE Service
    BLEService* pService = pServer->createService(SERVICE_UUID);

    // Create Transmit Characteristic
    pTxChar = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pTxChar->addDescriptor(new BLE2902());

    // Start the Service
    pService->start();

    // Start Advertising
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();

    Serial.println("BLE Serial initialized and advertising!");
    return true;
}

void BLESerial::println(const String& message) {
    print(message + "\n");
}

void BLESerial::print(const String& message) {
    if (connected) {
        pTxChar->setValue(message.c_str());
        pTxChar->notify();
    }
}

size_t BLESerial::write(uint8_t c) {
    String s((char)c);
    print(s);
    return 1;
}

void BLESerial::loop() {
    // Handle BLE events if needed
}

void BLESerial::onConnect(BLEServer* pServer) {
    Serial.println("BLE Client connected!");
}

void BLESerial::onDisconnect(BLEServer* pServer) {
    Serial.println("BLE Client disconnected!");
    BLEDevice::startAdver
