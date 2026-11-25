#include "dns_portal.h"
#include "portal_config.h"

PortalDnsServer portalDns;

namespace
{
constexpr size_t DNS_BUFFER_SIZE = 512;

uint16_t read16(const uint8_t *p)
{
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

void write16(uint8_t *p, uint16_t value)
{
    p[0] = static_cast<uint8_t>(value >> 8);
    p[1] = static_cast<uint8_t>(value & 0xff);
}
} // namespace

void PortalDnsServer::begin(IPAddress answer)
{
    _answer = answer;
    _udp.begin(53);
}

void PortalDnsServer::process()
{
    int size = _udp.parsePacket();
    if (size <= 0 || size > static_cast<int>(DNS_BUFFER_SIZE))
    {
        if (size > static_cast<int>(DNS_BUFFER_SIZE))
            _udp.flush();
        return;
    }

    uint8_t request[DNS_BUFFER_SIZE];
    uint8_t response[DNS_BUFFER_SIZE];
    int len = _udp.read(request, DNS_BUFFER_SIZE);
    if (len < 12)
        return;

    // QR must be 0 (query), OPCODE must be 0 (standard query), QDCOUNT == 1.
    if ((request[2] & 0x80) != 0 || (request[2] & 0x78) != 0 || read16(request + 4) != 1)
        return;

    // Walk the single QNAME to find the end of the question section.
    size_t pos = 12;
    bool valid = true;
    while (pos < static_cast<size_t>(len))
    {
        uint8_t label = request[pos];
        if (label == 0)
        {
            pos++;
            break;
        }
        if ((label & 0xc0) != 0 || pos + label + 1 > static_cast<size_t>(len))
        {
            valid = false;
            break;
        }
        pos += static_cast<size_t>(label) + 1;
    }
    if (!valid || pos + 4 > static_cast<size_t>(len))
        return;

    uint16_t qtype = read16(request + pos);
    uint16_t qclass = read16(request + pos + 2);
    size_t questionLen = pos + 4;

    memcpy(response, request, questionLen);
    write16(response + 2, 0x8180); // QR=1, RD=1, RA=1, RCODE=0
    write16(response + 4, 1);      // QDCOUNT

    size_t answerLen = 0;
    if (qclass == 1 && qtype == 1) // IN A
    {
        write16(response + 6, 1); // ANCOUNT
        uint8_t *answer = response + questionLen;
        answer[0] = 0xc0; // compression pointer to QNAME at offset 12
        answer[1] = 0x0c;
        write16(answer + 2, 1);                     // TYPE A
        write16(answer + 4, 1);                     // CLASS IN
        write16(answer + 6, 0);                     // TTL high
        write16(answer + 8, PORTAL_DNS_TTL);        // TTL low
        write16(answer + 10, 4);                    // RDLENGTH
        answer[12] = _answer[0];
        answer[13] = _answer[1];
        answer[14] = _answer[2];
        answer[15] = _answer[3];
        answerLen = 16;
    }
    else
    {
        write16(response + 6, 0); // ANCOUNT: NO DATA for AAAA / other types
    }

    write16(response + 8, 0);  // NSCOUNT
    write16(response + 10, 0); // ARCOUNT (drop EDNS OPT records)

    _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
    _udp.write(response, questionLen + answerLen);
    _udp.endPacket();
    _queries++;
}
