#include "wingman/process.hpp"

#if defined(__APPLE__)

#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <chrono>
#include <thread>
#include <libproc.h>
#include <libgen.h>

namespace wingman {

ProcessId Process::find(const std::string& name) {
    auto results = findAll(name);
    return results.empty() ? 0 : results[0];
}

std::vector<ProcessId> Process::findAll(const std::string& name) {
    std::vector<ProcessId> results;

    int pidCount = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
    if (pidCount <= 0) {
        return results;
    }

    std::vector<pid_t> pids(pidCount);
    proc_listpids(PROC_ALL_PIDS, 0, pids.data(), sizeof(pid_t) * pidCount);

    for (pid_t pid : pids) {
        if (pid == 0) {
            continue;
        }

        char path[PROC_PIDPATHINFO_MAXSIZE];
        if (proc_pidpath(pid, path, sizeof(path)) <= 0) {
            continue;
        }

        std::string processName = basename(path);

        if (processName == name || processName.find(name) != std::string::npos) {
            results.push_back(pid);
        }
    }

    return results;
}

std::vector<ProcessInfo> Process::enumerate() {
    std::vector<ProcessInfo> results;

    int pidCount = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
    if (pidCount <= 0) {
        return results;
    }

    std::vector<pid_t> pids(pidCount);
    proc_listpids(PROC_ALL_PIDS, 0, pids.data(), sizeof(pid_t) * pidCount);

    for (pid_t pid : pids) {
        if (pid == 0) {
            continue;
        }

        ProcessInfo info;
        info.pid = pid;

        char path[PROC_PIDPATHINFO_MAXSIZE];
        if (proc_pidpath(pid, path, sizeof(path)) > 0) {
            info.path = path;
            info.name = basename(path);
            results.push_back(info);
        }
    }

    return results;
}

ProcessId Process::start(const std::string& path,
                         const std::string& args,
                         const std::string& workingDir) {
    pid_t pid = fork();

    if (pid == -1) {
        return 0;
    }

    if (pid == 0) {
        if (!workingDir.empty()) {
            chdir(workingDir.c_str());
        }

        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(path.c_str()));

        if (!args.empty()) {
            std::string args_copy = args;
            char* token = strtok(&args_copy[0], " ");
            while (token != nullptr) {
                argv.push_back(token);
                token = strtok(nullptr, " ");
            }
        }

        argv.push_back(nullptr);

        execvp(path.c_str(), argv.data());
        _exit(1);
    }

    return pid;
}

bool Process::wait(ProcessId pid, int timeoutMs) {
    int status;
    pid_t result;

    if (timeoutMs > 0) {
        auto start = std::chrono::steady_clock::now();
        while (true) {
            result = waitpid(pid, &status, WNOHANG);

            if (result == pid) {
                return WIFEXITED(status) || WIFSIGNALED(status);
            }

            if (result == -1) {
                return false;
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeoutMs) {
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } else {
        result = waitpid(pid, &status, 0);
        return result == pid && (WIFEXITED(status) || WIFSIGNALED(status));
    }
}

bool Process::terminate(ProcessId pid, bool force) {
    int signal = force ? SIGKILL : SIGTERM;
    return kill(pid, signal) == 0;
}

bool Process::exists(ProcessId pid) {
    return kill(pid, 0) == 0 || errno == EPERM;
}

std::string Process::getName(ProcessId pid) {
    char path[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(pid, path, sizeof(path)) <= 0) {
        return "";
    }

    return basename(path);
}

std::string Process::getPath(ProcessId pid) {
    char path[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(pid, path, sizeof(path)) > 0) {
        return std::string(path);
    }

    return "";
}

ProcessId Process::getCurrentId() {
    return getpid();
}

bool Process::waitFor(const std::string& name, int timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (find(name) != 0) {
            return true;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeoutMs) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool Process::waitExit(const std::string& name, int timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (find(name) == 0) {
            return true;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeoutMs) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace wingman

#endif // __APPLE__
