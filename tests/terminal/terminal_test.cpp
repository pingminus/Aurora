#include "terminal/session.h"
#include "core/terminal_buffer.h"
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
using namespace opengod::terminal;using namespace std::chrono_literals;
int main(){int failures=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed line "<<__LINE__<<"\n";++failures;}}while(false)
OutputBuffer buffer;CHECK(buffer.append(std::string("\xe2\x82",2)));auto part=buffer.read(0);CHECK(part&&part->size()==2);CHECK(buffer.read(0)==part);CHECK(!buffer.read(3));CHECK(buffer.append(std::string("\xac",1)));CHECK(buffer.read(2)==std::string("\xac",1));CHECK(!buffer.read(0));CHECK(buffer.read(3)=="");CHECK(buffer.append(std::string(output_limit,'x')));CHECK(!buffer.append("x"));CHECK(!valid_size(0,20));CHECK(!valid_size(401,20));CHECK(!valid_size(80,201));CHECK(valid_size(80,24));
Options alpha;alpha.test_cmd=true;Session a(alpha),b(alpha);for(int i=0;i<200&&(a.inspect().status=="starting"||b.inspect().status=="starting");++i)std::this_thread::sleep_for(25ms);
CHECK(a.inspect().status=="running");CHECK(b.inspect().status=="running");if(a.inspect().status!="running"||b.inspect().status!="running"){std::cerr<<a.inspect().error<<b.inspect().error;return 1;}
CHECK(a.resize(120,40));CHECK(!a.resize(900,40));CHECK(!a.input(std::string(16385,'x')));
CHECK(a.input("echo OPENGOD_ALPHA_ONLY\r"));CHECK(b.input("echo OPENGOD_BETA_ONLY\r"));
uint64_t ca=0,cb=0;std::string oa,ob;
for(int i=0;i<100;++i){auto x=a.read(ca),y=b.read(cb);CHECK(x&&y);if(x){oa+=x->bytes;ca=x->next;}if(y){ob+=y->bytes;cb=y->next;}if(oa.find("OPENGOD_ALPHA_ONLY")!=std::string::npos&&ob.find("OPENGOD_BETA_ONLY")!=std::string::npos)break;std::this_thread::sleep_for(25ms);}
CHECK(oa.find("OPENGOD_ALPHA_ONLY")!=std::string::npos);CHECK(ob.find("OPENGOD_BETA_ONLY")!=std::string::npos);CHECK(oa.find("OPENGOD_BETA_ONLY")==std::string::npos);CHECK(ob.find("OPENGOD_ALPHA_ONLY")==std::string::npos);
CHECK(a.input("cd /d %SystemRoot%\r"));CHECK(a.input("echo OPENGOD_DIR=%CD%\r"));CHECK(a.input("start /b ping.exe -t 127.0.0.1 >nul\r"));
std::this_thread::sleep_for(600ms);
std::vector<HANDLE> children;HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
if(Process32FirstW(snapshot,&entry))do{if(entry.th32ParentProcessID==a.inspect().pid){auto child=OpenProcess(SYNCHRONIZE,FALSE,entry.th32ProcessID);if(child)children.push_back(child);}}while(Process32NextW(snapshot,&entry));CloseHandle(snapshot);CHECK(!children.empty());
for(int i=0;i<40;++i){auto x=a.read(ca);if(x){oa+=x->bytes;ca=x->next;}if(oa.find("OPENGOD_DIR=C:\\Windows")!=std::string::npos)break;std::this_thread::sleep_for(25ms);}
CHECK(oa.find("OPENGOD_DIR=")!=std::string::npos);
a.close();for(int i=0;i<200&&!a.finished();++i)std::this_thread::sleep_for(25ms);CHECK(a.finished());CHECK(!a.input("echo stale\r"));
for(auto child:children){CHECK(WaitForSingleObject(child,2000)==WAIT_OBJECT_0);CloseHandle(child);}
CHECK(b.inspect().status=="running");CHECK(b.input("exit\r"));for(int i=0;i<200&&!b.finished();++i)std::this_thread::sleep_for(25ms);CHECK(b.finished());CHECK(b.inspect().status=="exited");
Session c({.initial_command="echo OPENGOD_INIT"});
for(int i=0;i<200&&c.inspect().status=="starting";++i)std::this_thread::sleep_for(25ms);
CHECK(c.inspect().status=="running");
std::string oc;uint64_t cc=0;
for(int i=0;i<100;++i){auto x=c.read(cc);if(x){oc+=x->bytes;cc=x->next;}if(oc.find("OPENGOD_INIT")!=std::string::npos)break;std::this_thread::sleep_for(25ms);}
CHECK(oc.find("OPENGOD_INIT")!=std::string::npos);
c.close();for(int i=0;i<200&&!c.finished();++i)std::this_thread::sleep_for(25ms);CHECK(c.finished());
std::cout<<"ConPTY tests completed: "<<failures<<" failures\n";return failures?1:0;
}
