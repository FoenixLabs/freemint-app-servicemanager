/* Configuration/account transactions; never store plaintext passwords. GPL-2.0-or-later. */
#include "service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <crypt.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifndef SERVICE_ROOT
#define SERVICE_ROOT ""
#endif
#define CONFIG SERVICE_ROOT "/etc/a2560me-services.conf"
#define PASSWD SERVICE_ROOT "/etc/passwd"
#define OWNER "FTP file maintenance"
static int fail(char *e,size_t n,const char *text) { snprintf(e,n,"%s",text);return 0; }
static int ipv4(const char *s,unsigned long *value) {
    unsigned a,b,c,d;char tail;
    if(sscanf(s,"%u.%u.%u.%u%c",&a,&b,&c,&d,&tail)!=4 || a>255 || b>255 || c>255 || d>255)return 0;
    { const char *p=s;for(;*p;p++)if(!isdigit((unsigned char)*p) && *p!='.')return 0; }
    *value=((unsigned long)a<<24)|((unsigned long)b<<16)|((unsigned long)c<<8)|d;return 1;
}
int svc_validate(const struct svc_config *c,char *e,size_t n) {
    unsigned long ip,mask,gw,tmp;size_t i,l=strlen(c->hostname);
    if((c->network&~1)||(c->dhcp&~1)||(c->uart&~1)||(c->ftp&~1))return fail(e,n,"Invalid service switch.");
    if(!l || l>63 || !isalnum((unsigned char)c->hostname[0]) || !isalnum((unsigned char)c->hostname[l-1]))return fail(e,n,"Hostname must be 1-63 letters/digits/hyphens.");
    for(i=0;i<l;i++)if(!isalnum((unsigned char)c->hostname[i]) && c->hostname[i]!='-')return fail(e,n,"Invalid hostname.");
    l=strlen(c->username);
    if(!l || l>31 || !islower((unsigned char)c->username[0]))return fail(e,n,"FTP username must start with a lowercase letter.");
    for(i=0;i<l;i++)if(!islower((unsigned char)c->username[i]) && !isdigit((unsigned char)c->username[i]) && c->username[i]!='_' && c->username[i]!='-')return fail(e,n,"Invalid FTP username.");
    if(!strcmp(c->username,"root") || !strcmp(c->username,"nobody") || !strcmp(c->username,"ftp") || !strcmp(c->username,"anonymous"))return fail(e,n,"Choose a dedicated FTP username.");
    if(!c->dhcp) {
        if(!ipv4(c->address,&ip) || !ip || (ip>>24)>=224 || (ip>>24)==127)return fail(e,n,"Enter a usable static IPv4 address.");
        if(!ipv4(c->netmask,&mask) || !mask || !(~mask & 0xffffffffUL) || ((~mask & 0xffffffffUL)&((~mask & 0xffffffffUL)+1)))return fail(e,n,"Enter a contiguous IPv4 netmask.");
        if(!(ip & ~mask) || (ip & ~mask)==(~mask & 0xffffffffUL))return fail(e,n,"Address is the subnet or broadcast address.");
        if(c->gateway[0] && (!ipv4(c->gateway,&gw) || !gw || (ip&mask)!=(gw&mask) || !(gw&~mask) || (gw&~mask)==(~mask&0xffffffffUL) || gw==ip))return fail(e,n,"Gateway must be another host on this subnet.");
        if(c->dns[0] && (!ipv4(c->dns,&tmp) || !tmp || (tmp>>24)>=224))return fail(e,n,"Enter an IPv4 DNS server, or leave blank.");
    } else {
        const char *v[]={c->address,c->netmask,c->gateway,c->dns};
        for(i=0;i<4;i++)if(v[i][0] && !ipv4(v[i],&tmp))return fail(e,n,"Saved static addresses must be valid IPv4.");
    }
    return 1;
}
int svc_load(struct svc_config *c,char *e,size_t n) {
    FILE *f;char line[256];unsigned seen=0;
    memset(c,0,sizeof(*c));c->network=1;c->dhcp=1;
    strcpy(c->hostname,"a2560me");strcpy(c->netmask,"255.255.255.0");strcpy(c->username,"transfer");
    c->network=access(SERVICE_ROOT "/d/mint/net/disabled",F_OK)!=0;
    f=fopen(CONFIG,"r");
    if(!f) {
        if(errno!=ENOENT)return fail(e,n,"Cannot read service settings.");
        f=fopen(PASSWD,"r");if(f) { while(fgets(line,sizeof(line),f))if(!strncmp(line,"transfer:",9) && line[9]!='*' && line[9]!='!' && line[9]!=':')c->ftp=1;fclose(f); }
        if(access(SERVICE_ROOT "/d/mint/net/ftp-disabled",F_OK)==0)c->ftp=0;
        return 1;
    }
    while(fgets(line,sizeof(line),f)) {
        char *value,*end;unsigned bit=0;int *flag=NULL;char *str=NULL;size_t cap=0;
        if(!strchr(line,'\n') && !feof(f)){fclose(f);return fail(e,n,"Service settings line too long.");}
        line[strcspn(line,"\r\n")]=0;if(!*line || *line=='#')continue;
        value=strchr(line,'=');if(!value){fclose(f);return fail(e,n,"Invalid service settings.");}*value++=0;
#define FIELD(key,index,member) if(!strcmp(line,key)){bit=1U<<index;str=c->member;cap=sizeof(c->member);}
        FIELD("HOSTNAME",0,hostname) else FIELD("ADDRESS",1,address) else FIELD("NETMASK",2,netmask) else FIELD("GATEWAY",3,gateway) else FIELD("DNS",4,dns) else FIELD("FTP_USER",5,username)
        else if(!strcmp(line,"NETWORK")){bit=1U<<6;flag=&c->network;}
        else if(!strcmp(line,"UART")){bit=1U<<7;flag=&c->uart;}
        else if(!strcmp(line,"FTP")){bit=1U<<8;flag=&c->ftp;}
        else if(!strcmp(line,"MODE")){bit=1U<<9;if(strcmp(value,"dhcp") && strcmp(value,"static")){fclose(f);return fail(e,n,"Invalid address mode.");}c->dhcp=!strcmp(value,"dhcp");}
        else {fclose(f);return fail(e,n,"Unknown service setting.");}
#undef FIELD
        if(seen&bit){fclose(f);return fail(e,n,"Duplicate service setting.");}seen|=bit;
        if(flag){long v=strtol(value,&end,10);if(*end || !*value || (v!=0 && v!=1)){fclose(f);return fail(e,n,"Invalid switch.");}*flag=(int)v;}
        if(str){if(strlen(value)>=cap){fclose(f);return fail(e,n,"Service setting too long.");}strcpy(str,value);}
    }
    if(ferror(f)){fclose(f);return fail(e,n,"Cannot read service settings.");}fclose(f);
    return svc_validate(c,e,n);
}
static FILE *exclusive(const char *path) {
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);FILE *f;
    if(fd<0)return NULL;
    f=fdopen(fd,"w");if(!f){close(fd);unlink(path);}return f;
}
static int finish(FILE *f) {
    int ok=!ferror(f);if(fflush(f))ok=0;if(fsync(fileno(f)))ok=0;if(fclose(f))ok=0;return ok;
}
/* FAT-safe transaction: retain originals until BOTH replacements are installed. */
static int commit(int accounts,char *e,size_t n) {
    const char *paths[]={PASSWD,CONFIG};const char *news[]={PASSWD ".svc-new",CONFIG ".svc-new"};
    const char *olds[]={PASSWD ".svc-old",CONFIG ".svc-old"};int exists[2]={0,0},installed[2]={0,0},i,start=accounts?0:1;
    for(i=start;i<2;i++)if(access(olds[i],F_OK)==0)return fail(e,n,"Recover existing .svc-old backup before saving.");
    for(i=start;i<2;i++) {
        exists[i]=access(paths[i],F_OK)==0;
        if(exists[i] && rename(paths[i],olds[i]))goto rollback;
#ifdef SERVICE_TEST_FAIL_CONFIG
        if(i==1){errno=EIO;goto rollback;}
#endif
        if(rename(news[i],paths[i]))goto rollback;
        installed[i]=1;
    }
    for(i=start;i<2;i++)if(exists[i])unlink(olds[i]);return 1;
rollback:
    { int restore=1,j;for(j=i;j>=start;j--) {
        if(installed[j] && unlink(paths[j]))restore=0;
        if(exists[j] && access(olds[j],F_OK)==0 && rename(olds[j],paths[j]))restore=0;
      }
      return fail(e,n,restore?"Settings were not saved; previous files restored.":"Restore .svc-old backups before continuing."); }
}
int svc_password_check(const char *password,const char *confirm,char *e,size_t n) {
    size_t length=strlen(password);
    if(length && (length<SVC_PASSWORD_MIN || length>SVC_PASSWORD_MAX)) {
        snprintf(e,n,"Use a password of %d-%d characters.",SVC_PASSWORD_MIN,SVC_PASSWORD_MAX);
        return SVC_PASSWORD_LENGTH;
    }
    if(strcmp(password,confirm)) {
        fail(e,n,"The two passwords do not match.");return SVC_PASSWORD_MISMATCH;
    }
    return SVC_PASSWORD_OK;
}
int svc_save(const struct svc_config *c,const char *password,const char *confirm,char *e,size_t n) {
    struct svc_config old;FILE *in=NULL,*out=NULL;char line[2048],hash[256]="",salt[24];int found=0,accounts=0,ok=0,lockfd=-1,pnew=0,cnew=0;struct flock lock;
    if(geteuid()!=0
#ifdef SERVICE_HOST_TEST
       && !getenv("SERVICE_HOST_TEST")
#endif
       )return fail(e,n,"Run ServiceManager as root.");
    if(!svc_validate(c,e,n))return 0;
    if(svc_password_check(password,confirm,e,n)!=SVC_PASSWORD_OK)return 0;
    lockfd=open(SERVICE_ROOT "/etc/services.lock",O_RDWR|O_CREAT,0600);memset(&lock,0,sizeof(lock));lock.l_type=F_WRLCK;lock.l_whence=SEEK_SET;
    if(lockfd<0 || fcntl(lockfd,F_SETLK,&lock)<0){if(lockfd>=0)close(lockfd);return fail(e,n,"Another settings update is running.");}
    if(!svc_load(&old,e,n))goto done;
    if(access(PASSWD ".svc-old",F_OK)==0 || access(CONFIG ".svc-old",F_OK)==0){fail(e,n,"Recover .svc-old backups before saving.");goto done;}
    in=fopen(PASSWD,"r");if(!in){fail(e,n,"Cannot read account database.");goto done;}
    while(fgets(line,sizeof(line),in)) {
        char copy[2048],*name,*encoded,*uid,*gid,*gecos;int owned;
        if(!strchr(line,'\n') && !feof(in)){fail(e,n,"Account database line too long.");goto done;}
        strcpy(copy,line);
        { char *parts[7],*p=copy;int i;
          for(i=0;i<7;i++){char *sep;parts[i]=p;sep=strchr(p,':');if(i<6){if(!sep){fail(e,n,"Invalid account database.");goto done;}*sep=0;p=sep+1;}}
          name=parts[0];encoded=parts[1];uid=parts[2];gid=parts[3];gecos=parts[4];
          if(!*name || !*uid || !*gid){fail(e,n,"Invalid account database.");goto done;}
        }
        owned=!strcmp(name,old.username) && !strcmp(uid,"0") && !strcmp(gecos,OWNER);
        if(!strcmp(name,c->username) && !owned){fail(e,n,"That username belongs to another account.");goto done;}
        if(owned){if(found++){fail(e,n,"Duplicate managed FTP account.");goto done;}if(strlen(encoded)>=sizeof(hash)){fail(e,n,"Password hash too long.");goto done;}strcpy(hash,encoded);}
    }
    if(ferror(in)){fail(e,n,"Cannot read account database.");goto done;}
    fclose(in);in=NULL;
    if(*password) {
        struct timeval now;char *encoded;gettimeofday(&now,NULL);
        snprintf(salt,sizeof(salt),"$1$%08lx$",(unsigned long)(now.tv_usec^now.tv_sec^((long)getpid()<<16))&0xffffffffUL);
        encoded=crypt(password,salt);if(!encoded || strncmp(encoded,"$1$",3) || strlen(encoded)>=sizeof(hash)){fail(e,n,"Password hashing failed.");goto done;}strcpy(hash,encoded);
    }
    if(c->ftp && (!*hash || *hash=='*' || *hash=='!')){fail(e,n,"Set a password before enabling FTP.");goto done;}
    accounts=!!*password || (found && strcmp(old.username,c->username));
    if(accounts) {
        in=fopen(PASSWD,"r");out=exclusive(PASSWD ".svc-new");if(out)pnew=1;if(!in || !out){fail(e,n,"Cannot create account update.");goto done;}
        while(fgets(line,sizeof(line),in)) {
            size_t len=strlen(old.username);
            if(!strncmp(line,old.username,len) && line[len]==':' && found)continue;
            fputs(line,out);if(!strchr(line,'\n'))fputc('\n',out);
        }
        if(ferror(in)){fail(e,n,"Cannot read account database.");goto done;}
        fclose(in);in=NULL;
        fprintf(out,"%s:%s:0:0:" OWNER ":/:/bin/bash\n",c->username,hash);
        { FILE *f=out;out=NULL;if(!finish(f)){fail(e,n,"Cannot flush account update.");goto done;} }
    }
    out=exclusive(CONFIG ".svc-new");if(out)cnew=1;if(!out){fail(e,n,"Cannot create settings update.");goto done;}
    fprintf(out,"# A2560Me ServiceManager - no plaintext passwords\nNETWORK=%d\nHOSTNAME=%s\nMODE=%s\nADDRESS=%s\nNETMASK=%s\nGATEWAY=%s\nDNS=%s\nUART=%d\nFTP=%d\nFTP_USER=%s\n",c->network,c->hostname,c->dhcp?"dhcp":"static",c->address,c->netmask,c->gateway,c->dns,c->uart,c->ftp,c->username);
    {FILE *f=out;out=NULL;if(!finish(f)){fail(e,n,"Cannot flush settings update.");goto done;} }
    ok=commit(accounts,e,n);
done:
    if(in)fclose(in);if(out)fclose(out);
    if(pnew)unlink(PASSWD ".svc-new");
    if(cnew)unlink(CONFIG ".svc-new");
    /* Never remove an unrelated pre-existing temporary file. Only remove files
     * we created: exclusive-open failures leave recovery files for inspection. */
    if(lockfd>=0)close(lockfd);
    memset(hash,0,sizeof(hash));return ok;
}
int svc_hostname(void) {
    struct svc_config c;char e[128];if(!svc_load(&c,e,sizeof(e)))return 1;
    return sethostname(c.hostname,strlen(c.hostname))?1:0;
}

int svc_current_address(char *address,size_t size) {
    struct ifreq iface;struct sockaddr_in *inet;int fd,ok=0;
    if(!size)return 0;*address=0;
    fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return 0;
    memset(&iface,0,sizeof(iface));strcpy(iface.ifr_name,"en0");
    if(ioctl(fd,SIOCGIFFLAGS,&iface)>=0 && (iface.ifr_flags&IFF_UP)) {
        memset(&iface,0,sizeof(iface));strcpy(iface.ifr_name,"en0");iface.ifr_addr.sa_family=AF_INET;
        if(ioctl(fd,SIOCGIFADDR,&iface)>=0) {
            inet=(struct sockaddr_in *)&iface.ifr_addr;
            if(inet->sin_addr.s_addr && inet->sin_addr.s_addr!=0xffffffffUL) {
                snprintf(address,size,"%s",inet_ntoa(inet->sin_addr));ok=1;
            }
        }
    }
    close(fd);return ok;
}

/* 2: network restart; 1: sharing restart; 0: UART-only update. */
int svc_scope(const struct svc_config *a,const struct svc_config *b,int password_changed) {
    if(a->network!=b->network || a->dhcp!=b->dhcp || strcmp(a->hostname,b->hostname) ||
       strcmp(a->address,b->address) || strcmp(a->netmask,b->netmask) ||
       strcmp(a->gateway,b->gateway) || strcmp(a->dns,b->dns))return 2;
    if(a->ftp!=b->ftp || strcmp(a->username,b->username) || password_changed)return 1;
    return 0;
}
