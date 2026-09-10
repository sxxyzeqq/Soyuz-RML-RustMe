#define _CRT_SECURE_NO_WARNINGS
#include "OpenGLHooks.h"
#include "../gui/GUI.h"
#include "../modules/ESP.h"
#include "../modules/AspectRatio.h"
#include "../modules/ChamsHand.h"
#include "../modules/PlayerChams.h"
#include "../modules/WallCheck.h"
#include "../modules/TracersNew.h"
#include "../core/Config.h"
#include "Hooks.h"
#include "../modules/Aimbot.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <set>
#include <utility>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <chrono>

#ifndef GL_QUERY_RESULT
#define GL_QUERY_RESULT 0x8866
#endif

extern bool g_hadYRotationBeforeScale;
extern bool g_isInScope;
extern float g_debugFovRatio;
extern std::vector<RotateCall> g_pendingRotations;

void WINAPI hooked_glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) {
    float width = (float)(right - left);
    float height = (float)(top - bottom);
    if (width > 100.0f && width < 10000.0f) g_debugFovRatio = width;
    g_guiWidth = std::abs(width);
    g_guiHeight = std::abs(height);

    glPixelTransferf(GL_RED_SCALE, 1.0f);
    glPixelTransferf(GL_GREEN_SCALE, 1.0f);
    glPixelTransferf(GL_BLUE_SCALE, 1.0f);
    glPixelTransferf(GL_RED_BIAS, 0.0f);
    glPixelTransferf(GL_GREEN_BIAS, 0.0f);
    glPixelTransferf(GL_BLUE_BIAS, 0.0f);
    WallCheckProcessEntities();

    if (original_glOrtho_addr) ((Ortho_fn)original_glOrtho_addr)(left, right, bottom, top, zNear, zFar);
}

void WINAPI hooked_glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    if (g_loadIdentityCount > 0) {
        if (g_ambince && g_sky) {
            if (original_glClearColor_addr) ((ClearColor_fn)original_glClearColor_addr)(g_ambinceColor[0], g_ambinceColor[1], g_ambinceColor[2], g_ambinceColor[3]);
            return;
        }
    }
    if (original_glClearColor_addr) ((ClearColor_fn)original_glClearColor_addr)(red, green, blue, alpha);
}

void WINAPI hooked_glClear(GLbitfield mask) {
    if ((mask & GL_DEPTH_BUFFER_BIT) != 0) {
        WallCheckProcessEntities();
    }
    if (original_glClear_addr) ((Clear_fn)original_glClear_addr)(mask);
}

void WINAPI hooked_glFogfv(GLenum pname, const GLfloat* params) {
    if (g_ambince && g_sky && g_imguiInitialized) {
        if (pname == GL_FOG_COLOR) {
            if (original_glFogfv_addr) ((Fogfv_fn)original_glFogfv_addr)(pname, g_ambinceColor);
            return;
        }
        if (pname == GL_FOG_DENSITY) {
            GLfloat d = 50.0f;
            if (original_glFogfv_addr) ((Fogfv_fn)original_glFogfv_addr)(pname, &d);
            return;
        }
    }
    if (original_glFogfv_addr) ((Fogfv_fn)original_glFogfv_addr)(pname, params);
}

void WINAPI hooked_glFogf(GLenum pname, GLfloat param) {
    if (g_ambince && g_sky && g_imguiInitialized) {
        if (pname == GL_FOG_DENSITY) {
            if (original_glFogf_addr) ((Fogf_fn)original_glFogf_addr)(pname, 50);
            return;
        }
        if (pname == GL_FOG_START) {
            if (original_glFogf_addr) ((Fogf_fn)original_glFogf_addr)(pname, g_density);
            return;
        }
        if (pname == GL_FOG_END) {
            if (original_glFogf_addr) ((Fogf_fn)original_glFogf_addr)(pname, 500.0f);
            return;
        }
    }
    if (original_glFogf_addr) ((Fogf_fn)original_glFogf_addr)(pname, param);
}

void WINAPI hooked_glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) {
    static float lastFov = 0.0f;
    static float baseFov = 0.0f;
    float width = (float)(right - left);
    g_debugFovRatio = width;

    if (width > 10.0f && width < 1000.0f) {
        if (baseFov == 0.0f) { baseFov = width; g_baseFovValue = width; }
        if (baseFov > 0.0f) g_currentFovMultiplier = width / baseFov;
        if (lastFov > 0.0f) {
            float ratio = width / lastFov;
            if (ratio < 0.7f)      g_isInScope = true;
            else if (ratio > 1.3f) g_isInScope = false;
        }
        lastFov = width;
    }

    double maxExtent = 10000.0;
    if (left   < -maxExtent) left   = -maxExtent;
    if (right  >  maxExtent) right  =  maxExtent;
    if (bottom < -maxExtent) bottom = -maxExtent;
    if (top    >  maxExtent) top    =  maxExtent;

    if (g_fovChangerEnabled) {
        left   *= (GLdouble)g_fovMultiplier;
        right  *= (GLdouble)g_fovMultiplier;
        top    *= (GLdouble)g_fovMultiplier;
        bottom *= (GLdouble)g_fovMultiplier;
    }

    if (g_zoomEnabled && g_zoomAmount > 0.01f) {
        left   /= (GLdouble)g_zoomAmount;
        right  /= (GLdouble)g_zoomAmount;
        top    /= (GLdouble)g_zoomAmount;
        bottom /= (GLdouble)g_zoomAmount;
    }

    if (g_aspectRatioEnabled && g_aspectRatioValue != 1.0f) {
        left   /= (GLdouble)g_aspectRatioValue;
        right  /= (GLdouble)g_aspectRatioValue;
    }

    if (original_glFrustum_addr) ((Frustum_fn)original_glFrustum_addr)(left, right, bottom, top, zNear, zFar);
}

void WINAPI hooked_glLoadMatrixf(const GLfloat* m) {
    GLint mode; glGetIntegerv(GL_MATRIX_MODE, &mode);
    if (mode == GL_PROJECTION && m[15] == 0.0f) {
        GLfloat modified[16];
        memcpy(modified, m, sizeof(modified));
        static float lastFov = 0.0f;
        float currentFov = m[0];
        if (currentFov > 0.001f) {
            if (g_baseFovValue == 0.0f || !g_isInScope) g_baseFovValue = currentFov;
            if (g_baseFovValue > 0.001f) g_currentFovMultiplier = currentFov / g_baseFovValue;
            if (lastFov > 0.001f) {
                float ratio = currentFov / lastFov;
                if (ratio > 1.5f)      g_isInScope = true;
                else if (ratio < 0.9f) g_isInScope = false;
            }
            lastFov = currentFov;
        }
        if (g_fovChangerEnabled) { modified[0] /= g_fovMultiplier; modified[5] /= g_fovMultiplier; }
        if (g_zoomEnabled && g_zoomAmount > 0.01f) { modified[0] *= g_zoomAmount; modified[5] *= g_zoomAmount; }
        if (g_aspectRatioEnabled && g_aspectRatioValue != 1.0f) { modified[0] *= g_aspectRatioValue; }
        if (original_glLoadMatrixf_addr) ((LoadMatrixf_fn)original_glLoadMatrixf_addr)(modified);
        return;
    }
    // Ortho projection loaded - kill chams
    if (mode == GL_PROJECTION && m[15] != 0.0f && g_restoreChams) {
        ChamsEntityEnd();
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_LIGHTING);
        if (g_chamsThroughWalls) glEnable(GL_DEPTH_TEST);
        g_restoreChams = false;
    }
    // PlayerChams — выходим из 3D, страхуемся на случай если шейдер ещё активен
    if (mode == GL_PROJECTION && m[15] != 0.0f) {
        PlayerChamsForceEnd();
        // WallCheck — depth-буфер ещё цел, читаем сейчас
        WallCheckProcessEntities();
    }
    // Сбрасываем pixel transfer перед 2D рендером (HUD/GUI),
    // чтобы fullbright/nightmode/ambience не красили 2D.
    if (mode == GL_PROJECTION && m[15] != 0.0f &&
        (g_nightModeEnabled || (g_ambince && g_world) || g_fullbrightEnabled)) {
        glPixelTransferf(GL_RED_SCALE,   1.0f);
        glPixelTransferf(GL_GREEN_SCALE, 1.0f);
        glPixelTransferf(GL_BLUE_SCALE,  1.0f);
        glPixelTransferf(GL_RED_BIAS,    0.0f);
        glPixelTransferf(GL_GREEN_BIAS,  0.0f);
        glPixelTransferf(GL_BLUE_BIAS,   0.0f);
    }
    if (original_glLoadMatrixf_addr) ((LoadMatrixf_fn)original_glLoadMatrixf_addr)(m);
}

void WINAPI hooked_glLoadMatrixd(const GLdouble* m) {
    GLint mode; glGetIntegerv(GL_MATRIX_MODE, &mode);
    if (mode == GL_PROJECTION && m[15] == 0.0) {
        GLdouble modified[16];
        memcpy(modified, m, sizeof(modified));
        if (g_fovChangerEnabled) { modified[0] /= (GLdouble)g_fovMultiplier; modified[5] /= (GLdouble)g_fovMultiplier; }
        if (g_zoomEnabled && g_zoomAmount > 0.01f) { modified[0] *= (GLdouble)g_zoomAmount; modified[5] *= (GLdouble)g_zoomAmount; }
        if (g_aspectRatioEnabled && g_aspectRatioValue != 1.0f) { modified[0] *= (GLdouble)g_aspectRatioValue; }
        if (original_glLoadMatrixd_addr) ((LoadMatrixd_fn)original_glLoadMatrixd_addr)(modified);
        return;
    }
    // 2D ortho через glLoadMatrixd — гасим pixel transfer чтобы HUD не красился.
    if (mode == GL_PROJECTION && m[15] != 0.0 &&
        (g_nightModeEnabled || (g_ambince && g_world) || g_fullbrightEnabled)) {
        glPixelTransferf(GL_RED_SCALE,   1.0f);
        glPixelTransferf(GL_GREEN_SCALE, 1.0f);
        glPixelTransferf(GL_BLUE_SCALE,  1.0f);
        glPixelTransferf(GL_RED_BIAS,    0.0f);
        glPixelTransferf(GL_GREEN_BIAS,  0.0f);
        glPixelTransferf(GL_BLUE_BIAS,   0.0f);
    }
    if (original_glLoadMatrixd_addr) ((LoadMatrixd_fn)original_glLoadMatrixd_addr)(m);
}

void WINAPI hooked_glMultMatrixf(const GLfloat* m) {
    GLint mode; glGetIntegerv(GL_MATRIX_MODE, &mode);
    if (mode == GL_PROJECTION) {
        GLfloat modified[16];
        memcpy(modified, m, sizeof(modified));
        if (g_fovChangerEnabled) { modified[0] /= g_fovMultiplier; modified[5] /= g_fovMultiplier; }
        if (g_zoomEnabled && g_zoomAmount > 0.01f) { modified[0] *= g_zoomAmount; modified[5] *= g_zoomAmount; }
        if (g_aspectRatioEnabled && g_aspectRatioValue != 1.0f) { modified[0] *= g_aspectRatioValue; }
        if (original_glMultMatrixf_addr) ((MultMatrixf_fn)original_glMultMatrixf_addr)(modified);
        return;
    }
    if (original_glMultMatrixf_addr) ((MultMatrixf_fn)original_glMultMatrixf_addr)(m);
}

void WINAPI hooked_glMultMatrixd(const GLdouble* m) {
    GLint mode; glGetIntegerv(GL_MATRIX_MODE, &mode);
    if (mode == GL_PROJECTION) {
        GLdouble modified[16];
        memcpy(modified, m, sizeof(modified));
        if (g_fovChangerEnabled) { modified[0] /= (GLdouble)g_fovMultiplier; modified[5] /= (GLdouble)g_fovMultiplier; }
        if (g_zoomEnabled && g_zoomAmount > 0.01f) { modified[0] *= (GLdouble)g_zoomAmount; modified[5] *= (GLdouble)g_zoomAmount; }
        if (g_aspectRatioEnabled && g_aspectRatioValue != 1.0f) { modified[0] *= (GLdouble)g_aspectRatioValue; }
        if (original_glMultMatrixd_addr) ((MultMatrixd_fn)original_glMultMatrixd_addr)(modified);
        return;
    }
    if (original_glMultMatrixd_addr) ((MultMatrixd_fn)original_glMultMatrixd_addr)(m);
}

void WINAPI hooked_glMatrixMode(GLenum mode) {
    if (original_glMatrixMode_addr) ((MatrixMode_fn)original_glMatrixMode_addr)(mode);
}

void WINAPI hooked_glGetFloatv(GLenum pname, GLfloat* params) {
    if (original_glGetFloatv_addr) {
        ((GetFloatv_fn)original_glGetFloatv_addr)(pname, params);
    }
    else {
        glGetFloatv(pname, params);
    }

    if (params) {
        TracerNewOnGetFloatv(pname, params);
    }
}

void WINAPI hooked_glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params) {
    if (original_glGetQueryObjectiv_addr) {
        ((GetQueryObjectiv_fn)original_glGetQueryObjectiv_addr)(id, pname, params);
    }

    if (pname == GL_QUERY_RESULT && params) {
        *params = 1;
    }
}

PROC WINAPI hooked_wglGetProcAddress(LPCSTR name) {
    PROC result = nullptr;

    if (original_wglGetProcAddress_addr) {
        result = ((wglGetProcAddress_fn)original_wglGetProcAddress_addr)(name);
    }
    else {
        result = wglGetProcAddress(name);
    }

    if (name && std::strcmp(name, "glGetQueryObjectiv") == 0) {
        if (result && reinterpret_cast<INT_PTR>(result) > 0x10000) {
            original_glGetQueryObjectiv_addr = reinterpret_cast<void*>(result);
        }
        return reinterpret_cast<PROC>(hooked_glGetQueryObjectiv);
    }

    return result;
}

void WINAPI hooked_glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    if (original_glColor4f_addr) ((Color4f_fn)original_glColor4f_addr)(r, g, b, a);
}

void WINAPI hooked_glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    if (original_glColor3f_addr) ((Color3f_fn)original_glColor3f_addr)(r, g, b);
}

void WINAPI hooked_glEnable(GLenum cap) {
    if (g_hideGameCrosshair && (cap == GL_COLOR_LOGIC_OP || cap == GL_INDEX_LOGIC_OP)) return;
    if ((g_wallHackEnabled || g_fullbrightEnabled) && (cap == GL_LIGHTING) && g_loadIdentityCount >= 2) return;
    if (g_wallHackEnabled && cap == GL_DEPTH_TEST) return;
    if (original_glEnable_addr) ((Enable_fn)original_glEnable_addr)(cap);
}

void WINAPI hooked_glDisable(GLenum cap) {
    if (original_glDisable_addr) ((Disable_fn)original_glDisable_addr)(cap);
}

void WINAPI hooked_glLoadIdentity() {
    if (original_glLoadIdentity_addr) ((LoadIdentity_fn)original_glLoadIdentity_addr)();
    GLint matrixMode;
    glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
    if (matrixMode == GL_MODELVIEW) {
        // End chams hand shader when leaving viewmodel pass
        if (g_loadIdentityCount == 3) ChamsHandEnd();
        g_loadIdentityCount++;
        if (g_viewModelEnabled && g_loadIdentityCount == 3) {
            if (std::abs(g_viewModelX) > 0.001f || std::abs(g_viewModelY) > 0.001f || std::abs(g_viewModelZ) > 0.001f) {
                if (original_glTranslatef_addr) ((Translatef_fn)original_glTranslatef_addr)(g_viewModelX, g_viewModelY, g_viewModelZ);
                else glTranslatef(g_viewModelX, g_viewModelY, g_viewModelZ);
            }
            if (std::abs(g_viewModelScaleX - 1.0f) > 0.001f || std::abs(g_viewModelScaleY - 1.0f) > 0.001f || std::abs(g_viewModelScaleZ - 1.0f) > 0.001f) {
                if (original_glScalef_addr) ((Scalef_fn)original_glScalef_addr)(g_viewModelScaleX, g_viewModelScaleY, g_viewModelScaleZ);
                else glScalef(g_viewModelScaleX, g_viewModelScaleY, g_viewModelScaleZ);
            }
            if (std::abs(g_viewModelRotX) > 0.01f || std::abs(g_viewModelRotY) > 0.01f || std::abs(g_viewModelRotZ) > 0.01f) {
                if (original_glRotatef_addr) {
                    ((Rotatef_fn)original_glRotatef_addr)(g_viewModelRotX, 1.0f, 0.0f, 0.0f);
                    ((Rotatef_fn)original_glRotatef_addr)(g_viewModelRotY, 0.0f, 1.0f, 0.0f);
                    ((Rotatef_fn)original_glRotatef_addr)(g_viewModelRotZ, 0.0f, 0.0f, 1.0f);
                } else {
                    glRotatef(g_viewModelRotX, 1.0f, 0.0f, 0.0f);
                    glRotatef(g_viewModelRotY, 0.0f, 1.0f, 0.0f);
                    glRotatef(g_viewModelRotZ, 0.0f, 0.0f, 1.0f);
                }
            }
            if (g_glassHand && g_loadIdentityCount == 3) glPixelTransferf(GL_ALPHA_SCALE, g_glassAmount);
            else glPixelTransferf(GL_ALPHA_SCALE, 1.0f);
        }
        if (g_loadIdentityCount == 3) ChamsHandBegin();
        if (!g_frameCameraCaptured) {
            GLfloat pr[16]; glGetFloatv(GL_PROJECTION_MATRIX, pr);
            if (pr[15] == 0.0f) {
                g_cameraSetupState = 1;
                g_cameraMatrix = glm::mat4(1.0f);
            }
        }
    }
}

void WINAPI hooked_glPushMatrix() {
    if (g_cameraSetupState == 1 && !g_frameCameraCaptured) {
        GLint m; glGetIntegerv(GL_MATRIX_MODE, &m);
        if (m == GL_MODELVIEW) {
            glGetFloatv(GL_MODELVIEW_MATRIX, (GLfloat*)glm::value_ptr(g_cameraMatrix));
            if (std::fabs(g_cameraMatrix[3][0]) > 0.001f || std::fabs(g_cameraMatrix[3][2]) > 0.001f) {
                g_frameCameraCaptured = true;
                g_cameraSetupState = 2;
            }
        }
    }
    if (original_glPushMatrix_addr) ((PushMatrix_fn)original_glPushMatrix_addr)();
    PlayerChamsOnPushMatrix();
}

void WINAPI hooked_glPopMatrix() {
    PlayerChamsOnPopMatrix();
    if (original_glPopMatrix_addr) ((PopMatrix_fn)original_glPopMatrix_addr)();
}

void WINAPI hooked_glTranslated(GLdouble x, GLdouble y, GLdouble z) {
    // Скрываем только прицел, не весь GUI
    if (g_crosshairEnabled && g_guiWidth > 0.0 && g_guiHeight > 0.0) {
        double centerX = g_guiWidth * 0.5;
        double centerY = g_guiHeight * 0.5;
        
        // Очень точная проверка - только центр экрана (прицел)
        // Прицел обычно рендерится ТОЧНО в центре
        if (std::abs(x - centerX) < 2.0 && std::abs(y - centerY) < 2.0 && std::abs(z) < 0.1) {
            x = -99999.0; 
            y = -99999.0;
        }
    }
    if (original_glTranslated_addr) ((Translated_fn)original_glTranslated_addr)(x, y, z);
}

void WINAPI hooked_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    const GLfloat originalX = x;
    const GLfloat originalY = y;
    const GLfloat originalZ = z;

    if (g_noHurtCamEnabled && g_loadIdentityCount >= 2) {
        if (x > 0.001f && x < 0.6f && std::abs(y) < 0.001f && std::abs(z) < 0.001f) return;
    }
    g_lastTranslate = glm::vec3(x, y, z);
    g_hadYRotationBeforeScale = false;
    g_pendingRotations.clear();

    // Скрываем только прицел, не весь GUI
    if (g_crosshairEnabled && g_guiWidth > 0.0 && g_guiHeight > 0.0) {
        double centerX = g_guiWidth * 0.5;
        double centerY = g_guiHeight * 0.5;
        
        // Очень точная проверка - только центр экрана (прицел)
        // Прицел обычно рендерится ТОЧНО в центре
        if (std::abs((double)x - centerX) < 2.0 && std::abs((double)y - centerY) < 2.0 && std::abs(z) < 0.1f) {
            x = -99999.0f; 
            y = -99999.0f;
        }
    }

    if (g_tracerNewEnabled) {
        TracerNewOnTranslatef(originalX, originalY, originalZ);
    }

    if (original_glTranslatef_addr) ((Translatef_fn)original_glTranslatef_addr)(x, y, z);

    if (g_cameraSetupState == 1 && !g_frameCameraCaptured) {
        glGetFloatv(GL_MODELVIEW_MATRIX, (GLfloat*)glm::value_ptr(g_cameraMatrix));
        if (std::fabs(x) > 5.0f || std::fabs(z) > 5.0f || std::fabs(y) > 2.0f) {
            g_frameCameraCaptured = true;
            g_cameraSetupState = 2;
        }
    }

    if ((g_espEnabled || g_aimEnabled || g_ammoEspEnabled || g_tracerEnabled) && g_frameCameraCaptured) {

        float dist = std::sqrt(x * x + y * y + z * z);
        if (dist > 1.0f && dist < 2000.0f && g_loadIdentityCount >= 2) {
            GLfloat proj[16];
            glGetFloatv(GL_PROJECTION_MATRIX, proj);
            if (proj[15] == 0.0f) {
                g_pendingEntityTranslate = glm::vec3(x, y, z);
                g_pendingEntityProjection = glm::make_mat4(proj);
                glGetFloatv(GL_MODELVIEW_MATRIX, glm::value_ptr(g_pendingEntityModelview));
                g_hasPendingEntity = true;
            }
        }
    }
}

void WINAPI hooked_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    if (g_noHurtCamEnabled && g_loadIdentityCount >= 2) {
        if (std::abs(x) < 0.001f && std::abs(y) < 0.001f && std::abs(z - 1.0f) < 0.001f && std::abs(angle) < 30.0f) return;
    }
    if (original_glRotatef_addr) ((Rotatef_fn)original_glRotatef_addr)(angle, x, y, z);
    if (g_cameraSetupState == 1 && !g_frameCameraCaptured) {
        glGetFloatv(GL_MODELVIEW_MATRIX, (GLfloat*)glm::value_ptr(g_cameraMatrix));
    }
    if (fabs(y - 1.0f) < 0.01f && fabs(x) < 0.01f && fabs(z) < 0.01f) {
        g_hadYRotationBeforeScale = true;
    }
    g_pendingRotations.push_back({ angle, x, y, z });
}

void WINAPI hooked_glScalef(GLfloat x, GLfloat y, GLfloat z) {
    PlayerChamsOnScale(x, y, z);

    if (g_tracerNewEnabled) {
        TracerNewOnScalef(x, y, z);
    }

    if (g_espEnabled || g_aimEnabled || g_ammoEspEnabled || g_chamsEnabled || g_tracerEnabled) {
        const bool isNameTagScale =
            x >= -0.026f && x <= -0.025f &&
            y >= -0.026f && y <= -0.025f &&
            z >= 0.025f && z <= 0.026f;

        if (g_aimEnabled && g_aimMcfEnabled && isNameTagScale) {
            NameTagInfo tag{};
            glGetFloatv(GL_MODELVIEW_MATRIX, glm::value_ptr(tag.modelview));
            tag.relativePos = GetEntityRelativePos(tag.modelview);
            tag.translatePos = g_lastTranslate;
            std::lock_guard<std::mutex> lock(g_entitiesMutex);
            g_nameTags.push_back(tag);
        }

        bool isEnt = (x == 0.9375f && y == 0.9375f && z == 0.9375f) ||
            (fabs(fabs(x) - 1.20f) <= 0.03f && fabs(fabs(y) - 1.20f) <= 0.03f && fabs(fabs(z) - 1.20f) <= 0.03f);

        if (g_rustmeESP) {
            if (x == -1 && y == -1 && z == 1) {
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(1, -1100000);
            }
        }

        bool found = false;
        EntityInfo ent{};
        if (x == 0.9375f && y == 0.9375f && z == 0.9375f) {
            ent.isPlayer = true; found = true;
        }
        else if (fabs(fabs(x) - 1.20f) <= 0.03f && fabs(fabs(y) - 1.20f) <= 0.03f && fabs(fabs(z) - 1.20f) <= 0.03f) {
            found = true;
            if (g_hadYRotationBeforeScale) ent.isBear = true;
        }
        else if (fabs(fabs(x) - 0.44f) <= 0.03f && fabs(fabs(y) - 0.44f) <= 0.03f && fabs(fabs(z) - 0.44f) <= 0.03f) {
            found = true;
            if (g_hadYRotationBeforeScale) ent.isBot = true;
        }
        else if (fabs(fabs(x) - 0.50f) <= 0.03f && fabs(fabs(y) - 0.50f) <= 0.03f && fabs(fabs(z) - 0.50f) <= 0.03f) {
            ent.isAmmo = true; found = true;
        }
        else {
            if (fabs(x) < 2.0f && fabs(x) != 1.0f) found = true;
        }

        if (x == 0.9375f && y == 0.9375f && z == 0.9375f) {
            Object obj(Object::Entity);
            obj.relativePos = g_lastTranslate;
            std::lock_guard<std::mutex> lock(g_objectsMutex);
            g_objects.push_back(obj);
        }

        if (found) {
            glGetFloatv(GL_PROJECTION_MATRIX, glm::value_ptr(ent.projection));
            if (fabs(ent.projection[3][3] - 1.0f) > 0.1f) {
                glGetFloatv(GL_MODELVIEW_MATRIX, glm::value_ptr(ent.modelview));
                ent.worldPos = GetEntityRelativePos(ent.modelview);
                ent.translatePos = g_lastTranslate;
                ALfloat lx = 0, ly = 0, lz = 0;
                if (alGetListener3f) alGetListener3f(AL_POSITION, &lx, &ly, &lz);
                ent.capturedListenerPos = glm::vec3(lx, ly, lz);
                ent.scale = glm::vec3(x, y, z);
                ent.hasWorldPos = true;
                ent.rotations = g_pendingRotations;
                std::lock_guard<std::mutex> lock(g_entitiesMutex);
                g_entities.push_back(ent);
            }
        }
        g_hasPendingEntity = false;
    }

    if (original_glScalef_addr) ((Scalef_fn)original_glScalef_addr)(x, y, z);
}

void WINAPI hooked_glLogicOp(GLenum opcode) {
    if (g_hideGameCrosshair && g_imguiInitialized) {
        if (opcode == GL_INVERT || opcode == GL_XOR) return;
    }
    if (original_glLogicOp_addr) ((LogicOp_fn)original_glLogicOp_addr)(opcode);
}

void WINAPI hooked_glBlendFunc(GLenum sfactor, GLenum dfactor) {
    if (g_hideGameCrosshair && g_imguiInitialized) {
        // Minecraft crosshair often uses (GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_COLOR)
        if (sfactor == GL_ONE_MINUS_DST_COLOR && dfactor == GL_ONE_MINUS_SRC_COLOR) return;
    }
    if (original_glBlendFunc_addr) ((BlendFunc_fn)original_glBlendFunc_addr)(sfactor, dfactor);
}


