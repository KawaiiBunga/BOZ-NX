#include "game_setup.h"
#include <stdio.h>
#include <stdlib.h>
static int progress(const char *message, int permille, void *user) {
    (void)message; (void)user;
    const char *cancel = getenv("BOZ_SETUP_CANCEL_AT");
    return !cancel || permille < atoi(cancel);
}
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    char error[512];
    int result = boz_setup(argv[1], argv[2], progress, NULL, error, sizeof error);
    if (result) fprintf(stderr, "%s\n", error);
    return result ? 1 : 0;
}
