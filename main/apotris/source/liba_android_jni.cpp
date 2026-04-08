/* JNI hooks for org.libsdl.app.SDLActivity — touch overlay + portrait layout (shipped APK parity). */

#ifdef ANDROID

#include <jni.h>

void androidVirtualButtonEvent(int buttonId, bool pressed);
void androidSetPortraitMode(bool portrait);

extern "C" JNIEXPORT void JNICALL
Java_org_libsdl_app_SDLActivity_nativeVirtualButtonEvent(JNIEnv*, jclass, jint buttonId,
                                                         jboolean pressed) {
    androidVirtualButtonEvent(static_cast<int>(buttonId), pressed != JNI_FALSE);
}

extern "C" JNIEXPORT void JNICALL
Java_org_libsdl_app_SDLActivity_nativeSetPortraitMode(JNIEnv*, jclass, jboolean portrait) {
    androidSetPortraitMode(portrait != JNI_FALSE);
}

#endif
