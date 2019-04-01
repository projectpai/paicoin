// Copyright (c) 2018 The PAI Coin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_HTTP_H
#define PAICOIN_HTTP_H

/**
 * HTTP status codes
 */
enum class HTTPStatusCode
{
    OK                    = 200,
    BAD_REQUEST           = 400,
    UNAUTHORIZED          = 401,
    FORBIDDEN             = 403,
    NOT_FOUND             = 404,
    BAD_METHOD            = 405,
    INTERNAL_SERVER_ERROR = 500,
    SERVICE_UNAVAILABLE   = 503,
};

#endif // PAICOIN_HTTP_H
