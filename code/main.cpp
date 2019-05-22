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
  i8 offset = *((i8 *)((u64)src + opcode_length));
  u64 rip = (u64)src + opcode_length + sizeof(u8);
  
  return (void *)(rip + offset);
}

internal void * RIPRel32(void *src, u8 opcode_length)
{
  i32 offset = *((i32 *)((u64)src + opcode_length));
  u64 rip = (u64)src + opcode_length + sizeof(u32);
  
  return (void *)(rip + offset);
}

//FIX(adm244): swap topic and dialog flags

enum BioConversation_TopicFlags {
  Topic_Skip = 0x4,
  Topic_Ambient = 0x40,
  Topic_IsSpeaking = 0x100,
  Topic_Patch_ManualSkip = 0x40000000,
};

enum BioConversation_DialogFlags {
  Dialog_IsVoicePlaying = 0x10,
  Dialog_Patch_DialogWheelActive = 0x40000000,
};

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

typedef bool (__thiscall *_BioConversation_NeedToDisplayReplies)(BioConversation *conversation);
typedef bool (__thiscall *_BioConversation_IsAmbient)(BioConversation *conversation);

internal _BioConversation_NeedToDisplayReplies BioConversation_NeedToDisplayReplies = 0;
internal _BioConversation_IsAmbient BioConversation_IsAmbient = 0;

internal bool ShouldReply(BioConversation *conversation)
{
  return (conversation->dialogFlags & Dialog_Patch_DialogWheelActive);
}

internal bool IsSkipped(BioConversation *conversation)
{
  //FIX(adm244): NeedToDisplayReplies() modifies timers (by adding 0.5)
  // it introduces bugs with an abrupt sound (it is clearly when switching to "wait for a reply" state)
  // Possible solution is to hook the positive result of a function (or recreate it)
  
  //NOTE(adm244): there's still some sound clicking and poping
  // is this a patch problem or game itself comes with these bugs?
  
  //FIX(adm244): breaks teleportation from dialogs (fast-travel)
  
  if (!BioConversation_IsAmbient(conversation) && !ShouldReply(conversation)) {
    bool isSkipped = (conversation->topicFlags & Topic_Patch_ManualSkip);
    conversation->topicFlags &= ~Topic_Patch_ManualSkip;
    return isSkipped;
  }
  
  return true;
}

internal void SkipNode(BioConversation *conversation)
{
  // (NOT speaking) AND DialogWheelActive
  // (!Speaking && DialogWheelActive)
  // !(!Speaking && DialogWheelActive)
  // Speaking || !DisalogWheelActive
  
  if ((conversation->dialogFlags & Dialog_IsVoicePlaying)
    || !(conversation->dialogFlags & Dialog_Patch_DialogWheelActive)) {
    conversation->topicFlags |= Topic_Patch_ManualSkip;
  }
}

internal void *skip_jz_dest_address = 0;
internal void *skip_jnz_dest_address = 0;
internal void *skip_post_jnz_address = 0;

internal void *skip_node_address = 0;
internal void *skip_node_mov_address = 0;

internal __declspec(naked) void IsSkipped_Hook()
{
  _asm {
    push ebx
    push esi
    push edi
    push ebp
    push esp
    
    push esi
    call IsSkipped
    add esp, 4
    
    pop esp
    pop ebp
    pop edi
    pop esi
    pop ebx
    
    test al, al
    jz dont_skip_dialog
    jmp skip_dialog
    
  dont_skip_dialog:
    jmp [skip_jz_dest_address]
    
  skip_dialog:
    test ebp, ebp
    jnz jnz_jump
    mov eax, [esp + 24h]
    jmp [skip_post_jnz_address]
    
  jnz_jump:
    jmp [skip_jnz_dest_address]
  }
}

internal __declspec(naked) void SkipNode_Hook()
{
  _asm {
    pushad
    push ecx
    call SkipNode
    add esp, 4
    popad
    
    push esi
    mov esi, ecx
    mov ecx, [skip_node_mov_address]
    mov ecx, [ecx]
    
    mov eax, skip_node_address
    add eax, 9
    jmp eax
  }
}

internal __declspec(naked) void RepliesActive_Hook()
{
  _asm {
    mov eax, Dialog_Patch_DialogWheelActive
    or [esi+150h], eax
    
    mov eax, 1
    pop esi
    retn
  }
}

internal __declspec(naked) void RepliesInactive_Hook()
{
  _asm {
    mov eax, Dialog_Patch_DialogWheelActive
    not eax
    and [esi+150h], eax
    
    xor eax, eax
    pop esi
    retn
  }
}

internal BOOL WINAPI DllMain(HMODULE loader, DWORD reason, LPVOID reserved)
{
  if( reason == DLL_PROCESS_ATTACH )
  {
    //TODO(adm244): get addresses by a signature search
    
    // get function pointers for BioConversation object
    void *bio_conversation_vtable = (void *)0x118FFD20;
    BioConversation_NeedToDisplayReplies = (_BioConversation_NeedToDisplayReplies)(*((u32 *)bio_conversation_vtable + 105));
    BioConversation_IsAmbient = (_BioConversation_IsAmbient)(*((u32 *)bio_conversation_vtable + 93));
#ifdef DEBUG
    assert((u64)BioConversation_NeedToDisplayReplies == 0x1090BC67);
    assert((u64)BioConversation_IsAmbient == 0x10950A8D);
#endif
    
    // hook NeedToDisplayReplies function
    void *replies_active_patch_address = (u8 *)RIPRel32(BioConversation_NeedToDisplayReplies, 1) + 0x3B;
    void *replies_inactive_patch_address = (u8 *)RIPRel32(BioConversation_NeedToDisplayReplies, 1) + 0x71;
    
    WriteDetour(replies_active_patch_address, &RepliesActive_Hook, 2);
    WriteDetour(replies_inactive_patch_address, &RepliesInactive_Hook, 0);
    
    // hook UpdateConversation function
    void *skip_jz_address = (void *)0x10D13FF5;
    skip_jz_dest_address = RIPRel8(skip_jz_address, 1);
    
    void *skip_address = (void *)((u64)skip_jz_address + 2);
    void *skip_jnz_address = (void *)((u64)skip_address + 2);
    skip_jnz_dest_address = RIPRel8(skip_jnz_address, 1);
    skip_post_jnz_address = (void *)((u64)skip_jnz_address + 6);

    WriteDetour(skip_address, &IsSkipped_Hook, 3);
    
    // hook SkipNode function
    skip_node_address = (void *)0x10CC9060;
    skip_node_mov_address = (void *)(*((u32 *)((u8 *)skip_node_address + 5)));
#ifdef DEBUG
    assert((u64)skip_node_mov_address == 0x11BC1A44);
#endif
    
    WriteDetour(skip_node_address, &SkipNode_Hook, 4);
  }

  return TRUE;
}
