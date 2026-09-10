#include "Config.h"
#include "hooks/OpenAL.h"
#include "../modules/AspectRatio.h"
#include "../modules/ChamsHand.h"
#include "../modules/PlayerChams.h"
#include "../modules/WallCheck.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <shlobj.h>

extern ToggleBind g_aimBind, g_espBind, g_sprintBind, g_timerBind, g_noFallBind, g_fogBind, g_wallhackBind, g_zoomBind;

void ShowConfigMessage(const std::string& msg) {
    g_configMessage = msg;
    g_showConfigMessage = true;
    g_configMessageTime = std::chrono::duration_cast<std::chrono::duration<double>>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

static std::string GetConfigDir() {
    char path[MAX_PATH] = {};
    SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path);
    std::string dir = std::string(path) + "\\Microsoft\\Windows\\Themes\\";
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir;
}

std::string GetProfilePath(int index) {
    std::string dir = GetConfigDir();
    if (index == 0) return dir + "soyuz.cfg";
    return dir + "soyuz_profile_" + std::to_string(index) + ".cfg";
}

void RefreshProfiles() {
    g_profileList.clear();
    g_profileList.push_back("Default");
    for (int i = 1; i <= 10; i++) {
        std::string path = GetProfilePath(i);
        std::ifstream f(path);
        if (f.good()) g_profileList.push_back("Profile " + std::to_string(i));
    }
}

void SaveConfig() {
    std::ofstream file(GetProfilePath(g_currentProfile));
    if (!file.is_open()) return;
    file << "aimEnabled=" << g_config.aimEnabled << "\n";
    file << "aimBind=" << g_config.aimBind << "\n";
    file << "aimBow=" << g_config.aimBow << "\n";
    file << "aimBulletSpeed=" << g_config.aimBulletSpeed << "\n";
    file << "aimPing=" << g_config.aimPing << "\n";
    file << "aimGravity=" << g_config.aimGravity << "\n";
    file << "aimPredictScale=" << g_config.aimPredictScale << "\n";
    file << "aimFov=" << g_config.aimFov << "\n";
    file << "aimAnimatedFov=" << g_config.aimAnimatedFov << "\n";
    file << "fovAnim=" << g_config.fovAnim << "\n";
    file << "fovColor=" << g_config.fovColor[0] << "," << g_config.fovColor[1] << "," << g_config.fovColor[2] << "," << g_config.fovColor[3] << "\n";
    file << "aimSmoothX=" << g_config.aimSmoothX << "\n";
    file << "aimSmoothY=" << g_config.aimSmoothY << "\n";
    file << "aimSens=" << g_config.aimSens << "\n";
    file << "aimSensEnabled=" << g_config.aimSensEnabled << "\n";
    file << "espEnabled=" << g_config.espEnabled << "\n";
    file << "espBind=" << g_config.espBind << "\n";
    file << "espBoxFadeTime=" << g_config.espBoxFadeTime << "\n";
    file << "espBreadcrumbs=" << g_config.espBreadcrumbs << "\n";
    file << "espBreadcrumbsTime=" << g_config.espBreadcrumbsTime << "\n";
    file << "espArrows=" << g_config.espArrows << "\n";
    file << "espArrowsRadius=" << g_config.espArrowsRadius << "\n";
    file << "espArrowsSize=" << g_config.espArrowsSize << "\n";
    file << "soundEspEnabled=" << g_config.soundEspEnabled << "\n";
    file << "soundEspDuration=" << g_config.soundEspDuration << "\n";
    file << "soundEspVisual=" << g_config.soundEspVisual << "\n";
    file << "ammoEspEnabled=" << g_config.ammoEspEnabled << "\n";
    file << "ammoCometMode=" << g_config.ammoCometMode << "\n";
    file << "ammoCometColor=" << g_config.ammoCometColor[0] << "," << g_config.ammoCometColor[1] << "," << g_config.ammoCometColor[2] << "," << g_config.ammoCometColor[3] << "\n";
    file << "wallHackEnabled=" << g_config.wallHackEnabled << "\n";
    file << "wallhackBind=" << g_config.wallhackBind << "\n";
    file << "nightModeEnabled=" << g_config.nightModeEnabled << "\n";
    file << "fullbrightEnabled=" << g_config.fullbrightEnabled << "\n";
    file << "fullbrightBind=" << g_config.fullbrightBind << "\n";
    file << "fogEnabled=" << g_config.fogEnabled << "\n";
    file << "fogBind=" << g_config.fogBind << "\n";
    file << "fogcolor=" << g_config.fogcolor[0] << "," << g_config.fogcolor[1] << "," << g_config.fogcolor[2] << "," << g_config.fogcolor[3] << "\n";
    file << "crosshairEnabled=" << g_config.crosshairEnabled << "\n";
    file << "crosshairType=" << g_config.crosshairType << "\n";
    file << "crosshairColor=" << g_config.crosshairColor[0] << "," << g_config.crosshairColor[1] << "," << g_config.crosshairColor[2] << "," << g_config.crosshairColor[3] << "\n";
    file << "crosshairSize=" << g_config.crosshairSize << "\n";
    file << "crosshairThickness=" << g_config.crosshairThickness << "\n";
    file << "hideGameCrosshair=" << g_config.hideGameCrosshair << "\n";
    file << "timerEnabled=" << g_config.timerEnabled << "\n";
    file << "timerBind=" << g_config.timerBind << "\n";
    file << "speedMultiplier=" << g_config.speedMultiplier << "\n";
    file << "smartTimerEnabled=" << g_config.smartTimerEnabled << "\n";
    file << "smartTimerDuration=" << g_config.smartTimerDuration << "\n";
    file << "autoSprintEnabled=" << g_config.autoSprintEnabled << "\n";
    file << "noFallEnabled=" << g_config.noFallEnabled << "\n";
    file << "noFallBind=" << g_config.noFallBind << "\n";
    file << "noHurtCamEnabled=" << g_config.noHurtCamEnabled << "\n";
    file << "sprintBind=" << g_config.sprintBind << "\n";
    file << "viewModelEnabled=" << g_config.viewModelEnabled << "\n";
    file << "viewModelX=" << g_config.viewModelX << "\n";
    file << "viewModelY=" << g_config.viewModelY << "\n";
    file << "viewModelZ=" << g_config.viewModelZ << "\n";
    file << "viewModelScaleX=" << g_config.viewModelScaleX << "\n";
    file << "viewModelScaleY=" << g_config.viewModelScaleY << "\n";
    file << "viewModelScaleZ=" << g_config.viewModelScaleZ << "\n";
    file << "viewModelRotX=" << g_config.viewModelRotX << "\n";
    file << "viewModelRotY=" << g_config.viewModelRotY << "\n";
    file << "viewModelRotZ=" << g_config.viewModelRotZ << "\n";
    file << "glassHand=" << g_config.glassHand << "\n";
    file << "glassAmount=" << g_config.glassAmount << "\n";
    file << "notificationsEnabled=" << g_config.notificationsEnabled << "\n";
    file << "openalEspEnabled=" << g_config.openalEspEnabled << "\n";
    file << "ammoEspBind=" << g_config.ammoEspBind << "\n";
    file << "tracerBind=" << g_config.tracerBind << "\n";
    file << "tracerNewBind=" << g_config.tracerNewBind << "\n";
    file << "chinaHatBind=" << g_config.chinaHatBind << "\n";
    file << "nightModeBind=" << g_config.nightModeBind << "\n";
    file << "coordsBind=" << g_config.coordsBind << "\n";
    file << "fovChangerBind=" << g_config.fovChangerBind << "\n";
    file << "rustmeEspBind=" << g_config.rustmeEspBind << "\n";
    file << "accentColor1=" << g_config.accentColor1[0] << "," << g_config.accentColor1[1] << "," << g_config.accentColor1[2] << "," << g_config.accentColor1[3] << "\n";
    file << "accentColor2=" << g_config.accentColor2[0] << "," << g_config.accentColor2[1] << "," << g_config.accentColor2[2] << "," << g_config.accentColor2[3] << "\n";
    file << "accentSpeed=" << g_config.accentSpeed << "\n";
    file << "fovChangerEnabled=" << g_config.fovChangerEnabled << "\n";
    file << "fovMultiplier=" << g_config.fovMultiplier << "\n";
    file << "zoomEnabled=" << g_config.zoomEnabled << "\n";
    file << "zoomAmount=" << g_config.zoomAmount << "\n";
    file << "zoomBind=" << g_config.zoomBind << "\n";
    file << "zoomHoldMode=" << g_config.zoomHoldMode << "\n";
    file << "chamsEnabled=" << g_config.chamsEnabled << "\n";
    file << "chamsThroughWalls=" << g_config.chamsThroughWalls << "\n";
    file << "useMemoryForPlayerPos=" << g_config.useMemoryForPlayerPos << "\n";
    file << "coordsDisplayEnabled=" << g_config.coordsDisplayEnabled << "\n";
    file << "coordsDisplayPosX=" << g_config.coordsDisplayPosX << "\n";
    file << "coordsDisplayPosY=" << g_config.coordsDisplayPosY << "\n";
    file << "coordsDisplayColor=" << g_config.coordsDisplayColor[0] << "," << g_config.coordsDisplayColor[1] << "," << g_config.coordsDisplayColor[2] << "," << g_config.coordsDisplayColor[3] << "\n";
    file << "coordsDisplayBgColor=" << g_config.coordsDisplayBgColor[0] << "," << g_config.coordsDisplayBgColor[1] << "," << g_config.coordsDisplayBgColor[2] << "," << g_config.coordsDisplayBgColor[3] << "\n";
    file << "coordsDisplayBackground=" << g_config.coordsDisplayBackground << "\n";
    file << "coordsDisplayDecimals=" << g_config.coordsDisplayDecimals << "\n";
    file << "coordsDisplayDecimalPlaces=" << g_config.coordsDisplayDecimalPlaces << "\n";
    file << "coordsDisplayLabels=" << g_config.coordsDisplayLabels << "\n";
    file << "coordsDisplayCompact=" << g_config.coordsDisplayCompact << "\n";
    file << "showWatermark=" << g_showWatermark << "\n";
    file << "showArrayList=" << g_showArrayList << "\n";
    file << "showKeyBinds=" << g_showKeyBinds << "\n";
    file << "patternCoordsEnabled=" << g_config.patternCoordsEnabled << "\n";
    file << "patternCoordsPosX=" << g_config.patternCoordsPosX << "\n";
    file << "patternCoordsPosY=" << g_config.patternCoordsPosY << "\n";
    file << "keyBindsX=" << g_config.keyBindsX << "\n";
    file << "keyBindsY=" << g_config.keyBindsY << "\n";
    file << "keyBindsInitialized=" << g_config.keyBindsInitialized << "\n";
    file << "aspectRatioEnabled=" << g_config.aspectRatioEnabled << "\n";
    file << "aspectRatioValue=" << g_config.aspectRatioValue << "\n";
    file << "tracerEnabled=" << g_config.tracerEnabled << "\n";
    file << "tracerThickness=" << g_config.tracerThickness << "\n";
    file << "tracerColor=" << g_config.tracerColor[0] << "," << g_config.tracerColor[1] << "," << g_config.tracerColor[2] << "," << g_config.tracerColor[3] << "\n";
    file << "tracerNewEnabled=" << g_config.tracerNewEnabled << "\n";
    file << "tracerNewThickness=" << g_config.tracerNewThickness << "\n";
    file << "tracerNewColor=" << g_config.tracerNewColor[0] << "," << g_config.tracerNewColor[1] << "," << g_config.tracerNewColor[2] << "," << g_config.tracerNewColor[3] << "\n";
    file << "chinaHatEnabled=" << g_config.chinaHatEnabled << "\n";
    file << "chinaHatBind=" << g_config.chinaHatBind << "\n";
    file << "chinaHatMode=" << g_config.chinaHatMode << "\n";
    file << "chinaHatHeight=" << g_config.chinaHatHeight << "\n";
    file << "chinaHatRadius=" << g_config.chinaHatRadius << "\n";
    file << "chinaHatPosY=" << g_config.chinaHatPosY << "\n";
    file << "chinaHatColor=" << g_config.chinaHatColor[0] << "," << g_config.chinaHatColor[1] << "," << g_config.chinaHatColor[2] << "," << g_config.chinaHatColor[3] << "\n";
    file << "chinaHatLineWidth=" << g_config.chinaHatLineWidth << "\n";
    file << "chinaHatFillAlpha=" << g_config.chinaHatFillAlpha << "\n";
    file << "ambince=" << g_config.ambince << "\n";
    file << "ambSky=" << g_config.sky << "\n";
    file << "ambWorld=" << g_config.world << "\n";
    file << "ambDensity=" << g_config.density << "\n";
    file << "ambColor=" << g_config.ambinceColor[0] << "," << g_config.ambinceColor[1] << "," << g_config.ambinceColor[2] << "," << g_config.ambinceColor[3] << "\n";
    file << "rustmeESP=" << g_config.rustmeESP << "\n";
    file << "nightColor=" << g_config.nightColor[0] << "," << g_config.nightColor[1] << "," << g_config.nightColor[2] << "\n";
    file << "chamsHandEnabled=" << g_config.chamsHandEnabled << "\n";
    file << "chamsHandColorVis=" << g_config.chamsHandColorVis[0] << "," << g_config.chamsHandColorVis[1] << "," << g_config.chamsHandColorVis[2] << "," << g_config.chamsHandColorVis[3] << "\n";
    file << "chamsHandColorHid=" << g_config.chamsHandColorHid[0] << "," << g_config.chamsHandColorHid[1] << "," << g_config.chamsHandColorHid[2] << "," << g_config.chamsHandColorHid[3] << "\n";
    file << "chamsHandXray=" << g_config.chamsHandXray << "\n";
    file << "chamsHandMode=" << g_config.chamsHandMode << "\n";
    file << "chamsHandSpeed=" << g_config.chamsHandSpeed << "\n";
    file << "playerChamsEnabled=" << g_config.playerChamsEnabled << "\n";
    file << "playerChamsColorVis=" << g_config.playerChamsColorVis[0] << "," << g_config.playerChamsColorVis[1] << "," << g_config.playerChamsColorVis[2] << "," << g_config.playerChamsColorVis[3] << "\n";
    file << "playerChamsColorHid=" << g_config.playerChamsColorHid[0] << "," << g_config.playerChamsColorHid[1] << "," << g_config.playerChamsColorHid[2] << "," << g_config.playerChamsColorHid[3] << "\n";
    file << "playerChamsXray=" << g_config.playerChamsXray << "\n";
    file << "playerChamsMode=" << g_config.playerChamsMode << "\n";
    file << "playerChamsSpeed=" << g_config.playerChamsSpeed << "\n";
    file << "aimVisibleOnly=" << g_config.aimVisibleOnly << "\n";
    file << "aimAutoDisable=" << g_config.aimAutoDisable << "\n";
    file << "aimMcfEnabled=" << g_config.aimMcfEnabled << "\n";
    file << "aimMcfMaxHorizontalDistance=" << g_config.aimMcfMaxHorizontalDistance << "\n";
    file << "aimMcfMaxVerticalDistance=" << g_config.aimMcfMaxVerticalDistance << "\n";
    file << "espColorByVisibility=" << g_config.espColorByVisibility << "\n";
    file << "espVisibleColor=" << g_config.espVisibleColor[0] << "," << g_config.espVisibleColor[1] << "," << g_config.espVisibleColor[2] << "," << g_config.espVisibleColor[3] << "\n";
    file << "espHiddenColor=" << g_config.espHiddenColor[0] << "," << g_config.espHiddenColor[1] << "," << g_config.espHiddenColor[2] << "," << g_config.espHiddenColor[3] << "\n";
    file.close();

}

void LoadConfig() {
    std::ifstream file(GetProfilePath(g_currentProfile));
    if (!file.is_open()) return;
    g_config = Config{};
    std::string line;
    while (std::getline(file, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        if (key == "aimEnabled") g_config.aimEnabled = (value == "1");
        else if (key == "aimBind") g_config.aimBind = std::stoi(value);
        else if (key == "aimBow") g_config.aimBow = (value == "1");
        else if (key == "aimBulletSpeed") g_config.aimBulletSpeed = std::stof(value);
        else if (key == "aimPing") g_config.aimPing = std::stof(value);
        else if (key == "aimGravity") g_config.aimGravity = std::stof(value);
        else if (key == "aimPredictScale") g_config.aimPredictScale = std::stof(value);
        else if (key == "aimFov") g_config.aimFov = std::stof(value);
        else if (key == "aimAnimatedFov") g_config.aimAnimatedFov = (value == "1");
        else if (key == "fovAnim") g_config.fovAnim = std::stof(value);
        else if (key == "fovColor") {
            std::replace(value.begin(), value.end(), ',', ' ');
            std::istringstream iss(value);
            iss >> g_config.fovColor[0] >> g_config.fovColor[1] >> g_config.fovColor[2] >> g_config.fovColor[3];
        }
        else if (key == "aimSmoothX") g_config.aimSmoothX = std::stof(value);
        else if (key == "aimSmoothY") g_config.aimSmoothY = std::stof(value);
        else if (key == "aimSens") g_config.aimSens = std::stof(value);
        else if (key == "aimSensEnabled") g_config.aimSensEnabled = (value == "1");
        else if (key == "espEnabled") g_config.espEnabled = (value == "1");
        else if (key == "espBind") g_config.espBind = std::stoi(value);
        else if (key == "espBoxFadeTime") g_config.espBoxFadeTime = std::stof(value);
        else if (key == "espBreadcrumbs") g_config.espBreadcrumbs = (value == "1");
        else if (key == "espBreadcrumbsTime") g_config.espBreadcrumbsTime = std::stof(value);
        else if (key == "espArrows") g_config.espArrows = (value == "1");
        else if (key == "espArrowsRadius") g_config.espArrowsRadius = std::stof(value);
        else if (key == "espArrowsSize") g_config.espArrowsSize = std::stof(value);
        else if (key == "soundEspEnabled") g_config.soundEspEnabled = (value == "1");
        else if (key == "soundEspDuration") g_config.soundEspDuration = std::stof(value);
        else if (key == "soundEspVisual") g_config.soundEspVisual = (value == "1");
        else if (key == "ammoEspEnabled") g_config.ammoEspEnabled = (value == "1");
        else if (key == "ammoCometMode") g_config.ammoCometMode = (value == "1");
        else if (key == "ammoCometColor") {
            std::replace(value.begin(), value.end(), ',', ' ');
            std::istringstream iss(value);
            iss >> g_config.ammoCometColor[0] >> g_config.ammoCometColor[1] >> g_config.ammoCometColor[2] >> g_config.ammoCometColor[3];
        }
        else if (key == "wallHackEnabled") g_config.wallHackEnabled = (value == "1");
        else if (key == "wallhackBind") g_config.wallhackBind = std::stoi(value);
        else if (key == "nightModeEnabled") g_config.nightModeEnabled = (value == "1");
        else if (key == "fullbrightEnabled") g_config.fullbrightEnabled = (value == "1");
        else if (key == "fullbrightBind") g_config.fullbrightBind = std::stoi(value);
        else if (key == "fogEnabled") g_config.fogEnabled = (value == "1");
        else if (key == "fogBind") g_config.fogBind = std::stoi(value);
        else if (key == "fogcolor") {
            std::replace(value.begin(), value.end(), ',', ' ');
            std::istringstream iss(value);
            iss >> g_config.fogcolor[0] >> g_config.fogcolor[1] >> g_config.fogcolor[2] >> g_config.fogcolor[3];
        }
        else if (key == "crosshairEnabled") g_config.crosshairEnabled = (value == "1");
        else if (key == "crosshairType") g_config.crosshairType = std::stoi(value);
        else if (key == "crosshairColor") {
            std::replace(value.begin(), value.end(), ',', ' ');
            std::istringstream iss(value);
            iss >> g_config.crosshairColor[0] >> g_config.crosshairColor[1] >> g_config.crosshairColor[2] >> g_config.crosshairColor[3];
        }
        else if (key == "crosshairSize") g_config.crosshairSize = std::stof(value);
        else if (key == "crosshairThickness") g_config.crosshairThickness = std::stof(value);
        else if (key == "hideGameCrosshair") g_config.hideGameCrosshair = (value == "1");
        else if (key == "timerEnabled") g_config.timerEnabled = (value == "1");
        else if (key == "timerBind") g_config.timerBind = std::stoi(value);
        else if (key == "speedMultiplier") g_config.speedMultiplier = std::stof(value);
        else if (key == "smartTimerEnabled") g_config.smartTimerEnabled = (value == "1");
        else if (key == "smartTimerDuration") g_config.smartTimerDuration = std::stof(value);
        else if (key == "autoSprintEnabled") g_config.autoSprintEnabled = (value == "1");
        else if (key == "noFallEnabled") g_config.noFallEnabled = (value == "1");
        else if (key == "noFallBind") g_config.noFallBind = std::stoi(value);
        else if (key == "noHurtCamEnabled") g_config.noHurtCamEnabled = (value == "1");

        // --- Разрыв цепочки else-if (C1061: MSVC limit on nesting) ---
        if (key == "sprintBind") g_config.sprintBind = std::stoi(value);
        else if (key == "viewModelEnabled") g_config.viewModelEnabled = (value == "1");
        else if (key == "viewModelX") g_config.viewModelX = std::stof(value);
        else if (key == "viewModelY") g_config.viewModelY = std::stof(value);
        else if (key == "viewModelZ") g_config.viewModelZ = std::stof(value);
        else if (key == "viewModelScaleX") g_config.viewModelScaleX = std::stof(value);
        else if (key == "viewModelScaleY") g_config.viewModelScaleY = std::stof(value);
        else if (key == "viewModelScaleZ") g_config.viewModelScaleZ = std::stof(value);
        else if (key == "viewModelRotX") g_config.viewModelRotX = std::stof(value);
        else if (key == "viewModelRotY") g_config.viewModelRotY = std::stof(value);
        else if (key == "viewModelRotZ") g_config.viewModelRotZ = std::stof(value);
        else if (key == "glassHand") g_config.glassHand = (value == "1");
        else if (key == "glassAmount") g_config.glassAmount = std::stof(value);
        else if (key == "notificationsEnabled") g_config.notificationsEnabled = (value == "1");
        else if (key == "openalEspEnabled") g_config.openalEspEnabled = (value == "1");
        else if (key == "accentColor1") {
            std::replace(value.begin(), value.end(), ',', ' ');
            std::istringstream iss(value);
            iss >> g_config.accentColor1[0] >> g_config.accentColor1[1] >> g_config.accentColor1[2] >> g_config.accentColor1[3];
        }
        else if (key == "accentColor2") {
            std::replace(value.begin(), value.end(), ',', ' ');
            std::istringstream iss(value);
            iss >> g_config.accentColor2[0] >> g_config.accentColor2[1] >> g_config.accentColor2[2] >> g_config.accentColor2[3];
        }
        else if (key == "accentSpeed") g_config.accentSpeed = std::stof(value);
        else if (key == "fovChangerEnabled") g_config.fovChangerEnabled = (value == "1");
        else if (key == "fovMultiplier") g_config.fovMultiplier = std::stof(value);
        else if (key == "zoomEnabled") g_config.zoomEnabled = (value == "1");
        else if (key == "zoomAmount") g_config.zoomAmount = std::stof(value);
        else if (key == "zoomBind") g_config.zoomBind = std::stoi(value);
        else if (key == "zoomHoldMode") g_config.zoomHoldMode = (value == "1");
        else if (key == "chamsEnabled") g_config.chamsEnabled = (value == "1");
        else if (key == "chamsThroughWalls") g_config.chamsThroughWalls = (value == "1");
        else if (key == "useMemoryForPlayerPos") g_config.useMemoryForPlayerPos = (value == "1");
        else if (key == "coordsDisplayEnabled") g_config.coordsDisplayEnabled = (value == "1");
        else if (key == "coordsDisplayPosX") g_config.coordsDisplayPosX = std::stof(value);
        else if (key == "coordsDisplayPosY") g_config.coordsDisplayPosY = std::stof(value);
        else if (key == "coordsDisplayColor") {
            std::replace(value.begin(), value.end(), ',', ' ');
            std::istringstream iss(value);
            iss >> g_config.coordsDisplayColor[0] >> g_config.coordsDisplayColor[1] >> g_config.coordsDisplayColor[2] >> g_config.coordsDisplayColor[3];
        }
        else if (key == "coordsDisplayBgColor") {
            std::replace(value.begin(), value.end(), ',', ' ');
            std::istringstream iss(value);
            iss >> g_config.coordsDisplayBgColor[0] >> g_config.coordsDisplayBgColor[1] >> g_config.coordsDisplayBgColor[2] >> g_config.coordsDisplayBgColor[3];
        }
        else if (key == "coordsDisplayBackground") g_config.coordsDisplayBackground = (value == "1");
        else if (key == "coordsDisplayDecimals") g_config.coordsDisplayDecimals = (value == "1");
        else if (key == "coordsDisplayDecimalPlaces") g_config.coordsDisplayDecimalPlaces = std::stoi(value);
        else if (key == "coordsDisplayLabels") g_config.coordsDisplayLabels = (value == "1");
        else if (key == "coordsDisplayCompact") g_config.coordsDisplayCompact = (value == "1");
        else if (key == "showWatermark") g_showWatermark = (value == "1");
        else if (key == "showArrayList") g_showArrayList = (value == "1");
        else if (key == "showKeyBinds") g_showKeyBinds = (value == "1");
        else if (key == "patternCoordsEnabled") g_config.patternCoordsEnabled = (value == "1");
        else if (key == "patternCoordsPosX") g_config.patternCoordsPosX = std::stof(value);
        else if (key == "patternCoordsPosY") g_config.patternCoordsPosY = std::stof(value);
        else if (key == "keyBindsX") g_config.keyBindsX = std::stof(value);
        else if (key == "keyBindsY") g_config.keyBindsY = std::stof(value);
        else if (key == "keyBindsInitialized") g_config.keyBindsInitialized = (value == "1");
        else if (key == "keyBindsInitialized") g_config.keyBindsInitialized = (value == "1");
        else if (key == "ammoEspBind") g_config.ammoEspBind = std::stoi(value);
        else if (key == "tracerBind") g_config.tracerBind = std::stoi(value);
        else if (key == "tracerNewBind") g_config.tracerNewBind = std::stoi(value);
        else if (key == "chinaHatBind") g_config.chinaHatBind = std::stoi(value);
        else if (key == "nightModeBind") g_config.nightModeBind = std::stoi(value);
        else if (key == "coordsBind") g_config.coordsBind = std::stoi(value);
        else if (key == "fovChangerBind") g_config.fovChangerBind = std::stoi(value);
        else if (key == "rustmeEspBind") g_config.rustmeEspBind = std::stoi(value);
        else if (key == "aspectRatioEnabled") g_config.aspectRatioEnabled = (value == "1");
        else if (key == "aspectRatioValue") g_config.aspectRatioValue = std::stof(value);
        else if (key == "tracerEnabled") g_config.tracerEnabled = (value == "1");
        else if (key == "tracerThickness") g_config.tracerThickness = std::stof(value);
        else if (key == "tracerColor") {
            std::replace(value.begin(), value.end(), ',', ' ');
            std::istringstream iss(value);
            iss >> g_config.tracerColor[0] >> g_config.tracerColor[1] >> g_config.tracerColor[2] >> g_config.tracerColor[3];
        }
        else if (key == "tracerNewEnabled") g_config.tracerNewEnabled = (value == "1");
        else if (key == "tracerNewThickness") g_config.tracerNewThickness = std::stof(value);
        else if (key == "tracerNewColor") {
            std::replace(value.begin(), value.end(), ',', ' ');
            std::istringstream iss(value);
            iss >> g_config.tracerNewColor[0] >> g_config.tracerNewColor[1] >> g_config.tracerNewColor[2] >> g_config.tracerNewColor[3];
        }
        else if (key == "chinaHatEnabled") g_config.chinaHatEnabled = (value == "1");
        else if (key == "chinaHatBind") g_config.chinaHatBind = std::stoi(value);
        else if (key == "chinaHatMode") g_config.chinaHatMode = std::stoi(value);
        else if (key == "chinaHatHeight") g_config.chinaHatHeight = std::stof(value);
        else if (key == "chinaHatRadius") g_config.chinaHatRadius = std::stof(value);
        else if (key == "chinaHatPosY") g_config.chinaHatPosY = std::stof(value);
        else if (key == "chinaHatColor") {
            std::replace(value.begin(), value.end(), ',', ' ');
            std::istringstream iss(value);
            iss >> g_config.chinaHatColor[0] >> g_config.chinaHatColor[1] >> g_config.chinaHatColor[2] >> g_config.chinaHatColor[3];
        }
        else if (key == "chinaHatLineWidth") g_config.chinaHatLineWidth = std::stof(value);
        else if (key == "chinaHatFillAlpha") g_config.chinaHatFillAlpha = std::stof(value);
        else if (key == "ambince") g_config.ambince = (value == "1");
        else if (key == "ambSky") g_config.sky = (value == "1");
        else if (key == "ambWorld") g_config.world = (value == "1");
        else if (key == "ambDensity") g_config.density = std::stof(value);
        else if (key == "ambColor") {
            std::replace(value.begin(), value.end(), ',', ' ');
            std::istringstream iss(value);
            iss >> g_config.ambinceColor[0] >> g_config.ambinceColor[1] >> g_config.ambinceColor[2] >> g_config.ambinceColor[3];
        }
        else if (key == "rustmeESP") g_config.rustmeESP = (value == "1");
        else if (key == "nightColor") {
            std::replace(value.begin(), value.end(), ',', ' ');
            std::istringstream iss(value);
            iss >> g_config.nightColor[0] >> g_config.nightColor[1] >> g_config.nightColor[2];
        }
        else if (key == "chamsHandEnabled") g_config.chamsHandEnabled = (value == "1");
        else if (key == "chamsHandColorVis") {
            std::replace(value.begin(), value.end(), ',', ' ');
            std::istringstream iss(value);
            iss >> g_config.chamsHandColorVis[0] >> g_config.chamsHandColorVis[1] >> g_config.chamsHandColorVis[2] >> g_config.chamsHandColorVis[3];
        }
        else if (key == "chamsHandColorHid") {
            std::replace(value.begin(), value.end(), ',', ' ');
            std::istringstream iss(value);
            iss >> g_config.chamsHandColorHid[0] >> g_config.chamsHandColorHid[1] >> g_config.chamsHandColorHid[2] >> g_config.chamsHandColorHid[3];
        }
        else if (key == "chamsHandXray") g_config.chamsHandXray = (value == "1");
        else if (key == "chamsHandMode") g_config.chamsHandMode = std::stoi(value);
        else if (key == "chamsHandSpeed") g_config.chamsHandSpeed = std::stof(value);
        else if (key == "playerChamsEnabled") g_config.playerChamsEnabled = (value == "1");
        else if (key == "playerChamsColorVis") {
            std::replace(value.begin(), value.end(), ',', ' ');
            std::istringstream iss(value);
            iss >> g_config.playerChamsColorVis[0] >> g_config.playerChamsColorVis[1] >> g_config.playerChamsColorVis[2] >> g_config.playerChamsColorVis[3];
        }
        else if (key == "playerChamsColorHid") {
            std::replace(value.begin(), value.end(), ',', ' ');
            std::istringstream iss(value);
            iss >> g_config.playerChamsColorHid[0] >> g_config.playerChamsColorHid[1] >> g_config.playerChamsColorHid[2] >> g_config.playerChamsColorHid[3];
        }
        else if (key == "playerChamsXray") g_config.playerChamsXray = (value == "1");
        else if (key == "playerChamsMode") g_config.playerChamsMode = std::stoi(value);
        else if (key == "playerChamsSpeed") g_config.playerChamsSpeed = std::stof(value);
        else if (key == "aimVisibleOnly") g_config.aimVisibleOnly = (value == "1");
        else if (key == "aimAutoDisable") g_config.aimAutoDisable = (value == "1");
        else if (key == "aimMcfEnabled") g_config.aimMcfEnabled = (value == "1");
        else if (key == "aimMcfMaxHorizontalDistance") g_config.aimMcfMaxHorizontalDistance = std::stof(value);
        else if (key == "aimMcfMaxVerticalDistance") g_config.aimMcfMaxVerticalDistance = std::stof(value);
        else if (key == "espColorByVisibility") g_config.espColorByVisibility = (value == "1");
        else if (key == "espVisibleColor") {
            std::replace(value.begin(), value.end(), ',', ' ');
            std::istringstream iss(value);
            iss >> g_config.espVisibleColor[0] >> g_config.espVisibleColor[1] >> g_config.espVisibleColor[2] >> g_config.espVisibleColor[3];
        }
        else if (key == "espHiddenColor") {
            std::replace(value.begin(), value.end(), ',', ' ');
            std::istringstream iss(value);
            iss >> g_config.espHiddenColor[0] >> g_config.espHiddenColor[1] >> g_config.espHiddenColor[2] >> g_config.espHiddenColor[3];
        }
    }

    file.close();
    g_aimEnabled = g_config.aimEnabled;
    g_aimBind.key = g_config.aimBind;
    g_aimBow = g_config.aimBow;
    g_aimBulletSpeed = g_config.aimBulletSpeed;
    g_aimPing = g_config.aimPing;
    g_aimGravity = g_config.aimGravity;
    g_aimPredictScale = g_config.aimPredictScale;
    g_aimFov = g_config.aimFov;
    g_aimAnimatedFov = g_config.aimAnimatedFov;
    g_fovAnim = g_config.fovAnim;
    memcpy(g_fovColor, g_config.fovColor, sizeof(g_fovColor));
    g_aimSmoothX = g_config.aimSmoothX;
    g_aimSmoothY = g_config.aimSmoothY;
    g_aimSens = g_config.aimSens;
    g_aimSensEnabled = g_config.aimSensEnabled;
    g_espEnabled = g_config.espEnabled;
    g_espBind.key = g_config.espBind;
    g_espBoxFadeTime = g_config.espBoxFadeTime;
    g_espBreadcrumbs = g_config.espBreadcrumbs;
    g_espBreadcrumbsTime = g_config.espBreadcrumbsTime;
    g_espArrows = g_config.espArrows;
    g_espArrowsRadius = g_config.espArrowsRadius;
    g_espArrowsSize = g_config.espArrowsSize;
    g_soundEspEnabled = g_config.soundEspEnabled;
    g_soundEspDuration = g_config.soundEspDuration;
    g_soundEspVisual = g_config.soundEspVisual;
    g_ammoEspEnabled = g_config.ammoEspEnabled;
    g_ammoCometMode = g_config.ammoCometMode;
    memcpy(g_ammoCometColor, g_config.ammoCometColor, sizeof(g_ammoCometColor));
    g_wallHackEnabled = g_config.wallHackEnabled;
    g_wallhackBind.key = g_config.wallhackBind;
    g_nightModeEnabled = g_config.nightModeEnabled;
    g_fullbrightEnabled = g_config.fullbrightEnabled;
    g_fullbrightBind.key = g_config.fullbrightBind;
    g_fogEnabled = g_config.fogEnabled;
    g_fogBind.key = g_config.fogBind;
    memcpy(g_fogcolor, g_config.fogcolor, sizeof(g_fogcolor));
    g_crosshairEnabled = g_config.crosshairEnabled;
    g_crosshairType = g_config.crosshairType;
    memcpy(g_crosshairColor, g_config.crosshairColor, sizeof(g_crosshairColor));
    g_crosshairSize = g_config.crosshairSize;
    g_crosshairThickness = g_config.crosshairThickness;
    g_hideGameCrosshair = g_config.hideGameCrosshair;
    TimerModule::g_timerEnabled = g_config.timerEnabled;
    g_timerBind.key = g_config.timerBind;
    TimerModule::g_SpeedMultiplier = g_config.speedMultiplier;
    TimerModule::g_smartTimerEnabled = g_config.smartTimerEnabled;
    TimerModule::g_smartTimerDuration = g_config.smartTimerDuration;
    g_autoSprintEnabled = g_config.autoSprintEnabled;
    g_noFallEnabled = g_config.noFallEnabled;
    g_noFallBind.key = g_config.noFallBind;
    g_noHurtCamEnabled = g_config.noHurtCamEnabled;
    g_sprintBind.key = g_config.sprintBind;
    g_viewModelEnabled = g_config.viewModelEnabled;
    g_viewModelX = g_config.viewModelX;
    g_viewModelY = g_config.viewModelY;
    g_viewModelZ = g_config.viewModelZ;
    g_viewModelScaleX = g_config.viewModelScaleX;
    g_viewModelScaleY = g_config.viewModelScaleY;
    g_viewModelScaleZ = g_config.viewModelScaleZ;
    g_viewModelRotX = g_config.viewModelRotX;
    g_viewModelRotY = g_config.viewModelRotY;
    g_viewModelRotZ = g_config.viewModelRotZ;
    g_glassHand = g_config.glassHand;
    g_glassAmount = g_config.glassAmount;
    g_notificationsEnabled = g_config.notificationsEnabled;
    g_openalEspEnabled = g_config.openalEspEnabled;
    memcpy(g_accentColor1, g_config.accentColor1, sizeof(g_accentColor1));
    memcpy(g_accentColor2, g_config.accentColor2, sizeof(g_accentColor2));
    g_accentSpeed = g_config.accentSpeed;
    g_ammoEspBind.key = g_config.ammoEspBind;
    g_tracerBind.key = g_config.tracerBind;
    g_tracerNewBind.key = g_config.tracerNewBind;
    g_chinaHatBind.key = g_config.chinaHatBind;
    g_nightModeBind.key = g_config.nightModeBind;
    g_coordsBind.key = g_config.coordsBind;
    g_fovChangerBind.key = g_config.fovChangerBind;
    g_rustmeEspBind.key = g_config.rustmeEspBind;
    g_fovChangerEnabled = g_config.fovChangerEnabled;
    g_fovMultiplier = g_config.fovMultiplier;
    g_zoomEnabled = g_config.zoomEnabled;
    g_zoomAmount = g_config.zoomAmount;
    g_zoomBind.key = g_config.zoomBind;
    g_zoomHoldMode = g_config.zoomHoldMode;
    g_chamsEnabled = g_config.chamsEnabled;
    g_chamsThroughWalls = g_config.chamsThroughWalls;
    g_useMemoryForPlayerPos = g_config.useMemoryForPlayerPos;
    g_coordsDisplayEnabled = g_config.coordsDisplayEnabled;
    g_coordsDisplayPosX = g_config.coordsDisplayPosX;
    g_coordsDisplayPosY = g_config.coordsDisplayPosY;
    memcpy(g_coordsDisplayColor, g_config.coordsDisplayColor, sizeof(g_coordsDisplayColor));
    memcpy(g_coordsDisplayBgColor, g_config.coordsDisplayBgColor, sizeof(g_coordsDisplayBgColor));
    g_coordsDisplayBackground = g_config.coordsDisplayBackground;
    g_coordsDisplayDecimals = g_config.coordsDisplayDecimals;
    g_coordsDisplayDecimalPlaces = g_config.coordsDisplayDecimalPlaces;
    g_coordsDisplayLabels = g_config.coordsDisplayLabels;
    g_coordsDisplayCompact = g_config.coordsDisplayCompact;
    g_patternCoordsEnabled = g_config.patternCoordsEnabled;
    g_patternCoordsPosX = g_config.patternCoordsPosX;
    g_patternCoordsPosY = g_config.patternCoordsPosY;
    g_keyBindsX = g_config.keyBindsX;
    g_keyBindsY = g_config.keyBindsY;
    g_keyBindsInitialized = g_config.keyBindsInitialized;
    g_keyBindsInitialized = g_config.keyBindsInitialized;
    g_aspectRatioEnabled = g_config.aspectRatioEnabled;
    g_aspectRatioValue = g_config.aspectRatioValue;
    g_tracerEnabled = g_config.tracerEnabled;
    g_tracerThickness = g_config.tracerThickness;
    memcpy(g_tracerColor, g_config.tracerColor, sizeof(g_tracerColor));
    g_tracerNewEnabled = g_config.tracerNewEnabled;
    g_tracerNewThickness = g_config.tracerNewThickness;
    memcpy(g_tracerNewColor, g_config.tracerNewColor, sizeof(g_tracerNewColor));
    g_chinaHatEnabled = g_config.chinaHatEnabled;
    g_chinaHatMode = g_config.chinaHatMode;
    g_chinaHatHeight = g_config.chinaHatHeight;
    g_chinaHatRadius = g_config.chinaHatRadius;
    g_chinaHatPosY = g_config.chinaHatPosY;
    memcpy(g_chinaHatColor, g_config.chinaHatColor, sizeof(g_chinaHatColor));
    g_chinaHatLineWidth = g_config.chinaHatLineWidth;
    g_chinaHatFillAlpha = g_config.chinaHatFillAlpha;
    g_ambince = g_config.ambince;
    g_sky = g_config.sky;
    g_world = g_config.world;
    g_density = g_config.density;
    memcpy(g_ambinceColor, g_config.ambinceColor, sizeof(g_ambinceColor));
    g_rustmeESP = g_config.rustmeESP;
    memcpy(g_nightColor, g_config.nightColor, sizeof(g_nightColor));
    g_chamsHandEnabled = g_config.chamsHandEnabled;
    memcpy(g_chamsHandColorVis, g_config.chamsHandColorVis, sizeof(g_chamsHandColorVis));
    memcpy(g_chamsHandColorHid, g_config.chamsHandColorHid, sizeof(g_chamsHandColorHid));
    g_chamsHandXray = g_config.chamsHandXray;
    g_chamsHandMode = g_config.chamsHandMode;
    g_chamsHandSpeed = g_config.chamsHandSpeed;
    g_playerChamsEnabled = g_config.playerChamsEnabled;
    memcpy(g_playerChamsColorVis, g_config.playerChamsColorVis, sizeof(g_playerChamsColorVis));
    memcpy(g_playerChamsColorHid, g_config.playerChamsColorHid, sizeof(g_playerChamsColorHid));
    g_playerChamsXray = g_config.playerChamsXray;
    g_playerChamsMode = g_config.playerChamsMode;
    g_playerChamsSpeed = g_config.playerChamsSpeed;
    g_aimVisibleOnly = g_config.aimVisibleOnly;
    g_aimAutoDisable = g_config.aimAutoDisable;
    g_aimMcfEnabled = g_config.aimMcfEnabled;
    g_aimMcfMaxHorizontalDistance = g_config.aimMcfMaxHorizontalDistance;
    g_aimMcfMaxVerticalDistance = g_config.aimMcfMaxVerticalDistance;
    g_espColorByVisibility = g_config.espColorByVisibility;
    memcpy(g_espVisibleColor, g_config.espVisibleColor, sizeof(g_espVisibleColor));
    memcpy(g_espHiddenColor, g_config.espHiddenColor, sizeof(g_espHiddenColor));
}


void UpdateConfigFromVars() {
    g_config.aimEnabled = g_aimEnabled;
    g_config.aimBind = g_aimBind.key;
    g_config.aimBow = g_aimBow;
    g_config.aimBulletSpeed = g_aimBulletSpeed;
    g_config.aimPing = g_aimPing;
    g_config.aimGravity = g_aimGravity;
    g_config.aimPredictScale = g_aimPredictScale;
    g_config.aimFov = g_aimFov;
    g_config.aimAnimatedFov = g_aimAnimatedFov;
    g_config.fovAnim = g_fovAnim;
    memcpy(g_config.fovColor, g_fovColor, sizeof(g_config.fovColor));
    g_config.aimSmoothX = g_aimSmoothX;
    g_config.aimSmoothY = g_aimSmoothY;
    g_config.aimSens = g_aimSens;
    g_config.aimSensEnabled = g_aimSensEnabled;
    g_config.espEnabled = g_espEnabled;
    g_config.espBind = g_espBind.key;
    g_config.espBoxFadeTime = g_espBoxFadeTime;
    g_config.espBreadcrumbs = g_espBreadcrumbs;
    g_config.espBreadcrumbsTime = g_espBreadcrumbsTime;
    g_config.soundEspEnabled = g_soundEspEnabled;
    g_config.soundEspDuration = g_soundEspDuration;
    g_config.soundEspVisual = g_soundEspVisual;
    g_config.ammoEspEnabled = g_ammoEspEnabled;
    g_config.ammoCometMode = g_ammoCometMode;
    memcpy(g_config.ammoCometColor, g_ammoCometColor, sizeof(g_config.ammoCometColor));
    g_config.wallHackEnabled = g_wallHackEnabled;
    g_config.wallhackBind = g_wallhackBind.key;
    g_config.nightModeEnabled = g_nightModeEnabled;
    g_config.fullbrightEnabled = g_fullbrightEnabled;
    g_config.fullbrightBind = g_fullbrightBind.key;
    g_config.fogEnabled = g_fogEnabled;
    g_config.fogBind = g_fogBind.key;
    memcpy(g_config.fogcolor, g_fogcolor, sizeof(g_fogcolor));
    g_config.crosshairEnabled = g_crosshairEnabled;
    g_config.crosshairType = g_crosshairType;
    memcpy(g_config.crosshairColor, g_crosshairColor, sizeof(g_crosshairColor));
    g_config.crosshairSize = g_crosshairSize;
    g_config.crosshairThickness = g_crosshairThickness;
    g_config.hideGameCrosshair = g_hideGameCrosshair;
    g_config.timerEnabled = TimerModule::g_timerEnabled;
    g_config.timerBind = g_timerBind.key;
    g_config.speedMultiplier = TimerModule::g_SpeedMultiplier;
    g_config.smartTimerEnabled = TimerModule::g_smartTimerEnabled;
    g_config.smartTimerDuration = TimerModule::g_smartTimerDuration;
    g_config.autoSprintEnabled = g_autoSprintEnabled;
    g_config.noFallEnabled = g_noFallEnabled;
    g_config.noFallBind = g_noFallBind.key;
    g_config.noHurtCamEnabled = g_noHurtCamEnabled;
    g_config.sprintBind = g_sprintBind.key;
    g_config.viewModelEnabled = g_viewModelEnabled;
    g_config.viewModelX = g_viewModelX;
    g_config.viewModelY = g_viewModelY;
    g_config.viewModelZ = g_viewModelZ;
    g_config.viewModelScaleX = g_viewModelScaleX;
    g_config.viewModelScaleY = g_viewModelScaleY;
    g_config.viewModelScaleZ = g_viewModelScaleZ;
    g_config.viewModelRotX = g_viewModelRotX;
    g_config.viewModelRotY = g_viewModelRotY;
    g_config.viewModelRotZ = g_viewModelRotZ;
    g_config.glassHand = g_glassHand;
    g_config.glassAmount = g_glassAmount;
    g_config.notificationsEnabled = g_notificationsEnabled;
    g_config.openalEspEnabled = g_openalEspEnabled;
    memcpy(g_config.accentColor1, g_accentColor1, sizeof(g_accentColor1));
    memcpy(g_config.accentColor2, g_accentColor2, sizeof(g_accentColor2));
    g_config.accentSpeed = g_accentSpeed;
    g_config.fovChangerEnabled = g_fovChangerEnabled;
    g_config.fovMultiplier = g_fovMultiplier;
    g_config.zoomEnabled = g_zoomEnabled;
    g_config.zoomAmount = g_zoomAmount;
    g_config.zoomBind = g_zoomBind.key;
    g_config.zoomHoldMode = g_zoomHoldMode;
    g_config.chamsEnabled = g_chamsEnabled;
    g_config.chamsThroughWalls = g_chamsThroughWalls;
    g_config.useMemoryForPlayerPos = g_useMemoryForPlayerPos;
    g_config.coordsDisplayEnabled = g_coordsDisplayEnabled;
    g_config.coordsDisplayPosX = g_coordsDisplayPosX;
    g_config.coordsDisplayPosY = g_coordsDisplayPosY;
    memcpy(g_config.coordsDisplayColor, g_coordsDisplayColor, sizeof(g_coordsDisplayColor));
    memcpy(g_config.coordsDisplayBgColor, g_coordsDisplayBgColor, sizeof(g_coordsDisplayBgColor));
    g_config.coordsDisplayBackground = g_coordsDisplayBackground;
    g_config.coordsDisplayDecimals = g_coordsDisplayDecimals;
    g_config.coordsDisplayDecimalPlaces = g_coordsDisplayDecimalPlaces;
    g_config.coordsDisplayLabels = g_coordsDisplayLabels;
    g_config.coordsDisplayCompact = g_coordsDisplayCompact;
    g_config.patternCoordsEnabled = g_patternCoordsEnabled;
    g_config.patternCoordsPosX = g_patternCoordsPosX;
    g_config.patternCoordsPosY = g_patternCoordsPosY;
    g_config.keyBindsX = g_keyBindsX;
    g_config.keyBindsY = g_keyBindsY;
    g_config.keyBindsInitialized = g_keyBindsInitialized;
    g_config.ammoEspBind = g_ammoEspBind.key;
    g_config.tracerBind = g_tracerBind.key;
    g_config.tracerNewBind = g_tracerNewBind.key;
    g_config.chinaHatBind = g_chinaHatBind.key;
    g_config.nightModeBind = g_nightModeBind.key;
    g_config.coordsBind = g_coordsBind.key;
    g_config.fovChangerBind = g_fovChangerBind.key;
    g_config.rustmeEspBind = g_rustmeEspBind.key;
    g_config.aspectRatioEnabled = g_aspectRatioEnabled;
    g_config.aspectRatioValue = g_aspectRatioValue;
    g_config.tracerEnabled = g_tracerEnabled;
    g_config.tracerThickness = g_tracerThickness;
    memcpy(g_config.tracerColor, g_tracerColor, sizeof(g_config.tracerColor));
    g_config.tracerNewEnabled = g_tracerNewEnabled;
    g_config.tracerNewThickness = g_tracerNewThickness;
    memcpy(g_config.tracerNewColor, g_tracerNewColor, sizeof(g_config.tracerNewColor));
    g_config.chinaHatEnabled = g_chinaHatEnabled;
    g_config.chinaHatMode = g_chinaHatMode;
    g_config.chinaHatHeight = g_chinaHatHeight;
    g_config.chinaHatRadius = g_chinaHatRadius;
    g_config.chinaHatPosY = g_chinaHatPosY;
    memcpy(g_config.chinaHatColor, g_chinaHatColor, sizeof(g_config.chinaHatColor));
    g_config.chinaHatLineWidth = g_chinaHatLineWidth;
    g_config.chinaHatFillAlpha = g_chinaHatFillAlpha;
    g_config.ambince = g_ambince;
    g_config.sky = g_sky;
    g_config.world = g_world;
    g_config.density = g_density;
    memcpy(g_config.ambinceColor, g_ambinceColor, sizeof(g_config.ambinceColor));
    g_config.rustmeESP = g_rustmeESP;
    memcpy(g_config.nightColor, g_nightColor, sizeof(g_config.nightColor));
    g_config.chamsHandEnabled = g_chamsHandEnabled;
    memcpy(g_config.chamsHandColorVis, g_chamsHandColorVis, sizeof(g_chamsHandColorVis));
    memcpy(g_config.chamsHandColorHid, g_chamsHandColorHid, sizeof(g_config.chamsHandColorHid));
    g_config.chamsHandXray = g_chamsHandXray;
    g_config.chamsHandMode = g_chamsHandMode;
    g_config.chamsHandSpeed = g_chamsHandSpeed;
    g_config.playerChamsEnabled = g_playerChamsEnabled;
    memcpy(g_config.playerChamsColorVis, g_playerChamsColorVis, sizeof(g_config.playerChamsColorVis));
    memcpy(g_config.playerChamsColorHid, g_playerChamsColorHid, sizeof(g_config.playerChamsColorHid));
    g_config.playerChamsXray = g_playerChamsXray;
    g_config.playerChamsMode = g_playerChamsMode;
    g_config.playerChamsSpeed = g_playerChamsSpeed;
    g_config.aimVisibleOnly = g_aimVisibleOnly;
    g_config.aimAutoDisable = g_aimAutoDisable;
    g_config.aimMcfEnabled = g_aimMcfEnabled;
    g_config.aimMcfMaxHorizontalDistance = g_aimMcfMaxHorizontalDistance;
    g_config.aimMcfMaxVerticalDistance = g_aimMcfMaxVerticalDistance;
    g_config.espColorByVisibility = g_espColorByVisibility;
    memcpy(g_config.espVisibleColor, g_espVisibleColor, sizeof(g_config.espVisibleColor));
    memcpy(g_config.espHiddenColor, g_espHiddenColor, sizeof(g_config.espHiddenColor));
}


const char* GetKeyName(int vk) {
    if (vk == 0) return "None";
    if (vk == VK_LBUTTON) return "LButton";
    if (vk == VK_RBUTTON) return "RButton";
    if (vk == VK_MBUTTON) return "MButton";
    if (vk == VK_XBUTTON1) return "XButton1";
    if (vk == VK_XBUTTON2) return "XButton2";
    if (vk == VK_SHIFT) return "Shift";
    if (vk == VK_MENU) return "Alt";
    if (vk == VK_CONTROL) return "Ctrl";
    static char name[32];
    UINT scanCode = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    if (GetKeyNameTextA((scanCode << 16) | (1 << 24), name, sizeof(name))) return name;
    return "Unknown";
}
