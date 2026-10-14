#include <string>
#include <gdiplus.h>
#include <windows.h>

using namespace std;

void bitmap_draw(auto graphics, Gdiplus::Bitmap *bitmap_ptr, int size) {
    Gdiplus::Graphics g(graphics);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    g.DrawImage(bitmap_ptr, 0, 0, size, size);
}

Gdiplus::Bitmap *bitmap_from_memory(const string &image_bytes, int size) { //converts string image data into drawable bitmap
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
        scaled = source->Clone(0, 0, size, size, PixelFormat32bppPARGB);
        Gdiplus::Graphics g(scaled);
        bitmap_draw(scaled, source, size);
    }
    delete source;
    input_stream->Release();
    return scaled;
}