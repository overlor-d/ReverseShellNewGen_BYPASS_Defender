#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    /* Includes Windows */
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")

    #define CLOSESOCK closesocket
#else
    /* Includes Linux */
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>

    #define SOCKET int
    #define CLOSESOCK close
#endif

#define SERVER_PORT 4444
#define XOR_KEY 0x5A

/* 
 * Fonction d'initialisation spécifique :
 * - Sur Windows, initialise la DLL Winsock.
 * - Sur Linux, ne fait rien (retourne 0 si OK).
 */
int init_sockets()
{
#ifdef _WIN32
    WSADATA wsaData;
    return (WSAStartup(MAKEWORD(2,2), &wsaData) == 0) ? 0 : -1;
#else
    return 0;
#endif
}

/* 
 * Fonction de nettoyage :
 * - Sur Windows, libère la DLL Winsock.
 * - Sur Linux, ne fait rien. 
 */
void cleanup_sockets()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

/* 
 * Petit utilitaire pour chiffrer/déchiffrer 
 * (l'opération XOR est réversible avec la même clé).
 */
void xor_data(char *data, size_t length, unsigned char key) {
    for (size_t i = 0; i < length; i++) {
        data[i] ^= key;
    }
}

/*
 * Récupère l'adresse IP locale de la machine.
 * Méthode simple : gethostname() puis gethostbyname() (ou getaddrinfo).
 * On stocke l'IP dans local_ip.
 */
void get_local_ip(char *local_ip, size_t size)
{
    char hostname[256];
    memset(hostname, 0, sizeof(hostname));
    memset(local_ip, 0, size);

    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct hostent *he = gethostbyname(hostname);
        if (he && he->h_addr_list && he->h_addr_list[0]) {
            struct in_addr **addr_list = (struct in_addr **)he->h_addr_list;
            if (addr_list[0] != NULL) {
                strncpy(local_ip, inet_ntoa(*addr_list[0]), size - 1);
                return;
            }
        }
    }
    /* Si problème, on met un fallback */
    strncpy(local_ip, "127.0.0.1", size - 1);
}

/*
 * Mode serveur :
 *  - Écoute sur le port 4444
 *  - Accepte une connexion
 *  - Reçoit des données XOR-chiffrées
 *  - Déchiffre et affiche 
 */
void run_server()
{
    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket() failed");
        return;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;   // Écoute sur toutes les interfaces
    server_addr.sin_port = htons(SERVER_PORT);  // Port 4444

    /* Associe le socket à l'adresse/port */
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind() failed");
        CLOSESOCK(server_sock);
        return;
    }

    /* Écoute des connexions */
    if (listen(server_sock, 1) < 0) {
        perror("listen() failed");
        CLOSESOCK(server_sock);
        return;
    }

    printf("[Serveur] En écoute sur le port %d...\n", SERVER_PORT);

    /* Accepte une connexion (bloquant pour la simplicité) */
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    SOCKET client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
    if (client_sock < 0) {
        perror("accept() failed");
        CLOSESOCK(server_sock);
        return;
    }
    printf("[Serveur] Client connecté : %s\n", inet_ntoa(client_addr.sin_addr));

    /* Reçoit les données */
    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    int received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (received > 0) {
        /* Déchiffrage XOR */
        xor_data(buffer, received, XOR_KEY);

        printf("[Serveur] Reçu déchiffré : %s\n", buffer);
    }

    CLOSESOCK(client_sock);
    CLOSESOCK(server_sock);
}

/*
 * Mode client :
 *  - Récupère l'IP locale
 *  - Chiffre (XOR avec 0x5A)
 *  - Envoie au serveur (IP spécifiée en paramètre)
 */
void run_client(const char *server_ip)
{
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket() failed");
        return;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() failed");
        CLOSESOCK(sock);
        return;
    }

    /* Récupère l'IP locale et la chiffre */
    char local_ip[64];
    get_local_ip(local_ip, sizeof(local_ip));
    xor_data(local_ip, strlen(local_ip), XOR_KEY);

    /* Envoie au serveur */
    int sent = send(sock, local_ip, strlen(local_ip), 0);
    if (sent < 0) {
        perror("send() failed");
    } else {
        printf("[Client] IP locale chiffrée envoyée avec succès.\n");
    }

    CLOSESOCK(sock);
}

int main(int argc, char *argv[])
{
    if (init_sockets() != 0) {
        fprintf(stderr, "Échec d'initialisation des sockets.\n");
        return EXIT_FAILURE;
    }

    /* 
     * Exemple de syntaxe :
     *  - ./myProgram -s
     *  - ./myProgram -c <server_ip>
     */
    if (argc > 1 && strcmp(argv[1], "-s") == 0) {
        /* Mode serveur */
        run_server();
    } 
    else if (argc > 2 && strcmp(argv[1], "-c") == 0) {
        /* Mode client, argv[2] = IP du serveur */
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
