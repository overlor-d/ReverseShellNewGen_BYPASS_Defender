#include <stdio.h>
#include "util.h"
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

void run_server_mode() {
    WSADATA wsa;
    SOCKET server_sock, client_sock;
    struct sockaddr_in server, client;
    char buffer[64];
    int recv_size;

    // Initialiser Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("[-] Erreur Winsock : %d\n", WSAGetLastError());
        return;
    }

    // Créer une socket d'écoute
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(DEFAULT_PORT);

    bind(server_sock, (struct sockaddr *)&server, sizeof(server));
    listen(server_sock, 3);
    printf("[+] En attente de connexions...\n");

    client_sock = accept(server_sock, NULL, NULL);
    recv_size = recv(client_sock, buffer, sizeof(buffer), 0);

    // Déchiffrement
    xor_encrypt(buffer, recv_size, ENCRYPTION_KEY);
    printf("[+] Données reçues : %s\n", buffer);

    closesocket(client_sock);
    closesocket(server_sock);
    WSACleanup();
}
