#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "FileDialog.h"
#include <windows.h>
#include <commdlg.h>
namespace Kizuri {
namespace {
unsigned long g_lastErr = 0;
void ToNarrow(const wchar_t* wide, std::string& out) {
  out.clear();
  if (wide == nullptr) {
    return;
  }
  int n = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
  if (n <= 1) {
    return;
  }
  out.resize(static_cast<size_t>(n - 1));
  WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), n, nullptr, nullptr);
}
void ToWide(const std::string& in, wchar_t* out, int capacity) {
  if (capacity <= 0) {
    return;
  }
  out[0] = L'\0';
  if (in.empty()) {
    return;
  }
  MultiByteToWideChar(CP_UTF8, 0, in.c_str(), -1, out, capacity);
}
}
bool ShowOpenSceneDialog(void* hwnd, std::string& outPath) {
  wchar_t file[1024];
  file[0] = L'\0';
  OPENFILENAMEW ofn;
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = static_cast<HWND>(hwnd);
  ofn.lpstrFile = file;
  ofn.nMaxFile = 1024;
  ofn.lpstrFilter = L"Kizuri Scene (*.kzscene)\0*.kzscene\0All Files (*.*)\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrDefExt = L"kzscene";
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
  if (GetOpenFileNameW(&ofn) == FALSE) {
    g_lastErr = CommDlgExtendedError();
    return false;
  }
  ToNarrow(file, outPath);
  return !outPath.empty();
}
bool ShowSaveSceneDialog(void* hwnd, std::string& outPath) {
  wchar_t file[1024];
  ToWide(outPath, file, 1024);
  OPENFILENAMEW ofn;
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = static_cast<HWND>(hwnd);
  ofn.lpstrFile = file;
  ofn.nMaxFile = 1024;
  ofn.lpstrFilter = L"Kizuri Scene (*.kzscene)\0*.kzscene\0All Files (*.*)\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrDefExt = L"kzscene";
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
  if (GetSaveFileNameW(&ofn) == FALSE) {
    g_lastErr = CommDlgExtendedError();
    return false;
  }
  ToNarrow(file, outPath);
  return !outPath.empty();
}
unsigned long GetLastDialogError() {
  return g_lastErr;
}
bool ShowOpenGltfDialog(void* hwnd, std::string& outPath) {
  wchar_t file[1024];
  file[0] = L'\0';
  OPENFILENAMEW ofn;
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = static_cast<HWND>(hwnd);
  ofn.lpstrFile = file;
  ofn.nMaxFile = 1024;
  ofn.lpstrFilter = L"glTF Models (*.glb;*.gltf)\0*.glb;*.gltf\0All Files (*.*)\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrDefExt = L"glb";
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
  if (GetOpenFileNameW(&ofn) == FALSE) {
    g_lastErr = CommDlgExtendedError();
    return false;
  }
  ToNarrow(file, outPath);
  return !outPath.empty();
}
bool ShowOpenImageDialog(void* hwnd, std::string& outPath) {
  wchar_t file[1024];
  file[0] = L'\0';
  OPENFILENAMEW ofn;
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = static_cast<HWND>(hwnd);
  ofn.lpstrFile = file;
  ofn.nMaxFile = 1024;
  ofn.lpstrFilter = L"Images (*.png;*.jpg;*.jpeg;*.tga;*.bmp)\0*.png;*.jpg;*.jpeg;*.tga;*.bmp\0All Files (*.*)\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrDefExt = L"png";
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
  if (GetOpenFileNameW(&ofn) == FALSE) {
    g_lastErr = CommDlgExtendedError();
    return false;
  }
  ToNarrow(file, outPath);
  return !outPath.empty();
}
bool ShowOpenCubeDialog(void* hwnd, std::string& outPath) {
  wchar_t file[1024];
  file[0] = L'\0';
  OPENFILENAMEW ofn;
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = static_cast<HWND>(hwnd);
  ofn.lpstrFile = file;
  ofn.nMaxFile = 1024;
  ofn.lpstrFilter = L"Color LUT (*.cube)\0*.cube\0All Files (*.*)\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrDefExt = L"cube";
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
  if (GetOpenFileNameW(&ofn) == FALSE) {
    g_lastErr = CommDlgExtendedError();
    return false;
  }
  ToNarrow(file, outPath);
  return !outPath.empty();
}
bool ShowOpenHdrDialog(void* hwnd, std::string& outPath) {
  wchar_t file[1024];
  file[0] = L'\0';
  OPENFILENAMEW ofn;
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = static_cast<HWND>(hwnd);
  ofn.lpstrFile = file;
  ofn.nMaxFile = 1024;
  ofn.lpstrFilter = L"HDR Images (*.hdr)\0*.hdr\0All Files (*.*)\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrDefExt = L"hdr";
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
  if (GetOpenFileNameW(&ofn) == FALSE) {
    g_lastErr = CommDlgExtendedError();
    return false;
  }
  ToNarrow(file, outPath);
  return !outPath.empty();
}
bool ShowOpenAudioDialog(void* hwnd, std::string& outPath) {
  wchar_t file[1024];
  file[0] = L'\0';
  OPENFILENAMEW ofn;
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = static_cast<HWND>(hwnd);
  ofn.lpstrFile = file;
  ofn.nMaxFile = 1024;
  ofn.lpstrFilter = L"Audio (*.wav;*.ogg;*.flac;*.mp3;*.aiff)\0*.wav;*.ogg;*.flac;*.mp3;*.aiff\0All Files (*.*)\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrDefExt = L"wav";
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
  if (GetOpenFileNameW(&ofn) == FALSE) {
    g_lastErr = CommDlgExtendedError();
    return false;
  }
  ToNarrow(file, outPath);
  return !outPath.empty();
}
}
