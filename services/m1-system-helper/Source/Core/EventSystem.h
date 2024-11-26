#pragma once

#include "../Common/Common.h"

namespace Mach1 {

class EventSystem {
public:
    using EventCallback = std::function<void(const juce::var&)>;
    
    void subscribe(const juce::String& eventName, EventCallback callback) {
        const juce::ScopedLock lock(mutex);
        subscribers[eventName].push_back(callback);
    }
    
    void publish(const juce::String& eventName, const juce::var& data) {
        const juce::ScopedLock lock(mutex);
        for (auto& callback : subscribers[eventName]) {
            callback(data);
        }
    }

private:
    std::unordered_map< juce::String, std::vector<EventCallback> > subscribers;
    juce::CriticalSection mutex;
};

} // namespace Mach1
