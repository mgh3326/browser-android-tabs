// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "android_webview/public/browser/draw_fn.h"
#include "base/containers/flat_map.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"

namespace {

struct FunctorData {
  int functor = 0;
  void* data = nullptr;
  AwDrawFnFunctorCallbacks* functor_callbacks = nullptr;
  bool released = false;
};

class FunctorMap {
 public:
  int allocate(void* data, AwDrawFnFunctorCallbacks* functor_callbacks) {
    base::AutoLock lock(lock_);
    int functor = next_functor_++;
    map_.emplace(functor, FunctorData{functor, data, functor_callbacks});
    return functor;
  }

  FunctorData get(int functor) {
    base::AutoLock lock(lock_);
    auto itr = map_.find(functor);
    DCHECK(itr != map_.end());
    return itr->second;
  }

  void mark_released(int functor) {
    base::AutoLock lock(lock_);
    auto itr = map_.find(functor);
    DCHECK(itr != map_.end());
    DCHECK(!itr->second.released);
    itr->second.released = true;
  }

  void destroy_released() {
    base::AutoLock lock(lock_);
    for (auto itr = map_.begin(); itr != map_.end();) {
      FunctorData& data = itr->second;
      if (data.released) {
        // Holding lock here, but not too terrible.
        data.functor_callbacks->on_context_destroyed(data.functor, data.data);
        data.functor_callbacks->on_destroyed(data.functor, data.data);
        itr = map_.erase(itr);
      } else {
        DLOG(ERROR) << "Functor not released. Possibly leaking instead";
        ++itr;
      }
    }
  }

 private:
  base::Lock lock_;
  base::flat_map<int, FunctorData> map_;
  int next_functor_ = 1;
};

base::LazyInstance<FunctorMap>::Leaky g_map;

AwDrawFnRenderMode QueryRenderMode() {
  return AW_DRAW_FN_RENDER_MODE_OPENGL_ES;
}

int CreateFunctor(void* data, AwDrawFnFunctorCallbacks* functor_callbacks) {
  return g_map.Get().allocate(data, functor_callbacks);
}

void ReleaseFunctor(int functor) {
  g_map.Get().mark_released(functor);
}

}  // namespace

extern "C" {

JNIEXPORT jlong JNICALL
Java_org_chromium_android_1webview_shell_DrawFn_nativeGetDrawFnFunctionTable(
    JNIEnv*,
    jclass) {
  static AwDrawFnFunctionTable table{
      kAwDrawFnVersion,
      &QueryRenderMode,
      &CreateFunctor,
      &ReleaseFunctor,
  };
  return reinterpret_cast<intptr_t>(&table);
}

JNIEXPORT void JNICALL
Java_org_chromium_android_1webview_shell_DrawFn_nativeSync(
    JNIEnv*,
    jclass,
    jint functor,
    jboolean force_apply_dark) {
  FunctorData data = g_map.Get().get(functor);
  AwDrawFn_OnSyncParams params{kAwDrawFnVersion, force_apply_dark};
  data.functor_callbacks->on_sync(functor, data.data, &params);
}

JNIEXPORT void JNICALL
Java_org_chromium_android_1webview_shell_DrawFn_nativeDestroyReleased(JNIEnv*,
                                                                      jclass) {
  g_map.Get().destroy_released();
}

JNIEXPORT void JNICALL
Java_org_chromium_android_1webview_shell_DrawFn_nativeDestroyed(JNIEnv*,
                                                                jclass,
                                                                jint functor) {}

JNIEXPORT void JNICALL
Java_org_chromium_android_1webview_shell_DrawFn_nativeDrawGL(JNIEnv*,
                                                             jclass,
                                                             jint functor,
                                                             jint width,
                                                             jint height,
                                                             jint scroll_x,
                                                             jint scroll_y) {
  FunctorData data = g_map.Get().get(functor);
  AwDrawFn_DrawGLParams params{kAwDrawFnVersion};
  params.is_layer = false;
  params.width = width;
  params.height = height;
  params.clip_left = 0;
  params.clip_top = 0;
  params.clip_bottom = height;
  params.clip_right = width;
  params.transform[0] = 1.0;
  params.transform[1] = 0.0;
  params.transform[2] = 0.0;
  params.transform[3] = 0.0;

  params.transform[4] = 0.0;
  params.transform[5] = 1.0;
  params.transform[6] = 0.0;
  params.transform[7] = 0.0;

  params.transform[8] = 0.0;
  params.transform[9] = 0.0;
  params.transform[10] = 1.0;
  params.transform[11] = 0.0;

  params.transform[12] = -scroll_x;
  params.transform[13] = -scroll_y;
  params.transform[14] = 0.0;
  params.transform[15] = 1.0;
  data.functor_callbacks->draw_gl(functor, data.data, &params);
}

}  // extern "C"
