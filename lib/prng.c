/*
 * prng.c
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


#include <prng.h>

/*
 * PRNG implementation
 *
 * A 64-bit three-rotate variety of Bob Jenkins' simple and fast PRNG.
 * Based on the public domain implementation by Robert J. Jenkins Jr.
 * [ http://burtleburtle.net/bob/rand/smallprng.html ]
 * [ https://web.archive.org/web/20151002090926/http://www.burtleburtle.net/bob/rand/smallprng.html ]
 *
 * Pros:
 *  + simple
 *  + good average cycle length
 *  + decent avalanche
 *  + fast mixing.
 *
 * Cons:
 *  - conceptually limited to values no wider than 64 bits
 *  - not fit for any cryptographic purpose
 */

#if ULONG_MAX > 18446744073709551615UL  /* 2**64-1 */
    #warning "[s]random[_r] is limited to values no wider than 64 bits!"
#endif

#define LEFTROT(x,k)    (((x)<<(k))|((x)>>(64-(k))))

static prng_random_ctx_t ctx_unsafe = PRNG_RANDOM_CTX_INITIALIZER;

uint64_t prng_random_r( prng_random_ctx_t *ctx )
{
    uint64_t
         e = ctx->a - LEFTROT( ctx->b,  7 );
    ctx->a = ctx->b ^ LEFTROT( ctx->c, 13 );
    ctx->b = ctx->c + LEFTROT( ctx->d, 37 );
    ctx->c = ctx->d + e;
    ctx->d = e + ctx->a;
    return ctx->d & PRNG_RANDOM_MAX;
}

uint64_t prng_random( void )
{
    return prng_random_r( &ctx_unsafe );
}

void prng_srandom_r( prng_random_ctx_t *ctx, uint64_t seed )
{
    ctx->a = PRNG_RANDOM_FLEASEED;
    ctx->b = ctx->c = ctx->d = seed;
    for ( int i = 20; i--; )
        prng_random_r( ctx );
}

void prng_srandom( uint64_t seed )
{
    prng_srandom_r( &ctx_unsafe, seed );
}

#undef LEFTROT


/*
 * RNG implementation agnostic functions.
 */

uint64_t prng_random_uni_r( prng_random_ctx_t *ctx, uint64_t upper )
{
    uint64_t r = 0;

    if ( 1 < upper )
    {
        /* Wait for a random number in the biggest range divisible by
         * upper without remainder.  This is not expected to loop
         * terribly often, if at all!
         */
        while ( ( r = prng_random_r( ctx ) ) >= PRNG_RANDOM_MAX - ( PRNG_RANDOM_MAX % upper ) )
            ;
        /* The modulo operation now cannot introduce any bias. */
        r %= upper;
    }
    return r;
}

uint64_t prng_random_uni( uint64_t upper )
{
    return prng_random_uni_r( &ctx_unsafe, upper );
}


/* EOF */
