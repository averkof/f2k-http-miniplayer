#include <algorithm>
#include <string>
#include <winsock2.h>
#include <windows.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include "httplib.h"
#include "network.h"

#define WM_APP_CHANGED (WM_APP + 1)

using namespace std;
using std::min; using std::max;

const wchar_t FONT_NAME[]       = L"Consolas";
const char g_szClassName[]      = "myWindowClass";
const int SIDE_MARGIN           = 10;
const int TOP_MARGIN            = 10;
const int WINDOW_LENGTH         = 500;
const int WINDOW_HEIGHT         = 200;
const int FONT_SIZE             = -15;
const int LINE_HEIGHT           = 30;
const COLORREF BACKGROUND_COLOR = RGB(18, 18, 18);

HFONT font          = NULL;
HBRUSH background   = NULL;

NetworkHandler *network_handler         = nullptr;
Gdiplus::Bitmap *song_artwork_display   = nullptr;

//look through later again
Gdiplus::Bitmap *bitmap_from_memory(const string &image_bytes) { //converts string image data into drawable bitmap
    if (image_bytes.empty()) return nullptr;

    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, image_bytes.size());
    if (!h) return nullptr;
    memcpy(GlobalLock(h), image_bytes.data(), image_bytes.size());
    GlobalUnlock(h);

    IStream *input_stream = nullptr;
    if (CreateStreamOnHGlobal(h, TRUE, &input_stream) != S_OK) {
        GlobalFree(h);
        return nullptr;
    }

    Gdiplus::Bitmap *source = Gdiplus::Bitmap::FromStream(input_stream);
    Gdiplus::Bitmap *scaled = nullptr;
    if (source && source->GetLastStatus() == Gdiplus::Ok) {
        scaled = source->Clone(0, 0, WINDOW_HEIGHT, WINDOW_HEIGHT, PixelFormat32bppPARGB);
        Gdiplus::Graphics g(scaled);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        g.DrawImage(source, 0, 0, WINDOW_HEIGHT, WINDOW_HEIGHT);
    }
    delete source;
    input_stream->Release();
    return scaled;
}

void draw_text(HDC &hdc, const wstring *str, LPRECT rect) {
    DrawTextW(hdc, str->c_str(), (int)str->size(), rect, DT_LEFT | DT_END_ELLIPSIS | DT_SINGLELINE | DT_NOPREFIX);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE: {
            HICON hIcon;

            //load icon and display
            hIcon = (HICON)LoadImage(NULL, "./assets/generic.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
            if (hIcon)
                SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            else
                MessageBox(hwnd, "Could not load large icon!", "Error", MB_OK | MB_ICONERROR);
            
            //load font and background
            font = CreateFontW(FONT_SIZE, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, FONT_NAME); //bullshit call
            background = CreateSolidBrush(BACKGROUND_COLOR);
        }
        break;
        case WM_NCHITTEST:
            return HTCAPTION;
        case WM_APP_CHANGED: {
            //update song details
            if (network_handler) {
                network_handler->update_details(network_handler->song_details);
                InvalidateRect(hwnd, NULL, TRUE);
            }

            //update artwork
            Gdiplus::Bitmap *fresh_cover = bitmap_from_memory(network_handler->song_details.artwork);
            delete song_artwork_display;
            song_artwork_display = fresh_cover;
            string().swap(network_handler->song_details.artwork);
        }
        break;
        //check later
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
            if (song_artwork_display) {
                Gdiplus::Graphics g(hdc);
                g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
                g.DrawImage(song_artwork_display, 0, 0, WINDOW_HEIGHT, WINDOW_HEIGHT);
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
        case WM_DESTROY:
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
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

    network_handler = new NetworkHandler(PostMessage, hwnd, WM_APP_CHANGED, 0, 0);
    network_handler->start();

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