//
// Copyright (c) 2017-2020 Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//


#include "stake/votebits.h"

#include <utility>

const VoteBits VoteBits::allRejected = VoteBits(0x0000);

VoteBits::VoteBits() : vb(0U)
{
}

VoteBits::VoteBits(const uint16_t& bits)
{
    vb = bits;
}

VoteBits::VoteBits(const uint8_t pos, bool value)
{
    if (pos < Feature::Count && value)
        vb = static_cast<uint16_t>(1 << pos);
    else
        vb = 0U;
}

VoteBits& VoteBits::operator=(const VoteBits& o)
{
    if (&o == this)
        return *this;

    vb = o.vb;

    return *this;
}

VoteBits& VoteBits::operator=(VoteBits&& o)
{
    if (&o == this)
        return *this;

    vb = std::move(o.vb);

    return *this;
}

bool VoteBits::operator==(const VoteBits& o) const
{
    return vb == o.vb;
}

bool VoteBits::operator!=(const VoteBits& o) const
{
    return vb != o.vb;
}


uint16_t VoteBits::getBits() const
{
    return vb;
}

void VoteBits::setBit(const uint8_t pos, bool value)
{
    if (pos < Feature::Count) {
        if (value)
            vb |= 1 << pos;
        else
            vb &= ~(1 << pos);
    }
}

bool VoteBits::getBit(const uint8_t pos) const
{
    return pos < Feature::Count ? (vb & (1 << pos)) > 0 : false;
}

const VoteBits VoteBits::rttAccepted = VoteBits(Feature::Rtt, true);

bool VoteBits::isRttAccepted() const
{
    return getBit(Feature::Rtt);
}

void VoteBits::setRttAccepted(bool accepted)
{
    setBit(Feature::Rtt, accepted);
}
