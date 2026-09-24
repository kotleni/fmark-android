#include "gl_engine.h"

#include <algorithm>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <cmath>
#include <cstring>
#include <random>
#include <vector>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "fmark", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "fmark", __VA_ARGS__)

static constexpr int kMaxFurPopulation = 5000000;

// ---------------- math helpers (column-major, OpenGL style) ----------------

static void matIdentity(float* m) {
    std::memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void matMul(const float* a, const float* b, float* out) {
    float r[16]{};
    for (int c = 0; c < 4; c++) {
        for (int row = 0; row < 4; row++) {
            float v = 0.0f;
            for (int k = 0; k < 4; k++) v += a[k * 4 + row] * b[c * 4 + k];
            r[c * 4 + row] = v;
        }
    }
    std::memcpy(out, r, sizeof(r));
}

static void matRotateX(float ang, float* m) {
    matIdentity(m);
    float c = std::cos(ang), s = std::sin(ang);
    m[5] = c;
    m[6] = s;
    m[9] = -s;
    m[10] = c;
}

static void matRotateY(float ang, float* m) {
    matIdentity(m);
    float c = std::cos(ang), s = std::sin(ang);
    m[0] = c;
    m[2] = -s;
    m[8] = s;
    m[10] = c;
}

static void matTranslate(float x, float y, float z, float* m) {
    matIdentity(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
}

static void matPerspective(float fovy, float aspect, float n, float f, float* m) {
    float t = 1.0f / std::tan(fovy * 0.5f);
    std::memset(m, 0, 16 * sizeof(float));
    m[0] = t / aspect;
    m[5] = t;
    m[10] = (f + n) / (n - f);
    m[11] = -1.0f;
    m[14] = 2.0f * f * n / (n - f);
}

static void matOrtho(float l, float r, float b, float t, float n, float f, float* m) {
    std::memset(m, 0, 16 * sizeof(float));
    m[0] = 2.0f / (r - l);
    m[5] = 2.0f / (t - b);
    m[10] = -2.0f / (f - n);
    m[12] = -(r + l) / (r - l);
    m[13] = -(t + b) / (t - b);
    m[14] = -(f + n) / (f - n);
    m[15] = 1.0f;
}

static void matLookAt(const float* eye, const float* center, const float* upv, float* m) {
    float fwd[3]{center[0] - eye[0], center[1] - eye[1], center[2] - eye[2]};
    float fLen = std::sqrt(fwd[0] * fwd[0] + fwd[1] * fwd[1] + fwd[2] * fwd[2]);
    fwd[0] /= fLen; fwd[1] /= fLen; fwd[2] /= fLen;
    float sside[3]{fwd[1] * upv[2] - fwd[2] * upv[1], fwd[2] * upv[0] - fwd[0] * upv[2],
                   fwd[0] * upv[1] - fwd[1] * upv[0]};
    float sLen = std::sqrt(sside[0] * sside[0] + sside[1] * sside[1] + sside[2] * sside[2]);
    sside[0] /= sLen; sside[1] /= sLen; sside[2] /= sLen;
    float u[3]{sside[1] * fwd[2] - sside[2] * fwd[1], sside[2] * fwd[0] - sside[0] * fwd[2],
               sside[0] * fwd[1] - sside[1] * fwd[0]};
    m[0] = sside[0]; m[1] = u[0]; m[2] = -fwd[0]; m[3] = 0.0f;
    m[4] = sside[1]; m[5] = u[1]; m[6] = -fwd[1]; m[7] = 0.0f;
    m[8] = sside[2]; m[9] = u[2]; m[10] = -fwd[2]; m[11] = 0.0f;
    m[12] = -(sside[0] * eye[0] + sside[1] * eye[1] + sside[2] * eye[2]);
    m[13] = -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]);
    m[14] = (fwd[0] * eye[0] + fwd[1] * eye[1] + fwd[2] * eye[2]);
    m[15] = 1.0f;
}

static long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// ---------------- geometry: torus ----------------

static void buildTorus(std::vector<float>& verts, std::vector<uint32_t>& indices, int major, int minor,
                       float R, float r) {
    for (int i = 0; i < major; i++) {
        float u = static_cast<float>(i) / major;
        float theta = u * 6.2831853f;
        for (int j = 0; j < minor; j++) {
            float v = static_cast<float>(j) / minor;
            float phi = v * 6.2831853f;
            float ct = std::cos(theta), st = std::sin(theta);
            float cp = std::cos(phi), sp = std::sin(phi);
            float px = (R + r * ct) * cp;
            float py = r * st;
            float pz = (R + r * ct) * sp;
            float nx = ct * cp, ny = st, nz = ct * sp;
            verts.insert(verts.end(), {px, py, pz, nx, ny, nz, u, v});
        }
    }
    for (int i = 0; i < major; i++) {
        for (int j = 0; j < minor; j++) {
            int a = i * minor + j;
            int b = ((i + 1) % major) * minor + j;
            int c = ((i + 1) % major) * minor + ((j + 1) % minor);
            int d = i * minor + ((j + 1) % minor);
            indices.insert(indices.end(), {static_cast<uint32_t>(a), static_cast<uint32_t>(b),
                                           static_cast<uint32_t>(d)});
            indices.insert(indices.end(), {static_cast<uint32_t>(b), static_cast<uint32_t>(c),
                                           static_cast<uint32_t>(d)});
        }
    }
}

static void buildFurInstances(std::vector<float>& instances, int count, float R, float r) {
    constexpr float kTwoPi = 6.28318530718f;
    std::mt19937 rng(0xF00DF00Du);
    std::uniform_real_distribution<float> random(0.0f, 1.0f);

    instances.reserve(static_cast<size_t>(count) * 11);
    for (int i = 0; i < count; i++) {
        const float phi = random(rng) * kTwoPi;
        const float cp = std::cos(phi), sp = std::sin(phi);

        // Rejection-sample the tube angle so hairs are distributed by surface
        // area instead of over-populating the inside of the torus.
        float theta;
        do {
            theta = random(rng) * kTwoPi;
            const float areaWeight = (R + r * std::cos(theta)) / (R + r);
            if (random(rng) <= areaWeight) break;
        } while (true);

        const float ct = std::cos(theta), st = std::sin(theta);
        const float ringRadius = R + r * ct;
        const float ox = ringRadius * cp;
        const float oy = r * st;
        const float oz = ringRadius * sp;
        const float nx = ct * cp, ny = st, nz = ct * sp;

        // Rotate an arbitrary tangent around the surface normal per hair.
        float tx = -sp, ty = 0.0f, tz = cp;
        float bx = ny * tz - nz * ty;
        float by = nz * tx - nx * tz;
        float bz = nx * ty - ny * tx;
        const float angle = random(rng) * kTwoPi;
        const float ca = std::cos(angle), sa = std::sin(angle);
        const float rtx = tx * ca + bx * sa;
        const float rty = ty * ca + by * sa;
        const float rtz = tz * ca + bz * sa;
        tx = rtx; ty = rty; tz = rtz;

        const float lengthScale = 0.58f + random(rng) * 0.72f;
        const float shade = random(rng);
        instances.insert(instances.end(), {
            ox, oy, oz,
            nx, ny, nz,
            tx, ty, tz,
            lengthScale, shade,
        });
    }
}

// ---------------- shaders (GLSL ES 3.00) ----------------

static const char* kShadowVs = R"GLSL(
#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
uniform mat4 uLightVP;
uniform mat4 uModel;
uniform float uFurLength;
void main() {
    vec3 pos = aPos + aNormal * uFurLength;
    gl_Position = uLightVP * uModel * vec4(pos, 1.0);
}
)GLSL";

static const char* kShadowFs = R"GLSL(
#version 300 es
precision highp float;
void main() {}
)GLSL";

static const char* kBaseVs = R"GLSL(
#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
out vec3 vWorld;
out vec3 vNormal;
out vec2 vUv;
void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorld = world.xyz;
    vNormal = mat3(uModel) * aNormal;
    vUv = aUv;
    gl_Position = uProj * uView * world;
}
)GLSL";

static const char* kBaseFs = R"GLSL(
#version 300 es
precision highp float;
in vec3 vWorld;
in vec3 vNormal;
in vec2 vUv;
uniform vec3 uCamPos;
uniform vec3 uLightDir;
out vec4 outColor;

float hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    vec3 n = normalize(vNormal);
    vec3 l = normalize(uLightDir);
    vec3 v = normalize(uCamPos - vWorld);
    float diffuse = max(dot(n, l), 0.0);
    float rim = pow(1.0 - max(dot(n, v), 0.0), 2.4);
    float grain = hash21(floor(vUv * vec2(96.0, 32.0)));

    vec3 root = vec3(0.30, 0.105, 0.025);
    vec3 warm = vec3(0.52, 0.22, 0.055);
    vec3 albedo = mix(root, warm, 0.28 + grain * 0.12);
    vec3 color = albedo * (0.48 + 0.62 * diffuse);
    color += vec3(0.32, 0.12, 0.025) * rim * 0.35;
    outColor = vec4(color, 1.0);
}
)GLSL";

static const char* kFurVs = R"GLSL(
#version 300 es
layout(location = 0) in vec3 aShape;      // tangent-plane offset + tip flag
layout(location = 1) in float aTip;
layout(location = 2) in vec3 aOrigin;      // per-instance torus position
layout(location = 3) in vec3 aNormal;      // per-instance surface normal
layout(location = 4) in vec3 aTangent;     // per-instance random orientation
layout(location = 5) in vec2 aFurParams;   // length scale, shade/bend seed
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform float uFurLength;
out vec3 vWorld;
out vec3 vNormal;
out float vTip;
out float vShade;
void main() {
    vec3 n = normalize(aNormal);
    vec3 t = normalize(aTangent);
    vec3 b = normalize(cross(n, t));
    float tip = clamp(aTip, 0.0, 1.0);
    float width = mix(1.0, 0.82, tip);
    float length = uFurLength * aFurParams.x;
    float bend = (aFurParams.y * 2.0 - 1.0) * 0.16 * uFurLength * tip * tip;

    vec3 objectPos = aOrigin
                   + t * aShape.x * width
                   + b * aShape.y * width
                   + n * aShape.z * length
                   + t * bend;
    vec4 world = uModel * vec4(objectPos, 1.0);

    vWorld = world.xyz;
    vNormal = mat3(uModel) * n;
    vTip = tip;
    vShade = aFurParams.y;
    gl_Position = uProj * uView * world;
}
)GLSL";

static const char* kFurFs = R"GLSL(
#version 300 es
precision highp float;
in vec3 vWorld;
in vec3 vNormal;
in float vTip;
in float vShade;
uniform vec3 uCamPos;
uniform vec3 uLightDir;
out vec4 outColor;
void main() {
    vec3 n = normalize(vNormal);
    if (!gl_FrontFacing) n = -n;
    vec3 l = normalize(uLightDir);
    vec3 v = normalize(uCamPos - vWorld);
    float diffuse = max(dot(n, l), 0.0);
    float rim = pow(1.0 - max(dot(n, v), 0.0), 2.2);

    vec3 root = vec3(0.20, 0.045, 0.010);
    vec3 tip = vec3(1.00, 0.56, 0.16);
    float gradient = smoothstep(0.0, 1.0, pow(vTip, 0.72));
    vec3 albedo = mix(root, tip, gradient) * mix(0.80, 1.15, vShade);
    vec3 color = albedo * (0.42 + 0.72 * diffuse);
    color += tip * rim * (0.04 + 0.16 * vTip);
    outColor = vec4(color, 1.0);
}
)GLSL";

// ---------------- engine ----------------

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]{};
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        LOGE("shader compile failed: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint GlEngine::compileProgram(const char* vs, const char* fs) {
    GLuint v = compileShader(GL_VERTEX_SHADER, vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) {
        if (v) glDeleteShader(v);
        if (f) glDeleteShader(f);
        return 0;
    }
    GLuint p = glCreateProgram();
    glBindAttribLocation(p, 0, "aPos");
    glBindAttribLocation(p, 1, "aNormal");
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]{};
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        LOGE("program link failed: %s", log);
        glDeleteProgram(p);
        p = 0;
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

GlEngine::GlEngine() {}

GlEngine::~GlEngine() { stop(); }

bool GlEngine::surfaceCreated(ANativeWindow* window, int width, int height) {
    if (window_ != nullptr) {
        LOGE("surface already set");
        return false;
    }
    window_ = window;
    ANativeWindow_acquire(window_);
    winW_ = width;
    winH_ = height;

    if (!initGL()) {
        LOGE("EGL init failed");
        ANativeWindow_release(window_);
        window_ = nullptr;
        return false;
    }

    // initGL() leaves the context current on the SurfaceView callback thread.
    // EGL does not allow a current context to be transferred to another
    // thread implicitly, so detach it before renderLoop() binds it.
    if (!eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
        LOGE("eglMakeCurrent before render handoff failed 0x%x", eglGetError());
        destroyGL();
        ANativeWindow_release(window_);
        window_ = nullptr;
        return false;
    }

    running_ = true;
    startTimeMs_ = nowMs();
    currentFrame_ = 0;
    renderThread_ = std::thread([this]() { renderLoop(); });
    LOGI("render loop started (%dx%d)", winW_, winH_);
    return true;
}

void GlEngine::surfaceChanged(int width, int height) {
    std::lock_guard<std::mutex> lock(configMutex_);
    winW_ = width;
    winH_ = height;
}

void GlEngine::surfaceDestroyed() {
    running_ = false;
    if (renderThread_.joinable()) renderThread_.join();
    destroyGL();
    if (window_) {
        ANativeWindow_release(window_);
        window_ = nullptr;
    }
    {
        std::lock_guard<std::mutex> lock(fpsMutex_);
        fpsHistory_.clear();
        fpsSmooth_ = 0.0f;
    }
}

void GlEngine::setResolution(int w, int h) {
    std::lock_guard<std::mutex> lock(configMutex_);
    renderW_ = w;
    renderH_ = h;
}

void GlEngine::setPresentMode(int mode) {
    std::lock_guard<std::mutex> lock(configMutex_);
    presentMode_ = mode;
    if (display_ != EGL_NO_DISPLAY && glInitialized_) {
        eglSwapInterval(display_, mode == 0 ? 0 : 1);
    }
}

void GlEngine::setAntialiasing(int samples) {
    std::lock_guard<std::mutex> lock(configMutex_);
    antialiasingSamples_ = samples == 2 || samples == 4 || samples == 8 ? samples : 0;
}

void GlEngine::setFurLength(float v) {
    std::lock_guard<std::mutex> lock(configMutex_);
    furLength_ = v;
}

void GlEngine::setFurPopulation(int population) {
    std::lock_guard<std::mutex> lock(configMutex_);
    furPopulation_ = std::clamp(population, 0, kMaxFurPopulation);
}

void GlEngine::stop() {
    running_ = false;
    if (renderThread_.joinable()) renderThread_.join();
}

float GlEngine::getFps() const {
    std::lock_guard<std::mutex> lock(fpsMutex_);
    return fpsSmooth_;
}

int GlEngine::getFpsHistory(float* out, int max) const {
    std::lock_guard<std::mutex> lock(fpsMutex_);
    int i = 0;
    for (float v : fpsHistory_) {
        if (i >= max) break;
        out[i++] = v;
    }
    return i;
}

bool GlEngine::initGL() {
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY) return false;
    if (!eglInitialize(display_, nullptr, nullptr)) return false;

    EGLint requestedSamples = 0;
    {
        std::lock_guard<std::mutex> lock(configMutex_);
        requestedSamples = antialiasingSamples_;
    }

    auto chooseConfig = [&](EGLint samples) {
        std::vector<EGLint> attributes = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
            EGL_SAMPLE_BUFFERS, samples > 0 ? 1 : 0,
        };
        if (samples > 0) {
            attributes.push_back(EGL_SAMPLES);
            attributes.push_back(samples);
        }
        attributes.push_back(EGL_NONE);
        EGLint count = 0;
        return eglChooseConfig(
                   display_, attributes.data(), &config_, 1, &count) && count > 0;
    };

    bool configSelected = chooseConfig(requestedSamples);
    if (!configSelected && requestedSamples > 0) {
        for (EGLint fallback : {4, 2, 0}) {
            if (fallback < requestedSamples && chooseConfig(fallback)) {
                LOGE("requested %d× MSAA unavailable; using %d×", requestedSamples, fallback);
                configSelected = true;
                break;
            }
        }
    }
    if (!configSelected) {
        LOGE("no EGL ES3 config");
        return false;
    }

    EGLint actualSamples = 0;
    eglGetConfigAttrib(display_, config_, EGL_SAMPLES, &actualSamples);
    LOGI("EGL samples requested=%d actual=%d", requestedSamples, actualSamples);

    surface_ = eglCreateWindowSurface(display_, config_, window_, nullptr);
    if (surface_ == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed 0x%x", eglGetError());
        return false;
    }

    const EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context_ = eglCreateContext(display_, config_, EGL_NO_CONTEXT, ctxAttribs);
    if (context_ == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed 0x%x", eglGetError());
        return false;
    }

    if (!eglMakeCurrent(display_, surface_, surface_, context_)) {
        LOGE("eglMakeCurrent failed 0x%x", eglGetError());
        return false;
    }

    eglQuerySurface(display_, surface_, EGL_WIDTH, &winW_);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &winH_);
    eglSwapInterval(display_, presentMode_ == 0 ? 0 : 1);

    buildGeometry();

    progShadow_ = compileProgram(kShadowVs, kShadowFs);
    progBase_ = compileProgram(kBaseVs, kBaseFs);
    progFur_ = compileProgram(kFurVs, kFurFs);
    if (!progShadow_ || !progBase_ || !progFur_) {
        LOGE("program build failed");
        return false;
    }

    // shadow map (depth texture + dummy color)
    glGenTextures(1, &smDepthTex_);
    glBindTexture(GL_TEXTURE_2D, smDepthTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, smSize_, smSize_, 0, GL_DEPTH_COMPONENT,
                 GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    GLuint smColorRb = 0;
    glGenRenderbuffers(1, &smColorRb);
    glBindRenderbuffer(GL_RENDERBUFFER, smColorRb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, smSize_, smSize_);

    glGenFramebuffers(1, &smFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, smFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, smDepthTex_, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, smColorRb);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("shadow FBO incomplete 0x%x", status);
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteRenderbuffers(1, &smColorRb);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glInitialized_ = true;
    LOGI("GL init ok");
    return true;
}

void GlEngine::destroyGL() {
    if (!glInitialized_) return;

    // The render thread releases the context when it exits. Rebind it on the
    // calling thread before deleting GL objects, otherwise those calls have no
    // current context and are invalid.
    bool contextCurrent = false;
    if (display_ != EGL_NO_DISPLAY && surface_ != EGL_NO_SURFACE && context_ != EGL_NO_CONTEXT) {
        contextCurrent = eglMakeCurrent(display_, surface_, surface_, context_);
        if (!contextCurrent) {
            LOGE("eglMakeCurrent before GL cleanup failed 0x%x", eglGetError());
        }
    }

    if (contextCurrent) {
        if (smFbo_) glDeleteFramebuffers(1, &smFbo_);
        if (smDepthTex_) glDeleteTextures(1, &smDepthTex_);
        if (progFur_) glDeleteProgram(progFur_);
        if (progBase_) glDeleteProgram(progBase_);
        if (progShadow_) glDeleteProgram(progShadow_);
        destroyGeometry();
    }
    glInitialized_ = false;

    if (contextCurrent) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    if (context_ != EGL_NO_CONTEXT && display_ != EGL_NO_DISPLAY) {
        eglDestroyContext(display_, context_);
        context_ = EGL_NO_CONTEXT;
    }
    if (surface_ != EGL_NO_SURFACE && display_ != EGL_NO_DISPLAY) {
        eglDestroySurface(display_, surface_);
        surface_ = EGL_NO_SURFACE;
    }
    if (display_ != EGL_NO_DISPLAY) {
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }
}

void GlEngine::buildGeometry() {
    constexpr float kMajorRadius = 1.3f;
    constexpr float kMinorRadius = 0.58f;
    constexpr float kHairRadius = 0.016f;
    constexpr int kMajorSegments = 96;
    constexpr int kMinorSegments = 32;

    std::vector<float> verts;
    std::vector<uint32_t> indices;
    buildTorus(verts, indices, kMajorSegments, kMinorSegments, kMajorRadius, kMinorRadius);
    indexCount_ = static_cast<GLsizei>(indices.size());

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &ibo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          reinterpret_cast<void*>(6 * sizeof(float)));
    glBindVertexArray(0);

    // One three-sided tapered pyramid is instanced for every hair. Shape data
    // contains the tangent-plane base offset and a 0/1 tip flag. Per-instance
    // data contains origin, surface normal, orientation, length and variation.
    std::vector<float> furShape;
    furShape.reserve(9 * 4);
    for (int side = 0; side < 3; side++) {
        const float a0 = static_cast<float>(side) * 2.0943951f;
        const float a1 = static_cast<float>((side + 1) % 3) * 2.0943951f;
        furShape.insert(furShape.end(), {
            std::cos(a0) * kHairRadius, std::sin(a0) * kHairRadius, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 1.0f,
            std::cos(a1) * kHairRadius, std::sin(a1) * kHairRadius, 0.0f, 0.0f,
        });
    }
    furShapeVertexCount_ = static_cast<GLsizei>(furShape.size() / 4);

    std::vector<float> furInstances;
    buildFurInstances(furInstances, kMaxFurPopulation, kMajorRadius, kMinorRadius);
    furCapacity_ = kMaxFurPopulation;

    glGenVertexArrays(1, &furVao_);
    glBindVertexArray(furVao_);
    glGenBuffers(1, &furShapeVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, furShapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, furShape.size() * sizeof(float), furShape.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));

    constexpr GLsizei kFurInstanceStride = 11 * sizeof(float);
    glGenBuffers(1, &furInstanceVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, furInstanceVbo_);
    glBufferData(GL_ARRAY_BUFFER, furInstances.size() * sizeof(float), furInstances.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, kFurInstanceStride, nullptr);
    glVertexAttribDivisor(2, 1);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, kFurInstanceStride,
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glVertexAttribDivisor(3, 1);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, kFurInstanceStride,
                          reinterpret_cast<void*>(6 * sizeof(float)));
    glVertexAttribDivisor(4, 1);
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, kFurInstanceStride,
                          reinterpret_cast<void*>(9 * sizeof(float)));
    glVertexAttribDivisor(5, 1);
    glBindVertexArray(0);
}

void GlEngine::destroyGeometry() {
    if (furInstanceVbo_) glDeleteBuffers(1, &furInstanceVbo_);
    if (furShapeVbo_) glDeleteBuffers(1, &furShapeVbo_);
    if (furVao_) glDeleteVertexArrays(1, &furVao_);
    if (ibo_) glDeleteBuffers(1, &ibo_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    furInstanceVbo_ = furShapeVbo_ = furVao_ = 0;
    ibo_ = vbo_ = vao_ = 0;
    furCapacity_ = 0;
}

void GlEngine::renderLoop() {
    // EGL contexts are current on one thread at a time. initGL() creates the
    // context on the SurfaceView callback thread, so transfer it to this thread
    // before making any GL calls or presenting frames.
    if (!eglMakeCurrent(display_, surface_, surface_, context_)) {
        LOGE("render thread eglMakeCurrent failed 0x%x", eglGetError());
        running_ = false;
        return;
    }
    LOGI("render thread EGL context current");

    constexpr auto kWarmupDuration = std::chrono::seconds(2);
    constexpr auto kSampleDuration = std::chrono::seconds(1);
    constexpr int kMinimumWarmupFrames = 2;
    bool metricsReady = false;
    int warmupFrames = 0;
    int framesInSample = 0;
    const auto warmupStarted = std::chrono::steady_clock::now();
    auto sampleStarted = warmupStarted;

    while (running_) {
        if (!glInitialized_) break;

        int renderW = 0, renderH = 0;
        int winW = 0, winH = 0;
        int population = 0;
        float furLen = 0.0f;
        {
            std::lock_guard<std::mutex> lock(configMutex_);
            renderW = renderW_;
            renderH = renderH_;
            winW = winW_;
            winH = winH_;
            population = std::clamp(furPopulation_, 0, furCapacity_);
            furLen = furLength_;
        }

        // SurfaceHolder may apply a fixed buffer size asynchronously. Query
        // EGL every frame so the viewport always matches the real backing
        // surface, not just the SurfaceView's layout dimensions.
        EGLint queriedW = 0;
        EGLint queriedH = 0;
        if (eglQuerySurface(display_, surface_, EGL_WIDTH, &queriedW) &&
            eglQuerySurface(display_, surface_, EGL_HEIGHT, &queriedH) && queriedW > 0 && queriedH > 0) {
            winW = queriedW;
            winH = queriedH;
        }
        if (winW <= 1 || winH <= 1) break;
        if (currentFrame_ == 0) {
            LOGI("render surface %dx%d (target %dx%d)", winW, winH, renderW, renderH);
        }

        float timeSec = static_cast<float>(nowMs() - startTimeMs_) / 1000.0f;

        // letterbox viewport to preserve the chosen aspect
        float renderAspect = renderW > 0 && renderH > 0 ? static_cast<float>(renderW) / renderH
                                                        : static_cast<float>(winW) / winH;
        float surfaceAspect = static_cast<float>(winW) / winH;
        int vpW = winW, vpH = winH;
        if (renderAspect > surfaceAspect) {
            vpH = static_cast<int>(vpW / renderAspect);
        } else {
            vpW = static_cast<int>(vpH * renderAspect);
        }
        int vpX = (winW - vpW) / 2;
        int vpY = (winH - vpH) / 2;

        // ---- camera / model matrices ----
        float model[16];
        {
            float rot[16], tilt[16], combined[16], trans[16];
            matRotateY(timeSec * 0.55f, rot);
            matRotateX(0.55f, tilt);
            matMul(rot, tilt, combined);
            matTranslate(0.0f, std::sin(timeSec * 1.1f) * 0.12f, 0.0f, trans);
            matMul(trans, combined, model);
        }
        float ang = timeSec * 0.42f;
        float eye[3]{std::sin(ang) * 10.5f, 2.2f + std::sin(timeSec * 0.8f) * 0.3f,
                     std::cos(ang) * 10.5f};
        float center[3]{0.0f, 0.0f, 0.0f};
        float upv[3]{0.0f, 1.0f, 0.0f};
        float view[16];
        matLookAt(eye, center, upv, view);
        float proj[16];
        matPerspective(0.90f, static_cast<float>(vpW) / std::max(vpH, 1), 0.1f, 100.0f, proj);

        // light direction (travels toward scene) + shadow matrices
        float ldir[3]{0.55f, 0.8f, 0.35f};
        float lLen = std::sqrt(ldir[0] * ldir[0] + ldir[1] * ldir[1] + ldir[2] * ldir[2]);
        ldir[0] /= lLen; ldir[1] /= lLen; ldir[2] /= lLen;
        const float kSmHalf = 3.6f;
        const float kLightDist = 7.0f;
        float lEye[3]{-ldir[0] * kLightDist, -ldir[1] * kLightDist, -ldir[2] * kLightDist};
        float lView[16], lProj[16], lVP[16];
        matLookAt(lEye, center, upv, lView);
        matOrtho(-kSmHalf, kSmHalf, -kSmHalf, kSmHalf, 1.0f, 15.0f, lProj);
        matMul(lProj, lView, lVP);

        // ---- pass 1: shadow map from the light (fur envelope silhouette) ----
        glBindFramebuffer(GL_FRAMEBUFFER, smFbo_);
        glViewport(0, 0, smSize_, smSize_);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        glUseProgram(progShadow_);
        glUniformMatrix4fv(glGetUniformLocation(progShadow_, "uLightVP"), 1, GL_FALSE, lVP);
        glUniformMatrix4fv(glGetUniformLocation(progShadow_, "uModel"), 1, GL_FALSE, model);
        glUniform1f(glGetUniformLocation(progShadow_, "uFurLength"), furLen);
        glBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);

        // ---- pass 2: main render ----
        // Restore all clear-affecting state before clearing. In particular, a
        // masked depth clear would retain stale values and reject large parts
        // of the rotating donut.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glViewport(0, 0, winW, winH);
        glClearColor(0.018f, 0.022f, 0.035f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(vpX, vpY, vpW, vpH);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, smDepthTex_);
        glBindVertexArray(vao_);

        // solid base
        glUseProgram(progBase_);
        glUniformMatrix4fv(glGetUniformLocation(progBase_, "uModel"), 1, GL_FALSE, model);
        glUniformMatrix4fv(glGetUniformLocation(progBase_, "uView"), 1, GL_FALSE, view);
        glUniformMatrix4fv(glGetUniformLocation(progBase_, "uProj"), 1, GL_FALSE, proj);
        glUniform3f(glGetUniformLocation(progBase_, "uCamPos"), eye[0], eye[1], eye[2]);
        glUniform3f(glGetUniformLocation(progBase_, "uLightDir"), ldir[0], ldir[1], ldir[2]);
        glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);

        // Each instance is one real, tapered 3D fiber. Draw them opaque so
        // depth sorting is deterministic and the tips stay sharply pointed.
        if (population > 0) {
            glBindVertexArray(furVao_);
            glDisable(GL_CULL_FACE);
            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            glUseProgram(progFur_);
            glUniformMatrix4fv(glGetUniformLocation(progFur_, "uModel"), 1, GL_FALSE, model);
            glUniformMatrix4fv(glGetUniformLocation(progFur_, "uView"), 1, GL_FALSE, view);
            glUniformMatrix4fv(glGetUniformLocation(progFur_, "uProj"), 1, GL_FALSE, proj);
            glUniform3f(glGetUniformLocation(progFur_, "uCamPos"), eye[0], eye[1], eye[2]);
            glUniform3f(glGetUniformLocation(progFur_, "uLightDir"), ldir[0], ldir[1], ldir[2]);
            glUniform1f(glGetUniformLocation(progFur_, "uFurLength"), furLen);
            glDrawArraysInstanced(GL_TRIANGLES, 0, furShapeVertexCount_, population);
            glEnable(GL_CULL_FACE);
        }

        if (!eglSwapBuffers(display_, surface_)) {
            LOGE("eglSwapBuffers failed 0x%x", eglGetError());
            break;
        }
        const auto frameEnd = std::chrono::steady_clock::now();
        currentFrame_++;

        if (!metricsReady) {
            // Keep warm-up synchronous so uncapped mode cannot queue a burst of
            // work that makes the first measured interval artificially fast.
            glFinish();
            warmupFrames++;
            const auto warmupFrameEnd = std::chrono::steady_clock::now();
            if (warmupFrames >= kMinimumWarmupFrames &&
                warmupFrameEnd - warmupStarted >= kWarmupDuration) {
                {
                    std::lock_guard<std::mutex> lock(fpsMutex_);
                    fpsHistory_.clear();
                    fpsSmooth_ = 0.0f;
                }
                sampleStarted = std::chrono::steady_clock::now();
                framesInSample = 0;
                metricsReady = true;
                LOGI("benchmark warm-up complete after %d frames", warmupFrames);
            }
            continue;
        }

        framesInSample++;
        const auto sampleElapsed = frameEnd - sampleStarted;
        if (sampleElapsed < kSampleDuration) continue;
        const float sampleMs = std::chrono::duration_cast<std::chrono::microseconds>(sampleElapsed)
                                   .count() / 1000.0f;
        const float fps = framesInSample * 1000.0f / std::max(sampleMs, 0.001f);
        {
            std::lock_guard<std::mutex> lock(fpsMutex_);
            fpsSmooth_ = fps;
            fpsHistory_.push_back(fps);
            if (fpsHistory_.size() > 300) fpsHistory_.pop_front();
        }
        framesInSample = 0;
        sampleStarted = frameEnd;
    }

    if (!eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
        LOGE("render thread eglMakeCurrent cleanup failed 0x%x", eglGetError());
    }
}