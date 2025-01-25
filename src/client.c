#include <stdio.h>
#include "util.h"
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

void run_client_mode(const char *server_ip) {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in server;
    char local_ip[16] = {0};
    char data_to_send[64] = {0};

    // Initialiser Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("[-] Erreur Winsock : %d\n", WSAGetLastError());
        return;
    }

    // Récupérer l'adresse IP locale
    get_local_ip(local_ip, sizeof(local_ip));
    if (strlen(local_ip) == 0) {
        printf("[-] Impossible de récupérer l'IP locale.\n");
        WSACleanup();
        return;
    }

    // Préparer et chiffrer les données
    snprintf(data_to_send, sizeof(data_to_send), "IP:%s", local_ip);
    xor_encrypt(data_to_send, strlen(data_to_send), ENCRYPTION_KEY);

    // Configurer le serveur
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(server_ip);
    server.sin_port = htons(DEFAULT_PORT);

    // Envoi des données
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        printf("[-] Erreur de connexion au serveur.\n");
    } else {
        send(sock, data_to_send, strlen(data_to_send), 0);
        printf("[+] Données envoyées.\n");
    }

    closesocket(sock);
    WSACleanup();
}
