// victim.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <openssl/aes.h>

unsigned char key[16] = {
    0x2a, 0x6b, 0x7e, 0x15,
    0x16, 0x28, 0xae, 0xd2,
    0xa6, 0xab, 0xf7, 0x15,
    0x88, 0x09, 0xcf, 0x4f};

int main()
{
    AES_KEY key_struct;
    unsigned char plaintext[16];
    unsigned char ciphertext[16];

    AES_set_encrypt_key(key, 128, &key_struct);

    printf("[Victim] PID=%d\n", getpid());
    fflush(stdout);

    while (1)
    {
        for (int i = 0; i < 16; i++)
            plaintext[i] = rand() & 0xff;

        AES_encrypt(plaintext, ciphertext, &key_struct);
    }
}
