#include "Viewmodel.h"
#include "../glm/glm.hpp"
#include "../glm/gtc/matrix_transform.hpp"

bool g_viewmodelEnabled = true;
float g_viewmodelSwingSpeed = 0.4f;

static bool g_isSwinging = false;
static float g_swingProgress = 0.0f;
static bool g_swingDirection = true;

void ViewmodelOnLeftClick() {
    if (!g_viewmodelEnabled) return;
    if (!g_isSwinging && g_swingProgress >= 1.0f) {
        g_isSwinging = true;
        g_swingDirection = true;
    }
}

void ViewmodelProcess() {
    if (!g_viewmodelEnabled) return;
    
    if (g_isSwinging) {
        if (g_swingDirection) {
            g_swingProgress += g_viewmodelSwingSpeed * 0.15f;
            if (g_swingProgress >= 1.0f) {
                g_swingProgress = 1.0f;
                g_swingDirection = false;
            }
        } else {
            g_swingProgress -= g_viewmodelSwingSpeed * 0.15f;
            if (g_swingProgress <= 0.0f) {
                g_swingProgress = 0.0f;
                g_isSwinging = false;
            }
        }
    }
}

void ViewmodelApplyTransform() {
    if (!g_viewmodelEnabled || g_swingProgress <= 0.0f) return;
    
    float swingAngle = g_swingProgress * 45.0f;
    glRotatef(swingAngle, 0.0f, 1.0f, 0.0f);
    glRotatef(swingAngle * 0.3f, 0.0f, 0.0f, 1.0f);
}
