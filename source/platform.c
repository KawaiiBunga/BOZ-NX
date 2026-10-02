/* Minimal android32 support used by the Marmalade wrapper. No clock setters
 * or automatic CPU boost are linked into this port. */
#include <switch.h>
#include <switch/arm/exception32.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "util.h"
#include "port_config.h"
#include <sys/iosupport.h>
#include "native_log.h"
extern volatile uint32_t __dcr_reloc_path;
int dcr_is_emulator(void) { return __dcr_reloc_path == 2; }
static const devoptab_t *log_forward;
static FILE *log_file;
static Mutex log_mutex;
static char log_buffer[256*1024];
static size_t log_used;
static ssize_t log_write(struct _reent *r,void *fd,const char *p,size_t n) {
  mutexLock(&log_mutex);
  size_t keep=n;
  if(keep>sizeof log_buffer-log_used)keep=sizeof log_buffer-log_used;
  memcpy(log_buffer+log_used,p,keep); log_used+=keep;
  if(log_forward && log_forward->write_r) log_forward->write_r(r,fd,p,n);
  mutexUnlock(&log_mutex);
  return n;
}
static const devoptab_t log_device={.name="boznativelog",.structSize=0,.write_r=log_write};
void native_log_start(void) {
  /* Record the actual forwarder title, so updating never requires guessing
   * which Atmosphere contents folder belongs to this port. */
  u64 title = 0;
  if (R_SUCCEEDED(svcGetInfo(&title, InfoType_ProgramId, CUR_PROCESS_HANDLE, 0))) {
    FILE *id = fopen("sdmc:" PORT_ROOT_PATH "/title_id.txt", "wb");
    if (id) { fprintf(id, "%016llX\n", (unsigned long long)title); fclose(id); }
  }
  log_file=fopen("sdmc:" PORT_ROOT_PATH "/debug.log","wb");
  log_forward=devoptab_list[STD_OUT];
  devoptab_list[STD_OUT]=&log_device; devoptab_list[STD_ERR]=&log_device;
}
void native_log_drop_console(int keep_network) { if(!keep_network)log_forward=NULL; }
void native_log_flush(void) {
  mutexLock(&log_mutex);
  if(log_file && log_used) { fwrite(log_buffer,1,log_used,log_file); fflush(log_file); }
  log_used=0; mutexUnlock(&log_mutex);
}
void debugPrintf(const char *fmt, ...) {
  char buf[1024]; va_list ap; va_start(ap,fmt);
  int n=vsnprintf(buf,sizeof buf,fmt,ap); va_end(ap);
  if(n>0) { if(n>=(int)sizeof buf)n=sizeof buf-1; svcOutputDebugString(buf,n); }
  printf("%s",buf);
}
/* Write through the FS service; stdio/malloc locks may be held at a crash. */
Result __libnx_exception_handler32(u32 type,ThreadExceptionInfo32 *i,ThreadExceptionFrame32 *f) {
  static char dump[8192];
  int n=snprintf(dump,sizeof dump,
    "BOZ native ARM32 exception=%x pc=%08x lr=%08x sp=%08x far=%08x esr=%08x cpsr=%08x\n",
    type,i->pc,i->lr,i->sp,i->far,i->esr,i->pstate);
  for(unsigned k=0;k<13;k++) n+=snprintf(dump+n,sizeof dump-n,"r%u=%08x\n",k,k<8?i->r[k]:f->r8_12[k-8]);
  /* No mutex acquisition in exception context: include a best-effort log tail. */
  size_t used=log_used < sizeof log_buffer ? log_used : sizeof log_buffer;
  size_t tail=used<4096?used:4096;
  if(tail && n+tail<sizeof dump) { memcpy(dump+n,log_buffer+used-tail,tail); n+=tail; }
  svcOutputDebugString(dump,n);
  FsFileSystem *sd=fsdevGetDeviceFileSystem("sdmc"); FsFile out;
  const char *path=PORT_ROOT_PATH "/crash.log";
  if(sd) {
    fsFsDeleteFile(sd,path); fsFsCreateFile(sd,path,n,0);
    if(R_SUCCEEDED(fsFsOpenFile(sd,path,FsOpenMode_Write,&out))) {
      fsFileWrite(&out,0,dump,n,FsWriteOption_Flush); fsFileClose(&out);
    }
  }
  return MAKERESULT(Module_Libnx,LibnxError_BadInput);
}
