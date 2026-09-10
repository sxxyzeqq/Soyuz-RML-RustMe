#pragma once
#include <Windows.h>
#include <gl/GL.h>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <string>
#include <map>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <algorithm>

#include <TlHelp32.h>
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/ext/matrix_transform.hpp"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
#include "../utils/Timer.h"

#ifdef _WIN64
const size_t HOOK_SIZE = 14;
#define MIN_COPY_SIZE 14
#else
const size_t HOOK_SIZE = 5;
#define MIN_COPY_SIZE 5
#endif

struct HookInfo {
    void* target = nullptr;
    void* hook_func = nullptr;
    byte original_bytes[HOOK_SIZE];
    BYTE original_byte = 0;
    void* trampoline = nullptr;
    bool patchAbove = false;
    bool int3 = false;
};


struct HWBP_HookInfo {
    void* target = nullptr;
    void* hook_func = nullptr;
    void* trampoline = nullptr;
    int reg_index = -1;
};

struct INT3_HookInfo {
    void* target = nullptr;
    void* hook_func = nullptr;
    void* trampoline = nullptr;
    BYTE original_byte = 0;
};

struct RotateCall { float angle, x, y, z; };

struct EntityInfo {
    glm::mat4 projection;
    glm::mat4 modelview;
    glm::vec3 worldPos;
    glm::vec3 translatePos;
    glm::vec3 capturedListenerPos;
    glm::vec3 scale;
    bool hasWorldPos;
    bool isPlayer;
    bool isBear;
    bool isBot;
    bool isAmmo;
    bool wallCheckDone = false;
    bool visibleFromCamera = false;
    int  wallCheckRawHits = 0; // сколько точек прошло проверку — для трекинга
    std::vector<RotateCall> rotations;
};

struct TracerNewTarget {
    glm::mat4 projection;
    glm::mat4 modelview;
    glm::vec3 translatePos;
    glm::vec3 scale;
};

struct NameTagInfo {
    glm::mat4 modelview;
    glm::vec3 relativePos;
    glm::vec3 translatePos;
};

struct AimTarget {
    glm::vec2 screenPosition;
    glm::vec3 worldPosition;
    glm::vec3 worldVelocity;
    float distance;
    double timestamp;
    bool valid;
};

struct AimTrack {
    glm::vec3 lastWorldPos;
    glm::vec3 velocity;
    glm::vec3 velocityPerTick;
    double lastUpdateTime;
    bool active;
};

struct CachedESPBox {
    glm::vec3 worldPos;
    float height;
    float width;
    float centerX;
    float minY;
    float maxY;
    bool isPlayer;
    double lastSeen;
};

using Clear_fn = void(WINAPI*)(GLbitfield);
using ClearColor_fn = void(WINAPI*)(GLclampf, GLclampf, GLclampf, GLclampf);
using Fogfv_fn = void(WINAPI*)(GLenum, const GLfloat*);
using Fogf_fn = void(WINAPI*)(GLenum, GLfloat);
using Ortho_fn = void(WINAPI*)(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
using Scalef_fn = void(WINAPI*)(GLfloat, GLfloat, GLfloat);
using Translatef_fn = void(WINAPI*)(GLfloat, GLfloat, GLfloat);
using Rotatef_fn = void(WINAPI*)(GLfloat, GLfloat, GLfloat, GLfloat);
using wglSwapBuffers_fn = BOOL(WINAPI*)(HDC);
using LoadIdentity_fn = void(WINAPI*)();
using PushMatrix_fn = void(WINAPI*)();
using Enable_fn = void(WINAPI*)(GLenum);
using Disable_fn = void(WINAPI*)(GLenum);
using Translated_fn = void(WINAPI*)(GLdouble, GLdouble, GLdouble);
using LoadMatrixf_fn = void(WINAPI*)(const GLfloat*);
using LoadMatrixd_fn = void(WINAPI*)(const GLdouble*);
using MultMatrixf_fn = void(WINAPI*)(const GLfloat*);
using MultMatrixd_fn = void(WINAPI*)(const GLdouble*);
using MatrixMode_fn = void(WINAPI*)(GLenum);
using GetFloatv_fn = void(WINAPI*)(GLenum, GLfloat*);
using wglGetProcAddress_fn = PROC(WINAPI*)(LPCSTR);
using GetQueryObjectiv_fn = void(WINAPI*)(GLuint, GLenum, GLint*);
using PopMatrix_fn = void(WINAPI*)();
using Color4f_fn = void(WINAPI*)(GLfloat, GLfloat, GLfloat, GLfloat);
using Color3f_fn = void(WINAPI*)(GLfloat, GLfloat, GLfloat);
using Frustum_fn = void(WINAPI*)(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);

typedef float ALfloat;
typedef int ALenum;
typedef int ALboolean;
typedef int ALsizei;
typedef unsigned int ALuint;

#define AL_POSITION 0x1004
#define AL_NO_ERROR 0
#define AL_BUFFER   0x1009
#define AL_FREQUENCY 0x2001
#define AL_SIZE      0x2004
#define AL_FORMAT_MONO8    0x1100
#define AL_FORMAT_MONO16   0x1101
#define AL_FORMAT_STEREO8  0x1102
#define AL_FORMAT_STEREO16 0x1103

typedef void(__cdecl* alGetListener3f_t)(ALenum, ALfloat*, ALfloat*, ALfloat*);
using alGetError_fn = ALenum(__cdecl*)();
using alSourcePlay_fn = void(__cdecl*)(ALuint);
using alGetSource3f_fn = void(__cdecl*)(ALuint, ALenum, ALfloat*, ALfloat*, ALfloat*);
using alGetSourcei_fn = void(__cdecl*)(ALuint, ALenum, int*);
using alGetBufferi_fn = void(__cdecl*)(ALuint, ALenum, int*);
using alBufferData_fn = void(__cdecl*)(ALuint, ALenum, const void*, ALsizei, ALsizei);
using LogicOp_fn = void(WINAPI*)(GLenum);
using BlendFunc_fn = void(WINAPI*)(GLenum, GLenum);
extern glm::mat4 g_lastProjection3D;

extern bool g_fogEnabled;
extern float g_fogDistance;
extern float g_fogDensity;
extern float g_fogcolor[4];

extern bool g_wallHackEnabled;
extern bool g_nightModeEnabled;
extern float g_nightColor[3];
extern bool g_fullbrightEnabled;

extern bool g_chamsEnabled;
extern bool g_chamsThroughWalls;
extern bool g_restoreChams;
extern int g_chamsDepth;
extern int g_chamsCallCount;

extern bool g_zoomEnabled;
extern float g_zoomAmount;
extern bool g_zoomHoldMode;

extern bool g_fovChangerEnabled;
extern float g_fovMultiplier;

extern bool g_viewModelEnabled;
extern float g_viewModelX, g_viewModelY, g_viewModelZ, g_viewModelScaleX, g_viewModelScaleY, g_viewModelScaleZ;
extern float g_viewModelRotX, g_viewModelRotY, g_viewModelRotZ;

extern bool g_glassHand;
extern float g_glassAmount;

extern bool g_rustmeESP;

extern bool g_crosshairEnabled;
extern int g_crosshairType;
extern float g_crosshairSize;
extern float g_crosshairThickness;
extern float g_crosshairColor[4];
extern bool g_hideGameCrosshair;

extern bool g_espEnabled, g_ammoEspEnabled, g_tracerEnabled, g_espBreadcrumbs, g_espArrows;
extern bool g_ammoCometMode;
extern float g_ammoCometColor[4];
extern float g_tracerColor[4], g_tracerThickness, g_espBoxFadeTime, g_espBreadcrumbsTime, g_espArrowsRadius, g_espArrowsSize;
extern bool g_tracerNewEnabled;
extern float g_tracerNewColor[4];
extern float g_tracerNewThickness;

extern bool g_chinaHatEnabled;
extern int g_chinaHatMode;
extern float g_chinaHatHeight;
extern float g_chinaHatRadius;
extern float g_chinaHatPosY;
extern float g_chinaHatColor[4];
extern float g_chinaHatLineWidth;
extern float g_chinaHatFillAlpha;

extern bool g_soundEspEnabled;
extern float g_soundEspDuration;
extern bool g_soundEspVisual;

extern bool g_openalEspEnabled;

extern bool g_coordsDisplayEnabled;
extern float g_coordsDisplayPosX, g_coordsDisplayPosY;
extern float g_coordsDisplayColor[4];
extern float g_coordsDisplayBgColor[4];
extern bool g_coordsDisplayBackground;
extern bool g_coordsDisplayDecimals;
extern int g_coordsDisplayDecimalPlaces;
extern bool g_coordsDisplayLabels;
extern bool g_coordsDisplayCompact;

extern bool g_patternCoordsEnabled;
extern float g_patternCoordsPosX, g_patternCoordsPosY;

extern float g_watermarkAnimTime;
extern float g_accentColor1[4];
extern float g_accentColor2[4];
extern float g_accentSpeed;
extern ImVec4 g_currentAccentColor;

extern bool g_notificationsEnabled;
extern bool g_showConfigMessage;
extern std::string g_configMessage;
extern double g_configMessageTime;

extern bool g_aimEnabled;
extern bool g_aimBow;
extern float g_aimFov;
extern float g_aimSmoothX;
extern float g_aimSmoothY;
extern float g_aimSens;
extern bool g_aimSensEnabled;
extern float g_aimBulletSpeed;
extern float g_aimPing;
extern float g_aimGravity;
extern float g_aimPredictScale;
extern bool g_aimAnimatedFov;
extern float g_aimFovSize;
extern bool g_aimAutoDisable;
extern bool g_aimMcfEnabled;
extern float g_aimMcfMaxHorizontalDistance;
extern float g_aimMcfMaxVerticalDistance;
extern float g_aimFov;
extern float g_aimCurrentFov;
extern glm::vec2 g_aimTargetPoint;
extern bool g_aimHasTarget;
extern float g_fovAnim;
extern float g_fovColor[4];

extern bool g_autoSprintEnabled;
extern bool g_noFallEnabled;
extern bool g_sprintKeyHeld;
extern bool g_togglePressed;
extern bool g_isInScope;
extern float g_debugFovRatio;
extern float g_currentFovMultiplier;
extern float g_baseFovValue;
extern float g_playerSpeedComp;
extern glm::vec3 g_lastTranslate;

extern bool g_noHurtCamEnabled;

extern bool        g_hasPendingEntity;
extern glm::vec3   g_pendingEntityTranslate;
extern glm::mat4   g_pendingEntityModelview;
extern glm::mat4   g_pendingEntityProjection;

extern float g_originalFogDensity, g_originalFogStart, g_originalFogEnd, g_originalFogMode;

extern int g_loadIdentityCount;

extern float g_listenerPosX, g_listenerPosY, g_listenerPosZ;
extern bool g_useMemoryForPlayerPos;
extern glm::vec3 g_playerPosFromMemory;

struct SoundInfo {
    glm::vec3 pos{};
    ULONGLONG time = 0;
    ALuint source = 0;
    std::string name;
};
extern std::vector<SoundInfo> g_sounds;
extern std::mutex g_soundsMutex;

extern alGetListener3f_t alGetListener3f;
extern alSourcePlay_fn p_alSourcePlay;

struct Object {
    enum Type { Entity };
    Type m_type;
    glm::mat4 m_projection;
    glm::mat4 m_modelview;
    glm::vec3 relativePos;
    Object(Type t) : m_type(t), relativePos(0.0f) {
        glGetFloatv(GL_PROJECTION_MATRIX, glm::value_ptr(m_projection));
        glGetFloatv(GL_MODELVIEW_MATRIX, glm::value_ptr(m_modelview));
    }
};
extern std::vector<Object> g_objects;
extern std::mutex g_objectsMutex;

extern bool g_hadYRotationBeforeScale;
extern std::vector<RotateCall> g_pendingRotations;

extern std::vector<EntityInfo> g_entities;
extern std::vector<NameTagInfo> g_nameTags;
extern std::mutex g_entitiesMutex;
extern std::vector<TracerNewTarget> g_tracerNewTargets;
extern std::mutex g_tracerNewTargetsMutex;
extern std::vector<CachedESPBox> g_cachedESPBoxes;
extern std::vector<CachedESPBox> g_prevCachedESPBoxes;
extern std::mutex g_cachedESPBoxesMutex;

extern std::vector<AimTarget> g_aimTargets;
extern std::vector<AimTrack> g_aimTracks;

extern glm::mat4 g_cameraMatrix;
extern glm::vec3 g_cameraPosGL;
extern int g_cameraSetupState;
extern bool g_frameCameraCaptured;

extern bool g_ambince;
extern float g_ambinceColor[4];
extern bool g_sky;
extern bool g_world;
extern float g_density;

extern double g_guiWidth, g_guiHeight;
extern bool g_imguiInitialized;
extern bool g_menuVisible;
extern bool g_unloading;
extern bool g_uiRenderingStarted;
extern std::atomic<bool> g_renderingFrame;
extern HWND g_hwnd;
extern WNDPROC g_originalWndProc;
extern ImFont* g_fontMain;
extern ImFont* g_fontSmall;
extern ImFont* g_fontMedium;

extern bool g_showWatermark;
extern bool g_showArrayList;
extern bool g_showKeyBinds;

extern float g_keyBindsX;
extern float g_keyBindsY;
extern bool g_keyBindsInitialized;

extern bool g_openglLoaded;
extern bool g_openglFuncsLoaded;
extern bool g_glHooksInstalled;
extern bool g_winmmLoaded;
extern bool g_timeGetTimeHooked;
extern bool g_openalLoaded;
extern bool g_openalFuncsLoaded;

extern void* original_glOrtho_addr;
extern void* original_glScalef_addr;
extern void* original_glTranslatef_addr;
extern void* original_glRotatef_addr;
extern void* original_wglSwapBuffers_addr;
extern void* original_glLoadIdentity_addr;
extern void* original_glPushMatrix_addr;
extern void* original_glPopMatrix_addr;
extern void* original_glEnable_addr;
extern void* original_glDisable_addr;
extern void* original_glColor4f_addr;
extern void* original_glColor3f_addr;
extern void* original_glTranslated_addr;
extern void* original_glClear_addr;
extern void* original_glClearColor_addr;
extern void* original_glFogfv_addr;
extern void* original_glFogf_addr;
extern void* original_glFrustum_addr;
extern void* original_glLoadMatrixf_addr;
extern void* original_glLoadMatrixd_addr;
extern void* original_glMultMatrixf_addr;
extern void* original_glMultMatrixd_addr;
extern void* original_glMatrixMode_addr;
extern void* original_glGetFloatv_addr;
extern void* original_wglGetProcAddress_addr;
extern void* original_glGetQueryObjectiv_addr;
extern void* original_timeGetTime_addr;
extern void* original_alSourcePlay_addr;
extern void* original_glLogicOp_addr;
extern void* original_glBlendFunc_addr;

extern HookInfo hook_ortho, hook_scalef, hook_translatef, hook_rotatef;
extern HookInfo hook_swapbuffers, hook_loadidentity, hook_pushmatrix, hook_popmatrix;
extern HookInfo hook_timegettime;
extern HookInfo hook_enable, hook_disable, hook_translated;
extern HookInfo hook_alSourcePlay;
extern HookInfo hook_frustum;
extern HookInfo hook_loadmatrixf, hook_loadmatrixd;
extern HookInfo hook_multmatrixf, hook_multmatrixd, hook_matrixmode;
extern HookInfo hook_color4f, hook_color3f;
extern HookInfo hook_logicop;

extern HookInfo hook_blendfunc;

extern HWBP_HookInfo hwbp_clear, hwbp_clearcolor, hwbp_fogfv, hwbp_fogf;

struct ToggleBind {
    bool* pEnabled;
    int key;
    bool waiting;
    bool wasPressed;
};

extern ToggleBind g_aimBind, g_espBind, g_sprintBind, g_timerBind, g_noFallBind;
extern ToggleBind g_fogBind, g_wallhackBind, g_zoomBind, g_fullbrightBind;
extern ToggleBind g_ammoEspBind, g_tracerBind, g_tracerNewBind, g_chinaHatBind, g_nightModeBind, g_coordsBind, g_fovChangerBind, g_rustmeEspBind;

struct Config {
    bool aimEnabled = false;
    int aimBind = 0;
    bool aimBow = false;
    float aimBulletSpeed = 80.0f;
    float aimPing = 100.0f;
    float aimGravity = 20.0f;
    float aimPredictScale = 1.0f;
    float aimFov = 150.0f;
    bool aimAnimatedFov = false;
    float fovAnim = 20.0f;
    float fovColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float aimSmoothX = 0.1f;
    float aimSmoothY = 0.1f;
    float aimSens = 0.5f;
    bool aimSensEnabled = false;
    bool espEnabled = false;
    int espBind = 0;
    float espBoxFadeTime = 0.5f;
    bool espBreadcrumbs = true;
    float espBreadcrumbsTime = 5.0f;
    bool espArrows = false;
    float espArrowsRadius = 150.0f;
    float espArrowsSize = 15.0f;
    bool soundEspEnabled = false;
    float soundEspDuration = 3.0f;
    bool soundEspVisual = false;
    bool ammoEspEnabled = false;
    bool ammoCometMode = false;
    float ammoCometColor[4] = { 1.0f, 0.5f, 0.1f, 1.0f };
    bool wallHackEnabled = false;
    int wallhackBind = 0;
    bool nightModeEnabled = false;
    float nightColor[3] = { 0.1f, 0.1f, 0.15f };
    bool fullbrightEnabled = false;
    int fullbrightBind = 0;
    int ammoEspBind = 0;
    int tracerBind = 0;
    int nightModeBind = 0;
    int coordsBind = 0;
    int fovChangerBind = 0;
    int rustmeEspBind = 0;
    bool fogEnabled = false;
    int fogBind = 0;
    float fogcolor[4] = { 0.85f, 0.85f, 0.85f, 1.0f };
    bool crosshairEnabled = false;
    int crosshairType = 0;
    float crosshairColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float crosshairSize = 30.0f;
    float crosshairThickness = 1.0f;
    bool hideGameCrosshair = false;
    bool timerEnabled = false;
    int timerBind = 0;
    float speedMultiplier = 1.0f;
    bool smartTimerEnabled = false;
    float smartTimerDuration = 3.0f;
    bool autoSprintEnabled = false;
    bool noFallEnabled = false;
    int noFallBind = 0;
    bool noHurtCamEnabled = false;
    int sprintBind = 0;
    bool viewModelEnabled = false;
    float viewModelX = 0.0f;
    float viewModelY = 0.0f;
    float viewModelZ = 0.0f;
    float viewModelScaleX = 1.0f;
    float viewModelScaleY = 1.0f;
    float viewModelScaleZ = 1.0f;
    float viewModelRotX = 0.0f;
    float viewModelRotY = 0.0f;
    float viewModelRotZ = 0.0f;
    bool glassHand = false;
    float glassAmount = 0.5f;
    bool swingAnimEnabled = false;
    bool notificationsEnabled = true;
    bool openalEspEnabled = false;
    float accentColor1[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float accentColor2[4] = { 0.4f, 0.4f, 0.4f, 1.0f };
    float accentSpeed = 2.0f;
    bool fovChangerEnabled = false;
    float fovMultiplier = 1.5f;
    bool zoomEnabled = false;
    float zoomAmount = 1.0f;
    int zoomBind = 0;
    bool zoomHoldMode = false;
    bool chamsEnabled = false;
    bool chamsThroughWalls = false;
    bool useMemoryForPlayerPos = true;
    bool coordsDisplayEnabled = false;
    float coordsDisplayPosX = 10.0f;
    float coordsDisplayPosY = 10.0f;
    float coordsDisplayColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float coordsDisplayBgColor[4] = { 0.0f, 0.0f, 0.0f, 0.7f };
    bool coordsDisplayBackground = true;
    bool coordsDisplayDecimals = true;
    int coordsDisplayDecimalPlaces = 2;
    bool coordsDisplayLabels = true;
    bool coordsDisplayCompact = false;
    bool patternCoordsEnabled = false;
    float patternCoordsPosX = 10.0f;
    float patternCoordsPosY = 200.0f;
    float keyBindsX = 10.0f;
    float keyBindsY = 200.0f;
    bool keyBindsInitialized = false;
    // AspectRatio
    bool aspectRatioEnabled = false;
    float aspectRatioValue = 1.3f;
    // Tracers
    bool tracerEnabled = false;
    float tracerThickness = 1.5f;
    float tracerColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    // TracerNew
    bool tracerNewEnabled = false;
    int tracerNewBind = 0;
    float tracerNewThickness = 1.5f;
    float tracerNewColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    // ChinaHat
    bool chinaHatEnabled = false;
    int chinaHatBind = 0;
    int chinaHatMode = 0;
    float chinaHatHeight = 0.35f;
    float chinaHatRadius = 0.55f;
    float chinaHatPosY = -1.75f;
    float chinaHatColor[4] = { 1.0f, 0.85f, 0.2f, 0.9f };
    float chinaHatLineWidth = 1.5f;
    float chinaHatFillAlpha = 0.25f;
    // Ambience
    bool ambince = false;
    bool sky = false;
    bool world = false;
    float density = -5.0f;
    float ambinceColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    // RustMe ESP
    bool rustmeESP = false;
    // Chams Hand
    bool chamsHandEnabled = false;
    float chamsHandColorVis[4] = { 0.4f, 0.2f, 0.9f, 1.0f };
    float chamsHandColorHid[4] = { 0.2f, 0.1f, 0.4f, 1.0f };
    bool chamsHandXray = false;
    int chamsHandMode = 0;
    float chamsHandSpeed = 1.0f;
    // Player Chams
    bool playerChamsEnabled = false;
    float playerChamsColorVis[4] = { 1.0f, 0.2f, 0.2f, 1.0f };
    float playerChamsColorHid[4] = { 0.5f, 0.0f, 0.0f, 1.0f };
    bool playerChamsXray = false;
    int playerChamsMode = 1;
    float playerChamsSpeed = 1.0f;
    // Visibility (depth-based)
    bool aimVisibleOnly = false;
    bool aimAutoDisable = false;
    bool aimMcfEnabled = true;
    float aimMcfMaxHorizontalDistance = 2.3f;
    float aimMcfMaxVerticalDistance = 100.2f;
    bool espColorByVisibility = false;
    float espVisibleColor[4] = { 0.2f, 1.0f, 0.2f, 1.0f };
    float espHiddenColor[4]  = { 1.0f, 0.2f, 0.2f, 1.0f };
};

extern Config g_config;
extern int g_currentProfile;
extern std::vector<std::string> g_profileList;

extern HINSTANCE g_hInstance;
DWORD WINAPI UnloadThread(LPVOID lpParam);
void ShowConfigMessage(const std::string& msg);
