#ifndef inputTesterCoreInputEventSinkH
#define inputTesterCoreInputEventSinkH

#include "inputEvent.h"

namespace inputTester {

class inputEventSink {
public:
    virtual ~inputEventSink() = default;
    virtual void onInputEvent(const inputEvent& event) = 0;
};

} // namespace inputTester

#endif // inputTesterCoreInputEventSinkH
