#pragma once
#include "Render.h"
#include "util/Json.h"

namespace gnarl {
constexpr int kPatchSchemaVersion = 1;
Json patchToJson(const Patch& p, bool fxPlugin);
// Unknown parameter ids are ignored, missing ones keep registry defaults (compatibility contract).
// Older schema versions are migrated in migratePatchJson().
bool patchFromJson(const Json& j, Patch& out, std::string* error = nullptr);
Json migratePatchJson(Json j);
} // namespace gnarl
