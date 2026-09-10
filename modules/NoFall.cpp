#include "NoFall.h"

#include <WinSock2.h>
#include <Windows.h>

#include "../core/Globals.h"
#include "../core/TrampolineHook.h"

using WSASend_t = int(WSAAPI*)(
    SOCKET,
    LPWSABUF,
    DWORD,
    LPDWORD,
    DWORD,
    LPWSAOVERLAPPED,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE
);

static TrampolineHook g_wsasendHook;
static TrampolineHook g_sendHook;
static WSASend_t o_WSASend = nullptr;
using send_t = int(WSAAPI*)(SOCKET, const char*, int, int);
static send_t o_send = nullptr;
static bool g_noFallWasEnabled = false;
static bool g_noFallBlocking = false;
static bool g_noFallSavedTimerEnabled = false;
static bool g_noFallSavedTimerState = false;
static float g_noFallSavedTimerSpeed = 1.0f;
static ULONGLONG g_noFallStartTick = 0;
static constexpr ULONGLONG kNoFallBlockMs = 3000;

static void NoFall_ApplyFreeze() {
    TimerModule::UpdateSpeedHack(0.0);
}

static void NoFall_RestoreFreeze() {
    const double speed = g_noFallSavedTimerState ? (double)g_noFallSavedTimerSpeed : 1.0;
    TimerModule::UpdateSpeedHack(speed);
    g_noFallSavedTimerEnabled = false;
}

void NoFall_Update() {
    const ULONGLONG now = GetTickCount64();

    if (g_noFallEnabled) {
        if (!g_noFallWasEnabled) {
            g_noFallWasEnabled = true;
            g_noFallBlocking = true;
            g_noFallStartTick = now;
            g_noFallSavedTimerEnabled = true;
            g_noFallSavedTimerState = TimerModule::g_timerEnabled;
            g_noFallSavedTimerSpeed = TimerModule::g_SpeedMultiplier;
        }

        if (g_noFallBlocking && now - g_noFallStartTick >= kNoFallBlockMs) {
            g_noFallBlocking = false;
            g_noFallEnabled = false;
            g_noFallWasEnabled = false;
            NoFall_RestoreFreeze();
        } else if (g_noFallBlocking) {
            NoFall_ApplyFreeze();
        }
    } else {
        if (g_noFallBlocking && g_noFallSavedTimerEnabled) {
            NoFall_RestoreFreeze();
        }
        g_noFallWasEnabled = false;
        g_noFallBlocking = false;
        g_noFallStartTick = 0;
    }
}

static bool NoFall_ShouldBlockPackets() {
    if (g_unloading) return false;
    NoFall_Update();
    return g_noFallBlocking;
}

static DWORD CountWSABufferBytes(LPWSABUF buffers, DWORD bufferCount) {
    DWORD total = 0;
    if (!buffers) return total;
    for (DWORD i = 0; i < bufferCount; ++i) {
        total += buffers[i].len;
    }
    return total;
}

static int WSAAPI hooked_WSASend(
    SOCKET s,
    LPWSABUF lpBuffers,
    DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesSent,
    DWORD dwFlags,
    LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
) {
    if (NoFall_ShouldBlockPackets()) {
        if (lpNumberOfBytesSent) {
            *lpNumberOfBytesSent = CountWSABufferBytes(lpBuffers, dwBufferCount);
        }
        return 0;
    }

    if (!o_WSASend) return SOCKET_ERROR;
    return o_WSASend(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
}

static int WSAAPI hooked_send(SOCKET s, const char* buf, int len, int flags) {
    if (NoFall_ShouldBlockPackets()) {
        return len;
    }

    if (!o_send) return SOCKET_ERROR;
    return o_send(s, buf, len, flags);
}

bool InitNoFallHook() {
    HMODULE ws2 = GetModuleHandleA("ws2_32.dll");
    if (!ws2) ws2 = LoadLibraryA("ws2_32.dll");
    if (!ws2) return false;

    bool installedAny = false;

    if (!g_wsasendHook.active) {
        void* target = reinterpret_cast<void*>(GetProcAddress(ws2, "WSASend"));
        if (target) {
            installedAny = TrampolineHook_Install(target, reinterpret_cast<void*>(hooked_WSASend), reinterpret_cast<void**>(&o_WSASend), &g_wsasendHook) || installedAny;
        }
    } else {
        installedAny = true;
    }

    if (!g_sendHook.active) {
        void* target = reinterpret_cast<void*>(GetProcAddress(ws2, "send"));
        if (target) {
            installedAny = TrampolineHook_Install(target, reinterpret_cast<void*>(hooked_send), reinterpret_cast<void**>(&o_send), &g_sendHook) || installedAny;
        }
    } else {
        installedAny = true;
    }

    return installedAny;
}

void ShutdownNoFallHook() {
    TrampolineHook_Remove(&g_wsasendHook);
    TrampolineHook_Remove(&g_sendHook);
    o_WSASend = nullptr;
    o_send = nullptr;
}
