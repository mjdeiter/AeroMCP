#pragma once
#include <string>
#include <functional>
#include <thread>
#include <atomic>

struct ExecResult {
    std::string stdout_out;
    std::string stderr_out;
    int returncode;
};

class Executor {
public:
    // Blocking run — call from worker thread
    static ExecResult run(const std::string& command, int timeout_sec = 60);

    // Async run — cb called on completion (from worker thread, marshal to main if needed)
    static void runAsync(const std::string& command,
                         std::function<void(ExecResult)> cb,
                         int timeout_sec = 60);
};
