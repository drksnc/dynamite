#pragma once
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

enum class Event {
    None = 0,
    LoadMission,
    TestFunction,
    Connect,
    ResetConnect
};

class EventStack {
  private:
    std::deque<Event> stack;
    mutable std::mutex mtx;
    std::condition_variable cv;

  public:
    void Push(Event e) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            stack.push_back(e);
        }
        cv.notify_one();
    }

    std::optional<Event> Pop() {
        std::unique_lock<std::mutex> lock(mtx);

        if (stack.empty())
            return std::nullopt;

        Event e = stack.back();
        stack.pop_back();
        return e;
    }

    Event WaitAndPop() {
        std::unique_lock<std::mutex> lock(mtx);

        cv.wait(lock, [this] { return !stack.empty(); });

        Event e = stack.back();
        stack.pop_back();
        return e;
    }

    bool Empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return stack.empty();
    }

    size_t Size() const {
        std::lock_guard<std::mutex> lock(mtx);
        return stack.size();
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(mtx);
        stack.clear();
    }
};