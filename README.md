# Portion of Screen
## Share a portion of your screen in Microsoft Teams or Google Meet
_Portion of Screen_ is a lightweight Windows application that enables you to share a part of your screen in video conferencing apps that only support full screen or just a single window.

This is a fork of [egonl/PortionOfScreen](https://github.com/egonl/PortionOfScreen) (MIT) that adds **size presets**: the shared area can be locked to a standard size such as 1920 x 1080, so that participants on smaller or scaled displays receive a readable picture instead of a downscaled 4K or ultrawide desktop.

### Usage in Microsoft Teams
- Start _PortionOfScreen.exe_
- In a Teams meeting, select **Share** > **Window** > **Portion of Screen**.

### Modes
_Portion of Screen_ supports two modes, selected via the menu > **Options**. The menu opens with a **right-click anywhere in the window** (or on its title bar), via the icon in the title bar, or with Alt+Space. Activate the window first (taskbar, Alt+Tab); without focus it is invisible and clicks pass through it.
- _Fixed Mode_: Only the area defined by the _Portion of Screen_ window will be shared. You can resize and move this window while presenting. Send the window to the background by left clicking it.
- _Focus Mode_: When Focus Mode is enabled, the window that currently has the focus will automatically be shared. Use this mode if you're regularly switching between windows.

### Size presets (version 1.3)
- Menu > **Size** locks the shared area to 1280 x 720, 1600 x 900, 1920 x 1080, 1920 x 1200, 2560 x 1440 or a custom size. Sizes are physical pixels, also on scaled (125 %, 150 %) displays and mixed multi-monitor setups. Choosing a size in Focus Mode switches to Fixed Mode.
- While a size is active the window can be moved but not resized. **Free** restores manual resizing.
- The window title always shows the current size of the shared area, e.g. "Portion of Screen 1920 x 1080".
- The title bar is transmitted as part of the window. Once sharing runs, choose menu > **Hide title bar** so that the shared window is exactly the chosen size. The title bar comes back whenever the window is activated (taskbar, Alt+Tab). Hide it only after you selected the window in Teams; windows without a title bar are not listed in the share picker.
- A size that does not fit on the current monitor is refused with a message. The last size is remembered and applied on the next start.

### Notes
- You (the presenter) won't see the _Portion of Screen_ window if it doesn't have the focus. However, your audience will see the _Portion of Screen_ window.
- You can send the _Portion of Screen_ window to the background by left clicking it.
- If you close the _Portion of Screen_ window, Teams will automatically stop sharing.
- Teams and Meet may still re-encode the stream at a lower resolution depending on bandwidth; the preset removes the downscaling of a large desktop, it cannot force the receivers' quality.

### Building
- Visual Studio 2026 (platform toolset v145) or the Build Tools 2026 with the "Desktop development with C++" workload: `msbuild /p:Configuration=Release /p:Platform=x64 PortionOfScreen.sln`
- Pushing a tag `release/<version>` builds and publishes `PortionOfScreen.exe` as a GitHub release.

See [CONTEXT.md](CONTEXT.md) for the terms used in the code, [docs/spec-1.3-size-presets.md](docs/spec-1.3-size-presets.md) for the design of the size presets and [docs/adr](docs/adr) for the decisions behind it.
