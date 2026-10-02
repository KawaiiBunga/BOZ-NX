#ifndef BOZ_NATIVE_EXEC_H
#define BOZ_NATIVE_EXEC_H
#include "guest.h"
uint32_t native_original_hook(unsigned hook);
#include "s3e_loader.h"
int native_reserve_image(uint32_t bytes);
uint32_t native_image_base(void);
int native_stage_image(S3eImage *image);
int native_prepare(Guest *g, S3eImage *image);
void native_enter(Guest *g);
void native_request_exit(void);
uint32_t native_stub_address(unsigned slot);
void native_dispatch(GuestCpu *frame, unsigned slot);
void native_diag(void);
void native_counters(uint64_t *instructions, uint64_t *translated, uint64_t *calls);
#endif
