# Size Presets are physical pixels of the client area, with Per-Monitor DPI V2

A Size Preset such as 1920x1080 sets the Share Region (the client area) to exactly that many
physical monitor pixels, not the outer window and not DPI-scaled logical pixels. Teams and Meet
transmit physical pixels, so this is the only definition under which the audience receives the
promised resolution. To make it hold on scaled displays and mixed-DPI multi-monitor setups the
process moves from system DPI awareness (`SetProcessDPIAware`) to Per-Monitor DPI V2 via the
application manifest, and frame sizes are computed with `AdjustWindowRectExForDpi`.

## Considered Options

- Outer window = preset: simpler, but the content is then ~30 px short and the size of that
  shortfall depends on theme and DPI.
- Stay system-DPI-aware and document "exact only at 100 %": rejected because the feature exists
  for colleagues on scaled laptop displays, where 1920x1080 would silently become 2880x1620.

## Consequences

- Because the title bar is transmitted too, the window is slightly taller than the preset while
  the title bar is shown. Hiding the title bar (system menu) makes window and Share Region equal.
- Presets that do not fit the current monitor, including the frame, are refused with a message
  rather than clamped, so a shown size is always the real size.
