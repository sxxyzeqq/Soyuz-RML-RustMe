#pragma once
#include "../core/Globals.h"

void TracerNewOnTranslatef(GLfloat x, GLfloat y, GLfloat z);
void TracerNewOnScalef(GLfloat x, GLfloat y, GLfloat z);
void TracerNewOnGetFloatv(GLenum pname, const GLfloat* params);
void RenderTracersNew();
void ClearTracersNewTargets();
