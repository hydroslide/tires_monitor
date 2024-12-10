#include "BLEServerCallbacksHandler.h"
#include <BLEDevice.h>

BLEServerCallbacksHandler::BLEServerCallbacksHandler(bool* connectedFlag)
    : connectedFlag(connectedFlag) {}

void BLEServerCallbacksHandler::onConnect(BLEServer* pServer) {
    *connectedFlag = true;
    Serial.println("BLE Client connected!");
}

void BLEServerCallbacksHandler::onDisconnect(BLEServer* pServer) {
    *connectedFlag = false;
    Serial.println("BLE Client disconnected!");
    BLEDevice::startAdvertising();
}
