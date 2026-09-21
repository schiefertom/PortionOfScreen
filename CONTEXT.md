# Portion of Screen

A Windows overlay window that video-conferencing apps (Teams, Google Meet) can share as a
"window", so that only the desktop area beneath it reaches the audience. This fork adds
fixed-size presets so that the shared area matches a standard resolution such as 1920x1080.

## Language

**Share Region**:
The rectangular desktop area the audience sees. It is exactly the client area of the Window;
caption and frame are not part of it.
_Avoid_: portion, capture area, shared window, viewport

**Window**:
The operating-system window Portion of Screen manages: the Share Region plus caption and frame.
Teams transmits the whole Window, so it is larger than the Share Region whenever the caption is shown.
_Avoid_: overlay, PoS window

**Physical Pixel**:
One pixel of the monitor, independent of Windows display scaling. All sizes in this project are
physical pixels, because that is what the conferencing app transmits.
_Avoid_: logical pixel, DIP, scaled pixel

**Size Preset**:
A named fixed Share Region size in physical pixels, such as 1920x1080. While a preset is active
the Share Region is Locked.
_Avoid_: resolution, dimension, screen size, format

**Custom Size**:
A Size Preset whose width and height the user typed in rather than picked from the list.
_Avoid_: manual size, free size

**Free**:
The sizing state without a Size Preset: the Share Region is whatever the user drags it to.
This is the upstream behaviour and the default on first start.
_Avoid_: unlocked, manual, custom

**Locked**:
The sizing state while a Size Preset is active: the Window can be moved but not resized.
_Avoid_: fixed (clashes with Fixed Mode), frozen

**Title Bar**:
The caption strip above the Share Region. It is transmitted to the audience and is what makes
the Window larger than the Share Region. It can be hidden once sharing has started and returns
whenever the Window is activated.
_Avoid_: caption (Win32 term, fine in code), header

## Operating modes (upstream)

**Fixed Mode**:
The Share Region stays where the user put the Window.
_Avoid_: manual mode, static mode

**Focus Mode**:
The Window follows the window that currently has keyboard focus, so the focused window is
shared. Size Presets do not apply in Focus Mode.
_Avoid_: follow mode, auto mode
