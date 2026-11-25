#include "portal_common.h"
#include <LittleFS.h>

String portalMimeType(const String &path)
{
    String lower = path;
    lower.toLowerCase();

    if (lower.endsWith(".html") || lower.endsWith(".htm"))
        return "text/html; charset=utf-8";
    if (lower.endsWith(".css"))
        return "text/css; charset=utf-8";
    if (lower.endsWith(".js") || lower.endsWith(".mjs"))
        return "application/javascript; charset=utf-8";
    if (lower.endsWith(".json"))
        return "application/json; charset=utf-8";
    if (lower.endsWith(".svg"))
        return "image/svg+xml";
    if (lower.endsWith(".png"))
        return "image/png";
    if (lower.endsWith(".jpg") || lower.endsWith(".jpeg"))
        return "image/jpeg";
    if (lower.endsWith(".gif"))
        return "image/gif";
    if (lower.endsWith(".ico"))
        return "image/x-icon";
    if (lower.endsWith(".woff"))
        return "font/woff";
    if (lower.endsWith(".woff2"))
        return "font/woff2";
    if (lower.endsWith(".appcache") || lower.endsWith(".manifest") || lower.endsWith(".cache"))
        return "text/cache-manifest; charset=utf-8";
    if (lower.endsWith(".txt") || lower.endsWith(".lua"))
        return "text/plain; charset=utf-8";

    return "application/octet-stream";
}

bool portalIsManifest(const String &mime)
{
    return mime.startsWith("text/cache-manifest");
}

bool portalIsProbePath(const String &path)
{
    return path == "/generate_204" ||
           path == "/gen_204" ||
           path == "/hotspot-detect.html" ||
           path == "/connecttest.txt" ||
           path == "/ncsi.txt" ||
           path == "/redirect" ||
           path == "/netstart/icst";
}

String portalNormalizePath(const String &uri)
{
    String path = uri;
    int query = path.indexOf('?');
    if (query >= 0)
        path = path.substring(0, query);
    if (path.length() == 0)
        path = "/";

    bool trailingSlash = path.endsWith("/");

    // Rebuild the path segment by segment, dropping "." / ".." components.
    String clean = "/";
    int start = 1;
    while (start <= path.length())
    {
        int end = path.indexOf('/', start);
        if (end < 0)
            end = path.length();
        String part = path.substring(start, end);
        if (part.length() > 0 && part != ".")
        {
            if (part == "..")
            {
                int slash = clean.lastIndexOf('/', clean.length() - 2);
                clean = slash < 0 ? "/" : clean.substring(0, slash + 1);
            }
            else
            {
                if (!clean.endsWith("/"))
                    clean += "/";
                clean += part;
            }
        }
        start = end + 1;
    }

    if (trailingSlash && !clean.endsWith("/"))
        clean += "/";
    return clean;
}

PortalResolvedFile portalResolveFile(const String &uri)
{
    PortalResolvedFile result;
    result.logicalPath = portalNormalizePath(uri);
    String path = result.logicalPath;

    if (path.endsWith("/"))
    {
        path += "index.html";
        result.logicalPath = path;
    }

    // If the request points at a directory without a trailing slash, append
    // index.html as well.
    File probe = LittleFS.open(path, "r");
    if (probe && probe.isDirectory())
    {
        probe.close();
        if (!path.endsWith("/"))
            path += "/";
        path += "index.html";
        result.logicalPath = path;
    }
    else if (probe)
    {
        probe.close();
    }

    String mime = portalMimeType(path);
    if (!portalIsManifest(mime))
    {
        String gzPath = path + ".gz";
        File gz = LittleFS.open(gzPath, "r");
        if (gz && !gz.isDirectory())
        {
            gz.close();
            result.storagePath = gzPath;
            result.gzip = true;
            result.found = true;
            return result;
        }
        if (gz)
            gz.close();
    }

    File plain = LittleFS.open(path, "r");
    if (plain && !plain.isDirectory())
    {
        plain.close();
        result.storagePath = path;
        result.found = true;
        return result;
    }
    if (plain)
        plain.close();

    result.storagePath = path;
    return result;
}

String portalJsonEscape(const String &value)
{
    String out;
    out.reserve(value.length() + 8);
    for (size_t i = 0; i < value.length(); i++)
    {
        char c = value.charAt(i);
        switch (c)
        {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<uint8_t>(c) < 0x20)
                out += "?";
            else
                out += c;
        }
    }
    return out;
}
