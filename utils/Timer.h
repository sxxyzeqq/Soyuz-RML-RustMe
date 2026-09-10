#pragma once
#include <windows.h>
#include <mmsystem.h>
#include <functional>

#pragma comment(lib, "winmm.lib")

namespace TimerModule {
    class Timer {
    public:
        using Callback = std::function<void()>;

        Timer() : m_timerId(0), m_period(1) {
            timeBeginPeriod(m_period);
        }

        ~Timer() {
            Stop();
            timeEndPeriod(m_period);
        }

        bool Start(UINT delayMs, Callback callback) {
            Stop();
            m_callback = callback;
            m_timerId = timeSetEvent(
                delayMs, 
                m_period, 
                StaticCallback, 
                reinterpret_cast<DWORD_PTR>(this), 
                TIME_PERIODIC | TIME_CALLBACK_FUNCTION
            );
            return m_timerId != 0;
        }

        void Stop() {
            if (m_timerId != 0) {
                timeKillEvent(m_timerId);
                m_timerId = 0;
            }
        }

        static DWORD GetTimestamp() {
            return timeGetTime();
        }

        static void PrecisionSleep(DWORD ms) {
            if (ms == 0) return;
            DWORD start = timeGetTime();
            if (ms > 10) Sleep(ms - 5);
            while (timeGetTime() - start < ms) YieldProcessor();
        }

    private:
        static void CALLBACK StaticCallback(UINT uID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
            auto* pTimer = reinterpret_cast<Timer*>(dwUser);
            if (pTimer && pTimer->m_callback) pTimer->m_callback();
        }
        UINT m_timerId;
        UINT m_period;
        Callback m_callback;
    };

    // Simple lock implementation
    class TSimpleLock {
    public:
        TSimpleLock() : count(0), owner(GetCurrentThreadId()) {}
        unsigned long count;
        DWORD owner;
    };

    inline void lock(TSimpleLock& d) {
        auto tid = GetCurrentThreadId();
        if (d.owner != tid) {
            while (InterlockedExchange(&d.count, 1) != 0) {
                Sleep(0);
            }
            d.owner = tid;
        } else {
            InterlockedIncrement(&d.count);
        }
    }

    inline void unlock(TSimpleLock& d) {
        if (d.count == 1)
            d.owner = 0;
        InterlockedDecrement(&d.count);
    }

    // SpeedHack class template
    template<class T>
    class SpeedHackClass {
    private:
        double speed;
        T initialoffset;
        T initialtime;
    public:
        SpeedHackClass() : speed(1.0), initialoffset(0), initialtime(0) {}
        
        SpeedHackClass(T _initialtime, T _initialoffset, double _speed = 1.0)
            : speed(_speed), initialoffset(_initialoffset), initialtime(_initialtime) {}

        double get_speed() const { return speed; }

        T get(T currentTime) {
            T false_val = (T)((currentTime - initialtime) * speed) + initialoffset;
            return false_val;
        }

        void set_speed(double _speed) {
            speed = _speed;
        }
    };

    extern bool g_timerEnabled;
    inline float g_SpeedMultiplier = 1.0f;
    inline bool g_smartTimerEnabled = false;
    inline float g_smartTimerDuration = 3.0f; // секунды
    inline double g_timerActivationTime = 0.0;
    inline bool g_timerWasActive = false;
    
    typedef DWORD(WINAPI* timeGetTime_t)();
    inline timeGetTime_t o_timeGetTime = nullptr;
    
    inline TSimpleLock GTCLock;
    inline SpeedHackClass<DWORD> h_GetTime;

    inline DWORD WINAPI h_timeGetTime() {
        if (!o_timeGetTime) return timeGetTime();
        return h_GetTime.get(o_timeGetTime());
    }

    inline void InitSpeedhack() {
        GTCLock = TSimpleLock();
        
        if (o_timeGetTime) {
            DWORD initialtime = o_timeGetTime();
            DWORD initialoffset = initialtime;
            h_GetTime = SpeedHackClass<DWORD>(initialtime, initialoffset, 1.0);
        }
    }

    inline void UpdateSpeedHack(double speed) {
        lock(GTCLock);
        
        if (o_timeGetTime) {
            DWORD currentReal = o_timeGetTime();
            DWORD currentFake = h_GetTime.get(currentReal);
            h_GetTime = SpeedHackClass<DWORD>(currentReal, currentFake, speed);
        }
        
        unlock(GTCLock);
    }

    inline void UpdateTime() {
        // Проверяем Smart Timer
        if (g_smartTimerEnabled && g_timerEnabled) {
            double currentTime = (double)GetTickCount64() / 1000.0; // в секундах
            
            if (!g_timerWasActive) {
                // Таймер только что включился
                g_timerActivationTime = currentTime;
                g_timerWasActive = true;
            } else {
                // Проверяем прошло ли время
                double elapsed = currentTime - g_timerActivationTime;
                if (elapsed >= g_smartTimerDuration) {
                    // Время вышло, отключаем таймер
                    g_timerEnabled = false;
                    g_timerWasActive = false;
                }
            }
        } else if (!g_timerEnabled) {
            g_timerWasActive = false;
        }
        
        // Обновляем скорость
        if (g_timerEnabled) {
            UpdateSpeedHack((double)g_SpeedMultiplier);
        } else {
            UpdateSpeedHack(1.0);
        }
    }
}
