#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "portal_config.h"
#include "portal_common.h"
#include "dns_portal.h"
#include "http_portal.h"
#include "https_portal.h"
#include "usb_msc.h"

static void mountStorage()
{
    if (!LittleFS.begin(false))
    {
        Serial.println("[FS] mount failed, formatting...");
        LittleFS.format();
        if (!LittleFS.begin(true))
        {
            Serial.println("[FS] format failed; payload pages will be unavailable");
            return;
        }
    }

    Serial.printf("[FS] LittleFS ready: %u / %u bytes used\n",
                  LittleFS.usedBytes(), LittleFS.totalBytes());
}

static void startAccessPoint()
{
    WiFi.persistent(false);
    WiFi.mode(WIFI_OFF);
    delay(200);
    WiFi.mode(WIFI_AP);

    IPAddress apIp(PORTAL_AP_IP_OCTETS);
    IPAddress gateway(PORTAL_AP_GW_OCTETS);
    IPAddress subnet(PORTAL_AP_MASK_OCTETS);

    Serial.printf("[WiFi] softAPConfig: %s\n",
                  WiFi.softAPConfig(apIp, gateway, subnet) ? "OK" : "FAILED");
    Serial.printf("[WiFi] softAP: %s\n",
                  WiFi.softAP(PORTAL_AP_SSID, PORTAL_AP_PASSWORD,
                              PORTAL_AP_CHANNEL, false, PORTAL_AP_MAX_CLIENT)
                      ? "OK" : "FAILED");
    Serial.printf("[WiFi] AP %s @ %s\n", PORTAL_AP_SSID, WiFi.softAPIP().toString().c_str());
}

void setup()
{
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("=== PS4 + PS5 S3 Fusion Portal ===");
    Serial.printf("Flash: %u MB, free heap: %u\n",
                  ESP.getFlashChipSize() / 1024 / 1024, ESP.getFreeHeap());

    mountStorage();
    portalSeedManifestVersion();
    startAccessPoint();

    portalDns.begin(WiFi.softAPIP());
    Serial.println("[DNS] wildcard resolver started");

    portalHttpBegin();
    portalHttpsBegin();

#if PORTAL_USB_MSC && USB_MSC_EXFATHAX
    portalUsbBegin();
#endif

    Serial.println("READY");
    Serial.printf("PS5: connect to %s and open the User Guide\n", PORTAL_AP_SSID);
    Serial.printf("PS4: http://%s/ps4/\n", WiFi.softAPIP().toString().c_str());
}

void loop()
{
    portalDns.process();
    portalHttpLoop();
    delay(2);
}
