#pragma once
#include <Windows.h>
#include <cstdint>

// ---------------------------------------------------------------------------
// Fast trampoline hook subsystem.
//
// Hot paths use a direct inline jump to hook_fn, while orig_out points at a
// trampoline containing the relocated original prologue. If a safe inline patch
// cannot be written, the implementation falls back to the INT3+VEH path.
//
// Trampoline/disassembly is delegated to the project's CreateTrampolineFunction
// (handles CALL/JMP/JCC relative fixup, RIP-relative ModRM, patchAbove, etc.)
// ---------------------------------------------------------------------------

#ifdef _WIN64
static constexpr size_t TRAMP_PATCH_BYTES = 14u;
#else
static constexpr size_t TRAMP_PATCH_BYTES = 5u;
#endif

// Persistent metadata for one hook.
struct TrampolineHook {
    void*   target     = nullptr;          // hooked function address
    void*   hook_fn    = nullptr;          // detour address
    void*   trampoline = nullptr;          // allocated trampoline buffer
    uint8_t saved[TRAMP_PATCH_BYTES]{};   // original bytes before inline patch
    uint8_t original_byte = 0;             // byte replaced by INT3 fallback
    bool    patchAbove = false;
    bool    int3Fallback = false;
    bool    active     = false;
};

bool TrampolineHook_Install(void* target, void* hook_fn, void** orig_out, TrampolineHook* ctx);
void TrampolineHook_Remove(TrampolineHook* ctx);
