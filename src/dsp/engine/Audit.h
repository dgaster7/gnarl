#pragma once
// Test-only hooks used by the Check 1 parameter audit. In normal operation `active` is false
// and every check below is a single predictable branch per block/stage.
#include <atomic>
#include <cstdint>

namespace gnarl::audit {
constexpr int kMaxParams = 4096;
// thread_local so the audit can render many patches in parallel threads.
extern thread_local bool active;                 // set only by test code, never while a host is running
extern thread_local uint8_t removed[kMaxParams]; // 1 = hard-remove the stage owned by this parameter
extern thread_local int zeroWetParam;            // the mixer owned by this Mix parameter feeds silence into its wet path
extern thread_local int hookHits[kMaxParams];    // counts how often a removal hook was consulted (proves the hook exists)
inline bool isRemoved(int paramIndex) {
    if (!active || paramIndex < 0) return false;
    ++hookHits[paramIndex];
    return removed[paramIndex] != 0;
}
inline bool zeroWet(int mixParam) { return active && mixParam >= 0 && zeroWetParam == mixParam; }
void clear();
} // namespace gnarl::audit
