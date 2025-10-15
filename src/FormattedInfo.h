#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <psapi.h>
#include <string>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <chrono>

namespace FormattedInfo
{
    // -------- Cached values --------
    inline std::string lastCpuUsage = "0%";
    inline std::string lastRamUsage = "0/0GB";
    inline std::string lastProcessCount = "0";

    inline std::chrono::steady_clock::time_point lastUpdate = std::chrono::steady_clock::now();
    constexpr float POLL_INTERVAL = 1.5f; // seconds

    // -------- Update all metrics together --------
    inline void UpdateMetrics()
    {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<float> elapsed = now - lastUpdate;
        if (elapsed.count() < POLL_INTERVAL) return; // skip if not enough time

        // --- CPU usage ---
        FILETIME idleTime, kernelTime, userTime;
        if (GetSystemTimes(&idleTime, &kernelTime, &userTime))
        {
            static ULONGLONG prevIdle = 0, prevKernel = 0, prevUser = 0;

            ULONGLONG idle = ((ULONGLONG)idleTime.dwHighDateTime << 32) | idleTime.dwLowDateTime;
            ULONGLONG kernel = ((ULONGLONG)kernelTime.dwHighDateTime << 32) | kernelTime.dwLowDateTime;
            ULONGLONG user = ((ULONGLONG)userTime.dwHighDateTime << 32) | userTime.dwLowDateTime;

            ULONGLONG deltaIdle = idle - prevIdle;
            ULONGLONG deltaKernel = kernel - prevKernel;
            ULONGLONG deltaUser = user - prevUser;

            prevIdle = idle;
            prevKernel = kernel;
            prevUser = user;

            ULONGLONG total = deltaKernel + deltaUser;
            lastCpuUsage = total == 0 ? "0%" : std::to_string(static_cast<int>((deltaKernel + deltaUser - deltaIdle) * 100 / total)) + "%";
        }

        // --- RAM usage ---
        MEMORYSTATUSEX memStatus{};
        memStatus.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memStatus))
        {
            double totalMemory = static_cast<double>(memStatus.ullTotalPhys) / (1024 * 1024 * 1024);
            double usedMemory = static_cast<double>(memStatus.ullTotalPhys - memStatus.ullAvailPhys) / (1024 * 1024 * 1024);

            std::ostringstream ramUsage;
            ramUsage << std::fixed << std::setprecision(1) << usedMemory << "/"
                << std::fixed << std::setprecision(1) << totalMemory << "GB";
            lastRamUsage = ramUsage.str();
        }

        // --- Process count ---
        DWORD processCount = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32 entry{};
            entry.dwSize = sizeof(PROCESSENTRY32);
            if (Process32First(snap, &entry))
            {
                do { ++processCount; } while (Process32Next(snap, &entry));
            }
            CloseHandle(snap);
        }
        lastProcessCount = std::to_string(processCount);

        lastUpdate = now;
    }

    // -------- Accessors --------
    inline std::string GetFormattedCPUUsage() { UpdateMetrics(); return lastCpuUsage; }
    inline std::string GetFormattedRAMUsage() { UpdateMetrics(); return lastRamUsage; }
    inline std::string GetFormattedProcessCount() { UpdateMetrics(); return lastProcessCount; }

    // -------- Time & Date (cheap, per-frame ok) --------
    inline std::string GetFormattedTime()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t nowTime = std::chrono::system_clock::to_time_t(now);

        std::tm localTime{};
        localtime_s(&localTime, &nowTime);

        std::ostringstream formattedTime;
        formattedTime << std::setw(2) << std::setfill('0') << localTime.tm_hour
            << ":" << std::setw(2) << std::setfill('0') << localTime.tm_min;
        return formattedTime.str();
    }

    inline std::string GetFormattedDate()
    {
        char buffer[11]{};
        std::time_t now = std::time(nullptr);
        std::tm localTime{};
        localtime_s(&localTime, &now);
        std::strftime(buffer, sizeof(buffer), "%d/%m/%Y", &localTime);
        return std::string(buffer);
    }
}
