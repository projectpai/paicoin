// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_STAKE_EXTENDEDVOTEBITS_H
#define PAICOIN_STAKE_EXTENDEDVOTEBITS_H

#include <string>
#include <vector>

// Extended vote bits

class ExtendedVoteBits {
public:
    // verify if the string contains a valid representation of extended vote bits
    static bool containsValidExtendedVoteBits(const std::string& hex);

    // verify if the vector contains a valid representation of extended vote bits
    static bool containsValidExtendedVoteBits(const std::vector<unsigned char>& vect);

    // minimum size of the extended vote bits vector
    static size_t minSize();

    // maximum size of the extended vote bits vector
    static size_t maxSize();

    // default constructor
    ExtendedVoteBits();

    // copy constructor (may create invalid object, if o is invalid)
    ExtendedVoteBits(const ExtendedVoteBits& o) = default;

    // move constructor (may create invalid object, if o is invalid)
    ExtendedVoteBits(ExtendedVoteBits&& o) = default;

    // creates the extended vote bits from the hexadecimal string representation
    // if the hex is not valid, the default value is used
    ExtendedVoteBits(const std::string& hex);

    // creates the extended vote bits from the vector representation
    // if the vector is not valid, the default value is used
    ExtendedVoteBits(const std::vector<unsigned char>& vect);

    // copy assignment operator (may invalidate the contents)
    ExtendedVoteBits& operator=(const ExtendedVoteBits& o);

    // move assignment operator (may invalidate the contents)
    ExtendedVoteBits& operator=(ExtendedVoteBits&& o);

    // equality operator
    bool operator==(const ExtendedVoteBits& o) const;

    // inequality operator
    bool operator!=(const ExtendedVoteBits& o) const;

    // verify the validity of the inner vector data
    bool isValid() const;

    // get the actual vector of bytes
    const std::vector<unsigned char>& getVector() const;

    // get the hexadecimal string representation
    std::string getHex() const;

private:
    const unsigned char DefaultExtendedVoteBits = 0x00;

    std::vector<unsigned char> v;
};

#endif //PAICOIN_STAKE_EXTENDEDVOTEBITS_H
