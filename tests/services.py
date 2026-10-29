#!/usr/bin/env python3
"""Exercise production service validation and FAT-safe account/config transactions."""
from pathlib import Path
import os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
SRC=ROOT
harness=r'''
#include "service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv) {
 struct svc_config c;char e[160]="";
 if(!svc_load(&c,e,sizeof(e))){puts(e);return 2;}
 {struct svc_config other=c;
 assert(svc_scope(&c,&other,0)==0);other.uart=!other.uart;assert(svc_scope(&c,&other,0)==0);
 other=c;other.ftp=!other.ftp;assert(svc_scope(&c,&other,0)==1);
 other=c;assert(svc_scope(&c,&other,1)==1);strcpy(other.hostname,"changed");assert(svc_scope(&c,&other,0)==2);}
 {char e[160],eight[9]="password",upper[129],longer[130];
 memset(upper,'a',128);upper[128]=0;memset(longer,'a',129);longer[129]=0;
 assert(svc_password_check("","",e,sizeof(e))==SVC_PASSWORD_OK);
 assert(svc_password_check("1234567","1234567",e,sizeof(e))==SVC_PASSWORD_LENGTH);
 assert(svc_password_check(eight,eight,e,sizeof(e))==SVC_PASSWORD_OK);
 assert(svc_password_check(upper,upper,e,sizeof(e))==SVC_PASSWORD_OK);
 assert(svc_password_check(longer,longer,e,sizeof(e))==SVC_PASSWORD_LENGTH);
 assert(svc_password_check(eight,"different",e,sizeof(e))==SVC_PASSWORD_MISMATCH);
 assert(svc_password_check("",eight,e,sizeof(e))==SVC_PASSWORD_MISMATCH);}
 if(argc<2)return 3;
 if(!strcmp(argv[1],"load")){printf("%d %d %d %d %s %s\n",c.network,c.dhcp,c.uart,c.ftp,c.hostname,c.username);return 0;}
 if(!strcmp(argv[1],"save")) {
  if(argc!=14)return 4;
  c.network=atoi(argv[2]);c.dhcp=atoi(argv[3]);c.uart=atoi(argv[4]);c.ftp=atoi(argv[5]);
  strcpy(c.hostname,argv[6]);strcpy(c.address,argv[7]);strcpy(c.netmask,argv[8]);strcpy(c.gateway,argv[9]);strcpy(c.dns,argv[10]);strcpy(c.username,argv[11]);
  if(!svc_save(&c,argv[12],argv[13],e,sizeof(e))){puts(e);return 1;}return 0;
 }
 return 5;
}
'''
with tempfile.TemporaryDirectory() as temp:
 root=Path(temp);(root/'etc').mkdir();(root/'d/mint/net').mkdir(parents=True)
 source=root/'test.c';source.write_text('#include <stdlib.h>\n'+harness)
 base='root:*:0:0:Local user:/root:/bin/bash\nnobody:*:65534:65534::/tmp:/bin/false\nother:*:100:100::/home/other:/bin/bash\n'
 passwd=root/'etc/passwd';config=root/'etc/a2560me-services.conf';passwd.write_text(base)
 def compile(name,extra=[]):
  exe=root/name
  subprocess.run(['cc','-O2','-Wall','-Wextra','-Wno-misleading-indentation','-DSERVICE_HOST_TEST',f'-DSERVICE_ROOT="{root}"']+extra+['-I'+str(SRC),str(source),str(SRC/'service.c'),'-lcrypt','-o',str(exe)],check=True)
  return exe
 exe=compile('test');broken=compile('broken',['-DSERVICE_TEST_FAIL_CONFIG'])
 env=dict(os.environ,SERVICE_HOST_TEST='1')
 def call(args,good=True,program=exe):
  p=subprocess.run([str(program)]+args,env=env,capture_output=True,text=True)
  assert (p.returncode==0)==good,(p.returncode,p.stdout,p.stderr)
  return p.stdout
 assert call(['load']).strip()=='1 1 0 0 a2560me transfer'
 password='eight123'
 def save(**kw):
  v=dict(network='1',dhcp='1',uart='0',ftp='1',host='a2560me',ip='',mask='255.255.255.0',gw='',dns='',user='transfer',password='',confirm='');v.update(kw)
  return ['save']+[v[k] for k in ['network','dhcp','uart','ftp','host','ip','mask','gw','dns','user','password','confirm']]
 call(save(),False);assert passwd.read_text()==base and not config.exists()
 call(save(password=password,confirm=password))
 saved=passwd.read_text();assert 'transfer:$1$' in saved and password not in saved and password not in config.read_text();assert base in saved
 old_hash=saved.split('transfer:',1)[1].split(':',1)[0]
 call(save(user='upload',host='foenix-lab',uart='1'))
 assert 'transfer:' not in passwd.read_text() and f'upload:{old_hash}:0:0:' in passwd.read_text() and base in passwd.read_text()
 assert call(['load']).strip()=='1 1 1 1 foenix-lab upload'
 stable=(passwd.read_bytes(),config.read_bytes())
 for kwargs in [dict(user='root'),dict(user='other'),dict(user='bad;name'),dict(host='bad$(cmd)'),dict(host='-bad'),dict(password='short',confirm='short'),dict(password='1234567',confirm='1234567'),dict(password='x'*129,confirm='x'*129),dict(password=password,confirm='different'),dict(dhcp='0',ip='192.168.1.9',mask='255.0.255.0'),dict(dhcp='0',ip='192.168.1.0'),dict(dhcp='0',ip='192.168.1.9',gw='10.1.1.1'),dict(dhcp='0',ip='127.0.0.1'),dict(dhcp='0',ip='224.1.2.3'),dict(dhcp='0',ip='192.168.1.9',dns='1.2.3.999')]:
  call(save(user='upload',**{k:v for k,v in kwargs.items() if k!='user'}) if 'user' not in kwargs else save(**kwargs),False)
  assert (passwd.read_bytes(),config.read_bytes())==stable
 call(save(user='upload',dhcp='0',ip='192.168.1.42',gw='192.168.1.1',dns='1.1.1.1',host='foenix-static'))
 assert 'MODE=static' in config.read_text()
 stable=(passwd.read_bytes(),config.read_bytes())
 call(save(user='upload',host='changed',password=password+'x',confirm=password+'x'),False,broken)
 assert (passwd.read_bytes(),config.read_bytes())==stable
 assert not list((root/'etc').glob('*.svc-*'))
 target=root/'untouched';target.write_text('do not overwrite')
 (root/'etc/passwd.svc-new').symlink_to(target)
 call(save(user='upload',password=password+'x',confirm=password+'x'),False)
 assert target.read_text()=='do not overwrite';assert (root/'etc/passwd.svc-new').is_symlink()
 (root/'etc/passwd.svc-new').unlink()
 # Data files with shell expressions and duplicate keys must never be evaluated.
 config.write_text('HOSTNAME=$(touch victim)\n');call(['load'],False)
 config.write_text('MODE=dhcp\nMODE=static\n');call(['load'],False)
 print('PASS: service validation, masked-password backend, account collision/rename/hash preservation, static IPv4, rollback and exclusive temporary files')
