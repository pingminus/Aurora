#include "session.h"
#include "core/terminal_buffer.h"
#include <windows.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include <filesystem>
namespace opengod::terminal {
namespace {
class Handle {
 public:
  HANDLE value=nullptr;
  Handle()=default; explicit Handle(HANDLE h):value(h){}
  ~Handle(){reset();}
  void reset(HANDLE h=nullptr){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);value=h;}
  Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
};
std::wstring to_wide(const std::string& text){
  if(text.empty())return {};
  const int size=MultiByteToWideChar(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0);
  if(size<=0)return {};
  std::wstring result(size,L'\0');
  MultiByteToWideChar(CP_UTF8,0,text.data(),static_cast<int>(text.size()),result.data(),size);
  return result;
}
struct Console { HPCON value=nullptr; ~Console(){if(value)ClosePseudoConsole(value);} };
struct Attributes { std::vector<unsigned char> storage; LPPROC_THREAD_ATTRIBUTE_LIST list=nullptr;
 ~Attributes(){if(list)DeleteProcThreadAttributeList(list);} };
}
struct Session::State {
 mutable std::mutex mutex;std::condition_variable cv;bool stop=false,done=false;
 int columns=100,rows=30;bool resized=false;
 std::string input,status="starting",error,shell;OutputBuffer output;DWORD pid=0;
};
Session::Session(const Options& options):state_(std::make_shared<State>()) {
  // The supervisor owns all handles and joins its two pipe workers before exiting.
  // Its captured state has no CEF, window, browser or Session pointer.
  std::thread([state=state_,options]{run(state,options);}).detach();
}
Session::~Session(){close();}
void Session::close(){std::lock_guard lock(state_->mutex);state_->stop=true;if(!state_->done)state_->status="stopping";state_->cv.notify_all();}
Snapshot Session::inspect()const{std::lock_guard lock(state_->mutex);return Snapshot{state_->status,state_->error,state_->shell,{},state_->output.next(),state_->pid};}
bool Session::finished()const{std::lock_guard lock(state_->mutex);return state_->done;}
bool Session::input(const std::string& bytes){std::lock_guard lock(state_->mutex);if(state_->stop||state_->status!="running"||bytes.empty()||bytes.size()>16384||bytes.size()>input_limit-state_->input.size())return false;state_->input+=bytes;state_->cv.notify_all();return true;}
bool Session::resize(int cols,int rows){if(!valid_size(cols,rows))return false;std::lock_guard lock(state_->mutex);if(state_->stop)return false;state_->columns=cols;state_->rows=rows;state_->resized=true;return true;}
std::optional<Snapshot> Session::read(uint64_t ack){std::lock_guard lock(state_->mutex);auto bytes=state_->output.read(ack);if(!bytes)return {};state_->cv.notify_all();return Snapshot{state_->status,state_->error,state_->shell,*bytes,state_->output.next(),state_->pid};}
void Session::run(std::shared_ptr<State> s,const Options& options){
 auto fail=[&](const char* message){std::lock_guard lock(s->mutex);s->error=message;s->status="error";s->done=true;s->stop=true;s->cv.notify_all();};
 if(options.initial_command.size()>256){fail("Terminal command too long.");return;}
 Handle token; if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token.value)){fail("Cannot verify shell privileges.");return;}
 TOKEN_ELEVATION elevation{};DWORD returned=0;
 if(!GetTokenInformation(token.value,TokenElevation,&elevation,sizeof(elevation),&returned)||elevation.TokenIsElevated){fail("Terminal requires OpenGod to run without administrator privileges.");return;}
 wchar_t system[MAX_PATH]{};if(!GetSystemDirectoryW(system,MAX_PATH)){fail("Cannot locate Windows shell.");return;}
 const bool force_cmd=options.test_cmd||!options.initial_command.empty();
 std::filesystem::path shell=std::filesystem::path(system)/(force_cmd?L"cmd.exe":L"WindowsPowerShell/v1.0/powershell.exe");
 if(GetFileAttributesW(shell.c_str())==INVALID_FILE_ATTRIBUTES)shell=std::filesystem::path(system)/L"cmd.exe";
 const bool cmd=shell.filename()==L"cmd.exe";
 {std::lock_guard lock(s->mutex);s->shell=cmd?"Command Prompt":"Windows PowerShell";}
 Handle input_read,input_write,output_read,output_write;
 if(!CreatePipe(&input_read.value,&input_write.value,nullptr,0)||!CreatePipe(&output_read.value,&output_write.value,nullptr,0)){fail("Cannot create terminal pipes.");return;}
 Console console;
 if(FAILED(CreatePseudoConsole({100,30},input_read.value,output_write.value,0,&console.value))){fail("ConPTY creation failed; Windows 10 version 1809 or newer is required.");return;}
 Attributes attributes;SIZE_T size=0;InitializeProcThreadAttributeList(nullptr,1,0,&size);attributes.storage.resize(size);
 auto list=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.storage.data());
 if(!InitializeProcThreadAttributeList(list,1,0,&size)){fail("Cannot initialize terminal process attributes.");return;}attributes.list=list;
 if(!UpdateProcThreadAttribute(list,0,PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,console.value,sizeof(HPCON),nullptr,nullptr)){fail("Cannot attach shell to ConPTY.");return;}
 Handle job(CreateJobObjectW(nullptr,nullptr));JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
 if(!job.value||!SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits))){fail("Cannot isolate terminal process lifetime.");return;}
 STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.lpAttributeList=list;
 // Null explicit standard handles prevent inheriting redirected parent stdio;
 // Windows supplies console handles from the pseudoconsole attribute.
 startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
 std::wstring command=L"\""+shell.wstring()+L"\""+(cmd?L" /D /Q":L" -NoLogo -NoProfile");
 if(!options.initial_command.empty())command+=cmd?L" /K "+to_wide(options.initial_command):L" -NoExit -Command \""+to_wide(options.initial_command)+L"\"";
 PROCESS_INFORMATION info{};
 if(!CreateProcessW(shell.c_str(),command.data(),nullptr,nullptr,FALSE,EXTENDED_STARTUPINFO_PRESENT|CREATE_SUSPENDED|CREATE_UNICODE_ENVIRONMENT,nullptr,nullptr,&startup.StartupInfo,&info)){fail("Cannot launch terminal shell.");return;}
 Handle process(info.hProcess),primary(info.hThread);
 if(!AssignProcessToJobObject(job.value,process.value)){TerminateProcess(process.value,1);WaitForSingleObject(process.value,5000);fail("Cannot contain terminal process tree.");return;}
 input_read.reset();output_write.reset();
 std::thread reader([&]{char bytes[4096];DWORD read=0;while(ReadFile(output_read.value,bytes,sizeof(bytes),&read,nullptr)&&read){
   std::unique_lock lock(s->mutex);s->cv.wait(lock,[&]{return s->stop||s->output.size()+read<=output_limit;});
   if(s->output.size()+read<=output_limit)s->output.append(std::string(bytes,read));
 }});
 std::thread writer([&]{for(;;){std::string bytes;{std::unique_lock lock(s->mutex);s->cv.wait(lock,[&]{return s->stop||!s->input.empty();});if(s->stop)break;bytes.swap(s->input);}
   size_t offset=0;while(offset<bytes.size()){DWORD written=0;if(!WriteFile(input_write.value,bytes.data()+offset,static_cast<DWORD>(bytes.size()-offset),&written,nullptr)||written==0)return;offset+=written;}
 }});
 {std::lock_guard lock(s->mutex);s->pid=info.dwProcessId;s->status=s->stop?"stopping":"running";}
 if(ResumeThread(primary.value)==static_cast<DWORD>(-1)){std::lock_guard lock(s->mutex);s->error="Cannot resume terminal shell.";s->stop=true;}
 while(WaitForSingleObject(process.value,25)==WAIT_TIMEOUT){
   bool stop,resize;int cols,rows;{std::lock_guard lock(s->mutex);stop=s->stop;resize=s->resized;s->resized=false;cols=s->columns;rows=s->rows;}
   if(stop)break;
   if(resize&&FAILED(ResizePseudoConsole(console.value,{static_cast<SHORT>(cols),static_cast<SHORT>(rows)}))){std::lock_guard lock(s->mutex);s->error="Terminal resize failed.";}
 }
 // Signal workers before closing ConPTY. Reader keeps draining while it closes,
 // avoiding the documented ClosePseudoConsole output-pipe deadlock.
 {std::lock_guard lock(s->mutex);s->stop=true;s->input.clear();s->cv.notify_all();}
 TerminateJobObject(job.value,0);
 ClosePseudoConsole(console.value);console.value=nullptr;
 CancelSynchronousIo(writer.native_handle());writer.join();input_write.reset();
 CancelSynchronousIo(reader.native_handle());reader.join();output_read.reset();
 WaitForSingleObject(process.value,5000);
 {std::lock_guard lock(s->mutex);s->status="exited";s->done=true;s->cv.notify_all();}
}
}
