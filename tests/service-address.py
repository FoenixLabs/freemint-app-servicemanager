#!/usr/bin/env python3
"""Exercise production interface-query code, including MiNT's AF_INET requirement."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1];SRC=ROOT
code=r'''
#include "service.h"
#include <assert.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <string.h>
static int up=1,empty;
int __wrap_socket(int domain,int type,int protocol){assert(domain==AF_INET && type==SOCK_DGRAM && protocol==0);return 99;}
int __wrap_close(int fd){assert(fd==99);return 0;}
int __wrap_ioctl(int fd,unsigned long command,...) {
 struct ifreq *r;va_list ap;assert(fd==99);va_start(ap,command);r=va_arg(ap,struct ifreq*);va_end(ap);
 assert(!strcmp(r->ifr_name,"en0"));
 if(command==SIOCGIFFLAGS){r->ifr_flags=up?IFF_UP:0;return 0;}
 assert(command==SIOCGIFADDR);assert(r->ifr_addr.sa_family==AF_INET);
 ((struct sockaddr_in*)&r->ifr_addr)->sin_addr.s_addr=empty?0:inet_addr("192.168.1.254");return 0;
}
int main(void){char ip[16];assert(svc_current_address(ip,sizeof(ip)) && !strcmp(ip,"192.168.1.254"));up=0;assert(!svc_current_address(ip,sizeof(ip)) && !*ip);up=1;empty=1;assert(!svc_current_address(ip,sizeof(ip)) && !*ip);return 0;}
'''
with tempfile.TemporaryDirectory() as t:
 d=Path(t);(d/'test.c').write_text(code)
 subprocess.run(['cc','-O2','-I'+str(SRC),str(d/'test.c'),str(SRC/'service.c'),'-lcrypt','-Wl,--wrap=socket,--wrap=close,--wrap=ioctl','-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)
print('PASS: live en0 query sets AF_INET, displays its IPv4 address and clears stale addresses when down/unassigned')
