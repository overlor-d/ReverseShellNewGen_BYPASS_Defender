#include <stdio.h>
#include <string.h>
#include "util.h"
#include "client.h"
#include "server.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage :\n");
        printf("  %s -c <server_ip> : Mode cible (client).\n", argv[0]);
        printf("  %s -s            : Mode serveur.\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-c") == 0 && argc == 3) {
        run_client_mode(argv[2]); // Fonction dans client.c
    } else if (strcmp(argv[1], "-s") == 0) {
        run_server_mode(); // Fonction dans server.c
    } else {
        printf("Arguments invalides.\n");
        return 1;
    }

    return 0;
}
