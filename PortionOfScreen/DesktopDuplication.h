#pragma once

#include "framework.h"
#include <d3d11.h>
#include <dxgi1_5.h>
#include <wrl/client.h>
#include <vector>

// Keeps an image of the desktop beneath the Share Region, read with DXGI Desktop Duplication,
// and tells which parts of it changed.
//
// Reading the screen DC with BitBlt makes DWM read the frame back on the CPU; at a 200 ms
// refresh that costs DWM a large part of a core and makes the whole desktop lag. Duplication
// lets the GPU copy the composed desktop, and Windows only delivers a frame when something has
// changed (docs/adr/0002-desktop-duplication-instead-of-screen-bitblt.md).
//
// Duplication is per monitor: a Share Region that touches two monitors, a rotated monitor or a
// failure (UAC prompt, display mode change) leave the image unavailable, and the caller falls
// back to BitBlt until it is available again.
class DesktopDuplication
{
public:
    ~DesktopDuplication();

    // Brings the image up to date with the desktop beneath `region` (screen coordinates,
    // physical pixels). Returns the part of the image whose pixels changed, in client
    // coordinates, for InvalidateRgn; the region belongs to this object and is valid until the
    // next call. nullptr if nothing changed or the image is unavailable (see Covers()).
    HRGN Update(const RECT& region);

    // True if the image shows exactly `region`, so that Draw() can paint it.
    bool Covers(const RECT& region) const;

    // Paints the image at the top left of the client area.
    void Draw(HDC hdc) const;

private:
    bool Open(const RECT& region);
    void Close();
    bool CollectChanges(UINT metadataSize);
    void AddChange(const RECT& rect);
    bool CopyRegion(const RECT& region, bool whole);
    bool EnsureBitmap(int width, int height, bool& created);

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> duplication;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> desktop;    // last desktop frame of the monitor (GPU)
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;    // the region, readable by the CPU
    RECT monitorRect = {};          // desktop coordinates of the duplicated monitor
    RECT imageRect = {};            // screen rectangle the image shows; empty = no image
    bool haveFrame = false;         // `desktop` holds a frame
    ULONGLONG retryAfter = 0;       // GetTickCount64() before which Open() is not tried again
    std::vector<BYTE> metadata;     // move and dirty rectangles of the acquired frame
    HRGN frameChanges = nullptr;    // parts of the monitor the frame reports as changed, screen coordinates
    HRGN changed = nullptr;         // what Update() returns

    HDC memDC = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ oldBitmap = nullptr;
    BYTE* bits = nullptr;
    int bitmapWidth = 0;
    int bitmapHeight = 0;
};
