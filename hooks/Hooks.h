#pragma once
#include "../core/Globals.h"

extern "C" {
    VOID   InitializeBuffer(VOID);
    VOID   UninitializeBuffer(VOID);
    LPVOID AllocateBuffer(LPVOID pOrigin);
    VOID   FreeBuffer(LPVOID pBuffer);
    BOOL   IsExecutableAddress(LPVOID pAddress);
}

#pragma pack(push, 1)
typedef struct _JMP_REL_SHORT { UINT8 opcode; UINT8 operand; } JMP_REL_SHORT, * PJMP_REL_SHORT;
typedef struct _JMP_REL { UINT8 opcode; UINT32 operand; } JMP_REL, * PJMP_REL, CALL_REL;
typedef struct _JMP_ABS { UINT8 opcode0; UINT8 opcode1; UINT32 dummy; UINT64 address; } JMP_ABS, * PJMP_ABS;
typedef struct _CALL_ABS { UINT8 opcode0; UINT8 opcode1; UINT32 dummy0; UINT8 dummy1; UINT8 dummy2; UINT64 address; } CALL_ABS;
typedef struct _JCC_REL { UINT8 opcode0; UINT8 opcode1; UINT32 operand; } JCC_REL;
typedef struct _JCC_ABS { UINT8 opcode; UINT8 dummy0; UINT8 dummy1; UINT8 dummy2; UINT32 dummy3; UINT64 address; } JCC_ABS;
#pragma pack(pop)

typedef struct _TRAMPOLINE {
    LPVOID pTarget;
    LPVOID pDetour;
    LPVOID pTrampoline;
    BOOL   patchAbove;
    UINT   nIP;
    UINT8  oldIPs[8];
    UINT8  newIPs[8];
} TRAMPOLINE, * PTRAMPOLINE;

BOOL CreateTrampolineFunction(PTRAMPOLINE ct);

bool install_hook(void* target, void* hook_func, void** original, HookInfo* info);
void remove_hook(HookInfo* info);

bool install_hwbp_hook(void* target, void* hook_func, void** original, HWBP_HookInfo* info);
void remove_hwbp_hook(HWBP_HookInfo* info);

bool install_int3_hook(void* target, void* hook_func, void** original, INT3_HookInfo* info);
void remove_int3_hook(INT3_HookInfo* info);

PVOID GetVEHHandle();
void SetVEHHandle(PVOID h);
