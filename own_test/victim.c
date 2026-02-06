#include <stdio.h>
#include <string.h>
#include <openssl/aes.h>

int main()
{
    AES_KEY aes_key;

    unsigned char key[16] = {
        0x00, 0x01, 0x02, 0x03,
        0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b,
        0x0c, 0x0d, 0x0e, 0x0f};

    unsigned char plaintext[16] = "HELLO_AES_12345";
    unsigned char ciphertext[16];

    AES_set_encrypt_key(key, 128, &aes_key);

    AES_encrypt(plaintext, ciphertext, &aes_key);

    printf("Ciphertext: ");
    for (int i = 0; i < 16; i++)
        printf("%02x ", ciphertext[i]);
    printf("\n");

    return 0;
}
