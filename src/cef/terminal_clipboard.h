#pragma once
#include <windows.h>
#include <optional>
#include <string>
namespace aurora {
// Only called after terminal-frame authorization and explicit UI clipboard actions.
inline std::optional<std::wstring> terminal_clipboard(HWND window,const std::optional<std::wstring>& write) {
 if(GetForegroundWindow()!=window||!OpenClipboard(window))return {};
 struct Close { ~Close(){CloseClipboard();} } close;
 if(write){
  if(write->size()>32768)return {};
  const auto size=(write->size()+1)*sizeof(wchar_t);auto memory=GlobalAlloc(GMEM_MOVEABLE,size);if(!memory)return {};
  auto ptr=GlobalLock(memory);if(!ptr){GlobalFree(memory);return {};}memcpy(ptr,write->c_str(),size);GlobalUnlock(memory);
  if(!EmptyClipboard()||!SetClipboardData(CF_UNICODETEXT,memory)){GlobalFree(memory);return {};}
  return L"";
 }
 auto memory=GetClipboardData(CF_UNICODETEXT);if(!memory)return std::wstring{};
 auto size=GlobalSize(memory)/sizeof(wchar_t);if(size>32769)return {};
 auto ptr=static_cast<const wchar_t*>(GlobalLock(memory));if(!ptr)return {};
 size_t length=0;while(length<size&&ptr[length])++length;
 std::optional<std::wstring> result;if(length<size)result=std::wstring(ptr,length);GlobalUnlock(memory);return result;
}
}
