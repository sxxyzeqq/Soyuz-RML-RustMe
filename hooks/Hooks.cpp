#include "Hooks.h"
#include "../help/hde64.h"
#include "../help/buffer.h"
#include <TlHelp32.h>
#include <processthreadsapi.h>
#include <cstring>

static constexpr int MAX_INT3_HOOKS = 128;
static INT3_HookInfo g_int3_slots[MAX_INT3_HOOKS];
static PVOID g_int3_veh_handle = nullptr;
static CRITICAL_SECTION g_int3_cs;
static bool g_int3_cs_initialized = false;
static LONG g_int3_active_count = 0;

static void EnsureInt3CriticalSection() {
    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
    InitOnceExecuteOnce(&once, [](PINIT_ONCE, PVOID, PVOID*) -> BOOL {
        InitializeCriticalSection(&g_int3_cs);
        g_int3_cs_initialized = true;
        return TRUE;
    }, nullptr, nullptr);
}

static LONG CALLBACK Int3BreakpointHandler(PEXCEPTION_POINTERS pExc) {
    if (!pExc || !pExc->ExceptionRecord || !pExc->ContextRecord) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (pExc->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    void* exceptionAddress = pExc->ExceptionRecord->ExceptionAddress;
#if defined(_M_X64) || defined(__x86_64__)
    void* ripMinusInt3 = reinterpret_cast<void*>(pExc->ContextRecord->Rip - 1);
#else
    void* ripMinusInt3 = reinterpret_cast<void*>(pExc->ContextRecord->Eip - 1);
#endif
    EnsureInt3CriticalSection();
    EnterCriticalSection(&g_int3_cs);
    for (int i = MAX_INT3_HOOKS - 1; i >= 0; --i) {
        if (g_int3_slots[i].hook_func &&
            (g_int3_slots[i].target == exceptionAddress || g_int3_slots[i].target == ripMinusInt3)) {
#if defined(_M_X64) || defined(__x86_64__)
            pExc->ContextRecord->Rip = reinterpret_cast<DWORD64>(g_int3_slots[i].hook_func);
#else
            pExc->ContextRecord->Eip = reinterpret_cast<DWORD>(g_int3_slots[i].hook_func);
#endif
            LeaveCriticalSection(&g_int3_cs);
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    LeaveCriticalSection(&g_int3_cs);
    return EXCEPTION_CONTINUE_SEARCH;
}

static BOOL IsCodePadding(LPVOID pAddress, UINT size) {
    LPBYTE p = (LPBYTE)pAddress;
    while (size--) {
        if (*p != 0x90 && *p != 0xCC) return FALSE;
        p++;
    }
    return TRUE;
}

BOOL CreateTrampolineFunction(PTRAMPOLINE ct)
{
#if defined(_M_X64) || defined(__x86_64__)
    CALL_ABS call = { 0xFF, 0x15, 0x00000002, 0xEB, 0x08, 0x0000000000000000ULL };
    JMP_ABS jmp = { 0xFF, 0x25, 0x00000000, 0x0000000000000000ULL };
    JCC_ABS jcc = { 0x70, 0x0E, 0xFF, 0x25, 0x00000000, 0x0000000000000000ULL };
#else
    CALL_REL call = { 0xE8, 0x00000000 };
    JMP_REL jmp = { 0xE9, 0x00000000 };
    JCC_REL jcc = { 0x0F, 0x80, 0x00000000 };
#endif
    UINT8     oldPos = 0;
    UINT8     newPos = 0;
    ULONG_PTR jmpDest = 0;
    BOOL      finished = FALSE;
#if defined(_M_X64) || defined(__x86_64__)
    UINT8     instBuf[16];
#endif
    ct->patchAbove = FALSE;
    ct->nIP = 0;
    do {
        hde64s    hs;
        UINT      copySize;
        LPVOID    pCopySrc;
        ULONG_PTR pOldInst = (ULONG_PTR)ct->pTarget + oldPos;
        ULONG_PTR pNewInst = (ULONG_PTR)ct->pTrampoline + newPos;
        copySize = hde64_disasm((LPVOID)pOldInst, &hs);
        if (hs.flags & F_ERROR) return FALSE;
        pCopySrc = (LPVOID)pOldInst;
        if (oldPos >= MIN_COPY_SIZE) {
#if defined(_M_X64) || defined(__x86_64__)
            jmp.address = pOldInst;
#else
            jmp.operand = (UINT32)(pOldInst - (pNewInst + sizeof(jmp)));
#endif
            pCopySrc = &jmp;
            copySize = sizeof(jmp);
            finished = TRUE;
        }
#if defined(_M_X64) || defined(__x86_64__)
        else if ((hs.modrm & 0xC7) == 0x05) {
            PUINT32 pRelAddr;
            memcpy(instBuf, (LPBYTE)pOldInst, copySize);
            pCopySrc = instBuf;
            pRelAddr = (PUINT32)(instBuf + hs.len - ((hs.flags & 0x3C) >> 2) - 4);
            *pRelAddr = (UINT32)((pOldInst + hs.len + (INT32)hs.disp.disp32) - (pNewInst + hs.len));
            if (hs.opcode == 0xFF && hs.modrm_reg == 4) finished = TRUE;
        }
#endif
        else if (hs.opcode == 0xE8) {
            ULONG_PTR dest = pOldInst + hs.len + (INT32)hs.imm.imm32;
#if defined(_M_X64) || defined(__x86_64__)
            call.address = dest;
#else
            call.operand = (UINT32)(dest - (pNewInst + sizeof(call)));
#endif
            pCopySrc = &call;
            copySize = sizeof(call);
        }
        else if ((hs.opcode & 0xFD) == 0xE9) {
            ULONG_PTR dest = pOldInst + hs.len;
            if (hs.opcode == 0xEB) dest += (INT8)hs.imm.imm8;
            else dest += (INT32)hs.imm.imm32;
            if ((ULONG_PTR)ct->pTarget <= dest && dest < ((ULONG_PTR)ct->pTarget + MIN_COPY_SIZE)) {
                if (jmpDest < dest) jmpDest = dest;
            } else {
#if defined(_M_X64) || defined(__x86_64__)
                jmp.address = dest;
#else
                jmp.operand = (UINT32)(dest - (pNewInst + sizeof(jmp)));
#endif
                pCopySrc = &jmp;
                copySize = sizeof(jmp);
                finished = (pOldInst >= jmpDest);
            }
        }
        else if ((hs.opcode & 0xF0) == 0x70 || (hs.opcode & 0xFC) == 0xE0 || (hs.opcode2 & 0xF0) == 0x80) {
            ULONG_PTR dest = pOldInst + hs.len;
            if ((hs.opcode & 0xF0) == 0x70 || (hs.opcode & 0xFC) == 0xE0) dest += (INT8)hs.imm.imm8;
            else dest += (INT32)hs.imm.imm32;
            if ((ULONG_PTR)ct->pTarget <= dest && dest < ((ULONG_PTR)ct->pTarget + MIN_COPY_SIZE)) {
                if (jmpDest < dest) jmpDest = dest;
            } else if ((hs.opcode & 0xFC) == 0xE0) {
                return FALSE;
            } else {
                UINT8 cond = ((hs.opcode != 0x0F ? hs.opcode : hs.opcode2) & 0x0F);
#if defined(_M_X64) || defined(__x86_64__)
                jcc.opcode = 0x71 ^ cond;
                jcc.address = dest;
#else
                jcc.opcode1 = 0x80 | cond;
                jcc.operand = (UINT32)(dest - (pNewInst + sizeof(jcc)));
#endif
                pCopySrc = &jcc;
                copySize = sizeof(jcc);
            }
        }
        else if ((hs.opcode & 0xFE) == 0xC2) {
            finished = (pOldInst >= jmpDest);
        }
        if (pOldInst < jmpDest && copySize != hs.len) return FALSE;
        if ((newPos + copySize) > MEMORY_SLOT_SIZE) return FALSE;
        if (ct->nIP >= ARRAYSIZE(ct->oldIPs)) return FALSE;
        ct->oldIPs[ct->nIP] = oldPos;
        ct->newIPs[ct->nIP] = newPos;
        ct->nIP++;
        memcpy((LPBYTE)ct->pTrampoline + newPos, pCopySrc, copySize);
        newPos += copySize;
        oldPos += hs.len;
    } while (!finished);
    if (oldPos < MIN_COPY_SIZE && !IsCodePadding((LPBYTE)ct->pTarget + oldPos, MIN_COPY_SIZE - oldPos)) {
#if defined(_M_X64) || defined(__x86_64__)
#else
        if (oldPos < sizeof(JMP_REL_SHORT) && !IsCodePadding((LPBYTE)ct->pTarget + oldPos, sizeof(JMP_REL_SHORT) - oldPos)) return FALSE;
#endif
        if (!IsExecutableAddress((LPBYTE)ct->pTarget - MIN_COPY_SIZE)) return FALSE;
        if (!IsCodePadding((LPBYTE)ct->pTarget - MIN_COPY_SIZE, MIN_COPY_SIZE)) return FALSE;
        ct->patchAbove = TRUE;
    }
    return TRUE;
}

bool install_int3_hook(void* target, void* hook_func, void** original, INT3_HookInfo* info) {
    if (!target || !hook_func || !info) return false;
    if (info->target) return false;

    EnsureInt3CriticalSection();

    EnterCriticalSection(&g_int3_cs);
    for (int i = 0; i < MAX_INT3_HOOKS; ++i) {
        if (g_int3_slots[i].target == target) {
            LeaveCriticalSection(&g_int3_cs);
            return false;
        }
    }
    LeaveCriticalSection(&g_int3_cs);

    void* trampoline = AllocateBuffer(target);
    if (!trampoline) return false;

    TRAMPOLINE ct = {};
    ct.pTarget = target;
    ct.pDetour = hook_func;
    ct.pTrampoline = trampoline;
    if (!CreateTrampolineFunction(&ct)) {
        FreeBuffer(trampoline);
        return false;
    }
    FlushInstructionCache(GetCurrentProcess(), trampoline, MEMORY_SLOT_SIZE);

    bool veh_created = false;
    if (!g_int3_veh_handle) {
        g_int3_veh_handle = AddVectoredExceptionHandler(1, Int3BreakpointHandler);
        if (!g_int3_veh_handle) {
            FreeBuffer(trampoline);
            return false;
        }
        veh_created = true;
    }

    INT3_HookInfo installed = {};
    installed.target = target;
    installed.hook_func = hook_func;
    installed.trampoline = ct.pTrampoline;
    installed.original_byte = *static_cast<BYTE*>(target);

    int slot = -1;
    EnterCriticalSection(&g_int3_cs);
    for (int i = 0; i < MAX_INT3_HOOKS; ++i) {
        if (g_int3_slots[i].target == target) {
            slot = -2;
            break;
        }
        if (!g_int3_slots[i].target) {
            slot = i;
            g_int3_slots[i] = installed;
            break;
        }
    }
    LeaveCriticalSection(&g_int3_cs);

    if (slot < 0) {
        if (veh_created && g_int3_active_count == 0 && g_int3_veh_handle) {
            RemoveVectoredExceptionHandler(g_int3_veh_handle);
            g_int3_veh_handle = nullptr;
        }
        FreeBuffer(trampoline);
        return false;
    }

    DWORD old_prot = 0;
    if (!VirtualProtect(target, 1, PAGE_EXECUTE_READWRITE, &old_prot)) {
        EnterCriticalSection(&g_int3_cs);
        g_int3_slots[slot] = {};
        LeaveCriticalSection(&g_int3_cs);
        if (veh_created && g_int3_active_count == 0 && g_int3_veh_handle) {
            RemoveVectoredExceptionHandler(g_int3_veh_handle);
            g_int3_veh_handle = nullptr;
        }
        FreeBuffer(trampoline);
        return false;
    }

    *static_cast<BYTE*>(target) = 0xCC;
    VirtualProtect(target, 1, old_prot, &old_prot);
    FlushInstructionCache(GetCurrentProcess(), target, 1);

    *info = installed;
    if (original) *original = ct.pTrampoline;
    InterlockedIncrement(&g_int3_active_count);
    return true;
}

void remove_int3_hook(INT3_HookInfo* info) {
    if (!info || !info->target) return;

    void* target = info->target;
    BYTE original_byte = info->original_byte;

    DWORD old_prot = 0;
    if (!VirtualProtect(target, 1, PAGE_EXECUTE_READWRITE, &old_prot)) {
        return;
    }

    *static_cast<BYTE*>(target) = original_byte;
    VirtualProtect(target, 1, old_prot, &old_prot);
    FlushInstructionCache(GetCurrentProcess(), target, 1);

    EnsureInt3CriticalSection();
    bool removed = false;
    EnterCriticalSection(&g_int3_cs);
    for (int i = 0; i < MAX_INT3_HOOKS; ++i) {
        if (g_int3_slots[i].target == target) {
            g_int3_slots[i] = {};
            removed = true;
            break;
        }
    }
    LeaveCriticalSection(&g_int3_cs);

    if (info->trampoline) {
        FreeBuffer(info->trampoline);
        info->trampoline = nullptr;
    }

    info->target = nullptr;
    info->hook_func = nullptr;
    info->original_byte = 0;

    if (removed && InterlockedDecrement(&g_int3_active_count) <= 0 && g_int3_veh_handle) {
        RemoveVectoredExceptionHandler(g_int3_veh_handle);
        g_int3_veh_handle = nullptr;
        g_int3_active_count = 0;
    }
}

bool install_hook(void* target, void* hook_func, void** original, HookInfo* info) {
    if (!info) return false;

    INT3_HookInfo int3 = {};
    if (!install_int3_hook(target, hook_func, original, &int3)) {
        return false;
    }

    info->target = int3.target;
    info->hook_func = int3.hook_func;
    info->trampoline = int3.trampoline;
    info->original_byte = int3.original_byte;
    info->original_bytes[0] = int3.original_byte;
    info->patchAbove = false;
    info->int3 = true;
    return true;
}

void remove_hook(HookInfo* info) {
    if (!info || !info->target) return;

    if (info->int3) {
        INT3_HookInfo int3 = {};
        int3.target = info->target;
        int3.hook_func = info->hook_func;
        int3.trampoline = info->trampoline;
        int3.original_byte = info->original_byte;
        remove_int3_hook(&int3);
    }

    info->target = nullptr;
    info->hook_func = nullptr;
    info->trampoline = nullptr;
    info->original_byte = 0;
    info->patchAbove = false;
    info->int3 = false;
    std::memset(info->original_bytes, 0, sizeof(info->original_bytes));
}

static HWBP_HookInfo g_hwbp_slots[4];
static PVOID g_veh_handle = nullptr;

static void SetBits(DWORD64& dw, int lowBit, int bits, int newValue) {
    DWORD64 mask = ((1ULL << bits) - 1ULL) << lowBit;
    dw = (dw & ~mask) | ((DWORD64)newValue << lowBit);
}

static void ApplyHWBPToThread(HANDLE hThread, void* address, int index, bool enable) {
    CONTEXT ctx;
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(hThread, &ctx)) {
        if (enable) {
            switch (index) {
            case 0: ctx.Dr0 = (DWORD64)address; break;
            case 1: ctx.Dr1 = (DWORD64)address; break;
            case 2: ctx.Dr2 = (DWORD64)address; break;
            case 3: ctx.Dr3 = (DWORD64)address; break;
            }
            SetBits(ctx.Dr7, 2 * index, 1, 1);
            SetBits(ctx.Dr7, 16 + 4 * index, 2, 0);
            SetBits(ctx.Dr7, 18 + 4 * index, 2, 0);
        } else {
            SetBits(ctx.Dr7, 2 * index, 1, 0);
        }
        SetThreadContext(hThread, &ctx);
    }
}

static void UpdateAllThreadsHWBP(void* address, int index, bool enable) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te;
        te.dwSize = sizeof(te);
        if (Thread32First(hSnap, &te)) {
            do {
                if (te.th32OwnerProcessID == GetCurrentProcessId() && te.th32ThreadID != GetCurrentThreadId()) {
                    HANDLE hThread = OpenThread(THREAD_SET_CONTEXT | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
                    if (hThread) {
                        ApplyHWBPToThread(hThread, address, index, enable);
                        CloseHandle(hThread);
                    }
                }
            } while (Thread32Next(hSnap, &te));
        }
        CloseHandle(hSnap);
    }
    ApplyHWBPToThread(GetCurrentThread(), address, index, enable);
}

LONG CALLBACK HardwareBreakpointHandler(PEXCEPTION_POINTERS pExc) {
    if (pExc->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP) {
        for (int i = 0; i < 4; i++) {
            if (g_hwbp_slots[i].target && (pExc->ContextRecord->Rip == (DWORD64)g_hwbp_slots[i].target)) {
                pExc->ContextRecord->Rip = (DWORD64)g_hwbp_slots[i].hook_func;
                pExc->ContextRecord->EFlags |= 0x10000;
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

bool install_hwbp_hook(void* target, void* hook_func, void** original, HWBP_HookInfo* info) {
    if (!target || !hook_func) {
        FILE* f = nullptr;
        //fopen_s(&f, "C:\\soyuz_hwbp.log", "a");
        //if (f) { fprintf(f, "HWBP: target or hook_func is NULL\n"); fclose(f); }
        return false;
    }
    
    if (!g_veh_handle) {
        g_veh_handle = AddVectoredExceptionHandler(1, HardwareBreakpointHandler);
        FILE* f = nullptr;
       // fopen_s(&f, "C:\\soyuz_hwbp.log", "a");
       // if (f) { fprintf(f, "HWBP: VEH handle = %p\n", g_veh_handle); fclose(f); }
        if (!g_veh_handle) return false;
    }
    
    int slot = -1;
    for (int i = 0; i < 4; i++) {
        if (!g_hwbp_slots[i].target) { slot = i; break; }
    }
    if (slot == -1) {
        FILE* f = nullptr;
      //  fopen_s(&f, "C:\\soyuz_hwbp.log", "a");
      //  if (f) { fprintf(f, "HWBP: No free slots\n"); fclose(f); }
        return false;
    }
    
    info->trampoline = AllocateBuffer(target);
    if (!info->trampoline) {
        FILE* f = nullptr;
      //  fopen_s(&f, "C:\\soyuz_hwbp.log", "a");
       // if (f) { fprintf(f, "HWBP: AllocateBuffer failed for target %p\n", target); fclose(f); }
        return false;
    }
    
    TRAMPOLINE ct = {};
    ct.pTarget = target;
    ct.pDetour = hook_func;
    ct.pTrampoline = info->trampoline;
    if (!CreateTrampolineFunction(&ct)) {
        FILE* f = nullptr;
       // fopen_s(&f, "C:\\soyuz_hwbp.log", "a");
       // if (f) { fprintf(f, "HWBP: CreateTrampolineFunction failed for target %p\n", target); fclose(f); }
        FreeBuffer(info->trampoline);
        return false;
    }
    
    info->target = target;
    info->hook_func = hook_func;
    info->reg_index = slot;
    info->trampoline = ct.pTrampoline;
    if (original) *original = ct.pTrampoline;
    g_hwbp_slots[slot] = *info;
    UpdateAllThreadsHWBP(target, slot, true);
    
    FILE* f = nullptr;
    //fopen_s(&f, "C:\\soyuz_hwbp.log", "a");
   // if (f) { fprintf(f, "HWBP: Successfully installed hook for target %p in slot %d\n", target, slot); fclose(f); }
    
    return true;
}

void remove_hwbp_hook(HWBP_HookInfo* info) {
    if (info->target && info->reg_index != -1) {
        UpdateAllThreadsHWBP(nullptr, info->reg_index, false);
        g_hwbp_slots[info->reg_index].target = nullptr;
        if (info->trampoline) { FreeBuffer(info->trampoline); info->trampoline = nullptr; }
        info->reg_index = -1;
        info->target = nullptr;
    }
}

PVOID GetVEHHandle() { return g_veh_handle; }
void SetVEHHandle(PVOID h) { g_veh_handle = h; }
