/* Copyright (c) 2021 Project PAI Foundation
 */

#ifndef PAICOINHASH
#define PAICOINHASH

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *paicoin_hash_ctx;

// size of the output hash in bytes
#define paicoin_hash_size ((size_t)32)

paicoin_hash_ctx paicoin_hash_alloc(void);

void paicoin_hash_write(paicoin_hash_ctx ctx, const unsigned char *data, const size_t len);

void paicoin_hash_finalize(paicoin_hash_ctx ctx, unsigned char hash[]);

void paicoin_hash_reset(paicoin_hash_ctx ctx);

void paicoin_hash_free(paicoin_hash_ctx ctx);

void paicoin_hash(unsigned char hash[], const unsigned char *data, const size_t len);

#ifdef __cplusplus
}
#endif

#endif // PAICOINHASH