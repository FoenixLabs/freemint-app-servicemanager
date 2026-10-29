/* Modeless GEM ServiceManager. GPL-2.0-or-later. */
#include <gem.h>
#include "service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#define WIDTH 568
#define HEIGHT 382
static short win=-1,vh,planes;static GRECT work;
static struct svc_config cfg;static int tab,focus=-1,cursor,dirty=1,running=1,down;
static char password[SVC_PASSWORD_MAX+1],confirm[SVC_PASSWORD_MAX+1],notice[160]="Ready. Changes apply now and at the next boot.";
static pid_t worker=-1;
static char current_ip[16];static unsigned ip_ticks;static int apply_failed;
struct field {const char *label;char *text;int cap,y,secret;};
static struct field fields[6];static int count;
static void fields_for_tab(void) {
    memset(fields,0,sizeof(fields));count=0;
#define ADD(label,member,row) fields[count++]=(struct field){label,cfg.member,sizeof(cfg.member),row,0}
    if(!tab) {
        ADD("Hostname",hostname,98);ADD("IPv4 address",address,174);ADD("Subnet mask",netmask,204);
        ADD("Gateway",gateway,234);ADD("DNS server",dns,264);
    } else {
        ADD("FTP username",username,168);
        fields[count++]=(struct field){"New password",password,sizeof(password),204,1};
        fields[count++]=(struct field){"Repeat password",confirm,sizeof(confirm),240,1};
    }
#undef ADD
    focus=-1;cursor=0;
}
static void box(int x,int y,int w,int h,int c) {
    short p[4]={work.g_x+x,work.g_y+y,work.g_x+x+w-1,work.g_y+y+h-1};
    vsf_interior(vh,FIS_SOLID);vsf_color(vh,planes==1?(c==8?0:c):c);vsf_perimeter(vh,0);v_bar(vh,p);
}
static void text(int x,int y,const char *s) {vswr_mode(vh,MD_TRANS);vst_color(vh,1);v_gtext(vh,work.g_x+x,work.g_y+y+13,s);vswr_mode(vh,MD_REPLACE);}
static void disabled_text(int x,int y,const char *s) {
    vswr_mode(vh,MD_TRANS);vst_color(vh,planes==1?1:9);v_gtext(vh,work.g_x+x,work.g_y+y+13,s);vswr_mode(vh,MD_REPLACE);
}
static void button(int x,int y,int w,const char *s,int pressed) {
    box(x,y,w,25,8);box(x,y,w,1,pressed?1:0);box(x,y,1,25,pressed?1:0);box(x,y+24,w,1,pressed?0:1);box(x+w-1,y,1,25,pressed?0:1);text(x+10,y+4,s);
}
static void check(int x,int y,const char *s,int on) {
    box(x,y,16,16,1);box(x+1,y+1,14,14,0);if(on)text(x+4,y,"x");text(x+24,y,s);
}
static int editable(int i) {return tab || i==0 || !cfg.dhcp;}
static void draw(void) {
    int i;char s[64];box(0,0,WIDTH,HEIGHT,8);
    button(12,10,150,"Networking",tab==0);button(166,10,150,"Sharing",tab==1);
    if(!tab) {
        check(20,60,"Enable Ethernet networking",cfg.network);
        button(20,135,170,cfg.dhcp?"(x) DHCP":"( ) DHCP",cfg.dhcp);
        button(205,135,170,cfg.dhcp?"( ) Static IPv4":"(x) Static IPv4",!cfg.dhcp);
        text(20,298,"Static settings include optional gateway and DNS.");
    } else {
        check(20,60,"Enable background UART file transfer",cfg.uart);
        text(44,84,"COM3, 115200 baud; stays on when this app closes.");
        check(20,124,"Enable FTP file sharing",cfg.ftp);
        text(20,284,"Blank passwords keep the current password (8-128 to change).");
        text(20,302,"FTP/UART can write C: and D:. Use trusted connections.");
    }
    for(i=0;i<count;i++) {
        struct field *f=&fields[i];const char *value=f->text,*label=f->label;size_t len;int start=0,j;
        if(!tab && cfg.dhcp && i==1){value=*current_ip?current_ip:"Waiting for DHCP address";label="Current DHCP IP";}
        len=strlen(value);
        if(editable(i))text(20,f->y+3,label);else disabled_text(20,f->y+3,label);box(174,f->y,370,24,1);box(175,f->y+1,368,22,editable(i)?0:8);
        if(i==focus && cursor>42)start=cursor-42;
        for(j=0;j<43 && start+j<(int)len;j++)s[j]=f->secret?'*':value[start+j];s[j]=0;
        if(editable(i) || (!tab && cfg.dhcp && i==1))text(180,f->y+4,s);else disabled_text(180,f->y+4,s);
        if(i==focus)box(180+(cursor-start)*8,f->y+19,8,1,1);
    }
    box(12,326,544,22,0);snprintf(s,sizeof(s),"%.66s",notice);text(16,329,s);
    button(20,352,170,worker>0?"Applying...":"Save and apply",worker>0);button(210,352,140,"Reload",0);button(414,352,130,"Close",0);
}
static void redraw(GRECT *area) {
    GRECT r;short clip[4];wind_update(BEG_UPDATE);graf_mouse(M_OFF,NULL);
    wind_get(win,WF_FIRSTXYWH,&r.g_x,&r.g_y,&r.g_w,&r.g_h);
    while(r.g_w && r.g_h) {
        int x=r.g_x>area->g_x?r.g_x:area->g_x,y=r.g_y>area->g_y?r.g_y:area->g_y;
        int right=r.g_x+r.g_w<area->g_x+area->g_w?r.g_x+r.g_w:area->g_x+area->g_w;
        int bottom=r.g_y+r.g_h<area->g_y+area->g_h?r.g_y+r.g_h:area->g_y+area->g_h;
        if(right>x && bottom>y){clip[0]=x;clip[1]=y;clip[2]=right-1;clip[3]=bottom-1;vs_clip(vh,1,clip);draw();}
        wind_get(win,WF_NEXTXYWH,&r.g_x,&r.g_y,&r.g_w,&r.g_h);
    }
    vs_clip(vh,0,clip);graf_mouse(M_ON,NULL);wind_update(END_UPDATE);dirty=0;
}
static void wipe(void) {volatile char *p=password,*q=confirm;size_t i;for(i=0;i<sizeof(password);i++){p[i]=0;q[i]=0;}}
static void password_message(const char *message,int which) {
    char alert[256];
    tab=1;fields_for_tab();focus=which;cursor=strlen(fields[focus].text);
    snprintf(notice,sizeof(notice),"%s",message);
    snprintf(alert,sizeof(alert),"[1][Password was not changed.|%s][OK]",message);
    form_alert(1,alert);dirty=1;
}
static void save_message(const char *message) {
    char alert[256];snprintf(notice,sizeof(notice),"%s",message);
    snprintf(alert,sizeof(alert),"[1][Could not complete the save.|%s][OK]",message);
    form_alert(1,alert);dirty=1;
}
static void apply(void) {
    char error[160];struct svc_config before;int scope,check;const char *action;
    if(worker>0)return;
    check=svc_password_check(password,confirm,error,sizeof(error));
    if(check!=SVC_PASSWORD_OK){password_message(error,check==SVC_PASSWORD_LENGTH || !*password?1:2);return;}
    if(!svc_validate(&cfg,error,sizeof(error))){save_message(error);return;}
    if(!svc_load(&before,error,sizeof(error))){save_message(error);return;}
    scope=svc_scope(&before,&cfg,!!*password);
    if(apply_failed || (cfg.network && !*current_ip))scope=2;
    action=scope==2?"--apply":scope==1?"--sharing":"--uart";
    if(scope && form_alert(2,"[2][Apply service settings now?|Active FTP sessions will disconnect.][Apply|Cancel]")!=1)return;
    if(!svc_save(&cfg,password,confirm,error,sizeof(error))){save_message(error);return;}
    wipe();worker=fork();
    if(worker==0) {
        int fd=open("/var/log/services-apply.log",O_WRONLY|O_CREAT|O_TRUNC,0600);int input=open("/dev/null",O_RDONLY);
        if(input>=0){dup2(input,0);if(input>2)close(input);}if(fd>=0){dup2(fd,1);dup2(fd,2);if(fd>2)close(fd);}
        execl("/bin/bash","bash","/d/mint/net/SERVICES.SH",action,(char *)NULL);_exit(127);
    }
    if(worker<0)strcpy(notice,"Saved; could not apply. Reboot to apply settings.");
    else strcpy(notice,"Saved. Applying services in the background...");
    focus=-1;dirty=1;
}
static void click(int mx,int my) {
    int x=mx-work.g_x,y=my-work.g_y,i;if(x<0 || y<0 || x>=WIDTH || y>=HEIGHT)return;
    if(worker>0 && y<352)return;
    if(y>=10 && y<35 && x>=12 && x<316){tab=x>=166;fields_for_tab();dirty=1;return;}
    if(y>=352){if(x>=20 && x<190)apply();else if(x>=210 && x<350 && worker<=0){char e[160];struct svc_config next;if(svc_load(&next,e,sizeof(e))){cfg=next;wipe();fields_for_tab();strcpy(notice,"Saved settings reloaded.");}else snprintf(notice,sizeof(notice),"%s",e);dirty=1;}else if(x>=414 && x<544)running=0;return;}
    if(!tab){if(y>=60 && y<80)cfg.network=!cfg.network;else if(y>=135 && y<160 && x<375){cfg.dhcp=x<200;focus=-1;}}
    else {if(y>=60 && y<80)cfg.uart=!cfg.uart;else if(y>=124 && y<144)cfg.ftp=!cfg.ftp;}
    for(i=0;i<count;i++)if(x>=174 && x<544 && y>=fields[i].y && y<fields[i].y+24 && editable(i)){focus=i;cursor=strlen(fields[i].text);}
    dirty=1;
}
static void keypress(unsigned key,int shift) {
    int ch=key&255,scan=key>>8;char *s;int len;
    if(worker>0)return;
    if(ch==27){running=0;return;}
    if(ch==9){int i;for(i=0;i<count;i++){if(focus<0)focus=(shift&3)?count-1:0;else focus=(focus+(shift&3?count-1:1)+count)%count;if(editable(focus))break;}cursor=strlen(fields[focus].text);dirty=1;return;}
    if(ch==13){apply();return;}
    if(focus<0 || !editable(focus))return;
    s=fields[focus].text;len=strlen(s);
    if(scan==0x4b && cursor>0)--cursor;
    else if(scan==0x4d && cursor<len)++cursor;
    else if(scan==0x47)cursor=0;
    else if(scan==0x4f)cursor=len;
    else if(ch==8 && cursor>0){memmove(s+cursor-1,s+cursor,len-cursor+1);--cursor;}
    else if(ch==127 && cursor<len)memmove(s+cursor,s+cursor+1,len-cursor);
    else if(ch>=32 && ch<=126 && len+1<fields[focus].cap){memmove(s+cursor+1,s+cursor,len-cursor+1);s[cursor++]=ch;}
    else if(ch>=32 && ch<=126 && fields[focus].secret && len+1>=fields[focus].cap)
        password_message("Maximum length is 128 characters.|Extra character was not entered.",focus);
    dirty=1;
}
int main(void) {
    short app,input[11],out[57],ext[57],i,msg[8],mx,my,buttons,shift,key,clicks,dx,dy,dw,dh,x,y,w,h;
    char error[160];struct flock lock;int lockfd;
    app=appl_init();if(app<0)return 1;menu_register(app,"  ServiceManager");
    lockfd=open("/var/run/servicemanager.lock",O_RDWR|O_CREAT,0600);memset(&lock,0,sizeof(lock));lock.l_type=F_WRLCK;lock.l_whence=SEEK_SET;
    if(lockfd<0 || fcntl(lockfd,F_SETLK,&lock)<0){form_alert(1,"[1][ServiceManager is already running|or /var/run is not writable.][OK]");if(lockfd>=0)close(lockfd);appl_exit();return 1;}
    fcntl(lockfd,F_SETFD,FD_CLOEXEC);
    if(!svc_load(&cfg,error,sizeof(error))){form_alert(1,"[1][Cannot load service settings.|Check /etc/a2560me-services.conf.][OK]");close(lockfd);appl_exit();return 1;}
    fields_for_tab();svc_current_address(current_ip,sizeof(current_ip));vh=graf_handle(&i,&i,&i,&i);for(i=0;i<10;i++)input[i]=1;input[10]=2;v_opnvwk(input,&vh,out);vq_extnd(vh,1,ext);planes=ext[4];
    wind_get(0,WF_WORKXYWH,&dx,&dy,&dw,&dh);wind_calc(WC_BORDER,NAME|CLOSER|MOVER,0,0,WIDTH,HEIGHT,&x,&y,&w,&h);x=dx+(dw-w)/2;y=dy+(dh-h)/2;
    win=wind_create(NAME|CLOSER|MOVER,dx,dy,dw,dh);if(win<0)running=0;else{wind_set_str(win,WF_NAME,"ServiceManager");wind_open(win,x,y,w,h);wind_get(win,WF_WORKXYWH,&work.g_x,&work.g_y,&work.g_w,&work.g_h);}
    while(running) {
        int event,status;if(worker>0 && waitpid(worker,&status,WNOHANG)==worker){worker=-1;apply_failed=!(WIFEXITED(status)&&WEXITSTATUS(status)==0);strcpy(notice,!apply_failed?"Services applied. Settings will persist after reboot.":"Saved; apply failed. See /var/log/services-apply.log.");dirty=1;}
        if(++ip_ticks>=8){char ip[16];ip_ticks=0;svc_current_address(ip,sizeof(ip));if(strcmp(ip,current_ip)){strcpy(current_ip,ip);if(!tab && cfg.dhcp)dirty=1;}}
        if(dirty)redraw(&work);
        event=evnt_multi(MU_MESAG|MU_BUTTON|MU_KEYBD|MU_TIMER,1,1,down?0:1,0,0,0,0,0,0,0,0,0,0,msg,250,&mx,&my,&buttons,&shift,&key,&clicks);
        if(event&MU_MESAG){if(msg[0]==AC_OPEN && win>=0){wind_set(win,WF_TOP,0,0,0,0);dirty=1;}if(msg[0]==AP_TERM)running=0;if(msg[3]==win)switch(msg[0]){
            case WM_CLOSED:running=0;break;
            case WM_TOPPED:wind_set(win,WF_TOP,0,0,0,0);break;
            case WM_BOTTOMED:wind_set(win,WF_BOTTOM,0,0,0,0);break;
            case WM_REDRAW:{GRECT r={msg[4],msg[5],msg[6],msg[7]};redraw(&r);break;}
            case WM_MOVED:wind_set(win,WF_CURRXYWH,msg[4],msg[5],msg[6],msg[7]);wind_get(win,WF_WORKXYWH,&work.g_x,&work.g_y,&work.g_w,&work.g_h);dirty=1;break;
        }}
        if(event&MU_BUTTON){if(!down)click(mx,my);down=!down;}
        if(event&MU_KEYBD)keypress((unsigned short)key,shift);
    }
    wipe();if(win>=0){wind_close(win);wind_delete(win);}v_clsvwk(vh);close(lockfd);appl_exit();return 0;
}
