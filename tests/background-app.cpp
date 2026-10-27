#include "Dock.hpp"
#include "ForceQuit.hpp"
#include <gdk/gdkx.h>
#include <libxfce4windowing/xfw-x11.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cassert>
#include <iostream>
int main(int argc,char** argv) {
 gtk_init(&argc,&argv);
 auto* plugin=XFCE_PANEL_PLUGIN(g_object_new(XFCE_TYPE_PANEL_PLUGIN,"name","docklike-test","unique-id",100,nullptr));
 construct(plugin); Dock::onPanelResize(61);
 Display* d=GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
 Window root=DefaultRootWindow(d);
 for(int mode=0;mode<5;++mode) {
  int ready[2];assert(pipe(ready)==0);
  pid_t child=fork();assert(child>=0);
  if(!child) {
   close(ready[0]); alarm(10);
   
   Display* cd=XOpenDisplay(nullptr); if(!cd)_exit(3);
   Window w=XCreateSimpleWindow(cd,DefaultRootWindow(cd),0,0,100,100,0,0,0);
   XStoreName(cd,w,"Background app fixture");
   XClassHint cls={const_cast<char*>("background-test"),const_cast<char*>("BackgroundTest")};
   XSetClassHint(cd,w,&cls);
   const char* host=mode==2?"remote.invalid":g_get_host_name();
   XChangeProperty(cd,w,XInternAtom(cd,"WM_CLIENT_MACHINE",False),XA_STRING,8,PropModeReplace,reinterpret_cast<const unsigned char*>(host),strlen(host));
   unsigned long pid=getpid();
   XChangeProperty(cd,w,XInternAtom(cd,"_NET_WM_PID",False),XA_CARDINAL,32,PropModeReplace,reinterpret_cast<unsigned char*>(&pid),1);
   Window leader = mode == 3 ? XCreateSimpleWindow(cd,DefaultRootWindow(cd),0,0,1,1,0,0,0) : w;
   XChangeProperty(cd,w,XInternAtom(cd,"WM_CLIENT_LEADER",False),XA_WINDOW,32,PropModeReplace,reinterpret_cast<unsigned char*>(&leader),1);
   long state[2]={NormalState,None};
   XChangeProperty(cd,w,XInternAtom(cd,"WM_STATE",False),XInternAtom(cd,"WM_STATE",False),32,PropModeReplace,reinterpret_cast<unsigned char*>(state),2);
   XMapWindow(cd,w);XFlush(cd);
   if(write(ready[1],&w,sizeof(w))!=sizeof(w))_exit(4);
   close(ready[1]);
   for(;;){XEvent event;XNextEvent(cd,&event);}
  }
  close(ready[1]);Window w; assert(read(ready[0],&w,sizeof(w))==sizeof(w));close(ready[0]);
  XChangeProperty(d,root,XInternAtom(d,"_NET_CLIENT_LIST",False),XA_WINDOW,32,PropModeReplace,reinterpret_cast<unsigned char*>(&w),1);
  XChangeProperty(d,root,XInternAtom(d,"_NET_CLIENT_LIST_STACKING",False),XA_WINDOW,32,PropModeReplace,reinterpret_cast<unsigned char*>(&w),1);
  XFlush(d);
  XfwWindow* target=nullptr;
  for(int n=0;n<200&&!target;++n){while(g_main_context_iteration(nullptr,false)){};for(GList* l=xfw_screen_get_windows(Xfw::mXfwScreen);l;l=l->next)if(xfw_window_x11_get_xid(XFW_WINDOW(l->data))==w)target=XFW_WINDOW(l->data);usleep(10000);}
  if(!target){kill(child,SIGKILL);waitpid(child,nullptr,0);std::cerr<<"fixture not discovered\n";return 1;}
  auto* app=xfw_window_get_application(target);auto* inst=xfw_application_get_instance(app,target);
  std::cout<<"mode="<<mode<<" pid="<<xfw_application_instance_get_pid(inst)<<" expected="<<child<<std::endl;
  if(mode != 3) assert(xfw_application_instance_get_pid(inst)==child);
  auto gw=Xfw::mGroupWindows.get(target);assert(gw&&gw->mGroup);
  Group* group=gw->mGroup;
  gw->leaveGroup();
  assert(!group->mBackgroundRunning); // Filtering a window is not closing it.
  assert(!gtk_widget_get_visible(group->mButton));
  gw->getInGroup();
  group->mPinned = mode == 1;
  gw.reset();
  if(mode == 4) {
   Atom skip=XInternAtom(d,"_NET_WM_STATE_SKIP_TASKBAR",False);
   XChangeProperty(d,w,XInternAtom(d,"_NET_WM_STATE",False),XA_ATOM,32,PropModeReplace,reinterpret_cast<unsigned char*>(&skip),1);
   XFlush(d);
   for(int n=0;n<60;++n){while(g_main_context_iteration(nullptr,false)){};usleep(10000);}
   assert(group->mWindowsCount == 0);
   assert(group->mBackgroundRunning);
   assert(gtk_widget_get_visible(group->mButton));
  }
  XChangeProperty(d,root,XInternAtom(d,"_NET_CLIENT_LIST",False),XA_WINDOW,32,PropModeReplace,nullptr,0);
  XChangeProperty(d,root,XInternAtom(d,"_NET_CLIENT_LIST_STACKING",False),XA_WINDOW,32,PropModeReplace,nullptr,0);
  XFlush(d);
  for(int n=0;n<60;++n){while(g_main_context_iteration(nullptr,false)){};usleep(10000);}
  assert(group->mWindowsCount == 0);
  assert(group->mBackgroundRunning == (mode != 2));
  assert(gtk_widget_get_visible(group->mButton) == (mode != 2));
  assert(gtk_style_context_has_class(gtk_widget_get_style_context(group->mButton), "open_group") == (mode != 2));
  kill(child,SIGTERM); waitpid(child,nullptr,0);
  for(int n=0;n<100 && group->mBackgroundRunning;++n){while(g_main_context_iteration(nullptr,false)){};usleep(10000);}
  assert(!group->mBackgroundRunning);
  assert(gtk_widget_get_visible(group->mButton) == (mode == 1));
  assert(!gtk_style_context_has_class(gtk_widget_get_style_context(group->mButton), "open_group"));
  std::cout << "Background indicator mode " << mode << " passed" << std::endl;

 }
 return 0;
}
