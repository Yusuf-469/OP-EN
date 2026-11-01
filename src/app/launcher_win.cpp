// Launch.exe: starts the local C++ server, opens the UI in the default browser
// and shows a small control window. Closing the window stops the server.
// Options (for automated tests): --no-browser, --port-file <path>.

#include <windows.h>
#include <shellapi.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

#include "api_server.h"
#include "system_info.h"

namespace fs = std::filesystem;

namespace {

constexpr int kOpenButton = 101;
constexpr int kStopButton = 102;

std::wstring g_url;
std::ofstream g_log;
std::mutex g_logMutex;

fs::path exeDirectory() {
    wchar_t buffer[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return fs::path(std::wstring(buffer, n)).parent_path();
}

// Packaged layout first (app/web next to Launch.exe), then the source tree
// so a developer build in build/ also works.
fs::path findWebRoot(const fs::path& exeDir) {
    for (const fs::path& candidate : {exeDir / "app" / "web", exeDir / ".." / "src" / "web"})
        if (fs::exists(candidate / "index.html")) return fs::weakly_canonical(candidate);
    return {};
}

double msSinceProcessCreated() {
    FILETIME created, exited, kernel, user, now;
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    GetSystemTimePreciseAsFileTime(&now);
    auto ticks = [](const FILETIME& f) { return (static_cast<unsigned long long>(f.dwHighDateTime) << 32) | f.dwLowDateTime; };
    return static_cast<double>(ticks(now) - ticks(created)) / 10'000.0;  // 100 ns ticks -> ms
}

void openLog(const fs::path& exeDir) {
    std::error_code ec;
    fs::create_directories(exeDir / "logs", ec);
    g_log.open(exeDir / "logs" / "workbench.log", std::ios::app);
    if (!g_log) g_log.open(fs::temp_directory_path() / "uew_workbench.log", std::ios::app);  // read-only folder
}

void log(const std::string& line) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char stamp[32];
    std::strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    g_log << stamp << "  " << line << std::endl;
}

void openBrowser() { ShellExecuteW(nullptr, L"open", g_url.c_str(), nullptr, nullptr, SW_SHOWNORMAL); }

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND:
            if (LOWORD(wParam) == kOpenButton) openBrowser();
            if (LOWORD(wParam) == kStopButton) DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

HWND createControlWindow(HINSTANCE instance) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.lpszClassName = L"UEWLauncher";
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Unified Engineering Workbench",
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT,
                                CW_USEDEFAULT, 520, 200, nullptr, nullptr, instance, nullptr);
    HFONT font = CreateFontW(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    const std::wstring text = L"The workbench is running at " + g_url +
                              L"\nKeep this window open while you use it. Closing it stops the local server.";
    HWND label = CreateWindowW(L"STATIC", text.c_str(), WS_CHILD | WS_VISIBLE, 16, 16, 480, 60, hwnd, nullptr, instance, nullptr);
    HWND open = CreateWindowW(L"BUTTON", L"Open in browser", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 16, 92, 170, 44,
                              hwnd, reinterpret_cast<HMENU>(kOpenButton), instance, nullptr);
    HWND stop = CreateWindowW(L"BUTTON", L"Stop and exit", WS_CHILD | WS_VISIBLE, 200, 92, 150, 44, hwnd,
                              reinterpret_cast<HMENU>(kStopButton), instance, nullptr);
    for (HWND h : {label, open, stop}) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return hwnd;
}

void fail(const std::wstring& message) {
    MessageBoxW(nullptr, message.c_str(), L"Unified Engineering Workbench - cannot start", MB_ICONERROR | MB_OK);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    SetProcessDPIAware();
    const bool throttlingOff = uew::disablePowerThrottling();  // see system_info.h
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool noBrowser = false;
    fs::path portFile;
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        if (arg == L"--no-browser") noBrowser = true;
        else if (arg == L"--port-file" && i + 1 < argc) portFile = argv[++i];
    }
    LocalFree(argv);

    const fs::path exeDir = exeDirectory();
    openLog(exeDir);
    if (!throttlingOff) log("Warning: could not opt out of Windows power throttling; benchmark timings may vary.");
    const fs::path webRoot = findWebRoot(exeDir);
    if (webRoot.empty()) {
        fail(L"The web UI folder 'app\\web' was not found next to Launch.exe.\n"
             L"Extract the whole ZIP (not just Launch.exe) and run it from the extracted folder.");
        return 1;
    }

    // Sample data: data/ next to Launch.exe (package) or in the source tree (dev build).
    fs::path dataRoot = exeDir / "data";
    if (!fs::exists(dataRoot)) dataRoot = exeDir / ".." / "data";
    uew::ApiServer server(webRoot.string(), fs::exists(dataRoot) ? fs::weakly_canonical(dataRoot).string() : "", log,
                          msSinceProcessCreated);
    int port = 0;
    try {
        port = server.start();
    } catch (const std::exception& e) {
        log(std::string("Startup failed: ") + e.what());
        const std::string what = e.what();
        fail(L"The local server could not start:\n" + std::wstring(what.begin(), what.end()));
        return 1;
    }
    g_url = L"http://127.0.0.1:" + std::to_wstring(port) + L"/";
    if (!portFile.empty()) std::ofstream(portFile) << port;
    if (!noBrowser) openBrowser();

    HWND hwnd = createControlWindow(instance);
    ShowWindow(hwnd, noBrowser ? SW_SHOWMINNOACTIVE : showCommand);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    log("Shutting down.");
    server.stop();
    return 0;
}
