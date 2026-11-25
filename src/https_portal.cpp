#include "https_portal.h"
#include "portal_common.h"
#include "portal_config.h"
#include "server_certs.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <esp_https_server.h>

static httpd_handle_t httpsServer = nullptr;

static void setNoCacheHeaders(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Cache-Control", PORTAL_NO_CACHE);
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
}

static esp_err_t handleFixed(httpd_req_t *req)
{
    setNoCacheHeaders(req);
    String uri = req->uri;

    if (uri == "/redirect.manifest")
    {
        // Clear an old PS4 User Guide AppCache if one exists.
        httpd_resp_set_status(req, "410 Gone");
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_send(req, "Gone", HTTPD_RESP_USE_STRLEN);
    }

    if (uri == "/generate_204" || uri == "/gen_204")
    {
        httpd_resp_set_status(req, "204 No Content");
        return httpd_resp_send(req, nullptr, 0);
    }
    if (uri == "/hotspot-detect.html")
    {
        httpd_resp_set_type(req, "text/html");
        return httpd_resp_send(req, "Success", HTTPD_RESP_USE_STRLEN);
    }
    if (uri == "/connecttest.txt")
    {
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_send(req, "Microsoft Connect Test", HTTPD_RESP_USE_STRLEN);
    }
    if (uri == "/ncsi.txt")
    {
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_send(req, "Microsoft NCSI", HTTPD_RESP_USE_STRLEN);
    }

    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "Success", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handleFile(httpd_req_t *req)
{
    String uri = req->uri;

    if (portalIsProbePath(uri) || uri == "/redirect.manifest")
        return handleFixed(req);

    PortalResolvedFile resolved = portalResolveFile(uri);
    if (!resolved.found)
    {
        setNoCacheHeaders(req);
        if (uri.startsWith("/document/") &&
            (uri.indexOf("/ps5") >= 0 || uri.indexOf("/ps4") >= 0))
        {
            httpd_resp_set_status(req, "302 Found");
            httpd_resp_set_hdr(req, "Location", PORTAL_REDIRECT_URL);
            httpd_resp_set_hdr(req, "Connection", "close");
            httpd_resp_send(req, nullptr, 0);
            return ESP_FAIL;
        }
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_send(req, "404 Not Found", HTTPD_RESP_USE_STRLEN);
    }

    File file = LittleFS.open(resolved.storagePath, "r");
    if (!file)
    {
        setNoCacheHeaders(req);
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_send(req, "404 Not Found", HTTPD_RESP_USE_STRLEN);
    }

    setNoCacheHeaders(req);
    String mime = portalMimeType(resolved.logicalPath);
    if (portalIsManifest(mime))
    {
        String manifest = portalBuildManifest(file);
        file.close();
        httpd_resp_set_type(req, "text/cache-manifest; charset=utf-8");
        return httpd_resp_send(req, manifest.c_str(), manifest.length());
    }
    if (resolved.gzip)
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_type(req, mime.c_str());

    char buffer[1024];
    while (file.available())
    {
        size_t len = file.readBytes(buffer, sizeof(buffer));
        if (httpd_resp_send_chunk(req, buffer, len) != ESP_OK)
        {
            file.close();
            return ESP_FAIL;
        }
    }
    file.close();
    return httpd_resp_send_chunk(req, nullptr, 0);
}

// PS5 checks for internet access via POST/GET /networktest/* before loading
// the user guide. Upstream answers "OK" instead of letting it hit a 404.
static esp_err_t handleNetworkTest(httpd_req_t *req)
{
    char buffer[256];
    int remaining = req->content_len;
    while (remaining > 0)
    {
        int len = httpd_req_recv(req, buffer, min(remaining, (int)sizeof(buffer)));
        if (len <= 0)
        {
            if (len == HTTPD_SOCK_ERR_TIMEOUT)
                continue;
            return ESP_FAIL;
        }
        remaining -= len;
    }

    setNoCacheHeaders(req);
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
}

bool portalHttpsBegin()
{
    httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
    config.transport_mode = HTTPD_SSL_TRANSPORT_SECURE;
    config.port_secure = PORTAL_HTTPS_PORT;
    config.cacert_pem = server_crt;
    config.cacert_len = server_crt_len;
    config.prvtkey_pem = server_key;
    config.prvtkey_len = server_key_len;
    config.httpd.uri_match_fn = httpd_uri_match_wildcard;
    config.httpd.max_uri_handlers = 14;

    esp_err_t err = httpd_ssl_start(&httpsServer, &config);
    if (err != ESP_OK)
    {
        Serial.printf("[HTTPS] start failed: %s\n", esp_err_to_name(err));
        return false;
    }

    httpd_uri_t fixedUri = {
        .uri = "/redirect.manifest", .method = HTTP_GET,
        .handler = handleFixed, .user_ctx = nullptr
    };
    httpd_uri_t generateUri = {
        .uri = "/generate_204", .method = HTTP_GET,
        .handler = handleFixed, .user_ctx = nullptr
    };
    httpd_uri_t genUri = {
        .uri = "/gen_204", .method = HTTP_GET,
        .handler = handleFixed, .user_ctx = nullptr
    };
    httpd_uri_t hotspotUri = {
        .uri = "/hotspot-detect.html", .method = HTTP_GET,
        .handler = handleFixed, .user_ctx = nullptr
    };
    httpd_uri_t connectUri = {
        .uri = "/connecttest.txt", .method = HTTP_GET,
        .handler = handleFixed, .user_ctx = nullptr
    };
    httpd_uri_t ncsiUri = {
        .uri = "/ncsi.txt", .method = HTTP_GET,
        .handler = handleFixed, .user_ctx = nullptr
    };
    httpd_uri_t redirectUri = {
        .uri = "/redirect", .method = HTTP_GET,
        .handler = handleFixed, .user_ctx = nullptr
    };
    httpd_uri_t netstartUri = {
        .uri = "/netstart/icst", .method = HTTP_GET,
        .handler = handleFixed, .user_ctx = nullptr
    };
    httpd_uri_t netTestPostUri = {
        .uri = "/networktest/*", .method = HTTP_POST,
        .handler = handleNetworkTest, .user_ctx = nullptr
    };
    httpd_uri_t netTestGetUri = {
        .uri = "/networktest/*", .method = HTTP_GET,
        .handler = handleNetworkTest, .user_ctx = nullptr
    };
    httpd_uri_t fileUri = {
        .uri = "/*", .method = HTTP_GET,
        .handler = handleFile, .user_ctx = nullptr
    };

    const httpd_uri_t *handlers[] = {
        &fixedUri, &generateUri, &genUri, &hotspotUri,
        &connectUri, &ncsiUri, &redirectUri, &netstartUri,
        &netTestPostUri, &netTestGetUri, &fileUri
    };

    for (size_t i = 0; i < sizeof(handlers) / sizeof(handlers[0]); i++)
    {
        err = httpd_register_uri_handler(httpsServer, handlers[i]);
        if (err != ESP_OK)
        {
            Serial.printf("[HTTPS] route registration failed: %s\n", esp_err_to_name(err));
            httpd_ssl_stop(httpsServer);
            httpsServer = nullptr;
            return false;
        }
    }

    Serial.printf("[HTTPS] listening on %u\n", PORTAL_HTTPS_PORT);
    return true;
}
