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

// Only these extensions are real payloads that the bundled exploit pages load.
// Replacing them changes what gets injected after the exploit succeeds.
static bool isPayloadFile(const String &path)
{
    String lower = path;
    lower.toLowerCase();
    return lower.endsWith(".bin") || lower.endsWith(".elf") ||
           lower.endsWith(".bin.gz") || lower.endsWith(".elf.gz") ||
           lower.endsWith("/payloads/autoloader.js") ||
           lower.endsWith("/payloads/autoloader.js.gz") ||
           lower.endsWith("/payloads/autoload.js") ||
           lower.endsWith("/payloads/autoload.js.gz") ||
           lower.endsWith("/payloads/poops.js") ||
           lower.endsWith("/payloads/poops.js.gz") ||
           lower.endsWith("/payloads/relapse.js") ||
           lower.endsWith("/payloads/relapse.js.gz");
}

static String payloadLogicalPath(const String &storagePath)
{
    if (storagePath.endsWith(".gz"))
        return storagePath.substring(0, storagePath.length() - 3);
    return storagePath;
}

static void scanPayloads(File dir, const String &prefix, String &json, bool &first)
{
    File child = dir.openNextFile();
    while (child)
    {
        // LittleFS returns names with a leading slash on some cores.
        String name = child.name();
        while (name.startsWith("/"))
            name = name.substring(1);
        String full = prefix + "/" + name;

        if (child.isDirectory())
        {
            scanPayloads(child, full, json, first);
        }
        else if (isPayloadFile(full))
        {
            String logical = payloadLogicalPath(full);
            if (!first)
                json += ",";
            first = false;
            json += "{\"path\":\"" + portalJsonEscape(logical) +
                    "\",\"size\":" + String(child.size()) +
                    ",\"gzip\":" + String(full.endsWith(".gz") ? "true" : "false") + "}";
        }
        child = dir.openNextFile();
    }
}

static void handleFiles()
{
    setNoCache();
    String json = "[";
    bool first = true;

    File root = LittleFS.open("/", "r");
    if (root && root.isDirectory())
        scanPayloads(root, "", json, first);

    json += "]";
    webServer.send(200, "application/json; charset=utf-8", json);
}

// Upload target comes from the query string so it is parsed before the
// multipart body: POST /upload?path=/ps4/pspulse/900/goldhen.bin
static String uploadPath;

static void handleUploadData()
{
    HTTPUpload &upload = webServer.upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        uploadPath = portalNormalizePath(webServer.arg("path"));
        uploadSucceeded = false;

        // The target must be an existing payload the bundled pages actually
        // load. We never create arbitrary paths or touch exploit JS/HTML.
        if (!isPayloadFile(uploadPath))
        {
            Serial.printf("[HTTP] rejected payload target: %s\n", uploadPath.c_str());
            return;
        }

        // If the original was gzip'd, remove the cached .gz copy so the new
        // uncompressed payload is served immediately.
        String gz = uploadPath + ".gz";
        if (LittleFS.exists(gz))
            LittleFS.remove(gz);

        uploadFile = LittleFS.open(uploadPath, "w");
        uploadSucceeded = uploadFile ? true : false;
        uploadName = uploadPath;
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
            Serial.printf("[HTTP] replaced payload %s\n", uploadPath.c_str());
    }
}

static void handleUpload()
{
    setNoCache();
    if (!uploadSucceeded)
        webServer.send(500, "text/plain", "Upload failed or invalid target: " + uploadPath);
    else
        webServer.send(200, "text/plain", "OK: replaced " + uploadPath);
}

static void handleDelete()
{
    setNoCache();
    String path = portalNormalizePath(webServer.arg("path"));
    bool ok = false;
    if (isPayloadFile(path))
    {
        String gz = path + ".gz";
        bool removed = false;
        if (LittleFS.exists(gz))
            removed = LittleFS.remove(gz) || removed;
        if (LittleFS.exists(path))
            removed = LittleFS.remove(path) || removed;
        ok = removed;
    }
    webServer.send(ok ? 200 : 404, "text/plain", ok ? "OK" : "Not found or invalid target");
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
