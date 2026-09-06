#include "CFString.hpp"

namespace MonitorPlus {

CFString::CFString(const SystemAPIs &apis, const char *value)
    : apis(apis),
      string(apis.CFStringCreateWithCString(nullptr, value, kCFStringEncodingUTF8)) {
}

CFString::~CFString() {
    if (string != nullptr) {
        apis.CFRelease(string);
    }
}

CFStringRef CFString::get() const {
    return string;
}

} // namespace MonitorPlus
