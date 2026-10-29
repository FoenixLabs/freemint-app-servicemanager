/* Non-interactive validation/hostname helper; never accepts passwords. */
#include "service.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv) {
    struct svc_config c;char e[160];
    if(argc==2 && !strcmp(argv[1],"--address")){char ip[16];if(!svc_current_address(ip,sizeof(ip)))return 1;puts(ip);return 0;}
    if(argc==2 && !strcmp(argv[1],"--hostname"))return svc_hostname();
    if(argc==2 && !strcmp(argv[1],"--validate")){
        if(!svc_load(&c,e,sizeof(e))){fprintf(stderr,"%s\n",e);return 1;}return 0;
    }
    fputs("Usage: SVCCTL.TTP --validate | --hostname | --address\n",stderr);return 2;
}
