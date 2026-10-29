/* A2560Me ServiceManager. GPL-2.0-or-later. */
#ifndef SERVICE_H
#define SERVICE_H
#include <stddef.h>
#define SVC_PASSWORD_MIN 8
#define SVC_PASSWORD_MAX 128
enum svc_password_result { SVC_PASSWORD_OK, SVC_PASSWORD_LENGTH, SVC_PASSWORD_MISMATCH };
int svc_password_check(const char *password,const char *confirm,char *error,size_t size);
struct svc_config {
    int network, dhcp, uart, ftp;
    char hostname[64], address[16], netmask[16], gateway[16], dns[16], username[32];
};
int svc_load(struct svc_config *c, char *error, size_t size);
int svc_validate(const struct svc_config *c, char *error, size_t size);
int svc_save(const struct svc_config *c,const char *password,const char *confirm,char *error,size_t size);
int svc_scope(const struct svc_config *before,const struct svc_config *after,int password_changed);
int svc_hostname(void);
int svc_current_address(char *address,size_t size);
#endif
