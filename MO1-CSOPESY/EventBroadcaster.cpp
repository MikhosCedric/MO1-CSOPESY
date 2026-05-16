#include "EventBroadcaster.h"
#include <iostream>

EventBroadcaster& EventBroadcaster::getInstance() {
    static EventBroadcaster instance;
    return instance;
}

// Add a function to the list of listeners for a specific event
void EventBroadcaster::addListener(EventType type, EventCallback callback) {
    listeners[type].push_back(callback);
}

// Trigger all listening functions for a specific event
void EventBroadcaster::broadcast(EventType type, int processID, const std::string& message) {
    // Check if anyone is actually listening to this specific event type
    if (listeners.find(type) != listeners.end()) {

        // Loop through everyone who subscribed and call their function
        for (auto& callback : listeners[type]) {
            callback(processID, message);
        }
    }
}