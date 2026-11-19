// DesktopDuplication.cpp : image of the desktop beneath the Share Region, see DesktopDuplication.h.

#include "DesktopDuplication.h"
#include <algorithm>
#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

// How long to wait before trying again after duplication could not be started, e.g. while the
// Share Region touches two monitors or a UAC prompt holds the secure desktop.
#define RETRY_MS 3000

// Edge length of the tiles in which the image is compared and repainted, in pixels.
#define TILE 64

static BOOL CALLBACK CountMonitor(HMONITOR, HDC, LPRECT, LPARAM count)
{
    ++*(int*) count;
    return TRUE;
}

// The region may extend past the edge of the desktop (e.g. the Share Region of a Window whose
// Title Bar is shown and whose bottom is off-screen); that part stays black, as with BitBlt.
// It must not touch a second monitor, because one duplication only sees one monitor.
static bool OnSingleMonitor(const RECT& region, const RECT& monitor)
{
    RECT overlap;
    if (!IntersectRect(&overlap, &region, &monitor))
        return false;
    int count = 0;
    EnumDisplayMonitors(nullptr, &region, CountMonitor, (LPARAM) &count);
    return count == 1;
}

static void AddRect(HRGN target, int left, int top, int right, int bottom)
{
    HRGN rgn = CreateRectRgn(left, top, right, bottom);
    CombineRgn(target, target, rgn, RGN_OR);
    DeleteObject(rgn);
}

DesktopDuplication::~DesktopDuplication()
{
    Close();
    if (memDC)
    {
        SelectObject(memDC, oldBitmap);
        DeleteDC(memDC);
    }
    if (bitmap)
        DeleteObject(bitmap);
    if (frameChanges)
        DeleteObject(frameChanges);
    if (changed)
        DeleteObject(changed);
}

HRGN DesktopDuplication::Update(const RECT& region)
{
    if (!frameChanges)
    {
        frameChanges = CreateRectRgn(0, 0, 0, 0);
        changed = CreateRectRgn(0, 0, 0, 0);
        if (!frameChanges || !changed)
            return nullptr;
    }

    if (duplication && !OnSingleMonitor(region, monitorRect))
        Close();                                    // moved to another monitor
    if (!duplication)
    {
        if (GetTickCount64() < retryAfter)
            return nullptr;
        if (!Open(region))
        {
            retryAfter = GetTickCount64() + RETRY_MS;
            return nullptr;
        }
    }

    SetRectRgn(frameChanges, 0, 0, 0, 0);
    bool changesUnknown = false;
    DXGI_OUTDUPL_FRAME_INFO info;
    ComPtr<IDXGIResource> resource;
    HRESULT hr = duplication->AcquireNextFrame(0, &info, &resource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT)
    {
        // The screen has not changed since the last frame.
    }
    else if (FAILED(hr))
    {
        Close();                                    // e.g. DXGI_ERROR_ACCESS_LOST: reopen next time
        return nullptr;
    }
    else
    {
        // LastPresentTime is zero if only the mouse pointer moved, which is not part of the image.
        ComPtr<ID3D11Texture2D> frame;
        if (info.LastPresentTime.QuadPart != 0 && SUCCEEDED(resource.As(&frame)))
        {
            if (!desktop)
            {
                D3D11_TEXTURE2D_DESC desc;
                frame->GetDesc(&desc);
                desc.Usage = D3D11_USAGE_DEFAULT;
                desc.BindFlags = 0;
                desc.CPUAccessFlags = 0;
                desc.MiscFlags = 0;
                device->CreateTexture2D(&desc, nullptr, &desktop);
            }
            if (desktop)
            {
                context->CopyResource(desktop.Get(), frame.Get());
                haveFrame = true;
                changesUnknown = !CollectChanges(info.TotalMetadataBufferSize);
            }
        }
        duplication->ReleaseFrame();
    }
    if (!haveFrame)
        return nullptr;

    // Skip the copy if the frame reports no change beneath the region. Something changes
    // somewhere on the monitor nearly all the time (a blinking cursor, a clock).
    bool regionMoved = !EqualRect(&imageRect, &region);
    if (!regionMoved && !changesUnknown)
    {
        HRGN regionRgn = CreateRectRgnIndirect(&region);
        int kind = CombineRgn(changed, frameChanges, regionRgn, RGN_AND);
        DeleteObject(regionRgn);
        if (kind == NULLREGION || kind == ERROR)
            return nullptr;
    }

    return CopyRegion(region, regionMoved) ? changed : nullptr;
}

bool DesktopDuplication::Covers(const RECT& region) const
{
    return duplication && EqualRect(&imageRect, &region);
}

void DesktopDuplication::Draw(HDC hdc) const
{
    BitBlt(hdc, 0, 0, bitmapWidth, bitmapHeight, memDC, 0, 0, SRCCOPY);
}

// Starts duplicating the monitor the region is on.
bool DesktopDuplication::Open(const RECT& region)
{
    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        return false;

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT a = 0; factory->EnumAdapters1(a, &adapter) != DXGI_ERROR_NOT_FOUND; ++a)
    {
        ComPtr<IDXGIOutput> output;
        for (UINT o = 0; adapter->EnumOutputs(o, &output) != DXGI_ERROR_NOT_FOUND; ++o)
        {
            DXGI_OUTPUT_DESC outputDesc;
            if (FAILED(output->GetDesc(&outputDesc)) || !outputDesc.AttachedToDesktop ||
                !OnSingleMonitor(region, outputDesc.DesktopCoordinates))
                continue;

            // The device must live on the adapter the monitor is attached to (hybrid graphics).
            if (FAILED(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr, 0,
                                         D3D11_SDK_VERSION, &device, nullptr, &context)))
                return false;

            // DuplicateOutput1 converts HDR desktops to 8-bit BGRA; DuplicateOutput cannot.
            HRESULT hr = E_FAIL;
            ComPtr<IDXGIOutput5> output5;
            ComPtr<IDXGIOutput1> output1;
            if (SUCCEEDED(output.As(&output5)))
            {
                const DXGI_FORMAT formats[] = { DXGI_FORMAT_B8G8R8A8_UNORM };
                hr = output5->DuplicateOutput1(device.Get(), 0, ARRAYSIZE(formats), formats, &duplication);
            }
            else if (SUCCEEDED(output.As(&output1)))
                hr = output1->DuplicateOutput(device.Get(), &duplication);

            DXGI_OUTDUPL_DESC duplDesc = {};
            if (SUCCEEDED(hr))
                duplication->GetDesc(&duplDesc);
            bool usable = SUCCEEDED(hr) &&
                duplDesc.ModeDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM &&
                (duplDesc.Rotation == DXGI_MODE_ROTATION_IDENTITY || duplDesc.Rotation == DXGI_MODE_ROTATION_UNSPECIFIED);
            if (!usable)
            {
                Close();
                return false;
            }
            monitorRect = outputDesc.DesktopCoordinates;
            return true;
        }
    }
    return false;                                   // the region touches no or several monitors
}

void DesktopDuplication::Close()
{
    staging.Reset();
    desktop.Reset();
    duplication.Reset();
    context.Reset();
    device.Reset();
    haveFrame = false;
    imageRect = {};
}

// Adds the moved and dirty rectangles of the acquired frame to frameChanges. False if they
// could not be read.
bool DesktopDuplication::CollectChanges(UINT metadataSize)
{
    if (metadataSize == 0)
        return false;
    if (metadata.size() < metadataSize)
        metadata.resize(metadataSize);

    UINT moveBytes = 0;
    if (FAILED(duplication->GetFrameMoveRects(metadataSize, (DXGI_OUTDUPL_MOVE_RECT*) metadata.data(), &moveBytes)))
        return false;
    const DXGI_OUTDUPL_MOVE_RECT* moves = (const DXGI_OUTDUPL_MOVE_RECT*) metadata.data();
    for (UINT i = 0; i < moveBytes / sizeof(DXGI_OUTDUPL_MOVE_RECT); ++i)
        AddChange(moves[i].DestinationRect);

    UINT dirtyBytes = 0;
    if (FAILED(duplication->GetFrameDirtyRects(metadataSize, (RECT*) metadata.data(), &dirtyBytes)))
        return false;
    const RECT* dirty = (const RECT*) metadata.data();
    for (UINT i = 0; i < dirtyBytes / sizeof(RECT); ++i)
        AddChange(dirty[i]);
    return true;
}

// Adds a rectangle in monitor coordinates to frameChanges.
void DesktopDuplication::AddChange(const RECT& rect)
{
    AddRect(frameChanges, rect.left + monitorRect.left, rect.top + monitorRect.top,
            rect.right + monitorRect.left, rect.bottom + monitorRect.top);
}

// Copies the region out of the last desktop frame into the bitmap that Draw() paints, and sets
// `changed` to the tiles whose pixels differ from the previous image, or to the whole image if
// `whole` is set. True if anything changed.
//
// The pixel comparison is what keeps the Window from repainting itself forever: repainting the
// Window counts as a change of the screen beneath it, even while it is fully transparent, so
// every repaint would produce a frame whose dirty rectangles cover the whole region.
bool DesktopDuplication::CopyRegion(const RECT& region, bool whole)
{
    RECT visible;                                   // the part of the region on the monitor
    if (!IntersectRect(&visible, &region, &monitorRect))
        return false;
    int width = visible.right - visible.left;
    int height = visible.bottom - visible.top;
    int regionWidth = region.right - region.left;
    int regionHeight = region.bottom - region.top;

    D3D11_TEXTURE2D_DESC desc = {};
    if (staging)
        staging->GetDesc(&desc);
    if (!staging || (int) desc.Width != width || (int) desc.Height != height)
    {
        staging.Reset();
        desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging)))
            return false;
    }

    D3D11_BOX box;
    box.left = visible.left - monitorRect.left;
    box.top = visible.top - monitorRect.top;
    box.right = visible.right - monitorRect.left;
    box.bottom = visible.bottom - monitorRect.top;
    box.front = 0;
    box.back = 1;
    context->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, desktop.Get(), 0, &box);

    bool created = false;
    if (!EnsureBitmap(regionWidth, regionHeight, created))
        return false;
    whole = whole || created;

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
    {
        Close();
        return false;
    }
    GdiFlush();                                     // GDI must be done with the bitmap before it is written

    const BYTE* source = (const BYTE*) mapped.pData;
    size_t targetPitch = (size_t) regionWidth * 4;
    BYTE* target = bits + (size_t) (visible.top - region.top) * targetPitch + (size_t) (visible.left - region.left) * 4;
    int offsetX = visible.left - region.left;
    int offsetY = visible.top - region.top;
    bool anyChange = whole;

    if (whole)
    {
        if (!EqualRect(&visible, &region))
            memset(bits, 0, targetPitch * regionHeight);
        for (int y = 0; y < height; ++y)
            memcpy(target + y * targetPitch, source + (size_t) y * mapped.RowPitch, (size_t) width * 4);
        SetRectRgn(changed, 0, 0, regionWidth, regionHeight);
    }
    else
    {
        SetRectRgn(changed, 0, 0, 0, 0);
        for (int tileY = 0; tileY < height; tileY += TILE)
        {
            int tileHeight = (std::min)(TILE, height - tileY);
            int runStart = -1;                      // first changed tile of the current run in this row
            for (int tileX = 0; ; tileX += TILE)
            {
                bool differs = false;
                if (tileX < width)
                {
                    size_t bytes = (size_t) (std::min)(TILE, width - tileX) * 4;
                    for (int y = tileY; y < tileY + tileHeight; ++y)
                    {
                        const BYTE* from = source + (size_t) y * mapped.RowPitch + (size_t) tileX * 4;
                        BYTE* to = target + y * targetPitch + (size_t) tileX * 4;
                        if (differs || memcmp(to, from, bytes) != 0)
                        {
                            differs = true;
                            memcpy(to, from, bytes);
                        }
                    }
                }
                if (differs && runStart < 0)
                    runStart = tileX;
                if (!differs && runStart >= 0)
                {
                    AddRect(changed, offsetX + runStart, offsetY + tileY,
                            offsetX + (std::min)(tileX, width), offsetY + tileY + tileHeight);
                    runStart = -1;
                    anyChange = true;
                }
                if (tileX >= width)
                    break;
            }
        }
    }
    context->Unmap(staging.Get(), 0);

    imageRect = region;
    return anyChange;
}

// A top-down 32-bit DIB section of the given size, selected into memDC. `created` is set if a
// new bitmap had to be made, whose content is undefined.
bool DesktopDuplication::EnsureBitmap(int width, int height, bool& created)
{
    if (bitmap && bitmapWidth == width && bitmapHeight == height)
        return true;

    if (!memDC)
    {
        memDC = CreateCompatibleDC(nullptr);
        if (!memDC)
            return false;
    }

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;              // negative: top-down, like the texture
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* newBits = nullptr;
    HBITMAP newBitmap = CreateDIBSection(memDC, &info, DIB_RGB_COLORS, &newBits, nullptr, 0);
    if (!newBitmap)
        return false;

    HGDIOBJ previous = SelectObject(memDC, newBitmap);
    if (bitmap)
        DeleteObject(bitmap);                       // `previous` is the old DIB section
    else
        oldBitmap = previous;                       // the DC's stock bitmap, restored on destruction
    bitmap = newBitmap;
    bits = (BYTE*) newBits;
    bitmapWidth = width;
    bitmapHeight = height;
    created = true;
    return true;
}
