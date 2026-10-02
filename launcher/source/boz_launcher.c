#include <stdio.h>
#include <sys/stat.h>
#include <switch.h>
#include "game_setup.h"
#include "launcher.h"
#include "port_config.h"

static const char *const apk_entries[] = {"assets/boz.s3e", "assets/blackops_loader.dz", NULL};
static const RtApkRole apk_role = {.what="BOZ Android", .name="boz.apk", .need=apk_entries};
const RtApkRole *port_apk_roles(int *count) { *count=1; return &apk_role; }

static int setup_progress(const char *message, int permille, void *user) {
  (void)user;
  launcher_bar(message, permille);
  return appletMainLoop();
}
int port_launcher_prepare(void) {
  RtApkFound found;
  RtApkEnv environment = {0};
  if (rt_apk_find("sdmc:" PORT_ROOT_PATH, &apk_role, 1, &environment, &found)) return 0;
  char error[512];
  if (boz_setup("sdmc:" PORT_ROOT_PATH, found.path[0], setup_progress, NULL, error, sizeof error)) {
    launcher_bar_off();
    printf("\n%s\n", error);
    return 1;
  }
  launcher_bar_off();
  printf("Game data prepared on the SD card.\n");
  return 0;
}
int port_launcher_check(char *status,size_t scap,char *help,size_t hcap) {
  static const char *files[]={"boz.s3e.unpacked","blackops_etc.dz","blackops_loader.dz","boz_files.idx"};
  for(unsigned i=0;i<sizeof(files)/sizeof(files[0]);i++) {
    char path[256]; struct stat st;
    snprintf(path,sizeof path,"sdmc:%s/%s",PORT_ROOT_PATH,files[i]);
    if(stat(path,&st)!=0 || st.st_size==0) {
      snprintf(status,scap,"Game data missing: %s",files[i]);
      snprintf(help,hcap,"Put your BOZ Android v1.0.11 APK and blackops_etc.dz in " PORT_ROOT_PATH ". Launch codboz.nro to prepare the remaining files automatically.");
      return 1;
    }
  }
  snprintf(status,scap,"BOZ game data: ready"); return 0;
}
void port_launcher_instructions(void) {
  printf("  1. put codboz.nro, your BOZ v1.0.11 APK, and blackops_etc.dz\n"
         "     in " PORT_ROOT_PATH ". Game files are prepared automatically.\n");
}
void port_launcher_apk_help(void) {
  printf("\nCopy your BOZ Android v1.0.11 APK beside codboz.nro in " PORT_ROOT_PATH ".\n");
}
