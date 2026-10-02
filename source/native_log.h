#ifndef BOZ_NATIVE_LOG_H
#define BOZ_NATIVE_LOG_H
void native_log_start(void);
void native_log_drop_console(int keep_network);
void native_log_flush(void);
#endif
