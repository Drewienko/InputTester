#ifndef inputTesterCoreInputEventQueueH
#define inputTesterCoreInputEventQueueH

#include "inputtester/core/inputEventSink.h"
#include "inputtester/core/spscRingBuffer.h"

namespace inputTester
{

    class inputEventQueue final : public inputEventSink
    {
    public:
        void onInputEvent(const inputEvent &event) override
        {
            queue_.tryPush(event);
        }

        bool tryPop(inputEvent &out)
        {
            return queue_.tryPop(out);
        }

    private:
        spscRingBuffer<inputEvent, 1024> queue_{};
    };

} // namespace inputTester

#endif // inputTesterCoreInputEventQueueH
