#pragma once

#include <Arduino.h>
#include <FS.h>

struct PortalResolvedFile
{
    String logicalPath;   // Path used for MIME type and client-facing URL.
    String storagePath;   // Actual LittleFS path (may end in .gz).
    bool gzip = false;
    bool found = false;
};

String portalMimeType(const String &path);
bool portalIsManifest(const String &mime);
bool portalIsProbePath(const String &path);
String portalNormalizePath(const String &uri);
PortalResolvedFile portalResolveFile(const String &uri);
String portalJsonEscape(const String &value);
