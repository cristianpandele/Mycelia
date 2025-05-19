#pragma once

#include <deque>
#include <future>
#include <memory>

#include <juce_dsp/juce_dsp.h>

/** A store to create delay objects more quickly. */
class DelayStore
{
    public:
        DelayStore()
        {
            // start with a bunch in the queue
            for (int i = 0; i < storeSize; ++i)
                loadNewDelay();
        }

        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> *getNextDelay()
        {
            juce::SpinLock::ScopedLockType nextDelayLock(delayStoreLock);

            auto *delay = delayFutureStore.front().get().release();
            delayFutureStore.pop_front();
            loadNewDelay();

            return delay;
        }

    private:
        void loadNewDelay()
        {
            delayFutureStore.push_back(
                std::async(std::launch::async,
                    [] {
                            auto newDelay = std::make_unique<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>> (1 << 19);
                            newDelay->prepare ({ 48000.0, 512, 2 });
                            newDelay->reset();
                            return newDelay;
                        }
                    )
                );
        }

        static constexpr int storeSize = 32;

        std::deque<std::future<std::unique_ptr<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>>>> delayFutureStore;
        juce::SpinLock delayStoreLock;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayStore)
};
