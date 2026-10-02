/* mDNS is excluded from the socket regression test, not replaced in the port. */
#include "zeroconf_host.h"
void *s3eZeroConfStartSearch(const char *type,const char *domain,
    s3e_zeroconf_callback_fn found,s3e_zeroconf_callback_fn update,
    s3e_zeroconf_callback_fn lost,void *user) {
    (void)type;(void)domain;(void)found;(void)update;(void)lost;(void)user;return NULL;
}
void s3eZeroConfStopSearch(void *search) { (void)search; }
void *s3eZeroConfPublish(uint16_t port,const char *name,const char *type,
    const char *domain,uint16_t count,const char **txt) {
    (void)port;(void)name;(void)type;(void)domain;(void)count;(void)txt;return NULL;
}
int32_t s3eZeroConfUpdateTxtRecord(void *service,uint16_t count,const char **txt) {
    (void)service;(void)count;(void)txt;return -1;
}
int32_t s3eZeroConfUnpublish(void *service) { (void)service;return -1; }
void s3e_zero_conf_pump(void) {}
int zc_hostname(char *out,size_t size) { if(size) out[0]=0; return 0; }
int zc_pid(void) { return 1; }
