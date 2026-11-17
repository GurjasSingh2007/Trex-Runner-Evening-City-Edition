#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <string>
#include <cstdio>
#include <algorithm>
#include "game.h"

using namespace std;

static const int SCALE = 2;
static trex::Input g_in;
static bool g_space = false, g_down = false;

static string savePath() {
    char b[MAX_PATH]; DWORD n = GetEnvironmentVariableA("APPDATA", b, MAX_PATH);
    string d = (n && n < MAX_PATH) ? string(b) + "\\TRexRunner" : ".";
    CreateDirectoryA(d.c_str(), nullptr);
    return d + "\\highscore.txt";
}
static int loadHigh(const string& p) { int v = 0; if (FILE* f = fopen(p.c_str(), "r")) { if (fscanf(f, "%d", &v) != 1) v = 0; fclose(f); } return v < 0 ? 0 : v; }
static void saveHigh(const string& p, int v) { if (FILE* f = fopen(p.c_str(), "w")) { fprintf(f, "%d\n", v); fclose(f); } }

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_KEYDOWN:
        if (wp == VK_SPACE || wp == VK_UP) g_space = true;
        if (wp == VK_DOWN) g_down = true;
        if (wp == VK_RETURN) g_in.restartKey = true;
        if (wp == VK_ESCAPE) PostMessage(hwnd, WM_CLOSE, 0, 0);
        return 0;
    case WM_KEYUP:
        if (wp == VK_SPACE || wp == VK_UP) g_space = false;
        if (wp == VK_DOWN) g_down = false;
        return 0;
    case WM_KILLFOCUS: g_space = false; g_down = false; return 0;
    case WM_LBUTTONDOWN:
        g_in.click = true;
        g_in.mx = (short)LOWORD(lp) / SCALE;
        g_in.my = (short)HIWORD(lp) / SCALE;
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    SetProcessDPIAware();
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc; wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "TRexWnd";
    RegisterClassA(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT r = { 0, 0, trex::W * SCALE, trex::H * SCALE };
    AdjustWindowRect(&r, style, FALSE);
    HWND hwnd = CreateWindowA("TRexWnd", "T-Rex Runner  (Space/Up = jump, Down = duck, Space/click/Enter = restart, Esc = quit)",
                              style, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                              nullptr, nullptr, hInst, nullptr);
    ShowWindow(hwnd, nShow);

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = trex::W; bi.bmiHeader.biHeight = -trex::H;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC wdc = GetDC(hwnd);
    HDC mdc = CreateCompatibleDC(wdc);
    HBITMAP dib = CreateDIBSection(wdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    SelectObject(mdc, dib);
    HFONT font = CreateFontA(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, "Arial");
    SelectObject(mdc, font);
    SetBkMode(mdc, TRANSPARENT);
    SetTextColor(mdc, RGB(255, 243, 230));
    SetStretchBltMode(wdc, COLORONCOLOR);

    trex::Game game;
    const string hiPath = savePath();
    game.setHighScore(loadHigh(hiPath));
    HFONT big = CreateFontA(-18, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, "Arial");

    LARGE_INTEGER freq, last, now; QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&last);
    const double dt = 1.0 / 60.0; double acc = 0;
    timeBeginPeriod(1);

    MSG m; bool running = true;
    while (running) {
        while (PeekMessage(&m, nullptr, 0, 0, PM_REMOVE)) {
            if (m.message == WM_QUIT) running = false;
            TranslateMessage(&m); DispatchMessage(&m);
        }
        QueryPerformanceCounter(&now);
        acc += (double)(now.QuadPart - last.QuadPart) / freq.QuadPart; last = now;
        if (acc > 0.25) acc = 0.25;

        bool drew = false;
        while (acc >= dt) {
            g_in.space = g_space; g_in.down = g_down;
            game.step(g_in);
            if (game.takeRecordDirty()) saveHigh(hiPath, game.highScore);
            g_in.click = false; g_in.restartKey = false;
            acc -= dt; drew = true;
        }
        if (drew) {
            game.render((uint32_t*)bits);
            string s = "Score: " + to_string(game.score);
            TextOutA(mdc, 500, 44, s.c_str(), (int)s.size());
            string lv = "Level " + to_string(game.level) + (game.difficulty() > 0 ? "  HARD" : "");
            TextOutA(mdc, 500, 60, lv.c_str(), (int)lv.size());
            string best = "Best " + to_string(max(game.highScore, game.score));
            TextOutA(mdc, 500, 76, best.c_str(), (int)best.size());
            if (game.popupTicks > 0) {
                RECT r = { 190, 22, 410, 52 };
                HBRUSH gold = CreateSolidBrush(RGB(255, 210, 63)); FillRect(mdc, &r, gold); DeleteObject(gold);
                SelectObject(mdc, big); SetTextColor(mdc, RGB(40, 26, 60));
                DrawTextA(mdc, "NEW HIGH SCORE!", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(mdc, font); SetTextColor(mdc, RGB(255, 243, 230));
            }
            StretchBlt(wdc, 0, 0, trex::W * SCALE, trex::H * SCALE, mdc, 0, 0, trex::W, trex::H, SRCCOPY);
        } else {
            Sleep(1);
        }
    }
    timeEndPeriod(1);
    if (game.state == trex::PLAY && game.score > game.highScore) saveHigh(hiPath, game.score);
    return 0;
}
