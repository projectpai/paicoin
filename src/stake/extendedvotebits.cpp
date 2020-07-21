// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "stake/extendedvotebits.h"
#include "stake/staketx.h"
#include "utilstrencodings.h"

bool ExtendedVoteBits::containsValidExtendedVoteBits(const std::string& hex)
{
        const size_t stringSize = hex.length();
        const size_t bytesSize = stringSize / 2;
        return stringSize % 2 == 0 && bytesSize >= minSize() && bytesSize <= maxSize() && IsHex(hex);
}

bool ExtendedVoteBits::containsValidExtendedVoteBits(const std::vector<unsigned char>& vect)
{
        const size_t bytesSize = vect.size();
        return bytesSize >= minSize() && bytesSize <= maxSize();
}

size_t ExtendedVoteBits::minSize()
{
    return 1;
}

size_t ExtendedVoteBits::maxSize()
{
    return nMaxStructDatacarrierBytes - GetVoteDataSizeWithEmptyExtendedVoteBits() - 1 /* push opcode */;
}

ExtendedVoteBits::ExtendedVoteBits()
{
    v.push_back(DefaultExtendedVoteBits);
}

ExtendedVoteBits::ExtendedVoteBits(const std::string& hex)
{
    if (containsValidExtendedVoteBits(hex))
        v = ParseHex(hex);
    else
        v.push_back(DefaultExtendedVoteBits);
}

ExtendedVoteBits::ExtendedVoteBits(const std::vector<unsigned char>& vect)
{
    if (containsValidExtendedVoteBits(vect))
        v = vect;
    else
        v.push_back(DefaultExtendedVoteBits);
}

ExtendedVoteBits& ExtendedVoteBits::operator=(const ExtendedVoteBits& o)
{
    if (&o == this)
        return *this;

    v = o.v;

    return *this;
}

ExtendedVoteBits& ExtendedVoteBits::operator=(ExtendedVoteBits&& o)
{
    if (&o == this)
        return *this;

    v = std::move(o.v);

    return *this;
}

bool ExtendedVoteBits::operator==(const ExtendedVoteBits& o) const
{
    return v == o.v;
}

bool ExtendedVoteBits::operator!=(const ExtendedVoteBits& o) const
{
    return v != o.v;
}

bool ExtendedVoteBits::isValid() const
{
    return containsValidExtendedVoteBits(v);
}

const std::vector<unsigned char>& ExtendedVoteBits::getVector() const
{
    return v;
}

std::string ExtendedVoteBits::getHex() const
{
    return HexStr(v);
}
