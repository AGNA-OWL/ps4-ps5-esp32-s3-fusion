#include "usb_msc.h"
#include "portal_config.h"

#include <Arduino.h>

#if PORTAL_USB_MSC && USB_MSC_EXFATHAX && \
    (defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2))

#include "USB.h"
#include "USBMSC.h"
#include "exfathax.h"

static USBMSC mscDevice;
static bool usbEnabled = false;

static int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
    uint8_t *out = static_cast<uint8_t *>(buffer);
    uint32_t done = 0;

    while (done < bufsize)
    {
        uint32_t absolute = offset + done;
        uint32_t currentLba = lba + absolute / 512;
        uint32_t currentOffset = absolute % 512;
        if (currentLba > 4)
            currentLba = 4; // Last sector repeats, same as the upstream host.

        uint32_t chunk = 512 - currentOffset;
        if (chunk > bufsize - done)
            chunk = bufsize - done;

        memcpy(out + done, exfathax[currentLba] + currentOffset, chunk);
        done += chunk;
    }
    return static_cast<int32_t>(bufsize);
}

bool portalUsbEnabled()
{
    return usbEnabled;
}

void portalUsbBegin()
{
    mscDevice.vendorID("PS4");
    mscDevice.productID("PS5PS4 Fusion");
    mscDevice.productRevision("1.0");
    mscDevice.onRead(onRead);
    mscDevice.mediaPresent(true);
    mscDevice.begin(8192, 512);
    USB.begin();
    usbEnabled = true;
    Serial.println("[USB] exfathax MSC device started");
}

#else

bool portalUsbEnabled()
{
    return false;
}

void portalUsbBegin()
{
    Serial.println("[USB] MSC disabled in this build");
}

#endif
