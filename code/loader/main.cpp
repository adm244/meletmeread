/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

//IMPORTANT(adm244): SCRATCH VERSION JUST TO GET IT UP WORKING

#include <windows.h>
#include <Shlobj.h>
#include <assert.h>
#include <stdio.h>

#include "types.h"

typedef HRESULT __stdcall DInput8CreateFunc(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
internal HMODULE dinput8 = 0;

internal HMODULE LoadDInput8()
{
  const char *libname = "\\dinput8.dll";
  
  char libpath[MAX_PATH];
  HRESULT result = SHGetFolderPathA(0, CSIDL_SYSTEMX86, 0, SHGFP_TYPE_CURRENT, libpath);
  assert(result == S_OK);
  
  strcat_s(libpath, MAX_PATH, libname);
  
  return LoadLibraryA(libpath);
}

internal void LoadNativeMods()
{
  const char *modsdir = "NativeMods\\";
  const char *search_str = "NativeMods\\*.dll";
  char libpath[MAX_PATH];
  
  WIN32_FIND_DATA fileinfo = {0};
  HANDLE findfile = FindFirstFileA(search_str, &fileinfo);
  assert(findfile != INVALID_HANDLE_VALUE);
  
  do {
    if (fileinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      continue;
    }
    
    libpath[0] = 0;
    
    strcat_s(libpath, MAX_PATH, modsdir);
    strcat_s(libpath, MAX_PATH, fileinfo.cFileName);
    
    HMODULE nativemod = LoadLibraryA(libpath);
    assert(nativemod != 0);
  }
  while (FindNextFileA(findfile, &fileinfo));
  
  FindClose(findfile);
}

extern "C" HRESULT __stdcall
FakeDirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN punkOuter)
{
  if( !dinput8 ) {
    dinput8 = LoadDInput8();
    assert(dinput8 != 0);
    
    LoadNativeMods();
  }
  
  DInput8CreateFunc *DInput8Create = (DInput8CreateFunc *)GetProcAddress(dinput8, "DirectInput8Create");
  return DInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
}

internal BOOL WINAPI DllMain(HMODULE loader, DWORD reason, LPVOID reserved)
{
  return TRUE;
}
