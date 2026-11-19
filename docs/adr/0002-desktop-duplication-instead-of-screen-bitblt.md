# The desktop beneath the Share Region is read with DXGI Desktop Duplication, not BitBlt

The Window shows the desktop beneath the Share Region so that the conferencing app, which
captures the Window, transmits that desktop area. Upstream copies it every 200 ms with `BitBlt`
from the screen DC. Under DWM that read makes DWM read the composed frame back on the CPU. With a
2560x1440 Share Region on a 5120x1440 monitor this cost DWM about 30 % of a core, and once the
program had run for a day DWM was measured at 90-100 %. Every window, including keyboard input,
then lagged, also while nobody was sharing.

The image now comes from DXGI Desktop Duplication (`DesktopDuplication.h`): the GPU copies the
frame, Windows only delivers one when the screen changed, and only 64x64 tiles whose pixels
differ from the previous image are repainted. Measured on the same setup: DWM 30-33 % with
`BitBlt`, 6-8 % with duplication, against 3-7 % without Portion of Screen running.

## Considered Options

- Slower timer: makes the cost smaller only in proportion and makes the shared picture
  stutter. With `BitBlt` at 1000 ms DWM still used 10-24 %.
- Repaint only while the Window is visible: does not work, because the Window is fully
  transparent exactly while it is being shared.
- Repaint whatever the dirty rectangles of the duplicated frame cover: these always cover the
  whole Share Region. Repainting the Window counts as a change of the screen beneath it, even at
  zero opacity, so the Window would keep repainting itself. Comparing the pixels breaks that loop.
- `DwmRegisterThumbnail` or Windows.Graphics.Capture inside the Window: a DWM thumbnail cannot
  show the desktop, and it is not certain that the conferencing app's window capture would see
  content that DWM composes in.

## Consequences

- While the Window has the focus, and for 0.5 s after, it is half transparent and would appear
  in its own duplicated frame. During that time `BitBlt` is still used, at the old cost.
- `BitBlt` also stays the fallback whenever duplication is unavailable: a Share Region that
  touches two monitors, a rotated monitor, a UAC prompt or a display mode change. Starting
  duplication again is tried every 3 s.
- Parts of the Share Region beyond the edge of the desktop are black, as before.
- `tests/smoke.ps1` does not cover the image itself. The image was checked by comparing a
  `PrintWindow` capture of the Window with the screen beneath it: no differing pixels.
