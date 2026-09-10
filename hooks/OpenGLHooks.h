#pragma once
#include "../core/Globals.h"

void WINAPI hooked_glOrtho(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
void WINAPI hooked_glClearColor(GLclampf, GLclampf, GLclampf, GLclampf);
void WINAPI hooked_glClear(GLbitfield);
void WINAPI hooked_glFogfv(GLenum, const GLfloat*);
void WINAPI hooked_glFogf(GLenum, GLfloat);
void WINAPI hooked_glFrustum(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
void WINAPI hooked_glLoadMatrixf(const GLfloat*);
void WINAPI hooked_glLoadMatrixd(const GLdouble*);
void WINAPI hooked_glMultMatrixf(const GLfloat*);
void WINAPI hooked_glMultMatrixd(const GLdouble*);
void WINAPI hooked_glMatrixMode(GLenum);
void WINAPI hooked_glGetFloatv(GLenum, GLfloat*);
PROC WINAPI hooked_wglGetProcAddress(LPCSTR);
void WINAPI hooked_glGetQueryObjectiv(GLuint, GLenum, GLint*);
void WINAPI hooked_glColor4f(GLfloat, GLfloat, GLfloat, GLfloat);
void WINAPI hooked_glColor3f(GLfloat, GLfloat, GLfloat);
void WINAPI hooked_glEnable(GLenum);
void WINAPI hooked_glDisable(GLenum);
void WINAPI hooked_glLoadIdentity();
void WINAPI hooked_glPushMatrix();
void WINAPI hooked_glPopMatrix();
void WINAPI hooked_glTranslated(GLdouble, GLdouble, GLdouble);
void WINAPI hooked_glTranslatef(GLfloat, GLfloat, GLfloat);
void WINAPI hooked_glRotatef(GLfloat, GLfloat, GLfloat, GLfloat);
void WINAPI hooked_glScalef(GLfloat, GLfloat, GLfloat);
void WINAPI hooked_glLogicOp(GLenum);
void WINAPI hooked_glBlendFunc(GLenum, GLenum);

BOOL WINAPI hooked_wglSwapBuffers(HDC hdc);

DWORD WINAPI timeGetTime_hook();
