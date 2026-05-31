#pragma once
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "types.h"

class BambuMqttClient {
public:
    explicit BambuMqttClient(PrinterState* state);

    void begin();
    void loop();
    bool publish(const char* payload);
    bool connected() const;

private:
    void connect();
    void onMessage(char* topic, byte* payload, unsigned int len);

    PrinterState*    _state;
    WiFiClientSecure _tls;
    PubSubClient     _mqtt;
    uint32_t         _lastReconnectMs = 0;

    char _topicSub[72];   // device/{serial}/report
    char _topicPub[72];   // device/{serial}/request
    char _clientId[40];
};
