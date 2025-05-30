// OBDReader.cpp
/*
#include "OBDReader.h"

OBDReader::OBDReader()
    : client(nullptr), writeChar(nullptr), readChar(nullptr), lastLowFreqRead(0), useFahrenheit(false) {}

void OBDReader::begin(const String& deviceName, DataCallback highFreqCb, DataCallback lowFreqCb) {
    targetDeviceName = deviceName;
    highFreqCallback = highFreqCb;
    lowFreqCallback = lowFreqCb;

    BLEDevice::init("");
    client = BLEDevice::createClient();
    connectToServer();
}

void OBDReader::setFahrenheit(bool enabled) {
    useFahrenheit = enabled;
}

bool OBDReader::connectToServer() {
    BLEScan* scan = BLEDevice::getScan();
    scan->setActiveScan(true);
    BLEScanResults results = scan->start(5);

    for (int i = 0; i < results.getCount(); ++i) {
        BLEAdvertisedDevice device = results.getDevice(i);
        if (device.getName() == targetDeviceName) {
            if (client->connect(&device)) {
                Serial.println("Connected to ELM327 BLE device");

                BLEUUID serviceUUID("000018f0-0000-1000-8000-00805f9b34fb");
                BLERemoteService* remoteService = client->getService(serviceUUID);
                if (!remoteService) {
                    Serial.println("Service not found");
                    return false;
                }

                writeChar = remoteService->getCharacteristic("00002af1-0000-1000-8000-00805f9b34fb");
                readChar = remoteService->getCharacteristic("00002af0-0000-1000-8000-00805f9b34fb");

                if (!writeChar || !writeChar->canWrite()) {
                    Serial.println("Write characteristic not found or not writable");
                    return false;
                }

                if (readChar && readChar->canNotify()) {
                    readChar->registerForNotify([this](BLERemoteCharacteristic* pChar, uint8_t* data, size_t length, bool isNotify) {
                        String response = "";
                        for (size_t i = 0; i < length; ++i) response += (char)data[i];
                        Serial.print("Received: ");
                        Serial.println(response);

                        if (response.startsWith("41")) {
                            String pid = response.substring(2, 4);
                            float value = parsePIDResponse(response);

                            if (std::find(highFreqPIDs.begin(), highFreqPIDs.end(), pid) != highFreqPIDs.end()) {
                                highFreqCallback(pid, value);
                            } else {
                                lowFreqCallback(pid, value);
                            }
                        }
                    });
                }

                Serial.println("BLE characteristics configured");
                return true;
            }
        }
    }
    Serial.println("Device not found");
    return false;
}

void OBDReader::loop() {
    if (!client || !client->isConnected()) return;

    readHighFrequencyPIDs();

    if (millis() - lastLowFreqRead > 1000) {
        readLowFrequencyPIDs();
        lastLowFreqRead = millis();
    }
}

void OBDReader::readHighFrequencyPIDs() {
    for (const auto& pid : highFreqPIDs) {
        String command = "01" + pid + "\r";
        writeChar->writeValue(command.c_str(), command.length());
        delay(10);
    }
}

void OBDReader::readLowFrequencyPIDs() {
    for (const auto& pid : lowFreqPIDs) {
        String command = "01" + pid + "\r";
        writeChar->writeValue(command.c_str(), command.length());
        delay(10);
    }
}

float OBDReader::parsePIDResponse(const String& response) {
    if (response.length() < 6) return -1;

    String pid = response.substring(2, 4);
    int A = strtol(response.substring(4, 6).c_str(), nullptr, 16);
    int B = response.length() >= 8 ? strtol(response.substring(6, 8).c_str(), nullptr, 16) : 0;
    int C = response.length() >= 10 ? strtol(response.substring(8, 10).c_str(), nullptr, 16) : 0;
    int D = response.length() >= 12 ? strtol(response.substring(10, 12).c_str(), nullptr, 16) : 0;

    if (pid == "0C") return ((A * 256) + B) / 4.0;             // RPM
    if (pid == "0D") return A;                                  // Speed (km/h)
    if (pid == "11") return (A * 100.0) / 255.0;                 // Throttle Position
    if (pid == "10") return ((A * 256) + B) / 100.0;            // MAF (g/s)

    float tempC = -1;
    if (pid == "05") tempC = A - 40;                            // Coolant Temp
    if (pid == "0F") tempC = A - 40;                            // Intake Temp
    if (pid == "46") tempC = A;                                // Ambient Temp
    if (tempC != -1) return useFahrenheit ? (tempC * 9.0 / 5.0 + 32.0) : tempC;

    if (pid == "2F") return (A * 100.0) / 255.0;                 // Fuel Level (%)
    if (pid == "1C") return A;                                  // OBD standard
    if (pid == "03") return A;                                  // Fuel system status
    if (pid == "04") return (A * 100.0) / 255.0;                 // Engine load
    if (pid == "0E") return A;                                  // Timing advance
    if (pid == "1F") return A;                                  // Run time since engine start (sec)
    if (pid == "31") return ((A * 256) + B);                    // Distance since codes cleared (km)

    return -1;  // Unknown or unparsed
}
*/