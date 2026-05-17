#include "wingman/process.hpp"

#if defined(__linux__) || defined(__APPLE__)

#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <dirent.h>
#include <libgen.h>

#if defined(__APPLE__)
#include <libproc.h>
#endif

namespace wingman {

ProcessId Process::find(const std::string& name) {
    auto results = findAll(name);
    return results.empty() ? 0 : results[0];
}

std::vector<ProcessId> Process::findAll(const std::string& name) {
    std::vector<ProcessId> results;

#if defined(__linux__)
    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) {
        return results;
    }

    struct dirent* entry;
    while ((entry = readdir(proc_dir)) != nullptr) {
        // 检查是否为数字目录（PID）
        if (!isdigit(entry->d_name[0])) {
            continue;
        }

        ProcessId pid = atoi(entry->d_name);

        // 读取进程名称
        char comm_path[64];
        snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);

        std::ifstream comm_file(comm_path);
        if (!comm_file.is_open()) {
            continue;
        }

        std::string processName;
        std::getline(comm_file, processName);
        comm_file.close();

        // 移除可能的换行符
        if (!processName.empty() && processName.back() == '\n') {
            processName.pop_back();
        }

        if (processName == name || processName.find(name) != std::string::npos) {
            results.push_back(pid);
        }
    }

    closedir(proc_dir);

#elif defined(__APPLE__)
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
#endif

    return results;
}

std::vector<ProcessInfo> Process::enumerate() {
    std::vector<ProcessInfo> results;

#if defined(__linux__)
    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) {
        return results;
    }

    struct dirent* entry;
    while ((entry = readdir(proc_dir)) != nullptr) {
        if (!isdigit(entry->d_name[0])) {
            continue;
        }

        ProcessId pid = atoi(entry->d_name);

        ProcessInfo info;
        info.pid = pid;

        // 读取进程名称
        char comm_path[64];
        snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);

        std::ifstream comm_file(comm_path);
        if (comm_file.is_open()) {
            std::getline(comm_file, info.name);
            comm_file.close();
        }

        // 读取进程路径
        char exe_path[64];
        snprintf(exe_path, sizeof(exe_path), "/proc/%d/exe", pid);

        char link_path[PATH_MAX];
        ssize_t len = readlink(exe_path, link_path, sizeof(link_path) - 1);
        if (len > 0) {
            link_path[len] = '\0';
            info.path = link_path;
        }

        if (!info.name.empty()) {
            results.push_back(info);
        }
    }

    closedir(proc_dir);

#elif defined(__APPLE__)
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
#endif

    return results;
}

ProcessId Process::start(const std::string& path,
                         const std::string& args,
                         const std::string& workingDir) {
    pid_t pid = fork();

    if (pid == -1) {
        // Fork 失败
        return 0;
    }

    if (pid == 0) {
        // 子进程
        // 切换工作目录
        if (!workingDir.empty()) {
            chdir(workingDir.c_str());
        }

        // 构建命令参数
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(path.c_str()));

        // 简单的参数分割（按空格）
        if (!args.empty()) {
            std::string args_copy = args;
            char* token = strtok(&args_copy[0], " ");
            while (token != nullptr) {
                argv.push_back(token);
                token = strtok(nullptr, " ");
            }
        }

        argv.push_back(nullptr);

        // 执行程序
        execvp(path.c_str(), argv.data());

        // 如果执行失败，退出子进程
        _exit(1);
    }

    // 父进程返回子进程 PID
    return pid;
}

bool Process::wait(ProcessId pid, int timeoutMs) {
    int status;
    pid_t result;

    if (timeoutMs > 0) {
        // 带超时的等待
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
        // 无限等待
        result = waitpid(pid, &status, 0);
        return result == pid && (WIFEXITED(status) || WIFSIGNALED(status));
    }
}

bool Process::terminate(ProcessId pid, bool force) {
    int signal = force ? SIGKILL : SIGTERM;
    return kill(pid, signal) == 0;
}

bool Process::exists(ProcessId pid) {
    // 发送信号 0 检查进程是否存在
    return kill(pid, 0) == 0 || errno == EPERM;
}

std::string Process::getName(ProcessId pid) {
#if defined(__linux__)
    char comm_path[64];
    snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);

    std::ifstream comm_file(comm_path);
    if (!comm_file.is_open()) {
        return "";
    }

    std::string name;
    std::getline(comm_file, name);
    comm_file.close();

    // 移除可能的换行符
    if (!name.empty() && name.back() == '\n') {
        name.pop_back();
    }

    return name;

#elif defined(__APPLE__)
    char path[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(pid, path, sizeof(path)) <= 0) {
        return "";
    }

    return basename(path);
#endif
    return "";
}

std::string Process::getPath(ProcessId pid) {
#if defined(__linux__)
    char exe_path[64];
    snprintf(exe_path, sizeof(exe_path), "/proc/%d/exe", pid);

    char link_path[PATH_MAX];
    ssize_t len = readlink(exe_path, link_path, sizeof(link_path) - 1);
    if (len > 0) {
        link_path[len] = '\0';
        return std::string(link_path);
    }

    return "";

#elif defined(__APPLE__)
    char path[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(pid, path, sizeof(path)) > 0) {
        return std::string(path);
    }

    return "";
#endif
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

#endif // __linux__ || __APPLE__
