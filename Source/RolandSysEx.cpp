#include "RolandSysEx.h"

namespace roland
{

static constexpr uint8_t kRolandId   = 0x41;
static constexpr uint8_t kModelHi    = 0x00;
static constexpr uint8_t kModelLo    = 0x59;
static constexpr uint8_t kModelQuick = 0x5D;
static constexpr uint8_t kCmdRQ1     = 0x11;
static constexpr uint8_t kCmdDT1     = 0x12;

uint8_t checksum (const uint8_t* data, size_t numBytes) noexcept
{
    int sum = 0;
    for (size_t i = 0; i < numBytes; ++i)
        sum += data[i];

    const int remainder = sum % 128;
    return (uint8_t) (remainder == 0 ? 0 : 128 - remainder);
}

void toNibbles (int value, int numBytes, std::vector<uint8_t>& out)
{
    if (numBytes <= 1)
    {
        out.push_back ((uint8_t) (value & 0x7F));
        return;
    }

    for (int i = numBytes - 1; i >= 0; --i)
        out.push_back ((uint8_t) ((value >> (4 * i)) & 0x0F));
}

int fromNibbles (const uint8_t* data, int numBytes) noexcept
{
    if (numBytes <= 1)
        return data[0] & 0x7F;

    int v = 0;
    for (int i = 0; i < numBytes; ++i)
        v = (v << 4) | (data[i] & 0x0F);

    return v;
}

juce::MidiMessage makeDT1 (uint8_t deviceId,
                           const Address& addr,
                           const uint8_t* data,
                           size_t numDataBytes)
{
    std::vector<uint8_t> body;
    body.reserve (numDataBytes + 10);

    body.push_back (kRolandId);
    body.push_back (deviceId);
    body.push_back (kModelHi);
    body.push_back (kModelLo);
    body.push_back (kCmdDT1);

    const size_t checksumStart = body.size();

    for (auto byte : addr.b)
        body.push_back (byte);

    for (size_t i = 0; i < numDataBytes; ++i)
        body.push_back (data[i] & 0x7F);

    body.push_back (checksum (body.data() + checksumStart, body.size() - checksumStart));

    return juce::MidiMessage::createSysExMessage (body.data(), (int) body.size());
}

juce::MidiMessage makeRQ1 (uint8_t deviceId, const Address& addr, uint32_t sizeLinear)
{
    const Address sizeAddr = Address::fromLinear (sizeLinear);

    std::vector<uint8_t> body;
    body.reserve (16);

    body.push_back (kRolandId);
    body.push_back (deviceId);
    body.push_back (kModelHi);
    body.push_back (kModelLo);
    body.push_back (kCmdRQ1);

    const size_t checksumStart = body.size();

    for (auto byte : addr.b)     body.push_back (byte);
    for (auto byte : sizeAddr.b) body.push_back (byte);

    body.push_back (checksum (body.data() + checksumStart, body.size() - checksumStart));

    return juce::MidiMessage::createSysExMessage (body.data(), (int) body.size());
}

juce::MidiMessage makeQuickSysEx (uint8_t deviceId,
                                  uint8_t addrMsb, uint8_t addrLsb,
                                  uint8_t data0, uint8_t data1)
{
    std::vector<uint8_t> body;
    body.push_back (kRolandId);
    body.push_back (deviceId);
    body.push_back (kModelQuick);
    body.push_back (kCmdDT1);

    const size_t checksumStart = body.size();

    body.push_back (addrMsb & 0x7F);
    body.push_back (addrLsb & 0x7F);
    body.push_back (data0   & 0x7F);
    body.push_back (data1   & 0x7F);

    body.push_back (checksum (body.data() + checksumStart, body.size() - checksumStart));

    return juce::MidiMessage::createSysExMessage (body.data(), (int) body.size());
}

juce::MidiMessage makeIdentityRequest()
{
    const uint8_t body[] = { 0x7E, 0x7F, 0x06, 0x01 };
    return juce::MidiMessage::createSysExMessage (body, (int) sizeof (body));
}

bool parseDT1 (const juce::MidiMessage& m, DT1& out)
{
    if (! m.isSysEx())
        return false;

    const uint8_t* d = m.getSysExData();
    const int n = m.getSysExDataSize();

    // 41 dev 00 59 12 aa bb cc dd <data...> sum   => minimum 11 bytes
    if (n < 11)                     return false;
    if (d[0] != kRolandId)          return false;
    if (d[2] != kModelHi)           return false;
    if (d[3] != kModelLo)           return false;
    if (d[4] != kCmdDT1)            return false;

    const int checksumStart = 5;
    const int numChecked    = n - 1 - checksumStart;   // address + data
    if (numChecked < 4)             return false;

    if (checksum (d + checksumStart, (size_t) numChecked) != d[n - 1])
        return false;

    out.deviceId = d[1];
    out.address  = Address (d[5], d[6], d[7], d[8]);
    out.data.assign (d + 9, d + (n - 1));
    return true;
}

bool isMC909IdentityReply (const juce::MidiMessage& m)
{
    if (! m.isSysEx())
        return false;

    const uint8_t* d = m.getSysExData();
    const int n = m.getSysExDataSize();

    // 7E dev 06 02 41 59 01 ...
    return n >= 7
        && d[0] == 0x7E
        && d[2] == 0x06
        && d[3] == 0x02
        && d[4] == kRolandId
        && d[5] == 0x59
        && d[6] == 0x01;
}

} // namespace roland
