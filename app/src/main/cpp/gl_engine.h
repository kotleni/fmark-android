#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <android/native_window.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>

class GlEngine {
public:
    GlEngine();
    ~GlEngine();

    bool surfaceCreated(ANativeWindow* window, int width, int height);
    void surfaceChanged(int width, int height);
    void surfaceDestroyed();

    void setResolution(int w, int h);
    void setPresentMode(int mode);
    void setAntialiasing(int samples);
    void setFurLength(float v);
    void setFurPopulation(int population);
    void stop();

    float getFps() const;
    int getFpsHistory(float* out, int max) const;

private:
    void renderLoop();
    bool initGL();
    void destroyGL();
    void buildGeometry();
    void destroyGeometry();
    GLuint compileProgram(const char* vs, const char* fs);
    void updateState(float timeSec, int vpW, int vpH);

    ANativeWindow* window_ = nullptr;
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLConfig config_ = nullptr;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLSurface surface_ = EGL_NO_SURFACE;
    bool glInitialized_ = false;

    GLuint vbo_ = 0;
    GLuint ibo_ = 0;
    GLuint vao_ = 0;
    GLsizei indexCount_ = 0;

    GLuint furShapeVbo_ = 0;
    GLuint furInstanceVbo_ = 0;
    GLuint furVao_ = 0;
    GLsizei furShapeVertexCount_ = 0;
    int furCapacity_ = 0;

    GLuint progShadow_ = 0;
    GLuint progBase_ = 0;
    GLuint progFur_ = 0;

    GLuint smFbo_ = 0;
    GLuint smDepthTex_ = 0;
    GLint smSize_ = 1024;

    int winW_ = 0;
    int winH_ = 0;

    std::atomic<bool> running_{false};
    std::thread renderThread_;
    mutable std::mutex configMutex_;

    int renderW_ = 0;
    int renderH_ = 0;
    int presentMode_ = 0;
    int antialiasingSamples_ = 0;
    float furLength_ = 0.45f;
    int furPopulation_ = 8000;

    long long startTimeMs_ = 0;
    mutable std::mutex fpsMutex_;
    float fpsSmooth_ = 0.0f;
    std::deque<float> fpsHistory_;

    uint32_t currentFrame_ = 0;
};