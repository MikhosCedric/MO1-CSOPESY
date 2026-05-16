#pragma once
#ifndef EVENT_BROADCASTER_H
#define EVENT_BROADCASTER_H

#include <functional>
#include <unordered_map>
#include <vector>
#include <string>

// Define the types of events that can happen in the OS
enum class EventType {
    ON_CONFIG_LOADED,    // Fired when FileSystem successfully reads config.txt
    ON_PROCESS_STARTED,  // Fired when a new dummy process is created
    ON_PROCESS_FINISHED, // Fired when a process completes all its instructions
    ON_CPU_TICK          // Fired every time the master while-loop iterates
};

// The signature for any function that wants to listen to events
// It passes the processID (if applicable) and an optional message
using EventCallback = std::function<void(int processID, const std::string& message)>;

class EventBroadcaster {
public:
    // Singleton Accessor
    static EventBroadcaster& getInstance();

    // Pair A (ConsoleUI) will use this to listen for CPU updates
    void addListener(EventType type, EventCallback callback);

    // Pair B (Scheduler) will use this to announce when things happen
    void broadcast(EventType type, int processID = -1, const std::string& message = "");

private:
    EventBroadcaster() {} // Private constructor

    // Delete copy/assignment
    EventBroadcaster(const EventBroadcaster&) = delete;
    EventBroadcaster& operator=(const EventBroadcaster&) = delete;

    // Stores all the listening functions mapped to their event types
    std::unordered_map<EventType, std::vector<EventCallback>> listeners;
};

#endif // EVENT_BROADCASTER_H