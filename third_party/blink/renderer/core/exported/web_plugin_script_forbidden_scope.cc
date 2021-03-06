// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_plugin_script_forbidden_scope.h"

#include "third_party/blink/renderer/platform/plugins/plugin_script_forbidden_scope.h"

namespace blink {

bool WebPluginScriptForbiddenScope::IsForbidden() {
  return PluginScriptForbiddenScope::IsForbidden();
}

}  // namespace blink
