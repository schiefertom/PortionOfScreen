# Smoke test for the Size Preset feature. Builds nothing: run after
#   msbuild /p:Configuration=Release /p:Platform=x64 PortionOfScreen.sln
# Drives the built EXE through window messages on the current monitor at the current
# scaling, saves and restores the user's settings in HKCU\SOFTWARE\PortionOfScreen.
# Expected: every line PASS. Run it again after changing the display scaling (125 %, 150 %).
$ErrorActionPreference = 'Stop'
$exe = Join-Path $PSScriptRoot '..\x64\Release\PortionOfScreen.exe'
$regPath = 'HKCU:\SOFTWARE\PortionOfScreen'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class W {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [DllImport("user32.dll")] public static extern IntPtr GetWindowDpiAwarenessContext(IntPtr h);
  [DllImport("user32.dll")] public static extern bool AreDpiAwarenessContextsEqual(IntPtr a, IntPtr b);
  [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h, int i);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  public static IntPtr FindByPid(uint pid, string cls) {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h, l) => {
      uint p; GetWindowThreadProcessId(h, out p);
      if (p != pid || !IsWindowVisible(h)) return true;
      var sb = new StringBuilder(64); GetClassName(h, sb, 64);
      if (sb.ToString() == cls) { found = h; return false; }
      return true;
    }, IntPtr.Zero);
    return found;
  }
  public static string Title(IntPtr h) { var sb = new StringBuilder(256); GetWindowText(h, sb, 256); return sb.ToString(); }
}
"@

function Client($h) { $r = New-Object W+RECT; [W]::GetClientRect($h, [ref]$r) | Out-Null; "$($r.R)x$($r.B)" }
function Win($h) { $r = New-Object W+RECT; [W]::GetWindowRect($h, [ref]$r) | Out-Null; "$($r.R-$r.L)x$($r.B-$r.T) @ $($r.L),$($r.T)" }
function Check($name, $cond) { if ($cond) { "PASS  $name" } else { "FAIL  $name" } }
function HasCaption($h) { (([W]::GetWindowLong($h, -16)) -band 0x00C00000) -eq 0x00C00000 }
function Launch() {
  $script:p = Start-Process $exe -PassThru
  Start-Sleep -Milliseconds 1500
  [W]::FindByPid($script:p.Id, 'PORTIONOFSCREEN')
}
function CloseAndWait($h) { [W]::PostMessage($h, 0x0010, [IntPtr]0, [IntPtr]0) | Out-Null; $script:p.WaitForExit(5000) | Out-Null }
function DismissMessageBox() {
  Start-Sleep -Milliseconds 800
  $mb = [W]::FindByPid($script:p.Id, '#32770')
  if ($mb -ne [IntPtr]::Zero) { $text = [W]::Title($mb); [W]::SendMessage($mb, 0x0111, [IntPtr]1, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 300; return $text }
  return $null
}
$WM_SYSCOMMAND = 0x0112; $WM_ACTIVATE = 0x0006
$times = "$([char]0x00D7)"

# Preserve the user's settings, force Fixed Mode and Free for the test.
$backup = $null
if (Test-Path $regPath) { $backup = Get-ItemProperty $regPath }
New-Item -Path $regPath -Force | Out-Null
foreach ($kv in @{FocusMode=0; SizePreset=0; Left=100; Top=100; Right=900; Bottom=700}.GetEnumerator()) { Set-ItemProperty $regPath $kv.Key $kv.Value -Type DWord }

$h = Launch
Check "window found" ($h -ne [IntPtr]::Zero)
Check "DPI context is PerMonitorV2" ([W]::AreDpiAwarenessContextsEqual([W]::GetWindowDpiAwarenessContext($h), [IntPtr](-4)))
Check "title at start shows client size with U+00D7" ([W]::Title($h) -eq "Portion of Screen 784 $times 561")
"start: title='$([W]::Title($h))' window=$(Win $h)"

# 1920x1080 preset
[W]::SendMessage($h, $WM_SYSCOMMAND, [IntPtr]0x1130, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 300
Check "preset 1920x1080 -> client 1920x1080" ((Client $h) -eq '1920x1080')
Check "title shows 1920 x 1080" ([W]::Title($h) -eq "Portion of Screen 1920 $times 1080")
"preset: window=$(Win $h)"

# Locked: an attempt to resize must be clamped back to the preset size
[W]::SetWindowPos($h, [IntPtr]0, 0, 0, 800, 600, 0x0016) | Out-Null; Start-Sleep -Milliseconds 300
Check "locked: SetWindowPos 800x600 is clamped, client stays 1920x1080" ((Client $h) -eq '1920x1080')

# 2560x1440 fits the 5120x1440 monitor only without the title bar -> hint, preset applied, pushed to top
[W]::PostMessage($h, $WM_SYSCOMMAND, [IntPtr]0x1150, [IntPtr]0) | Out-Null
$hint = DismissMessageBox
Check "2560x1440 with title bar: hint message box shown" ($null -ne $hint)
Check "2560x1440: client is 2560x1440" ((Client $h) -eq '2560x1440')
Check "2560x1440: window pushed to monitor top (y=0)" ((Win $h) -like '* @ *,0')
"2560: window=$(Win $h)"

# Back to 1920x1080, then hide title bar: window rect must equal the share region
[W]::SendMessage($h, $WM_SYSCOMMAND, [IntPtr]0x1130, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 300
$regionBefore = Win $h
[W]::SendMessage($h, $WM_SYSCOMMAND, [IntPtr]0x1010, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 300
Check "hide title bar: WS_CAPTION cleared" (-not (HasCaption $h))
Check "hide title bar: window == share region 1920x1080" ((Win $h) -like '1920x1080 @*')
"hidden: window=$(Win $h) (was $regionBefore)"

# Activation -> title bar returns, share region keeps position and size
$r = New-Object W+RECT; [W]::GetWindowRect($h, [ref]$r) | Out-Null; $regionHidden = "$($r.L),$($r.T)"
[W]::SendMessage($h, $WM_ACTIVATE, [IntPtr]1, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 300
Check "activate: WS_CAPTION restored" (HasCaption $h)
Check "activate: client still 1920x1080" ((Client $h) -eq '1920x1080')
$c = New-Object W+RECT; [W]::GetClientRect($h, [ref]$c) | Out-Null
$pt = New-Object W+RECT; [W]::GetWindowRect($h, [ref]$pt) | Out-Null
"restored: window=$(Win $h); share region was at $regionHidden"
Check "activate: window grew upwards (top < hidden top)" ($pt.T -lt [int]($regionHidden.Split(',')[1]))

# Free again: resizing works, title follows
[W]::SendMessage($h, $WM_SYSCOMMAND, [IntPtr]0x1100, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 200
[W]::SetWindowPos($h, [IntPtr]0, 0, 0, 800, 600, 0x0016) | Out-Null; Start-Sleep -Milliseconds 300
Check "free: SetWindowPos 800x600 changes the window" ((Win $h) -like '800x600 @*')
Check "free: title follows client size" ([W]::Title($h) -eq "Portion of Screen $((Client $h) -replace 'x'," $times ")")

# Persistence: set 1920x1200, close, read registry
[W]::SendMessage($h, $WM_SYSCOMMAND, [IntPtr]0x1140, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 200
CloseAndWait $h
$saved = Get-ItemProperty $regPath
Check "persisted SizePreset == 4 (1920x1200)" ($saved.SizePreset -eq 4)
Check "persisted CustomWidth default 1920" ($saved.CustomWidth -eq 1920)

# Restart: preset applied at start
$h = Launch
Check "restart: client 1920x1200 from saved preset" ((Client $h) -eq '1920x1200')
CloseAndWait $h

# Saved custom size larger than the monitor: refused at start, falls back to Free
Set-ItemProperty $regPath SizePreset 6 -Type DWord
Set-ItemProperty $regPath CustomWidth 6000 -Type DWord
Set-ItemProperty $regPath CustomHeight 1000 -Type DWord
$h = Launch
$msg = DismissMessageBox
Check "oversized saved custom size: message box at start" ($null -ne $msg)
Check "oversized saved custom size: custom size not applied" ((Client $h) -ne '6000x1000')
[W]::SetWindowPos($h, [IntPtr]0, 0, 0, 640, 480, 0x0016) | Out-Null; Start-Sleep -Milliseconds 300
Check "oversized saved custom size: not locked" ((Win $h) -like '640x480 @*')
CloseAndWait $h
Check "after fallback SizePreset saved as Free" ((Get-ItemProperty $regPath).SizePreset -eq 0)

# Focus Mode: size commands are ignored
Set-ItemProperty $regPath FocusMode 1 -Type DWord
Set-ItemProperty $regPath SizePreset 3 -Type DWord
$h = Launch
[W]::SendMessage($h, $WM_SYSCOMMAND, [IntPtr]0x1140, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 300
Check "focus mode: preset command ignored" ((Client $h) -ne '1920x1200')
CloseAndWait $h
Check "focus mode: saved preset kept (3)" ((Get-ItemProperty $regPath).SizePreset -eq 3)

# Restore the user's settings
Remove-Item $regPath -Recurse -Force
if ($backup) {
  New-Item -Path $regPath -Force | Out-Null
  foreach ($n in 'Left','Top','Right','Bottom','FocusMode','SizePreset','CustomWidth','CustomHeight') {
    if ($null -ne $backup.$n) { Set-ItemProperty $regPath $n $backup.$n -Type DWord }
  }
  "settings restored"
}
