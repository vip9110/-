#pragma once
// 安全回收：只把界面中逐项展示、用户刚确认过的路径移入 Windows 回收站，绝不降级为永久删除。
#include "win_common.h"
#include <shobjidl.h>
#include <shlguid.h>
#include <knownfolders.h>
#include <set>
#include <map>
#include "pet_logic.h"

inline std::wstring shellPath(IShellItem* item) {
    PWSTR text = nullptr;
    if (!item || FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &text))) return {};
    std::wstring value(text); CoTaskMemFree(text); return value;
}

inline std::wstring knownFolder(REFKNOWNFOLDERID id) {
    PWSTR text = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &text))) return {};
    std::wstring value(text); CoTaskMemFree(text); return value;
}

inline bool safeCandidate(const std::wstring& input, std::wstring& output, std::wstring& reason) {
    if (input.size() < 4 || input.size() >= MAX_PATH || input[1] != L':' || input[2] != L'\\' ||
        input.find_first_of(L"*?\"", 2) != std::wstring::npos || input.find(L':', 2) != std::wstring::npos) {
        reason = L"仅支持普通本地绝对路径；网络、设备和过长路径不能投喂。"; return false;
    }
    wchar_t full[MAX_PATH]{};
    DWORD length = GetFullPathNameW(input.c_str(), MAX_PATH, full, nullptr);
    if (!length || length >= MAX_PATH) { reason = L"路径无法识别。"; return false; }
    output = full;
    while (output.size() > 3 && output.back() == L'\\') output.pop_back();
    if (output.size() <= 3) { reason = L"不能投喂整个磁盘。"; return false; }
    DWORD attributes = GetFileAttributesW(output.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) { reason = L"文件不存在或无权访问。"; return false; }
    if (attributes & FILE_ATTRIBUTE_SYSTEM) { reason = L"系统项目不能投喂。"; return false; }
    std::wstring ancestor = output;
    while (ancestor.size() > 3) {
        DWORD attr = GetFileAttributesW(ancestor.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_REPARSE_POINT)) {
            reason = L"无法访问的路径、链接目录和云占位文件不能投喂。"; return false;
        }
        auto slash = ancestor.find_last_of(L'\\');
        if (slash <= 2) break;
        ancestor.resize(slash);
    }
    wchar_t volume[MAX_PATH]{};
    if (!GetVolumePathNameW(output.c_str(), volume, MAX_PATH) || GetDriveTypeW(volume) != DRIVE_FIXED) {
        reason = L"仅支持本机固定磁盘；U 盘和网络盘不能投喂。"; return false;
    }
    wchar_t win[MAX_PATH]{}; GetWindowsDirectoryW(win, MAX_PATH);
    std::vector<std::wstring> blocked = {win, knownFolder(FOLDERID_ProgramFiles), knownFolder(FOLDERID_ProgramFilesX86),
        knownFolder(FOLDERID_ProgramData), knownFolder(FOLDERID_LocalAppData), knownFolder(FOLDERID_RoamingAppData),
        std::wstring(volume) + L"$Recycle.Bin", std::wstring(volume) + L"System Volume Information"};
    for (const auto& root : blocked) if (!root.empty() && (sameOrInside(output, root) || sameOrInside(root, output))) {
        reason = L"系统、应用程序和设置目录受到保护。"; return false;
    }
    for (const auto& id : {FOLDERID_Profile, FOLDERID_Desktop, FOLDERID_Documents, FOLDERID_Downloads, FOLDERID_Pictures, FOLDERID_Music, FOLDERID_Videos}) {
        auto root = knownFolder(id);
        if (!root.empty() && sameOrInside(root, output)) { reason = L"不能投喂整个用户文件夹，请选择里面不再需要的项目。"; return false; }
    }
    wchar_t executable[MAX_PATH]{}; GetModuleFileNameW(nullptr, executable, MAX_PATH);
    if (executable[0] && sameOrInside(executable, output)) { reason = L"不能投喂正在运行的桌宠或包含它的文件夹。"; return false; }
    SHQUERYRBINFO info{}; info.cbSize = sizeof(info);
    if (FAILED(SHQueryRecycleBinW(volume, &info))) { reason = L"此磁盘的 Windows 回收站暂不可用。"; return false; }
    return true;
}

class RecycleSink final : public IFileOperationProgressSink {
    volatile LONG refs = 1;
public:
    std::set<std::wstring> allowed, succeeded;
    std::map<IShellItem*, std::wstring> beforePaths;
    bool refusedPermanent = false;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (IsEqualIID(id, IID_IUnknown) || IsEqualIID(id, IID_IFileOperationProgressSink)) {
            *object = static_cast<IFileOperationProgressSink*>(this); AddRef(); return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refs)); }
    ULONG STDMETHODCALLTYPE Release() override { ULONG left = static_cast<ULONG>(InterlockedDecrement(&refs)); if (!left) delete this; return left; }
    HRESULT STDMETHODCALLTYPE StartOperations() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE FinishOperations(HRESULT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE PreDeleteItem(DWORD flags, IShellItem* item) override {
        // Shell 若准备改为永久删除，直接中止，不允许降级为 DeleteFile/RemoveDirectory。
        if (!mayRecycleFromFlags(flags)) { refusedPermanent = true; return E_ABORT; }
        const auto path = shellPath(item);
        std::wstring checked, reason;
        if (!allowed.count(pathKey(path)) || !safeCandidate(path, checked, reason)) return E_ABORT;
        beforePaths[item] = pathKey(checked);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PostDeleteItem(DWORD flags, IShellItem* original, HRESULT result, IShellItem* inRecycleBin) override {
        if (result == S_OK && inRecycleBin && mayRecycleFromFlags(flags)) {
            auto saved = beforePaths.find(original);
            auto key = saved != beforePaths.end() ? saved->second : pathKey(shellPath(original));
            if (allowed.count(key)) succeeded.insert(key);
        }
        if (SUCCEEDED(result) && !inRecycleBin) { refusedPermanent = true; return E_ABORT; }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PreRenameItem(DWORD, IShellItem*, LPCWSTR) override { return E_ABORT; }
    HRESULT STDMETHODCALLTYPE PostRenameItem(DWORD, IShellItem*, LPCWSTR, HRESULT, IShellItem*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE PreMoveItem(DWORD, IShellItem*, IShellItem*, LPCWSTR) override { return E_ABORT; }
    HRESULT STDMETHODCALLTYPE PostMoveItem(DWORD, IShellItem*, IShellItem*, LPCWSTR, HRESULT, IShellItem*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE PreCopyItem(DWORD, IShellItem*, IShellItem*, LPCWSTR) override { return E_ABORT; }
    HRESULT STDMETHODCALLTYPE PostCopyItem(DWORD, IShellItem*, IShellItem*, LPCWSTR, HRESULT, IShellItem*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE PreNewItem(DWORD, IShellItem*, LPCWSTR) override { return E_ABORT; }
    HRESULT STDMETHODCALLTYPE PostNewItem(DWORD, IShellItem*, LPCWSTR, LPCWSTR, DWORD, HRESULT, IShellItem*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE UpdateProgress(UINT, UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE ResetTimer() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE PauseTimer() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE ResumeTimer() override { return S_OK; }
};

inline std::set<std::wstring> recycleConfirmed(HWND owner, const std::vector<std::wstring>& paths) {
    // 这里只接收界面里已经逐项展示、用户刚确认的列表，不接受通配符或递归扫描结果。
    std::set<std::wstring> success;
    if (paths.empty()) return success;
    IFileOperation* operation = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&operation)))) return success;
    auto sink = new RecycleSink;
    HRESULT result = operation->SetOwnerWindow(owner);
    if (SUCCEEDED(result)) result = operation->SetOperationFlags(FOFX_RECYCLEONDELETE | FOFX_ADDUNDORECORD |
        FOFX_EARLYFAILURE | FOF_NOERRORUI | FOF_WANTNUKEWARNING | FOF_NO_CONNECTED_ELEMENTS);
    for (const auto& path : paths) {
        if (FAILED(result)) break;
        std::wstring checked, reason;
        if (!safeCandidate(path, checked, reason)) { result = E_ABORT; break; }
        IShellItem* item = nullptr;
        result = SHCreateItemFromParsingName(checked.c_str(), nullptr, IID_PPV_ARGS(&item));
        if (SUCCEEDED(result)) {
            sink->allowed.insert(pathKey(checked));
            result = operation->DeleteItem(item, sink); item->Release();
        }
    }
    // 任一预检或入队失败时，不执行已经排队的部分。
    if (SUCCEEDED(result)) operation->PerformOperations();
    success = sink->succeeded;
    operation->Release(); sink->Release();
    return success;
}
