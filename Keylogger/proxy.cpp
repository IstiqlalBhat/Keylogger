// proxy.cpp
#include <windows.h>
#include <string>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    WCHAR exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    // assume DLL is next to this EXE
    std::wstring dllPath(exePath);
    dllPath.replace(dllPath.find(L".exe"), 4, L".dll");

    // Load your logger DLL
    HMODULE h = LoadLibraryW(dllPath.c_str());
    // (you can check h != nullptr for error handling)

    // Build path to the “real” core app
    std::wstring corePath(exePath);
    corePath.replace(corePath.find(L"TestApp.exe"), std::wstring(L"TestApp.exe").length(),
                     L"TestAppCore.exe");

    // Launch the real application, forwarding any command-line args
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    std::wstring cmd = L"\"" + corePath + L"\"";

    if (!CreateProcessW(nullptr,
                        cmd.data(),
                        nullptr, nullptr,
                        FALSE,
                        0,
                        nullptr, nullptr,
                        &si, &pi))
    {
        // error handling...
        return GetLastError();
    }

    // Wait for the real app to exit, then unload your DLL
    WaitForSingleObject(pi.hProcess, INFINITE);
    if (h) FreeLibrary(h);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}
