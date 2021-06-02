/* Copyright (c) 2021 Project PAI Foundation
 */

#ifndef PAICOIN_HASH_CXX
#define PAICOIN_HASH_CXX

#include <cstddef>

class CPaicoinHash
{
public:
    static const size_t OutputSize;

    static void Hash(unsigned char hash[], const unsigned char *data, const size_t len);

    CPaicoinHash();
    ~CPaicoinHash();

    CPaicoinHash &Write(const unsigned char *data, const size_t len);

    void Finalize(unsigned char hash[]);

    CPaicoinHash &Reset();

private:
    void *impl;
};

#endif // PAICOIN_HASH_CXX