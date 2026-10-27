import json,shlex,subprocess,os,sys
from pathlib import Path
base=Path(__file__).resolve().parents[1]; build=Path(os.environ.get('DOCK_BUILD_DIR', str(base/'build'))).resolve()
source=base/'tests'/(sys.argv[1] if len(sys.argv)>1 else 'gtk-allocation.cpp')
output=source.stem if len(sys.argv)>1 else 'dock-gtk-test'
entries=json.loads((build/'compile_commands.json').read_text())
cmd=shlex.split(next(x['command'] for x in entries if x['file'].endswith('/Dock.cpp')))
cmd=cmd[:cmd.index('-MD')]+['-I'+str(base/'src'),str(source)]
cmd += [str(x) for x in (build/'src/libdocklike-test.so.p').glob('*.o') if x.name!='register.c.o']
env=os.environ.copy()
cmd+=shlex.split(subprocess.check_output(['pkg-config','--libs','libxfce4panel-2.0','libxfce4ui-2','libxfce4windowing-0','libxfce4windowingui-0','libxfconf-0','x11','xi'],env=env,text=True))
cmd+=['-o',str(build/output)]
subprocess.run(cmd,cwd=build,check=True)
