#ifndef BLE_SERVER_CALLBACKS_HANDLER_H
#define BLE_SERVER_CALLBACKS_HANDLER_H

#include <BLEServer.h>

class BLEServerCallbacksHandler : public BLEServerCallbacks {
public:
    BLEServerCallbacksHandler(bool* connectedFlag);
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;

private:
    bool* connectedFlag;  // Pointer to update the connected status
};

#endif  // BLE_SERVER_CALLBACKS_HANDLER_H
