#ifndef inputTesterPlatformInputBackendH
#define inputTesterPlatformInputBackendH

#include <memory>

#include <QString>

#include "inputtester/core/inputEventSink.h"

class QObject;

namespace inputTester
{

class inputBackend
{
public:
    virtual ~inputBackend() = default;
    virtual bool start(QObject* eventSource, QString* errorMessage) = 0;
    virtual void stop() = 0;
    virtual void setSink(inputEventSink* sink) = 0;
};

std::unique_ptr<inputBackend> createInputBackend();

} // namespace inputTester

#endif // inputTesterPlatformInputBackendH
