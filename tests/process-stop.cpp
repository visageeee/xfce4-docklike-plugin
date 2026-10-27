#include "../src/ProcessStop.hpp"
#include <cassert>
#include <sys/wait.h>
#include <iostream>
int main()
{
 assert(ProcessStop::open(0)<0);
 assert(ProcessStop::open(-1)<0);
 assert(ProcessStop::open(1)<0);
 assert(ProcessStop::open(getpid())<0);
 for (bool ignore : {false,true}) {
  int ready[2]; assert(pipe(ready)==0);
  pid_t child=fork(); assert(child>=0);
  if(child==0) {
   close(ready[0]);
   if(ignore) ::signal(SIGTERM,SIG_IGN);
   alarm(10); const char marker='x';
   if(write(ready[1],&marker,1)!=1) _exit(2);
   close(ready[1]);
   for(;;) pause();
  }
  close(ready[1]); char marker;
  assert(read(ready[0],&marker,1)==1); close(ready[0]);
  int fd=ProcessStop::open(child);
  if(fd<0) { std::cerr<<"pidfd: errno="<<errno<<" uid="<<geteuid()<<" pid="<<child<<std::endl; kill(child,SIGKILL); waitpid(child,nullptr,0); return 1; }
  assert(ProcessStop::signal(fd,SIGTERM));
  if(ignore) assert(ProcessStop::signal(fd,SIGKILL));
  int status; assert(waitpid(child,&status,0)==child);
  assert(WIFSIGNALED(status));
  assert(WTERMSIG(status)==(ignore?SIGKILL:SIGTERM));
  assert(!ProcessStop::signal(fd,SIGKILL) && errno==ESRCH);
  close(fd);
 }
 std::cout<<"Process signal and exited-handle tests passed\n";
}
