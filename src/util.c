// util.c
#include <stdio.h>
#include <iphlpapi.h>
#include <windows.h>

void xor_encrypt(char *data, int len, char key) {
    for (int i = 0; i < len; i++) {
        data[i] ^= key;
    }
}

void get_local_ip(char *buffer, int size) {
    // Récupérer l'IP locale (même implémentation que précédemment)
}

