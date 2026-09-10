#include "Globals.h"
#include <cmath>
#include <iostream>

bool g_fogEnabled = false;
float g_fogDistance = 500.0f;
float g_fogDensity = 0.01f;
float g_fogcolor[4] = { 0.85f, 0.85f, 0.85f, 1.0f };

bool g_wallHackEnabled = false;
bool g_nightModeEnabled = false;
float g_nightColor[3] = { 0.70f, 0.70f, 0.70f };
bool g_fullbrightEnabled = false;

bool g_chamsEnabled = false;
bool g_chamsThroughWalls = false;
bool g_restoreChams = false;
int g_chamsDepth = 0;
int g_chamsCallCount = 0;

bool g_zoomEnabled = false;
float g_zoomAmount = 1.0f;
bool g_zoomHoldMode = false;

bool g_fovChangerEnabled = false;
float g_fovMultiplier = 1.5f;

bool g_viewModelEnabled = false;
float g_viewModelX = 0.0f, g_viewModelY = 0.0f, g_viewModelZ = 0.0f, g_viewModelScaleX = 1.0f, g_viewModelScaleY = 1.0f, g_viewModelScaleZ = 1.0f;
float g_viewModelRotX = 0.0f, g_viewModelRotY = 0.0f, g_viewModelRotZ = 0.0f;

bool g_glassHand = false;
float g_glassAmount = 0.5f;

bool g_rustmeESP = false;

bool g_crosshairEnabled = false;
int g_crosshairType = 0;
float g_crosshairSize = 10.0f;
float g_crosshairThickness = 1.0f;
float g_crosshairColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
bool g_hideGameCrosshair = false;

bool g_espEnabled = true;
bool g_ammoEspEnabled = false;
bool g_ammoCometMode = false;
float g_ammoCometColor[4] = { 1.0f, 0.5f, 0.1f, 1.0f };
bool g_tracerEnabled = false;
float g_tracerColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
float g_tracerThickness = 1.5f;
bool g_tracerNewEnabled = false;
float g_tracerNewColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
float g_tracerNewThickness = 1.5f;

bool g_chinaHatEnabled = false;
int g_chinaHatMode = 0;
float g_chinaHatHeight = 0.35f;
float g_chinaHatRadius = 0.55f;
float g_chinaHatPosY = -1.75f;
float g_chinaHatColor[4] = { 1.0f, 0.85f, 0.2f, 0.9f };
float g_chinaHatLineWidth = 1.5f;
float g_chinaHatFillAlpha = 0.25f;

float g_espBoxFadeTime = 0.5f;
bool g_espBreadcrumbs = true;
float g_espBreadcrumbsTime = 5.0f;
bool g_espArrows = false;
float g_espArrowsRadius = 150.0f;
float g_espArrowsSize = 15.0f;

bool g_soundEspEnabled = false;
float g_soundEspDuration = 3.0f;
bool g_soundEspVisual = false;

bool g_openalEspEnabled = false;

bool g_coordsDisplayEnabled = false;
float g_coordsDisplayPosX = 10.0f, g_coordsDisplayPosY = 10.0f;
float g_coordsDisplayColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
float g_coordsDisplayBgColor[4] = { 0.0f, 0.0f, 0.0f, 0.7f };
bool g_coordsDisplayBackground = true;
bool g_coordsDisplayDecimals = true;
int g_coordsDisplayDecimalPlaces = 2;
bool g_coordsDisplayLabels = true;
bool g_coordsDisplayCompact = false;


bool g_patternCoordsEnabled = false;
float g_patternCoordsPosX = 10.0f, g_patternCoordsPosY = 200.0f;

float g_watermarkAnimTime = 0.0f;
float g_accentColor1[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
float g_accentColor2[4] = { 0.4f, 0.4f, 0.4f, 1.0f };
float g_accentSpeed = 0.8f;
ImVec4 g_currentAccentColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

bool g_notificationsEnabled = true;
bool g_showConfigMessage = false;
std::string g_configMessage = "";
double g_configMessageTime = 0.0;

bool g_aimEnabled = false;
bool g_aimBow = false;
float g_aimFov = 120.0f;
float g_aimSmoothX = 0.35f;
float g_aimSmoothY = 0.25f;
float g_aimSens = 0.5f;
bool g_aimSensEnabled = false;
float g_aimBulletSpeed = 80.0f;
float g_aimPing = 100.0f;
float g_aimGravity = 20.0f;
float g_aimPredictScale = 1.0f;
bool g_aimAnimatedFov = false;
float g_aimFovSize = 0.1f;
bool g_aimAutoDisable = false;
bool g_aimMcfEnabled = true;
float g_aimMcfMaxHorizontalDistance = 2.3f;
float g_aimMcfMaxVerticalDistance = 100.2f;
float g_aimCurrentFov = 0.0f;
glm::vec2 g_aimTargetPoint = glm::vec2(0.0f);
bool g_aimHasTarget = false;
float g_fovAnim = 20.0f;
float g_fovColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

bool g_autoSprintEnabled = true;
bool g_noFallEnabled = false;
bool g_sprintKeyHeld = false;
bool g_togglePressed = false;
bool g_isInScope = false;
float g_debugFovRatio = 0.0f;
float g_currentFovMultiplier = 1.0f;
float g_baseFovValue = 0.0f;
float g_playerSpeedComp = 4.3f;
glm::vec3 g_lastTranslate = glm::vec3(0.0f);

bool g_noHurtCamEnabled = false;

bool        g_hasPendingEntity        = false;
glm::vec3   g_pendingEntityTranslate  = glm::vec3(0.0f);
glm::mat4   g_pendingEntityModelview  = glm::mat4(1.0f);
glm::mat4   g_pendingEntityProjection = glm::mat4(1.0f);

float g_originalFogDensity = 0.0f;
float g_originalFogStart = 0.0f;
float g_originalFogEnd = 0.0f;
float g_originalFogMode = (float)GL_EXP;

int g_loadIdentityCount = 0;

float g_listenerPosX = 0.0f, g_listenerPosY = 0.0f, g_listenerPosZ = 0.0f;
bool g_useMemoryForPlayerPos = false;  // По умолчанию используем OpenAL (надежнее для Minecraft)
glm::vec3 g_playerPosFromMemory = glm::vec3(0.0f);
std::vector<SoundInfo> g_sounds;
std::mutex g_soundsMutex;

std::vector<Object> g_objects;
std::mutex g_objectsMutex;

bool g_hadYRotationBeforeScale = false;
std::vector<RotateCall> g_pendingRotations;

std::vector<EntityInfo> g_entities;
std::vector<NameTagInfo> g_nameTags;
std::mutex g_entitiesMutex;
std::vector<TracerNewTarget> g_tracerNewTargets;
std::mutex g_tracerNewTargetsMutex;
std::vector<CachedESPBox> g_cachedESPBoxes;
std::vector<CachedESPBox> g_prevCachedESPBoxes;
std::mutex g_cachedESPBoxesMutex;

std::vector<AimTarget> g_aimTargets;
std::vector<AimTrack> g_aimTracks;

glm::mat4 g_lastProjection3D = glm::mat4(1.0f);

glm::mat4 g_cameraMatrix = glm::mat4(1.0f);
glm::vec3 g_cameraPosGL = glm::vec3(0.0f);
int g_cameraSetupState = -1;
bool g_frameCameraCaptured = false;

double g_guiWidth = 0.0, g_guiHeight = 0.0;
bool g_imguiInitialized = false;
bool g_menuVisible = true;
bool g_unloading = false;
std::atomic<bool> g_renderingFrame{ false };
HWND g_hwnd = nullptr;
WNDPROC g_originalWndProc = nullptr;
ImFont* g_fontMain = nullptr;
ImFont* g_fontSmall = nullptr;
ImFont* g_fontMedium = nullptr;

// UI Elements visibility
bool g_showWatermark = true;
bool g_showArrayList = true;
bool g_showKeyBinds = true;

// KeyBinds position
float g_keyBindsX = 0.0f;
float g_keyBindsY = 0.0f;
bool g_keyBindsInitialized = false;

bool g_openglLoaded = false;
bool g_openglFuncsLoaded = false;
bool g_glHooksInstalled = false;
bool g_winmmLoaded = false;
bool g_timeGetTimeHooked = false;
bool g_openalLoaded = false;
bool g_openalFuncsLoaded = false;

bool g_ambince = true;
float g_ambinceColor[4] = { 0.8f, 0.5f, 0.6f, 1.0f };
bool g_sky = true;
bool g_world = true;
float g_density = 10.0f;
bool g_uiRenderingStarted = false;

void* original_glOrtho_addr = nullptr;
void* original_glScalef_addr = nullptr;
void* original_glTranslatef_addr = nullptr;
void* original_glRotatef_addr = nullptr;
void* original_wglSwapBuffers_addr = nullptr;
void* original_glLoadIdentity_addr = nullptr;
void* original_glPushMatrix_addr = nullptr;
void* original_glPopMatrix_addr = nullptr;
void* original_glEnable_addr = nullptr;
void* original_glDisable_addr = nullptr;
void* original_glColor4f_addr = nullptr;
void* original_glColor3f_addr = nullptr;
void* original_glTranslated_addr = nullptr;
void* original_glClear_addr = nullptr;
void* original_glClearColor_addr = nullptr;
void* original_glFogfv_addr = nullptr;
void* original_glFogf_addr = nullptr;
void* original_glFrustum_addr = nullptr;
void* original_glLoadMatrixf_addr = nullptr;
void* original_glLoadMatrixd_addr = nullptr;
void* original_glMultMatrixf_addr = nullptr;
void* original_glMultMatrixd_addr = nullptr;
void* original_glMatrixMode_addr = nullptr;
void* original_glGetFloatv_addr = nullptr;
void* original_wglGetProcAddress_addr = nullptr;
void* original_glGetQueryObjectiv_addr = nullptr;
void* original_timeGetTime_addr = nullptr;
void* original_alSourcePlay_addr = nullptr;
void* original_glLogicOp_addr = nullptr;
void* original_glBlendFunc_addr = nullptr;

HookInfo hook_ortho = {}, hook_scalef = {}, hook_translatef = {}, hook_rotatef = {};
HookInfo hook_swapbuffers = {}, hook_loadidentity = {}, hook_pushmatrix = {}, hook_popmatrix = {};
HookInfo hook_timegettime = {};
HookInfo hook_enable = {}, hook_disable = {}, hook_translated = {};
HookInfo hook_alSourcePlay = {};
HookInfo hook_frustum = {};
HookInfo hook_loadmatrixf = {}, hook_loadmatrixd = {};
HookInfo hook_multmatrixf = {}, hook_multmatrixd = {}, hook_matrixmode = {};
HookInfo hook_color4f = {}, hook_color3f = {};
HookInfo hook_logicop = {};
HookInfo hook_blendfunc = {};

HWBP_HookInfo hwbp_clear = {}, hwbp_clearcolor = {}, hwbp_fogfv = {}, hwbp_fogf = {};

ToggleBind g_aimBind = { &g_aimEnabled, 0, false, false };
ToggleBind g_espBind = { &g_espEnabled, 0, false, false };
ToggleBind g_sprintBind = { &g_autoSprintEnabled, 0, false, false };
ToggleBind g_timerBind = { &TimerModule::g_timerEnabled, 0, false, false };
ToggleBind g_noFallBind = { &g_noFallEnabled, 0, false, false };
ToggleBind g_fogBind = { &g_fogEnabled, 0, false, false };
ToggleBind g_wallhackBind = { &g_wallHackEnabled, 0, false, false };
ToggleBind g_zoomBind = { &g_zoomEnabled, 0, false, false };
ToggleBind g_fullbrightBind = { &g_fullbrightEnabled, 0, false, false };
ToggleBind g_ammoEspBind = { &g_ammoEspEnabled, 0, false, false };
ToggleBind g_tracerBind = { &g_tracerEnabled, 0, false, false };
ToggleBind g_tracerNewBind = { &g_tracerNewEnabled, 0, false, false };
ToggleBind g_chinaHatBind = { &g_chinaHatEnabled, 0, false, false };
ToggleBind g_nightModeBind = { &g_nightModeEnabled, 0, false, false };
ToggleBind g_coordsBind = { &g_openalEspEnabled, 0, false, false };
ToggleBind g_fovChangerBind = { &g_fovChangerEnabled, 0, false, false };
ToggleBind g_rustmeEspBind = { &g_rustmeESP, 0, false, false };

Config g_config;
int g_currentProfile = 0;
std::vector<std::string> g_profileList;

namespace TimerModule {
    bool g_timerEnabled = false;
}

HINSTANCE g_hInstance = nullptr;
