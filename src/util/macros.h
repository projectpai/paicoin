// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_UTIL_MACROS_H
#define PAICOIN_UTIL_MACROS_H

#define PASTE(x, y) x ## y
#define PASTE2(x, y) PASTE(x, y)

/**
 * Converts the parameter X to a string after macro replacement on X has been performed.
 * Don't merge these into one macro!
 */
#define STRINGIZE(X) DO_STRINGIZE(X)
#define DO_STRINGIZE(X) #X

#endif // PAICOIN_UTIL_MACROS_H
