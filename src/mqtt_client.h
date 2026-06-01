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
    bool connected();   // non-const: PubSubClient::connected() is not const

private:
    void connect();
    void onMessage(char* topic, byte* payload, unsigned int len);

    void sendPushall();

    PrinterState*    _state;
    WiFiClientSecure _tls;
    PubSubClient     _mqtt;
    uint32_t         _lastReconnectMs = 0;
    uint32_t         _lastPushallMs   = 0;

    char _topicSub[72];
    char _topicPub[72];
    char _clientId[40];
};
