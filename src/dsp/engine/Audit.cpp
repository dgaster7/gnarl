#include "Audit.h"
#include <cstring>
namespace gnarl::audit {
thread_local bool active = false;
thread_local uint8_t removed[kMaxParams] = {};
thread_local int zeroWetParam = -1;
thread_local int hookHits[kMaxParams] = {};
void clear() { std::memset(removed, 0, sizeof removed); std::memset(hookHits, 0, sizeof hookHits); zeroWetParam = -1; active = false; }
} // namespace gnarl::audit
