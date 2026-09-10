#include "TrampolineHook.h"
#include "hooks/Hooks.h"
#include <cstring>
#pragma comment(lib, "bcrypt.lib")

#pragma comment(lib, "C:\\Users\\unknown\\Desktop\\vmp 3.9.4 ultimate\\Lib\\Windows\\VMProtectSDK64.lib")
#include "C:\\Users\\unknown\\Desktop\\vmp 3.9.4 ultimate\\Include\\C\\VMProtectSDK.h"

static bool WriteInlineJump(void* patchSite, void* hookFn, uint8_t* saved) {
    if (!patchSite || !hookFn || !saved) return false;

    std::memcpy(saved, patchSite, TRAMP_PATCH_BYTES);

    DWORD oldProtect = 0;
    if (!VirtualProtect(patchSite, TRAMP_PATCH_BYTES, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }

#if defined(_M_X64) || defined(__x86_64__)
    VMProtectBeginUltra("hhru");
    uint8_t patch[TRAMP_PATCH_BYTES] = {
        0x48, 0xB8,                         // mov rax, imm64
        0, 0, 0, 0, 0, 0, 0, 0,
        0xFF, 0xE0,                         // jmp rax
        0x90, 0x90
    };
    VMProtectEnd();
    
    *reinterpret_cast<uint64_t*>(patch + 2) = reinterpret_cast<uint64_t>(hookFn);
#else
    uint8_t patch[TRAMP_PATCH_BYTES] = { 0xE9, 0, 0, 0, 0 };
    *reinterpret_cast<int32_t*>(patch + 1) =
        static_cast<int32_t>(reinterpret_cast<uintptr_t>(hookFn) -
        (reinterpret_cast<uintptr_t>(patchSite) + TRAMP_PATCH_BYTES));
#endif

    std::memcpy(patchSite, patch, TRAMP_PATCH_BYTES);
    VirtualProtect(patchSite, TRAMP_PATCH_BYTES, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), patchSite, TRAMP_PATCH_BYTES);
    return true;
}

static void RestoreInlineJump(TrampolineHook* ctx) {
    
    if (!ctx || !ctx->target) return;

    void* patchSite = ctx->patchAbove
        ? static_cast<void*>(static_cast<uint8_t*>(ctx->target) - TRAMP_PATCH_BYTES)
        : ctx->target;

    DWORD oldProtect = 0;
    if (VirtualProtect(patchSite, TRAMP_PATCH_BYTES, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        std::memcpy(patchSite, ctx->saved, TRAMP_PATCH_BYTES);
        VirtualProtect(patchSite, TRAMP_PATCH_BYTES, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), patchSite, TRAMP_PATCH_BYTES);
    }
    
}

bool TrampolineHook_Install(void* target, void* hook_fn, void** orig_out, TrampolineHook* ctx) {
    
    if (!target || !hook_fn || !ctx) return false;
    if (ctx->active) return false;

    void* trampoline = AllocateBuffer(target);
    if (!trampoline) return false;

    TRAMPOLINE ct = {};
    ct.pTarget = target;
    ct.pDetour = hook_fn;
    ct.pTrampoline = trampoline;
    
    if (!CreateTrampolineFunction(&ct)) {
        FreeBuffer(trampoline);

        INT3_HookInfo int3{};
        if (!install_int3_hook(target, hook_fn, orig_out, &int3)) {
            return false;
        }

        ctx->target = int3.target;
        ctx->hook_fn = int3.hook_func;
        ctx->trampoline = int3.trampoline;
        ctx->original_byte = int3.original_byte;
        ctx->saved[0] = int3.original_byte;
        ctx->patchAbove = false;
        ctx->int3Fallback = true;
        ctx->active = true;
        return true;
    }
    

    
    if (ct.patchAbove) {
        FreeBuffer(trampoline);

        INT3_HookInfo int3{};
        if (!install_int3_hook(target, hook_fn, orig_out, &int3)) {
            return false;
        }

        ctx->target = int3.target;
        ctx->hook_fn = int3.hook_func;
        ctx->trampoline = int3.trampoline;
        ctx->original_byte = int3.original_byte;
        ctx->saved[0] = int3.original_byte;
        ctx->patchAbove = false;
        ctx->int3Fallback = true;
        ctx->active = true;
        return true;
    }
    
    if (!WriteInlineJump(target, hook_fn, ctx->saved)) {
        FreeBuffer(trampoline);
        return false;
    }
    

    ctx->target = target;
    ctx->hook_fn = hook_fn;
    ctx->trampoline = ct.pTrampoline;
    ctx->original_byte = ctx->saved[0];
    ctx->patchAbove = false;
    ctx->int3Fallback = false;
    ctx->active = true;
    if (orig_out) *orig_out = ct.pTrampoline;
    return true;
}

void TrampolineHook_Remove(TrampolineHook* ctx) {
    if (!ctx || !ctx->active || !ctx->target) return;

    if (ctx->int3Fallback) {
        INT3_HookInfo int3{};
        int3.target = ctx->target;
        int3.hook_func = ctx->hook_fn;
        int3.trampoline = ctx->trampoline;
        int3.original_byte = static_cast<BYTE>(ctx->original_byte);
        remove_int3_hook(&int3);
    } else {
        RestoreInlineJump(ctx);
        if (ctx->trampoline) {
            FreeBuffer(ctx->trampoline);
        }
    }

    ctx->target = nullptr;
    ctx->hook_fn = nullptr;
    ctx->trampoline = nullptr;
    ctx->original_byte = 0;
    ctx->patchAbove = false;
    ctx->int3Fallback = false;
    ctx->active = false;
    std::memset(ctx->saved, 0, sizeof(ctx->saved));
}
