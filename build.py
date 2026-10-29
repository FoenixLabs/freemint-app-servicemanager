#!/usr/bin/env python3
"""Build this application with a MiNT cross compiler and external MiNTlib/GEMlib."""
from pathlib import Path
import argparse,subprocess,os
src=Path(__file__).resolve().parent
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--cc',default=os.environ.get('CC','m68k-atari-mint-gcc'))
p.add_argument('--mintlib',type=Path,required=True,help='MiNTlib source/build tree (include/ and build/m68020/)')
p.add_argument('--gemlib',type=Path,required=True,help='GEMlib tree with .lib68020-60/')
p.add_argument('--output',type=Path,default=src/'build')
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
lib=a.mintlib.resolve()/'build/m68020';gem=a.gemlib.resolve()
cc=[a.cc,'-isystem',str(a.mintlib.resolve()/'include')]
common=cc+['-m68060','-msoft-float','-Os','-Wall','-Wextra','-Werror','-nostdlib','-Wl,--stack,64k','-s',str(lib/'crt0.o'),'-I'+str(gem)]
libs=['-L'+str(lib),'-Wl,--start-group','-lc','-lgcc','-Wl,--end-group']
subprocess.run(common+[str(src/'app.c'),str(src/'service.c'),'-L'+str(gem/'.lib68020-60'),'-lgem']+libs+['-o',str(out/'SERVICES.APP')],check=True)
subprocess.run(common+[str(src/'control.c'),str(src/'service.c')]+libs+['-o',str(out/'SVCCTL.TTP')],check=True)
