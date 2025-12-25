#ifndef inputTesterPlatformInputBackendH
#define inputTesterPlatformInputBackendH

#include <memory>

#include "inputtester/core/inputEventSink.h"

class QObject;

namespace inputTester {

class inputBackend {
public:
    virtual ~inputBackend() = default;
    virtual void start(QObject* eventSource) = 0;
    virtual void stop() = 0;
    virtual void setSink(inputEventSink* sink) = 0;
};

std::unique_ptr<inputBackend> createInputBackend();

} // namespace inputTester

#endif // inputTesterPlatformInputBackendH
