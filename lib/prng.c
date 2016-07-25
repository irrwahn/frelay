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


#include "prng.h"

/*
 * PRNG implementation
 *
 * A 64-bit three-rotate variety of Bob Jenkins' simple and fast PRNG.
 * Based on the public domain implementation by Robert J. Jenkins Jr.
 * [ http://burtleburtle.net/bob/rand/smallprng.html ]
 * [ https://web.archive.org/web/20151002090926/http://www.burtleburtle.net/bob/rand/smallprng.html ]
 *
 * Pros: Simple, good average cycle length, decent avalanche, fast mixing.
 *
 * Cons: Conceptually limited to values no wider than 64 bits.
 */

#if ULONG_MAX > 18446744073709551615UL  /* 2**64-1 */
    #warning "[s]random[_r] is limited to values no wider than 64 bits!"
#endif

#define LEFTROT(x,k)    (((x)<<(k))|((x)>>(64-(k))))

static random_ctx_t ctx_unsafe = RANDOM_CTX_INITIALIZER;

unsigned long random_r( random_ctx_t *ctx )
{
    uint64_t
         e = ctx->a - LEFTROT( ctx->b,  7 );
    ctx->a = ctx->b ^ LEFTROT( ctx->c, 13 );
    ctx->b = ctx->c + LEFTROT( ctx->d, 37 );
    ctx->c = ctx->d + e;
    ctx->d = e + ctx->a;
    return ctx->d & RANDOM_MAX;
}

unsigned long random()
{
    return random_r( &ctx_unsafe );
}

void srandom_r( random_ctx_t *ctx, unsigned long seed )
{
    ctx->a = RANDOM_FLEASEED;
    ctx->b = ctx->c = ctx->d = seed;
    for ( int i = 20; i--; )
        random_r( ctx );
}

void srandom( unsigned long seed )
{
    srandom_r( &ctx_unsafe, seed );
}

#undef LEFTROT


/*
 * RNG implementation agnostic functions.
 */

unsigned long random_uni_r( random_ctx_t *ctx, unsigned long upper )
{
    unsigned long r = 0;

    if ( 1 < upper )
    {
        /* Wait for a random number in the biggest range divisible by
         * upper without remainder.  This is not expected to loop
         * terribly often, if at all!
         */
        while ( ( r = random_r( ctx ) ) >= RANDOM_MAX - ( RANDOM_MAX % upper ) )
            ;
        /* The modulo operation now cannot introduce any bias. */
        r %= upper;
    }
    return r;
}

unsigned long random_uni( unsigned long upper )
{
    return random_uni_r( &ctx_unsafe, upper );
}


/* EOF */
