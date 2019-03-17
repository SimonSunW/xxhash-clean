/*
 *  xxHash - Fast Hash algorithm
 *  Copyright (C) 2012-2019, Yann Collet
 *  Copyright (C) 2019, easyaspi314 (Devin)
 *
 *  BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  * Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *  copyright notice, this list of conditions and the following disclaimer
 *  in the documentation and/or other materials provided with the
 *  distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You can contact the author at :
 *  - xxHash homepage: http://www.xxhash.com
 *  - xxHash source repository : https://github.com/Cyan4973/xxHash */

/* This is a compact, 100% standalone reference XXH32 single-run implementation.
 * Instead of focusing on performance hacks, this focuses on cleanliness,
 * conformance, portability and simplicity.
 *
 * This file aims to be 100% compatible with C90/C++98, with the additional
 * requirement of stdint.h. No library functions are used. */

#include <stdlib.h> /* size_t, NULL, exit */
#include <stdint.h> /* uint8_t, uint32_t */

#ifdef __cplusplus
extern "C"
#endif
uint32_t XXH32(void const *const input, size_t const length, uint32_t const seed);

static uint32_t const PRIME32_1 = 0x9E3779B1U;   /* 0b10011110001101110111100110110001 */
static uint32_t const PRIME32_2 = 0x85EBCA77U;   /* 0b10000101111010111100101001110111 */
static uint32_t const PRIME32_3 = 0xC2B2AE3DU;   /* 0b11000010101100101010111000111101 */
static uint32_t const PRIME32_4 = 0x27D4EB2FU;   /* 0b00100111110101001110101100101111 */
static uint32_t const PRIME32_5 = 0x165667B1U;   /* 0b00010110010101100110011110110001 */

/* Rotates value left by amount. */
static uint32_t XXH_rotl32(uint32_t value, uint32_t amount)
{
    return (value << amount) | (value >> (32 - amount));
}

/* Portably reads a 32-bit little endian integer from p at the given offset. */
static uint32_t XXH_read32(uint8_t const *const p, size_t const offset)
{
    return (uint32_t)p[offset + 0]
        | ((uint32_t)p[offset + 1] << 8)
        | ((uint32_t)p[offset + 2] << 16)
        | ((uint32_t)p[offset + 3] << 24);
}

/* Mixes input into lane. */
static uint32_t XXH32_round(uint32_t lane, uint32_t const input)
{
    lane += input * PRIME32_2;
    lane  = XXH_rotl32(lane, 13);
    lane *= PRIME32_1;
    return lane;
}

/* Mixes all bits to finalize the hash. */
static uint32_t XXH32_avalanche(uint32_t hash)
{
    hash ^= hash >> 15;
    hash *= PRIME32_2;
    hash ^= hash >> 13;
    hash *= PRIME32_3;
    hash ^= hash >> 16;
    return hash;
}

/* The XXH32 hash function.
 * input:   The data to hash.
 * length:  The length of input. It is undefined behavior to have length larger than the
 *          capacity of input.
 * seed:    A 32-bit value to seed the hash with.
 * returns: The 32-bit calculated hash value. */
uint32_t XXH32(void const *const input, size_t const length, uint32_t const seed)
{
    uint8_t const *const p = (uint8_t const *) input;
    uint32_t hash;
    size_t remaining = length;
    size_t offset = 0;

    /* Don't dereference a null pointer. The reference implementation notably doesn't
     * check for this by default. */
    if (input == NULL) {
        return XXH32_avalanche(seed + PRIME32_5);
    }

    if (remaining >= 16) {
        /* Initialize our lanes */
        uint32_t lane1 = seed + PRIME32_1 + PRIME32_2;
        uint32_t lane2 = seed + PRIME32_2;
        uint32_t lane3 = seed + 0;
        uint32_t lane4 = seed - PRIME32_1;

        while (remaining >= 16) {
            lane1 = XXH32_round(lane1, XXH_read32(p, offset)); offset += 4;
            lane2 = XXH32_round(lane2, XXH_read32(p, offset)); offset += 4;
            lane3 = XXH32_round(lane3, XXH_read32(p, offset)); offset += 4;
            lane4 = XXH32_round(lane4, XXH_read32(p, offset)); offset += 4;
            remaining -= 16;
        }

        hash = XXH_rotl32(lane1, 1) + XXH_rotl32(lane2, 7) + XXH_rotl32(lane3, 12) + XXH_rotl32(lane4, 18);
    } else {
        /* Not enough data for the main loop, put something in there instead. */
        hash = seed + PRIME32_5;
    }

    hash += (uint32_t) length;

    /* Process the remaining data. */
    while (remaining >= 4) {
        hash += XXH_read32(p, offset) * PRIME32_3;
        hash  = XXH_rotl32(hash, 17);
        hash *= PRIME32_4;
        offset += 4;
        remaining -= 4;
    }

    while (remaining != 0) {
        hash += p[offset] * PRIME32_5;
        hash  = XXH_rotl32(hash, 11);
        hash *= PRIME32_1;
        --remaining;
        ++offset;
    }
    return XXH32_avalanche(hash);
}

#ifdef XXH_SELFTEST
#include <stdio.h> /* fprintf, puts */

#define TEST_DATA_SIZE 101
static int test_num = 0;

/* Checks a hash value. */
static void test_sequence(const uint8_t *const test_data, size_t const length,
                          uint32_t const seed, uint32_t const expected)
{
    uint32_t const result = XXH32(test_data, length, seed);
    if (result != expected) {
        fprintf(stderr, "\rError: Test %i: Internal sanity check failed!\n", test_num++);
        fprintf(stderr, "\rExpected value: 0x%08X. Actual value: 0x%08X.\n", expected, result);
        exit(1);
    }
}

int main(void)
{
    const uint32_t prime = PRIME32_1;
    uint8_t test_data[TEST_DATA_SIZE] = {0};
    uint32_t byte_gen = prime;
    int i = 0;

    for (; i < TEST_DATA_SIZE; i++) {
        test_data[i] = (uint8_t)(byte_gen >> 24);
        byte_gen *= byte_gen;
    }

    test_sequence(NULL     ,  0            , 0    , 0x02CC5D05U);
    test_sequence(NULL     ,  0            , prime, 0x36B78AE7U);
    test_sequence(test_data,  1            , 0    , 0xB85CBEE5U);
    test_sequence(test_data,  1            , prime, 0xD5845D64U);
    test_sequence(test_data, 14            , 0    , 0xE5AA0AB4U);
    test_sequence(test_data, 14            , prime, 0x4481951DU);
    test_sequence(test_data, TEST_DATA_SIZE, 0,     0x1F1AA412U);
    test_sequence(test_data, TEST_DATA_SIZE, prime, 0x498EC8E2U);

    puts("XXH32 reference implementation: OK");

    return 0;
}

#endif /* XXH_SELFTEST */

