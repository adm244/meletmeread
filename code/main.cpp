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

#define PSAPI_VERSION 1
#include <windows.h>
#include <psapi.h>
#include <assert.h>
#include <stdio.h>

#include "types.h"
#include "detours.cpp"

typedef HRESULT __stdcall DInput8CreateFunc(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);

internal HMODULE dinput8 = 0;

extern "C" HRESULT __stdcall
FakeDirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN punkOuter)
{
  if( !dinput8 ) {
    dinput8 = LoadLibrary("c:\\windows\\system32\\dinput8.dll");
  }
  
  DInput8CreateFunc *DInput8Create = (DInput8CreateFunc *)GetProcAddress(dinput8, "DirectInput8Create");
  return DInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
}

internal void * RIPRel8(void *src, u8 opcode_length)
{
  i8 offset = *((u8 *)((u64)src + opcode_length));
  u64 rip = (u64)src + opcode_length + sizeof(u8);
  
  return (void *)(rip + offset);
}

struct BioConversation {
  void *vtable;
  u32 unk04[40];
  void *owner; // 0xA4
  void *player; // 0xA8
  void *speaker; // 0xAC
  u32 unkB0;
  void *listener; // 0xB4
  u32 unkB8[4];
  void *kismetSequence; // 0xC8
  u32 unkCC[2];
  u32 topicFlags; // 0xD4
  u32 unkD8[30];
  u32 dialogFlags; // 0x150
  u32 unk154[41];
};

internal bool IsSkipped(BioConversation *conversation)
{
  //return (conversation->topicFlags & 0x40000000);
  MessageBox(0, "Fuck yeah!", "Yeah, fuck!", 0);
  return true;
}

internal void *skip_jz_dest_address = 0;

internal __declspec(naked) void IsSkipped_Hook()
{
  _asm {
    push esi
    call IsSkipped
    add esp, 4
    
    test al, al
    jz dont_skip_dialog
    jmp skip_dialog
    
  dont_skip_dialog:
    jmp [skip_jz_dest_address]
  
  skip_dialog:
  }
}

internal BOOL WINAPI DllMain(HMODULE loader, DWORD reason, LPVOID reserved)
{
  if( reason == DLL_PROCESS_ATTACH )
  {
    void *skip_jz_address = (void *)0x10D13FF5;
    skip_jz_dest_address = RIPRel8(skip_jz_address, 1);
    
    void *skip_address = (void *)((u64)skip_jz_address + 2);
    void *skip_jnz_address = (void *)((u64)skip_address + 2);
    void *skip_jnz_dest_address = RIPRel8(skip_jnz_address, 1);
    void *skip_post_jnz_address = (void *)((u64)skip_jnz_address + 2);
    
    uint jump_offsets[1] = {2};
    Detour(skip_address, &IsSkipped_Hook, 8, jump_offsets, 1);
  }

  return TRUE;
}
