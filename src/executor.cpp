#include "executor.h"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <sstream>
#include <unistd.h>

ExecResult Executor::run(const std::string& command, int timeout_sec) {
    ExecResult res{};

    // Run via bash, capture stdout+stderr separately using process substitution
    std::string cmd = "bash -c " + std::string("'") + command + std::string("'") + " 2>/tmp/aeromcp_stderr_$$";
    // Simpler approach: use popen for stdout, redirect stderr to tmp file
    std::string stderr_file = "/tmp/aeromcp_stderr_" + std::to_string(getpid());
    std::string full_cmd = "bash -c '" + command + "' 2>" + stderr_file;

    std::array<char, 4096> buf;
    FILE* pipe = popen(full_cmd.c_str(), "r");
    if (!pipe) { res.returncode = -1; res.stderr_out = "Failed to open pipe"; return res; }

    while (fgets(buf.data(), buf.size(), pipe))
        res.stdout_out += buf.data();

    res.returncode = pclose(pipe) >> 8;

    // Read stderr file
    FILE* ef = fopen(stderr_file.c_str(), "r");
    if (ef) {
        while (fgets(buf.data(), buf.size(), ef))
            res.stderr_out += buf.data();
        fclose(ef);
        remove(stderr_file.c_str());
    }
    return res;
}

void Executor::runAsync(const std::string& command,
                        std::function<void(ExecResult)> cb,
                        int timeout_sec) {
    std::thread([command, cb, timeout_sec]() {
        cb(run(command, timeout_sec));
    }).detach();
}
