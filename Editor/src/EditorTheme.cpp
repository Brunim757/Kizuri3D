#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "EditorTheme.h"
#include <imgui.h>
#include <windows.h>
namespace Kizuri {
namespace {
ImVec4 Rgb(int r, int g, int b, int a = 255) {
  return ImVec4(static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f, static_cast<float>(b) / 255.0f, static_cast<float>(a) / 255.0f);
}
const char* EditorFontCandidates() {
  static const char* paths[] = {
    "C:\\Windows\\Fonts\\segoeui.ttf",
    "C:\\Windows\\Fonts\\segoeui_0.ttf",
    "C:\\Windows\\Fonts\\tahoma.ttf",
    "C:\\Windows\\Fonts\\arial.ttf"
  };
  for (int i = 0; i < 4; ++i) {
    if (GetFileAttributesA(paths[i]) != INVALID_FILE_ATTRIBUTES) {
      return paths[i];
    }
  }
  return nullptr;
}
}
void ApplyEditorTheme() {
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  ImVec4* c = style.Colors;
  c[ImGuiCol_WindowBg] = Rgb(30, 30, 30);
  c[ImGuiCol_ChildBg] = Rgb(30, 30, 30);
  c[ImGuiCol_PopupBg] = Rgb(38, 38, 38);
  c[ImGuiCol_Border] = Rgb(18, 18, 18);
  c[ImGuiCol_BorderShadow] = Rgb(0, 0, 0, 0);
  c[ImGuiCol_FrameBg] = Rgb(48, 48, 48);
  c[ImGuiCol_FrameBgHovered] = Rgb(62, 62, 62);
  c[ImGuiCol_FrameBgActive] = Rgb(72, 72, 72);
  c[ImGuiCol_TitleBg] = Rgb(24, 24, 24);
  c[ImGuiCol_TitleBgActive] = Rgb(24, 24, 24);
  c[ImGuiCol_TitleBgCollapsed] = Rgb(20, 20, 20);
  c[ImGuiCol_MenuBarBg] = Rgb(26, 26, 26);
  c[ImGuiCol_ScrollbarBg] = Rgb(24, 24, 24);
  c[ImGuiCol_ScrollbarGrab] = Rgb(62, 62, 62);
  c[ImGuiCol_ScrollbarGrabHovered] = Rgb(80, 80, 80);
  c[ImGuiCol_ScrollbarGrabActive] = Rgb(98, 98, 98);
  c[ImGuiCol_CheckMark] = Rgb(0, 162, 255);
  c[ImGuiCol_SliderGrab] = Rgb(0, 162, 255);
  c[ImGuiCol_SliderGrabActive] = Rgb(52, 199, 255);
  c[ImGuiCol_Button] = Rgb(52, 52, 52);
  c[ImGuiCol_ButtonHovered] = Rgb(68, 68, 68);
  c[ImGuiCol_ButtonActive] = Rgb(82, 82, 82);
  c[ImGuiCol_Header] = Rgb(46, 46, 46);
  c[ImGuiCol_HeaderHovered] = Rgb(60, 60, 60);
  c[ImGuiCol_HeaderActive] = Rgb(0, 162, 255, 70);
  c[ImGuiCol_Separator] = Rgb(46, 46, 46);
  c[ImGuiCol_SeparatorHovered] = Rgb(70, 70, 70);
  c[ImGuiCol_SeparatorActive] = Rgb(0, 162, 255);
  c[ImGuiCol_Tab] = Rgb(34, 34, 34);
  c[ImGuiCol_TabHovered] = Rgb(56, 56, 56);
  c[ImGuiCol_TabActive] = Rgb(0, 162, 255, 170);
  c[ImGuiCol_TabUnfocused] = Rgb(30, 30, 30);
  c[ImGuiCol_TabUnfocusedActive] = Rgb(0, 120, 200, 120);
  c[ImGuiCol_DockingPreview] = Rgb(0, 162, 255, 80);
  c[ImGuiCol_DockingEmptyBg] = Rgb(24, 24, 24);
  c[ImGuiCol_Text] = Rgb(226, 226, 226);
  c[ImGuiCol_TextDisabled] = Rgb(138, 138, 138);
  c[ImGuiCol_TextSelectedBg] = Rgb(0, 162, 255, 70);
  c[ImGuiCol_NavCursor] = Rgb(0, 162, 255);
  c[ImGuiCol_NavWindowingHighlight] = Rgb(0, 162, 255);
  c[ImGuiCol_NavWindowingDimBg] = Rgb(0, 0, 0, 110);
  c[ImGuiCol_PlotLines] = Rgb(120, 120, 120);
  c[ImGuiCol_PlotLinesHovered] = Rgb(0, 162, 255);
  c[ImGuiCol_PlotHistogram] = Rgb(0, 162, 255);
  c[ImGuiCol_PlotHistogramHovered] = Rgb(52, 199, 255);
  c[ImGuiCol_TableHeaderBg] = Rgb(40, 40, 40);
  c[ImGuiCol_TableBorderStrong] = Rgb(24, 24, 24);
  c[ImGuiCol_TableBorderLight] = Rgb(40, 40, 40);
  c[ImGuiCol_TableRowBg] = Rgb(0, 0, 0, 0);
  c[ImGuiCol_TableRowBgAlt] = Rgb(255, 255, 255, 6);

  style.WindowRounding = 0.0f;
  style.ChildRounding = 0.0f;
  style.FrameRounding = 3.0f;
  style.PopupRounding = 4.0f;
  style.GrabRounding = 3.0f;
  style.TabRounding = 3.0f;
  style.ScrollbarRounding = 3.0f;
  style.WindowBorderSize = 1.0f;
  style.ChildBorderSize = 0.0f;
  style.PopupBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.WindowPadding = ImVec2(8.0f, 8.0f);
  style.FramePadding = ImVec2(6.0f, 3.0f);
  style.CellPadding = ImVec2(5.0f, 3.0f);
  style.ItemSpacing = ImVec2(8.0f, 4.0f);
  style.ItemInnerSpacing = ImVec2(5.0f, 4.0f);
  style.IndentSpacing = 16.0f;
  style.ScrollbarSize = 12.0f;
  style.GrabMinSize = 10.0f;
  style.WindowMenuButtonPosition = ImGuiDir_None;
  style.Alpha = 1.0f;

  ImGuiIO& io = ImGui::GetIO();
  const char* fontPath = EditorFontCandidates();
  ImFont* font = fontPath != nullptr ? io.Fonts->AddFontFromFileTTF(fontPath, 15.0f) : nullptr;
  if (font == nullptr) {
    io.Fonts->AddFontDefault();
  } else {
    io.FontDefault = font;
  }
}
}