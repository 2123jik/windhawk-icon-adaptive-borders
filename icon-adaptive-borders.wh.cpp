// ==WindhawkMod==
// @id              icon-adaptive-borders
// @name            Icon Adaptive Window Borders
// @description     Dynamically tints the window border based on the application's icon dominant color
// @version         0.1.0
// @author          YourName
// @github          https://github.com/YourName/windhawk-icon-adaptive-borders
// @include         *
// @compilerOptions -lgdi32 -ldwmapi -luxtheme
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Icon Adaptive Window Borders
Extracts the dominant color of the running application's icon 
and dynamically tints the window border/caption.
*/
// ==/WindhawkModReadme==

#include <windows.h>
#include <dwmapi.h>
#include <algorithm>
#include <vector>

// 兼容定义：DWM 属性常量 (Win11 原生支持 34/35)
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif

// ==========================================
// 1. 颜色处理工具：提取与调和（情绪降噪）
// ==========================================
COLORREF AdjustColorSaturationAndLightness(COLORREF srcColor) {
    BYTE r = GetRValue(srcColor);
    BYTE g = GetGValue(srcColor);
    BYTE b = GetBValue(srcColor);

    // 简单防瞎眼：向中间灰/暗色系拉偏，避免刺眼的高亮原色
    // 你可以在这里调节透明度或混入 30% 黑色
    r = (BYTE)(r * 0.75f);
    g = (BYTE)(g * 0.75f);
    b = (BYTE)(b * 0.75f);

    return RGB(r, g, b);
}

COLORREF ExtractDominantColor(HICON hIcon) {
    if (!hIcon) return RGB(45, 45, 45); // 默认沉浸深灰

    ICONINFO iconInfo;
    if (!GetIconInfo(hIcon, &iconInfo)) return RGB(45, 45, 45);

    BITMAP bmp;
    GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmp);

    HDC hdc = CreateCompatibleDC(NULL);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdc, iconInfo.hbmColor);

    // 对小图标进行网格快速采样（避免逐像素扫描拖慢速度）
    long totalR = 0, totalG = 0, totalB = 0;
    int sampleCount = 0;

    int stepX = std::max(1, bmp.bmWidth / 8);
    int stepY = std::max(1, bmp.bmHeight / 8);

    for (int y = 0; y < bmp.bmHeight; y += stepY) {
        for (int x = 0; x < bmp.bmWidth; x += stepX) {
            COLORREF pixel = GetPixel(hdc, x, y);
            BYTE r = GetRValue(pixel);
            BYTE g = GetGValue(pixel);
            BYTE b = GetBValue(pixel);

            // 过滤极黑与近乎透明的纯白色背景噪点
            if ((r < 25 && g < 25 && b < 25) || (r > 235 && g > 235 && b > 235)) {
                continue;
            }

            totalR += r;
            totalG += g;
            totalB += b;
            sampleCount++;
        }
    }

    SelectObject(hdc, hOldBmp);
    DeleteDC(hdc);
    if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
    if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);

    if (sampleCount == 0) return RGB(45, 45, 45);

    COLORREF avgColor = RGB(totalR / sampleCount, totalG / sampleCount, totalB / sampleCount);
    return AdjustColorSaturationAndLightness(avgColor);
}

// ==========================================
// 2. 窗口钩子与着色逻辑
// ==========================================
void ApplyIconBorder(HWND hWnd) {
    if (!IsWindow(hWnd) || !IsWindowVisible(hWnd)) return;

    // 过滤掉子窗口或没有标题栏的无边框托盘等窗口
    LONG style = GetWindowLong(hWnd, GWL_STYLE);
    if (!(style & WS_CAPTION)) return;

    // 步骤 A: 抓取当前窗口图标
    HICON hIcon = (HICON)SendMessage(hWnd, WM_GETICON, ICON_SMALL2, 0);
    if (!hIcon) hIcon = (HICON)SendMessage(hWnd, WM_GETICON, ICON_SMALL, 0);
    if (!hIcon) hIcon = (HICON)GetClassLongPtr(hWnd, GCLP_HICONSM);
    if (!hIcon) hIcon = (HICON)GetClassLongPtr(hWnd, GCLP_HICON);

    // 步骤 B: 提取主色
    COLORREF themeColor = ExtractDominantColor(hIcon);

    // 步骤 C: 注入 DWM 着色
    // 在 Win11 下以下两行直接生效高亮边框和标题栏；
    // 在 Win10 下需要挂钩 dwmcore 或拦截 WM_NCPAINT 绘制（后续迭代）
    DwmSetWindowAttribute(hWnd, DWMWA_BORDER_COLOR, &themeColor, sizeof(COLORREF));
    DwmSetWindowAttribute(hWnd, DWMWA_CAPTION_COLOR, &themeColor, sizeof(COLORREF));
}

// 拦截窗口创建与激活事件
using CreateWindowExW_t = decltype(&CreateWindowExW);
CreateWindowExW_t pOriginalCreateWindowExW = nullptr;

HWND WINAPI Hook_CreateWindowExW(
    DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName,
    DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
    HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam
) {
    HWND hWnd = pOriginalCreateWindowExW(
        dwExStyle, lpClassName, lpWindowName, dwStyle,
        X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam
    );

    if (hWnd && hWndParent == NULL) {
        // 主窗口生成后延迟染色
        ApplyIconBorder(hWnd);
    }
    return hWnd;
}

// ==========================================
// 3. Windhawk 初始化与销毁
// ==========================================
BOOL Wh_ModInit() {
    Wh_Log(L"Icon Adaptive Borders Mod Initialized!");

    // Hook 窗口创建
    Wh_SetFunctionHook(
        (void*)CreateWindowExW,
        (void*)Hook_CreateWindowExW,
        (void**)&pOriginalCreateWindowExW
    );

    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L"Icon Adaptive Borders Mod Unloaded.");
}
