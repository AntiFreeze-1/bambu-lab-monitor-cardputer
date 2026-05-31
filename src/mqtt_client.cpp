#include "mqtt_client.h"
#include "mqtt_parser.h"
#include "config.h"
#include <WiFi.h>

BambuMqttClient::BambuMqttClient(PrinterState* state)
    : _state(state), _mqtt(_tls) {}

void BambuMqttClient::begin() {
    _tls.setInsecure(); // Bambu uses self-signed cert

    _mqtt.setServer(_state->ip, MQTT_PORT);
    _mqtt.setBufferSize(MQTT_BUFFER_SIZE); // MUST be set before connect()
    _mqtt.setKeepAlive(60);
    _mqtt.setSocketTimeout(10);

    _mqtt.setCallback([this](char* t, byte* p, unsigned int l) {
        this->onMessage(t, p, l);
    });

    snprintf(_topicSub, sizeof(_topicSub), "device/%s/report", _state->serial);
    snprintf(_topicPub, sizeof(_topicPub), "device/%s/request", _state->serial);

    size_t slen = strlen(_state->serial);
    const char* tail = slen >= 8 ? _state->serial + slen - 8 : _state->serial;
    snprintf(_clientId, sizeof(_clientId), "cardputer_%s", tail);
}

void BambuMqttClient::loop() {
    if (WiFi.status() != WL_CONNECTED) {
        _state->mqttConnected = false;
        return;
    }

    if (!_mqtt.connected()) {
        _state->mqttConnected = false;
        uint32_t now = millis();
        if (now - _lastReconnectMs >= MQTT_RECONNECT_MS) {
            _lastReconnectMs = now;
            connect();
        }
    } else {
        _mqtt.loop();
    }
}

bool BambuMqttClient::publish(const char* payload) {
    if (!_mqtt.connected()) return false;
    return _mqtt.publish(_topicPub, payload);
}

bool BambuMqttClient::connected() {
    return _mqtt.connected();
}

void BambuMqttClient::connect() {
    if (_state->ip[0] == '\0' || _state->serial[0] == '\0') return;

    if (_mqtt.connect(_clientId, MQTT_USER, _state->accessCode)) {
        _mqtt.subscribe(_topicSub);
        _state->mqttConnected = true;

        // Request version info so mqtt_parser can auto-detect device model
        const char* verReq =
            "{\"info\":{\"sequence_id\":\"0\",\"command\":\"get_version\"}}";
        _mqtt.publish(_topicPub, verReq);
    }
}

void BambuMqttClient::onMessage(char* /*topic*/, byte* payload, unsigned int len) {
    _state->lastMessageMs = millis();
    MqttParser::parse(payload, len, *_state);
}
