#pragma once

// ---------------------------------------------------------------------------
// Local-only portal settings. Change the SSID/password before deploying.
// ---------------------------------------------------------------------------
#define PORTAL_AP_SSID       "PS4_PS5_PORTAL"
#define PORTAL_AP_PASSWORD   "12345678"
#define PORTAL_AP_CHANNEL     6
#define PORTAL_AP_MAX_CLIENT  4

#define PORTAL_AP_IP_OCTETS   192, 168, 4, 1
#define PORTAL_AP_GW_OCTETS   192, 168, 4, 1
#define PORTAL_AP_MASK_OCTETS 255, 255, 255, 0

#define PORTAL_DNS_TTL        30
#define PORTAL_HTTP_PORT      80
#define PORTAL_HTTPS_PORT     443

#define PORTAL_NO_CACHE       "no-store, no-cache, must-revalidate, max-age=0"

// PS5 user-guide requests arrive over HTTPS with a self-signed certificate.
// Redirect them to plain HTTP so AppCache / WebKit does not stay on TLS.
#define PORTAL_REDIRECT_URL   "http://192.168.4.1/"

// Routes. PS5 autoloader is served at "/", PS4 hosts live under "/ps4/".
#define PORTAL_PS5_ROOT       "/"
#define PORTAL_PS4_ROOT       "/ps4/"
#define PORTAL_ADMIN_PATH     "/admin.html"
#define PORTAL_PAYLOAD_DIR    "/payloads"
#define PORTAL_BUILD_INFO     "/build_info.json"
