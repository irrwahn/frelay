/*
 * prng.h
 *
 * THIS SOFTWARE IS UNFIT FOR CRYPTOGRAPHIC PURPOSES!
 *
 * Copyright 2016 Urban Wallasch <irrwahn35@freenet.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

/*
 * CAVEAT: It is strongly recommended against using this pseudo random
 * generator implementation for any cryptographic purpose!
 */

#ifndef PRNG_H_INCLUDED
#define PRNG_H_INCLUDED

#include <limits.h>
#include <stdint.h>

/*
 * RANDOM_MAX is the largest possible value returned by random[_r](),
 * and one more than the largest possible value returned by
 * random_uni[_r]().
 */
#define PRNG_RANDOM_MAX  (ULLONG_MAX)

/*
 * Constants suitable to initialize objects of type random_ctx_t.
 */
#define PRNG_RANDOM_FLEASEED         0xf1ea5eedULL
#define PRNG_RANDOM_CTX_INITIALIZER  { PRNG_RANDOM_FLEASEED, 0ULL, 0ULL, 0ULL }

/*
 * Type to hold the PRNG state information.
 */
struct prng_random_ctx_t_struct {
    uint64_t a, b, c, d;
};

typedef
    struct prng_random_ctx_t_struct
    prng_random_ctx_t;

/*
 * Generate a pseudorandom number in the range 0 <= n <= RANDOM_MAX,
 * that is, pick from the closed interval [0;RANDOM_MAX].
 */
uint64_t prng_random( void );
uint64_t prng_random_r( prng_random_ctx_t *ctx );

/*
 * Initialize the pseudorandom number generator with a given seed.
 */
void prng_srandom( uint64_t seed );
void prng_srandom_r( prng_random_ctx_t *ctx, uint64_t seed );

/*
 * Returns an unbiased PRN from the half-open interval [0;upper[,
 * hence the largest possible return value is RANDOM_MAX-1.
 * NOTE: Use random[_r]() to pick from the interval [0;RANDOM_MAX].
 * Returns 0 for upper < 2.
 */
uint64_t prng_random_uni( uint64_t upper );
uint64_t prng_random_uni_r( prng_random_ctx_t *ctx, uint64_t upper );

#endif  //ndef PRNG_H_INCLUDED

/* EOF */
