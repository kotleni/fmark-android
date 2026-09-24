#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>

#include <memory>
#include <vector>

#include "gl_engine.h"

#define LOG_TAG "fmark"

static std::unique_ptr<GlEngine> gEngine;

static GlEngine& engine() {
    if (!gEngine) gEngine = std::make_unique<GlEngine>();
    return *gEngine;
}

extern "C" {

JNIEXPORT void JNICALL
Java_app_kotleni_fmark_GlRenderer_init(JNIEnv* env, jobject /*thiz*/, jobject activity) {
    (void)env;
    (void)activity;
    engine();
}

JNIEXPORT void JNICALL
Java_app_kotleni_fmark_GlRenderer_surfaceCreated(JNIEnv* env, jobject /*thiz*/,
                                                 jobject surface) {
    ANativeWindow* window = surface ? ANativeWindow_fromSurface(env, surface) : nullptr;
    if (!window) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "surfaceCreated: null window");
        return;
    }
    const bool started = engine().surfaceCreated(window, 1, 1);
    // ANativeWindow_fromSurface returns its own reference. GlEngine retains its
    // own reference, so release the JNI-owned one after the call.
    ANativeWindow_release(window);
    if (!started) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "surfaceCreated: engine failed to start");
    }
}

JNIEXPORT void JNICALL
Java_app_kotleni_fmark_GlRenderer_surfaceChanged(JNIEnv* /*env*/, jobject /*thiz*/,
                                                 jint width, jint height) {
    engine().surfaceChanged(width, height);
}

JNIEXPORT void JNICALL
Java_app_kotleni_fmark_GlRenderer_surfaceDestroyed(JNIEnv* /*env*/, jobject /*thiz*/) {
    engine().surfaceDestroyed();
}

JNIEXPORT void JNICALL
Java_app_kotleni_fmark_GlRenderer_setResolution(JNIEnv* /*env*/, jobject /*thiz*/, jint w,
                                                jint h) {
    engine().setResolution(w, h);
}

JNIEXPORT void JNICALL
Java_app_kotleni_fmark_GlRenderer_setPresentMode(JNIEnv* /*env*/, jobject /*thiz*/, jint mode) {
    engine().setPresentMode(mode);
}

JNIEXPORT void JNICALL
Java_app_kotleni_fmark_GlRenderer_setAntialiasing(JNIEnv* /*env*/, jobject /*thiz*/,
                                                jint samples) {
    engine().setAntialiasing(samples);
}

JNIEXPORT void JNICALL
Java_app_kotleni_fmark_GlRenderer_setFurLength(JNIEnv* /*env*/, jobject /*thiz*/, jfloat value) {
    engine().setFurLength(value);
}

JNIEXPORT void JNICALL
Java_app_kotleni_fmark_GlRenderer_setFurPopulation(JNIEnv* /*env*/, jobject /*thiz*/,
                                                    jint population) {
    engine().setFurPopulation(population);
}

JNIEXPORT void JNICALL
Java_app_kotleni_fmark_GlRenderer_stop(JNIEnv* /*env*/, jobject /*thiz*/) {
    engine().stop();
}

JNIEXPORT jfloat JNICALL
Java_app_kotleni_fmark_GlRenderer_getFps(JNIEnv* /*env*/, jobject /*thiz*/) {
    return static_cast<jfloat>(engine().getFps());
}

JNIEXPORT jfloatArray JNICALL
Java_app_kotleni_fmark_GlRenderer_getFpsHistory(JNIEnv* env, jobject /*thiz*/, jint maxCount) {
    if (maxCount <= 0) return env->NewFloatArray(0);
    std::vector<float> hist(maxCount, 0.0f);
    const int count = engine().getFpsHistory(hist.data(), maxCount);
    jfloatArray out = env->NewFloatArray(static_cast<jsize>(count));
    if (out && count > 0) {
        env->SetFloatArrayRegion(out, 0, static_cast<jsize>(count), hist.data());
    }
    return out;
}

}  // extern "C"