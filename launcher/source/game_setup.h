#ifndef BOZ_GAME_SETUP_H
#define BOZ_GAME_SETUP_H
#include <stddef.h>
/* Return zero from progress to cancel. All writes stay inside root; saves and
 * config are never touched. Portable so the same setup code is host-tested. */
typedef int (*BozSetupProgress)(const char *message, int permille, void *user);
int boz_setup(const char *root, const char *apk, BozSetupProgress progress,
              void *user, char *error, size_t error_capacity);
#endif
