#include "http_portal.h"
#include "portal_common.h"
#include "portal_config.h"
#include "dns_portal.h"
#include "usb_msc.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>

static WebServer webServer(PORTAL_HTTP_PORT);
static File uploadFile;
static String uploadName;
static bool uploadSucceeded = false;

static void setNoCache()
{
    webServer.sendHeader("Cache-Control", PORTAL_NO_CACHE);
    webServer.sendHeader("Pragma", "no-cache");
    webServer.sendHeader("Expires", "0");
}

static String safeFileName(const String &input)
{
    String name = input;
    int slash = name.lastIndexOf('/');
    if (slash >= 0)
        name = name.substring(slash + 1);
    slash = name.lastIndexOf('\\');
    if (slash >= 0)
        name = name.substring(slash + 1);

    String safe;
    for (size_t i = 0; i < name.length() && safe.length() < 64; i++)
    {
        char c = name.charAt(i);
        if (isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '_')
            safe += c;
        else
            safe += '_';
    }
    if (safe.length() == 0)
        safe = "payload.bin";
    return safe;
}

static void handleStatus()
{
    setNoCache();
    size_t used = LittleFS.usedBytes();
    size_t total = LittleFS.totalBytes();

    String json = "{";
    json += "\"device\":\"ESP32-S3 N16R8\",";
    json += "\"ap_ssid\":\"" + portalJsonEscape(PORTAL_AP_SSID) + "\",";
    json += "\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\",";
    json += "\"clients\":" + String(WiFi.softAPgetStationNum()) + ",";
    json += "\"dns_queries\":" + String(portalDns.queries()) + ",";
    json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"flash_size\":" + String(ESP.getFlashChipSize()) + ",";
    json += "\"fs_used\":" + String(used) + ",";
    json += "\"fs_total\":" + String(total) + ",";
    json += "\"usb_msc\":" + String(portalUsbEnabled() ? "true" : "false");
    json += "}";
    webServer.send(200, "application/json; charset=utf-8", json);
}

static void handleFiles()
{
    setNoCache();
    String json = "[";
    bool first = true;

    File dir = LittleFS.open(PORTAL_PAYLOAD_DIR, "r");
    if (dir && dir.isDirectory())
    {
        File file = dir.openNextFile();
        while (file)
        {
            if (!file.isDirectory())
            {
                String name = safeFileName(file.name());
                if (!first)
                    json += ",";
                first = false;
                json += "{\"name\":\"" + portalJsonEscape(name) +
                        "\",\"size\":" + String(file.size()) + "}";
            }
            file = dir.openNextFile();
        }
        dir.close();
    }

    json += "]";
    webServer.send(200, "application/json; charset=utf-8", json);
}

static void handleUploadData()
{
    HTTPUpload &upload = webServer.upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        uploadName = safeFileName(upload.filename);
        String path = String(PORTAL_PAYLOAD_DIR) + "/" + uploadName;
        uploadFile = LittleFS.open(path, "w");
        uploadSucceeded = uploadFile ? true : false;
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (uploadFile && upload.currentSize > 0)
            uploadSucceeded =
                (uploadFile.write(upload.buf, upload.currentSize) == upload.currentSize) &&
                uploadSucceeded;
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (uploadFile)
            uploadFile.close();
        if (uploadSucceeded)
            Serial.printf("[HTTP] uploaded %s\n", uploadName.c_str());
    }
}

static void handleUpload()
{
    setNoCache();
    if (!uploadSucceeded)
        webServer.send(500, "text/plain", "Upload failed");
    else
        webServer.send(200, "text/plain", "OK: " + uploadName);
}

static void handleDelete()
{
    setNoCache();
    String name = safeFileName(webServer.arg("name"));
    String path = String(PORTAL_PAYLOAD_DIR) + "/" + name;
    bool removed = LittleFS.exists(path) && LittleFS.remove(path);
    webServer.send(removed ? 200 : 404, "text/plain", removed ? "OK" : "Not found");
}

static void handleReboot()
{
    setNoCache();
    webServer.send(200, "text/plain", "Rebooting");
    delay(200);
    ESP.restart();
}

static void handleProbe()
{
    setNoCache();
    String path = webServer.uri();

    if (path == "/generate_204" || path == "/gen_204")
    {
        webServer.send(204);
        return;
    }
    if (path == "/hotspot-detect.html")
    {
        webServer.send(200, "text/html", "Success");
        return;
    }
    if (path == "/connecttest.txt")
    {
        webServer.send(200, "text/plain", "Microsoft Connect Test");
        return;
    }
    if (path == "/ncsi.txt")
    {
        webServer.send(200, "text/plain", "Microsoft NCSI");
        return;
    }
    webServer.send(200, "text/plain", "Success");
}

static void handleNotFound()
{
    String path = webServer.uri();

    // PS5's connectivity check must not fall through to a 404.
    if (path.startsWith("/networktest/"))
    {
        setNoCache();
        webServer.send(200, "text/plain", "OK");
        return;
    }

    if (portalIsProbePath(path))
    {
        handleProbe();
        return;
    }

    PortalResolvedFile file = portalResolveFile(path);
    if (!file.found)
    {
        setNoCache();
        if (path.startsWith("/document/") &&
            (path.indexOf("/ps5") >= 0 || path.indexOf("/ps4") >= 0))
        {
            webServer.sendHeader("Location", PORTAL_REDIRECT_URL, true);
            webServer.send(302, "text/plain", "");
            return;
        }
        webServer.send(404, "text/plain", "404 Not Found");
        return;
    }

    File handle = LittleFS.open(file.storagePath, "r");
    if (!handle)
    {
        setNoCache();
        webServer.send(404, "text/plain", "404 Not Found");
        return;
    }

    setNoCache();
    String mime = portalMimeType(file.logicalPath);
    if (portalIsManifest(mime))
    {
        String manifest = portalBuildManifest(handle);
        handle.close();
        webServer.send(200, "text/cache-manifest; charset=utf-8", manifest);
        return;
    }
    if (file.gzip)
        webServer.sendHeader("Content-Encoding", "gzip");
    webServer.streamFile(handle, mime);
    handle.close();
}

void portalHttpBegin()
{
    webServer.on("/api/status", HTTP_GET, handleStatus);
    webServer.on("/api/files", HTTP_GET, handleFiles);
    webServer.on("/upload", HTTP_POST, handleUpload);
    webServer.onFileUpload(handleUploadData);
    webServer.on("/delete", HTTP_POST, handleDelete);
    webServer.on("/reboot", HTTP_POST, handleReboot);
    webServer.onNotFound(handleNotFound);
    webServer.begin();
    Serial.printf("[HTTP] listening on %u\n", PORTAL_HTTP_PORT);
}

void portalHttpLoop()
{
    webServer.handleClient();
}
