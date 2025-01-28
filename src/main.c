#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  // Windows
  #include <winsock2.h>
  #include <ws2tcpip.h>
  /* 
   * Sous Visual Studio, on peut utiliser :
   * #pragma comment(lib, "ws2_32.lib")
   * Mais sous MinGW/GCC, on utilisera -lws2_32 dans le Makefile.
   */
  #define CLOSESOCK closesocket
  #define SOCKET_ERROR_VALUE INVALID_SOCKET
#else
  // Linux / Unix
  #include <unistd.h>
  #include <sys/socket.h>
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <netdb.h>
  #define SOCKET int
  #define CLOSESOCK close
  #define SOCKET_ERROR_VALUE -1
#endif

#define SERVER_PORT 4444
#define XOR_KEY     0x5A

/*
 *  Initialisation spécifique aux sockets :
 *  - Sous Windows, on appelle WSAStartup().
 *  - Sous Linux, on ne fait rien de spécial.
 */
int init_sockets() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        fprintf(stderr, "Échec WSAStartup.\n");
        return -1;
    }
#endif
    return 0;
}

/*
 *  Nettoyage :
 *  - Sous Windows : WSACleanup()
 *  - Sous Linux   : rien
 */
void cleanup_sockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}

/*
 *  Fonction de chiffrement/déchiffrement XOR :
 *  L'opération XOR est réversible avec la même clé.
 */
void xor_data(char *data, size_t length, unsigned char key) {
    for (size_t i = 0; i < length; i++) {
        data[i] ^= key;
    }
}

/*
 *  Récupère l'IP locale réellement utilisée pour joindre
 *  "destination_ip" via un socket UDP, puis getsockname().
 */
void get_local_ip(char *local_ip, size_t size, const char *destination_ip) {
    // Crée un socket UDP
    SOCKET tmpSock = socket(AF_INET, SOCK_DGRAM, 0);

    // Vérifie succès (selon l'OS)
#ifdef _WIN32
    if (tmpSock == INVALID_SOCKET) {
#else
    if (tmpSock < 0) {
#endif
        strncpy(local_ip, "127.0.0.1", size - 1);
        local_ip[size - 1] = '\0';
        return;
    }

    // Prépare la structure pour se "connecter" en UDP
    struct sockaddr_in tmpAddr;
    memset(&tmpAddr, 0, sizeof(tmpAddr));
    tmpAddr.sin_family = AF_INET;
    tmpAddr.sin_port   = htons(80); // n'importe quel port distant
    tmpAddr.sin_addr.s_addr = inet_addr(destination_ip);

    // Connecte le socket UDP pour déterminer la route
    int ret = connect(tmpSock, (struct sockaddr*)&tmpAddr, sizeof(tmpAddr));
#ifdef _WIN32
    if (ret == SOCKET_ERROR) {
#else
    if (ret < 0) {
#endif
        CLOSESOCK(tmpSock);
        strncpy(local_ip, "127.0.0.1", size - 1);
        local_ip[size - 1] = '\0';
        return;
    }

    // Récupère l'adresse IP locale choisie
    struct sockaddr_in localAddr;
    socklen_t addrLen = sizeof(localAddr);
    if (getsockname(tmpSock, (struct sockaddr*)&localAddr, &addrLen) == 0) {
        strncpy(local_ip, inet_ntoa(localAddr.sin_addr), size - 1);
        local_ip[size - 1] = '\0';
    } else {
        strncpy(local_ip, "127.0.0.1", size - 1);
        local_ip[size - 1] = '\0';
    }

    CLOSESOCK(tmpSock);
}

/*
 *  Mode serveur :
 *   - Écoute sur le port 4444 (TCP)
 *   - Accepte 1 client
 *   - Reçoit les données XOR-chiffrées
 *   - Déchiffre puis affiche
 */
void run_server() {
    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    if (server_sock == INVALID_SOCKET) {
#else
    if (server_sock < 0) {
#endif
        perror("socket() failed");
        return;
    }

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family      = AF_INET;
    srv.sin_addr.s_addr = INADDR_ANY; // Écoute sur toutes les interfaces
    srv.sin_port        = htons(SERVER_PORT);

    if (bind(server_sock, (struct sockaddr*)&srv, sizeof(srv)) < 0) {
        perror("bind() failed");
        CLOSESOCK(server_sock);
        return;
    }

    if (listen(server_sock, 1) < 0) {
        perror("listen() failed");
        CLOSESOCK(server_sock);
        return;
    }

    printf("[Serveur] En écoute sur le port %d...\n", SERVER_PORT);

    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    SOCKET client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &len);
#ifdef _WIN32
    if (client_sock == INVALID_SOCKET) {
#else
    if (client_sock < 0) {
#endif
        perror("accept() failed");
        CLOSESOCK(server_sock);
        return;
    }

    printf("[Serveur] Client connecté : %s\n", inet_ntoa(client_addr.sin_addr));

    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    int received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (received > 0) {
        // Déchiffre
        xor_data(buffer, received, XOR_KEY);
        printf("[Serveur] Reçu déchiffré : %s\n", buffer);
    }

    CLOSESOCK(client_sock);
    CLOSESOCK(server_sock);
}

/*
 *  Mode client :
 *   - Se connecte au serveur <server_ip> sur le port 4444
 *   - Récupère son IP locale (méthode UDP + getsockname)
 *   - Chiffre cette IP locale (XOR_KEY)
 *   - Envoie au serveur
 */
void run_client(const char *server_ip) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    if (sock == INVALID_SOCKET) {
#else
    if (sock < 0) {
#endif
        perror("socket() failed");
        return;
    }

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family      = AF_INET;
    srv.sin_port        = htons(SERVER_PORT);
    srv.sin_addr.s_addr = inet_addr(server_ip);

    // Connexion au serveur
#ifdef _WIN32
    if (connect(sock, (struct sockaddr*)&srv, sizeof(srv)) == SOCKET_ERROR) {
#else
    if (connect(sock, (struct sockaddr*)&srv, sizeof(srv)) < 0) {
#endif
        perror("connect() failed");
        CLOSESOCK(sock);
        return;
    }

    // Récupère l'IP locale
    char local_ip[64];
    get_local_ip(local_ip, sizeof(local_ip), server_ip);

    // Chiffrement XOR
    xor_data(local_ip, strlen(local_ip), XOR_KEY);

    // Envoi
    int sent = send(sock, local_ip, strlen(local_ip), 0);
    if (sent < 0) {
        perror("send() failed");
    } else {
        printf("[Client] IP locale chiffrée envoyée avec succès.\n");
    }

    CLOSESOCK(sock);
}

/*
 *  main :
 *   - init_sockets()
 *   - parse arguments
 *   - run_server() ou run_client() suivant
 */
int main(int argc, char *argv[]) {
    if (init_sockets() != 0) {
        fprintf(stderr, "Impossible d'initialiser les sockets.\n");
        return EXIT_FAILURE;
    }

    if (argc > 1 && strcmp(argv[1], "-s") == 0) {
        // Mode serveur
        run_server();
    }
    else if (argc > 2 && strcmp(argv[1], "-c") == 0) {
        // Mode client, argv[2] = IP serveur
        run_client(argv[2]);
    }
    else {
        printf("Usage:\n");
        printf("  %s -s            (mode serveur)\n", argv[0]);
        printf("  %s -c <IP>       (mode client)\n", argv[0]);
    }

    cleanup_sockets();
    return EXIT_SUCCESS;
}
