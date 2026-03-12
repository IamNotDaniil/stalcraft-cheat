#pragma once
#include <atomic>

namespace overlay {
    void run(std::atomic<bool>& running);
}
