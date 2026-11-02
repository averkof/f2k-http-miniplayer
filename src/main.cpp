#include <algorithm>
#include <string>
#include <winsock2.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include "httplib.h"
#include "network.h"
#include "grphics.h"

#define IDT_PROGRESS    3
#define WM_APP_CHANGED  (WM_APP + 1)
#define WM_POS_CHANGED  (WM_APP + 2)

using namespace std;
using std::min; using std::max;

const char g_szClassName[]      = "myWindowClass";
const int SIDE_MARGIN           = 10;
const int TOP_MARGIN            = 10;
const int WINDOW_LENGTH         = 500;
const int WINDOW_HEIGHT         = 200;
const int FONT_SIZE             = -15;
const int LINE_HEIGHT           = 30;
const int TIMER_REFRESH         = 250;
const COLORREF BACKGROUND_COLOR = RGB(18, 18, 18);

HFONT font          = NULL;
HBRUSH background   = NULL;
wchar_t FONT_NAME[] = L"Consolas";
RECT update_region;

NetworkHandler *network_handler         = nullptr;
Gdiplus::Bitmap *song_artwork_display   = nullptr;

VOID CALLBACK PositionTimerProc(HWND hwnd, UINT, UINT_PTR, DWORD) {
    SendMessage(hwnd, WM_POS_CHANGED, 0, 0);
}

//look through later again
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE: {

            update_region.left = SIDE_MARGIN + WINDOW_HEIGHT;
            update_region.right = update_region.left + 117;
            update_region.top = TOP_MARGIN + 3 * LINE_HEIGHT;
            update_region.bottom = update_region.top + LINE_HEIGHT;

            HICON hIcon;

            //load icon and display
            hIcon = (HICON)LoadImage(NULL, "./assets/generic.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
            if (hIcon)
                SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            else
                MessageBox(hwnd, "Could not load large icon!", "Error", MB_OK | MB_ICONERROR);
            
            //load font and background brush
            font = create_font(FONT_SIZE, FW_NORMAL, FALSE, FALSE, FALSE, FONT_NAME);
            background = CreateSolidBrush(BACKGROUND_COLOR);
        }
        break;
        case WM_NCHITTEST:
            return HTCAPTION;
        case WM_APP_CHANGED: {
            //update song details
            if (network_handler) {
                network_handler->update_details();
                network_handler->update_position();
                InvalidateRect(hwnd, NULL, FALSE);
            }

            //update artwork
            Gdiplus::Bitmap *fresh_cover = bitmap_from_memory(network_handler->song_details.artwork, WINDOW_HEIGHT);
            delete song_artwork_display;
            song_artwork_display = fresh_cover;
            string().swap(network_handler->song_details.artwork);
        }
        break;
        //check later
        case WM_POS_CHANGED: {
            if (network_handler) {
                network_handler->update_position();
                InvalidateRect(hwnd, &update_region, FALSE);
            }
        }
        break;
        case WM_PAINT: {
            
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            //set font and text
            SelectObject(hdc, font);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));

            //find window rectangle + color background
            RECT r;
            GetClientRect(hwnd, &r);
            FillRect(hdc, &r, background);

            //draw artwork
            RECT art_rect = {0, 0, WINDOW_HEIGHT, WINDOW_HEIGHT};
            RECT tmp;
            if (song_artwork_display && IntersectRect(&tmp, &ps.rcPaint, &art_rect)) {
                bitmap_draw(hdc, song_artwork_display, WINDOW_HEIGHT);
            }

            //draw text lines
            r.left += SIDE_MARGIN + WINDOW_HEIGHT; r.right -= SIDE_MARGIN;
            r.top += TOP_MARGIN;
            for (const wstring *song_detail : network_handler->details) {
                draw_text(hdc, song_detail, &r);
                r.top       += LINE_HEIGHT;
                r.bottom    += LINE_HEIGHT;
            }

            EndPaint(hwnd, &ps);
        }
        break;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) DestroyWindow(hwnd);
        break;
        case WM_ERASEBKGND:
            return 1;
        case WM_DESTROY:
            KillTimer(hwnd, IDT_PROGRESS);
            network_handler->stop();
            delete network_handler;
            DeleteObject(font);
            PostQuitMessage(0);
        break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wc;
    HWND hwnd;
    MSG Msg;

    DWORD ws_style      = WS_POPUP;
    DWORD ws_ex_style   = WS_EX_TOPMOST;

    wc.cbSize           = sizeof(WNDCLASSEX);
    wc.style            = 0;
    wc.lpfnWndProc      = WndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = hInstance;
    wc.hIcon            = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = g_szClassName;
    wc.hIconSm          = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    //look through
    Gdiplus::GdiplusStartupInput gdiplus_in;
    ULONG_PTR gdiplus_token;
    Gdiplus::GdiplusStartup(&gdiplus_token, &gdiplus_in, NULL);

    RECT rect_window = {0, 0, WINDOW_LENGTH, WINDOW_HEIGHT};
    AdjustWindowRectEx(&rect_window, ws_style, FALSE, ws_ex_style);

    hwnd = CreateWindowEx(
        ws_ex_style,
        g_szClassName,
        "f2k-miniplayer",
        ws_style,
        CW_USEDEFAULT, CW_USEDEFAULT, 
        rect_window.right - rect_window.left, rect_window.bottom - rect_window.top,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) {
        MessageBox(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    int pref = DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));

    network_handler = new NetworkHandler(PostMessage, hwnd, WM_APP_CHANGED, WM_POS_CHANGED);
    network_handler->start();
    SetTimer(hwnd, IDT_PROGRESS, TIMER_REFRESH, PositionTimerProc);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while(GetMessage(&Msg, NULL, 0, 0) > 0) {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }

    delete song_artwork_display;
    Gdiplus::GdiplusShutdown(gdiplus_token);
    
    return Msg.wParam;
}