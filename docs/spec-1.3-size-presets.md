# Version 1.3: Size Presets

Decisions from the design session on 2026-09-21. Terms are defined in ../CONTEXT.md.

## Goal

The Share Region can be locked to a Size Preset in physical pixels so that colleagues on
smaller or scaled displays receive a standard resolution instead of a downscaled 5120x1440 desktop.

## Behaviour

- Presets: Free, 1280x720, 1600x900, 1920x1080, 1920x1200, 2560x1440, Custom.
- Selection: system menu submenu "Size" with a check mark on the active entry. "Custom..." opens a
  dialog with Width, Height, OK, Cancel. The system menu also opens on right-click anywhere in the
  Window, so the whole Share Region is the click target (the title bar icon is 16 px on a 5K display).
- Locked: while a preset is active the Window can be moved but not resized (min = max track size).
  Free restores the upstream behaviour and is the default on first start.
- Applying a preset keeps the top-left corner; if the Window would extend past the monitor it is
  pushed inwards. A preset that does not fit the monitor including the frame is refused with a
  message box, never clamped.
- Focus Mode: presets do not apply. Choosing a preset while in Focus Mode switches to Fixed Mode
  and applies it (the submenu is labelled "Size (leaves Focus Mode)"); a greyed submenu, the first
  design, turned out to be confusing in use.
- Title bar: system menu entry "Hide title bar", meant to be used once sharing runs (Teams does not
  list WS_POPUP windows in its picker). The title bar returns whenever the Window is activated.
  Hiding and showing keeps the Share Region at the same screen position and size; the Window
  grows upwards when the title bar returns.
- Window title always shows the current Share Region size: "Portion of Screen 1920x1080".
- Persistence: preset id and custom width/height in HKCU\SOFTWARE\PortionOfScreen next to FocusMode.
  Applied on start; if it does not fit the current monitor, fall back to Free with a message.

## Out of scope for 1.3

Hotkeys, zoom, multiple regions, installer, Windows.Graphics.Capture render path (candidate for 1.4,
see upstream issue #13), and any language or framework change.

## Toolchain

- Platform toolset v145 (Visual Studio 2026), C++20, Per-Monitor DPI V2 via manifest,
  AdjustWindowRectExForDpi for frame sizes.
- Local build: Visual Studio Build Tools 2026 (winget Microsoft.VisualStudio.BuildTools) with the
  C++ desktop workload.
- CI: existing release workflow on windows-latest with current actions/checkout and
  microsoft/setup-msbuild; tag release/1.3 publishes PortionOfScreen.exe.
- Version 1.3; About dialog points to this fork.

## Test matrix

100 %, 125 %, 150 % scaling on the 5120x1440 desktop; the laptop docked to an external monitor
as the mixed-DPI case; shared into Teams and Google Meet.
