#pragma once

#include <Arduino.h>
#include <WiFiUdp.h>

class PortalDnsServer
{
public:
    void begin(IPAddress answer);
    void process();
    uint32_t queries() const { return _queries; }

private:
    WiFiUDP _udp;
    IPAddress _answer;
    uint32_t _queries = 0;
};

extern PortalDnsServer portalDns;
