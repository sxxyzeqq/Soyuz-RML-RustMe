#include "ChamsHand.h"
#include <cmath>

bool g_chamsHandEnabled = false;
float g_chamsHandColorVis[4] = { 0.4f, 0.2f, 0.9f, 1.0f };
float g_chamsHandColorHid[4] = { 0.2f, 0.1f, 0.4f, 1.0f };
bool g_chamsHandXray = false;
int g_chamsHandMode = 0; // 0=Metallic, 1=Rainbow, 2=Galaxy, 3=Neon, 4=Lava, 5=Ice...
float g_chamsHandSpeed = 1.0f;

// OpenGL shader function pointers
typedef GLuint(APIENTRY* PFNGLCREATESHADERPROC)(GLenum type);
typedef void(APIENTRY* PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const char** string, const GLint* length);
typedef void(APIENTRY* PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef GLuint(APIENTRY* PFNGLCREATEPROGRAMPROC)(void);
typedef void(APIENTRY* PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void(APIENTRY* PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void(APIENTRY* PFNGLUSEPROGRAMPROC)(GLuint program);
typedef GLint(APIENTRY* PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const char* name);
typedef void(APIENTRY* PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void(APIENTRY* PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void(APIENTRY* PFNGLUNIFORM3FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void(APIENTRY* PFNGLDELETESHADERPROC)(GLuint shader);
typedef void(APIENTRY* PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef void(APIENTRY* PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint* params);

static PFNGLCREATESHADERPROC pglCreateShader = nullptr;
static PFNGLSHADERSOURCEPROC pglShaderSource = nullptr;
static PFNGLCOMPILESHADERPROC pglCompileShader = nullptr;
static PFNGLCREATEPROGRAMPROC pglCreateProgram = nullptr;
static PFNGLATTACHSHADERPROC pglAttachShader = nullptr;
static PFNGLLINKPROGRAMPROC pglLinkProgram = nullptr;
static PFNGLUSEPROGRAMPROC pglUseProgram = nullptr;
static PFNGLGETUNIFORMLOCATIONPROC pglGetUniformLocation = nullptr;
static PFNGLUNIFORM1FPROC pglUniform1f = nullptr;
static PFNGLUNIFORM1IPROC pglUniform1i = nullptr;
static PFNGLUNIFORM3FPROC pglUniform3f = nullptr;
static PFNGLDELETESHADERPROC pglDeleteShader = nullptr;
static PFNGLDELETEPROGRAMPROC pglDeleteProgram = nullptr;
static PFNGLGETSHADERIVPROC pglGetShaderiv = nullptr;

static GLuint g_program = 0;
static GLint g_locTime = -1;
static GLint g_locColor = -1;
static GLint g_locTex = -1;
static GLint g_locMode = -1;
static bool g_shaderReady = false;
static float g_shaderTime = 0.0f;          // для ChamsHand (рука)
static float g_shaderTimeEntity = 0.0f;    // для ChamsEntity (плоские чамсы — не используется здесь, оставлено)
static float g_shaderTimeGeneric = 0.0f;   // для ChamsShaderBegin (PlayerChams и другие)

static const char* vertexShader = 
    "varying vec3 vNormal;\n"
    "varying vec3 vPos;\n"
    "varying vec3 vWorldPos;\n"
    "void main() {\n"
    "    gl_Position = ftransform();\n"
    "    gl_TexCoord[0] = gl_MultiTexCoord0;\n"
    "    gl_FrontColor = gl_Color;\n"
    "    vNormal = normalize(gl_NormalMatrix * gl_Normal);\n"
    "    vPos = vec3(gl_ModelViewMatrix * gl_Vertex);\n"
    "    vWorldPos = gl_Vertex.xyz;\n"
    "}\n";

// Metallic/chrome shader - smooth color with specular sheen that shifts
static const char* fragmentShader =
    "uniform float uTime;\n"
    "uniform vec3 uColor;\n"
    "uniform sampler2D uTex;\n"
    "uniform int uMode;\n"
    "varying vec3 vNormal;\n"
    "varying vec3 vPos;\n"
    "varying vec3 vWorldPos;\n"
    "\n"
    "float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }\n"
    "float noise(vec2 p) {\n"
    "    vec2 i = floor(p); vec2 f = fract(p);\n"
    "    f = f*f*(3.0-2.0*f);\n"
    "    return mix(mix(hash(i), hash(i+vec2(1,0)), f.x), mix(hash(i+vec2(0,1)), hash(i+vec2(1,1)), f.x), f.y);\n"
    "}\n"
    "\n"
    "vec3 hsv2rgb(vec3 c) {\n"
    "    vec3 p = abs(fract(c.xxx + vec3(1.0, 2.0/3.0, 1.0/3.0)) * 6.0 - 3.0);\n"
    "    return c.z * mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), c.y);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec4 texColor = texture2D(uTex, gl_TexCoord[0].xy) * gl_Color;\n"
    "    if (texColor.a < 0.01) discard;\n"
    "    \n"
    "    vec3 viewDir = normalize(-vPos);\n"
    "    vec3 norm = normalize(vNormal);\n"
    "    float posGrad = dot(normalize(vWorldPos), vec3(0.3, 0.7, 0.5));\n"
    "    float fresnel = pow(1.0 - abs(dot(viewDir, norm)), 1.5);\n"
    "    \n"
    "    float a1 = uTime * 0.7;\n"
    "    vec3 light1 = normalize(vec3(sin(a1), 0.6, cos(a1)));\n"
    "    vec3 half1 = normalize(light1 + viewDir);\n"
    "    float diff = max(dot(norm, light1), 0.0) * 0.6 + 0.4;\n"
    "    float spec = pow(max(dot(norm, half1), 0.0), 30.0);\n"
    "    \n"
    "    vec3 col = vec3(0.0);\n"
    "    \n"
    "    if (uMode == 0) {\n"
    "        // Metallic\n"
    "        float grad = sin(posGrad * 6.0 + uTime) * 0.15 + 0.85;\n"
    "        col = uColor * diff * grad;\n"
    "        col += vec3(1.0) * spec * 0.7;\n"
    "        col += uColor * fresnel * 0.4;\n"
    "    }\n"
    "    else if (uMode == 1) {\n"
    "        // Rainbow\n"
    "        float hue = fract(posGrad * 0.5 + uTime * 0.2 + fresnel * 0.3);\n"
    "        col = hsv2rgb(vec3(hue, 0.8, 1.0)) * diff;\n"
    "        col += vec3(1.0) * spec * 0.5;\n"
    "        col += hsv2rgb(vec3(hue + 0.1, 0.6, 1.0)) * fresnel * 0.3;\n"
    "    }\n"
    "    else if (uMode == 2) {\n"
    "        // Galaxy\n"
    "        vec2 uv = vWorldPos.xy * 2.0 + vWorldPos.z;\n"
    "        float n1 = noise(uv * 3.0 + uTime * 0.2);\n"
    "        float n2 = noise(uv * 7.0 - uTime * 0.15);\n"
    "        float nebula = n1 * 0.6 + n2 * 0.4;\n"
    "        float stars = pow(hash(floor(uv * 30.0 + uTime * 0.5)), 15.0);\n"
    "        col = mix(uColor * 0.3, uColor, nebula) * diff;\n"
    "        col += vec3(stars) * 0.8;\n"
    "        col += uColor * fresnel * 0.5;\n"
    "        col += vec3(1.0) * spec * 0.4;\n"
    "    }\n"
    "    else if (uMode == 3) {\n"
    "        // Neon\n"
    "        float edge = fresnel * 2.0;\n"
    "        col = uColor * 0.2 * diff;\n"
    "        col += uColor * edge * 1.5;\n"
    "        col += vec3(1.0) * spec * 0.3;\n"
    "        float pulse = sin(uTime * 3.0 + posGrad * 4.0) * 0.2 + 0.8;\n"
    "        col *= pulse;\n"
    "    }\n"
    "    else if (uMode == 4) {\n"
    "        // Lava\n"
    "        vec2 uv = vWorldPos.xz * 2.0;\n"
    "        float n = noise(uv + uTime * 0.3) * 0.5 + noise(uv * 3.0 - uTime * 0.2) * 0.3;\n"
    "        vec3 hot = uColor * 1.2;\n"
    "        vec3 cool = uColor * 0.3;\n"
    "        col = mix(cool, hot, n) * diff;\n"
    "        col += vec3(1.0, 0.9, 0.7) * pow(n, 3.0) * 0.8;\n"
    "        col += vec3(1.0) * spec * 0.3;\n"
    "        col += hot * fresnel * 0.3;\n"
    "    }\n"
    "    else if (uMode == 5) {\n"
    "        // Ice\n"
    "        float crystal = abs(sin(posGrad * 10.0 + uTime * 0.5)) * 0.3;\n"
    "        col = uColor * (diff + crystal);\n"
    "        col += vec3(0.9, 0.95, 1.0) * spec * 1.2;\n"
    "        col += uColor * fresnel * 0.6;\n"
    "    }\n"
    "    else if (uMode == 6) {\n"
    "        // Toxic\n"
    "        float b = noise(vWorldPos.xz * 8.0 + uTime * 0.5) * noise(vWorldPos.xy * 4.0 + uTime);\n"
    "        col = uColor * diff;\n"
    "        col += vec3(0.2, 1.0, 0.2) * pow(b, 2.0) * 2.0;\n"
    "        col += vec3(0.5, 1.0, 0.5) * fresnel;\n"
    "    }\n"
    "    \n"
    "    gl_FragColor = vec4(col, 1.0);\n"
    "}\n";

static bool LoadGLFunctions() {
    pglCreateShader = (PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader");
    pglShaderSource = (PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource");
    pglCompileShader = (PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader");
    pglCreateProgram = (PFNGLCREATEPROGRAMPROC)wglGetProcAddress("glCreateProgram");
    pglAttachShader = (PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader");
    pglLinkProgram = (PFNGLLINKPROGRAMPROC)wglGetProcAddress("glLinkProgram");
    pglUseProgram = (PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram");
    pglGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)wglGetProcAddress("glGetUniformLocation");
    pglUniform1f = (PFNGLUNIFORM1FPROC)wglGetProcAddress("glUniform1f");
    pglUniform1i = (PFNGLUNIFORM1IPROC)wglGetProcAddress("glUniform1i");
    pglUniform3f = (PFNGLUNIFORM3FPROC)wglGetProcAddress("glUniform3f");
    pglDeleteShader = (PFNGLDELETESHADERPROC)wglGetProcAddress("glDeleteShader");
    pglDeleteProgram = (PFNGLDELETEPROGRAMPROC)wglGetProcAddress("glDeleteProgram");
    pglGetShaderiv = (PFNGLGETSHADERIVPROC)wglGetProcAddress("glGetShaderiv");
    
    return pglCreateShader && pglShaderSource && pglCompileShader && 
           pglCreateProgram && pglAttachShader && pglLinkProgram && 
           pglUseProgram && pglGetUniformLocation && pglUniform1f && pglUniform3f;
}

void ChamsHandInit() {
    if (g_shaderReady) return;
    if (!LoadGLFunctions()) return;
    
    GLuint vs = pglCreateShader(0x8B31); // GL_VERTEX_SHADER
    pglShaderSource(vs, 1, &vertexShader, nullptr);
    pglCompileShader(vs);
    GLint compiled = 0;
    if (pglGetShaderiv) pglGetShaderiv(vs, 0x8B81, &compiled);
    if (!compiled) { if (pglDeleteShader) pglDeleteShader(vs); return; }
    
    GLuint fs = pglCreateShader(0x8B30); // GL_FRAGMENT_SHADER
    pglShaderSource(fs, 1, &fragmentShader, nullptr);
    pglCompileShader(fs);
    if (pglGetShaderiv) pglGetShaderiv(fs, 0x8B81, &compiled);
    if (!compiled) { if (pglDeleteShader) { pglDeleteShader(vs); pglDeleteShader(fs); } return; }
    
    g_program = pglCreateProgram();
    pglAttachShader(g_program, vs);
    pglAttachShader(g_program, fs);
    pglLinkProgram(g_program);
    
    if (pglDeleteShader) { pglDeleteShader(vs); pglDeleteShader(fs); }
    
    g_locTime = pglGetUniformLocation(g_program, "uTime");
    g_locColor = pglGetUniformLocation(g_program, "uColor");
    g_locTex = pglGetUniformLocation(g_program, "uTex");
    g_locMode = pglGetUniformLocation(g_program, "uMode");
    
    g_shaderReady = true;
}

void ChamsHandBegin() {
    if (!g_chamsHandEnabled) return;
    if (!g_shaderReady) ChamsHandInit();
    if (!g_shaderReady) return;
    
    g_shaderTime += 0.016f * g_chamsHandSpeed;
    
    if (g_chamsHandXray) {
        glDisable(GL_DEPTH_TEST);
        ChamsShaderBegin(g_chamsHandColorHid[0], g_chamsHandColorHid[1], g_chamsHandColorHid[2], g_chamsHandMode, g_shaderTime);
        // We can't really do two passes easily here without the draw call being inside.
        // But we can at least show the hidden color if X-Ray is on.
        // Or if we want real dual-pass, we'd need to hook the actual draw call.
    } else {
        ChamsShaderBegin(g_chamsHandColorVis[0], g_chamsHandColorVis[1], g_chamsHandColorVis[2], g_chamsHandMode, g_shaderTime);
    }
}

void ChamsHandEnd() {
    if (!g_chamsHandEnabled || !g_shaderReady) return;
    pglUseProgram(0);
}

void ChamsHandShutdown() {
    if (g_program && pglDeleteProgram) {
        pglDeleteProgram(g_program);
        g_program = 0;
    }
    g_shaderReady = false;
}

void ChamsEntityBegin() {
    if (!g_chamsEnabled) return;
    if (!g_shaderReady) ChamsHandInit();
    if (!g_shaderReady) return;
    
    g_shaderTimeEntity += 0.016f;
    
    pglUseProgram(g_program);
    if (g_locTime >= 0) pglUniform1f(g_locTime, g_shaderTimeEntity);
    if (g_locColor >= 0) pglUniform3f(g_locColor, g_currentAccentColor.x, g_currentAccentColor.y, g_currentAccentColor.z);
    if (g_locTex >= 0 && pglUniform1i) pglUniform1i(g_locTex, 0);
    if (g_locMode >= 0 && pglUniform1i) pglUniform1i(g_locMode, g_chamsHandMode);
}

void ChamsEntityEnd() {
    if (!g_chamsEnabled || !g_shaderReady) return;
    pglUseProgram(0);
}

void ChamsShaderBegin(float r, float g, float b, int mode, float time) {
    if (!g_shaderReady) ChamsHandInit();
    if (!g_shaderReady) return;

    pglUseProgram(g_program);
    if (g_locTime  >= 0) pglUniform1f(g_locTime, time);
    if (g_locColor >= 0) pglUniform3f(g_locColor, r, g, b);
    if (g_locTex   >= 0 && pglUniform1i) pglUniform1i(g_locTex, 0);
    if (g_locMode  >= 0 && pglUniform1i) pglUniform1i(g_locMode, mode);
}

void ChamsShaderEnd() {
    if (!g_shaderReady) return;
    pglUseProgram(0);
}
