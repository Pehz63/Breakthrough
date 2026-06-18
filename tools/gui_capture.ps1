# gui_capture.ps1 - Targeted screenshot of the running breakthrough_gui window.
#
# Complements smoke_test_gui.ps1: that one is a full-screen pass/fail check (does
# the GUI build and stay alive). This one gives a precise client-area crop of the
# raylib window so you can read individual widgets, glyphs, and text.
#
# How it works: FindWindow by title returns 0 for raylib windows, so it enumerates
# top-level windows by process id and matches the window class "GLFW30", moves the
# window to a known size, then captures only the client rectangle.
#
# Usage (from the project root, after building breakthrough_gui.exe):
#   .\tools\gui_capture.ps1                          # default 940x820 -> build\cap.png
#   .\tools\gui_capture.ps1 -Out build\widget.png    # custom output path
#   .\tools\gui_capture.ps1 -Wd 1200 -Ht 900         # custom window size
#
# The output defaults under git-ignored build\, so captures never clutter commits.
# To inspect a single small widget, open the PNG and zoom with nearest-neighbor
# (no smoothing) so glyphs/text stay crisp.

param([string]$Out = "build\cap.png", [int]$Wd = 940, [int]$Ht = 820)
Get-Process breakthrough_gui -EA 0 | Stop-Process -Force
Start-Sleep -Milliseconds 300
$p = Start-Process -FilePath ".\breakthrough_gui.exe" -PassThru
Start-Sleep -Seconds 2
Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class Cap {
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Auto)] static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h,int x,int y,int w,int t,bool r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  public struct RECT { public int l,t,r,b; }
  public struct POINT { public int x,y; }
  delegate bool EnumProc(IntPtr h, IntPtr l);
  public static IntPtr FindGlfw(uint pid){
    IntPtr found=IntPtr.Zero;
    EnumWindows((h,l)=>{ uint pp; GetWindowThreadProcessId(h,out pp);
      if(pp==pid && IsWindowVisible(h)){ var sb=new StringBuilder(64); GetClassName(h,sb,64);
        if(sb.ToString()=="GLFW30"){found=h;return false;} } return true;},IntPtr.Zero);
    return found;
  }
}
"@
$h=[Cap]::FindGlfw([uint32]$p.Id)
[Cap]::MoveWindow($h,60,40,$Wd,$Ht,$true)|Out-Null
[Cap]::SetForegroundWindow($h)|Out-Null
Start-Sleep -Milliseconds 700
$r=New-Object Cap+RECT;[Cap]::GetClientRect($h,[ref]$r)|Out-Null
$pt=New-Object Cap+POINT;[Cap]::ClientToScreen($h,[ref]$pt)|Out-Null
Add-Type -AssemblyName System.Drawing
$bmp=New-Object System.Drawing.Bitmap($r.r,$r.b)
$g=[System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($pt.x,$pt.y,0,0,(New-Object System.Drawing.Size($r.r,$r.b)))
$bmp.Save($Out)
$g.Dispose();$bmp.Dispose()
Write-Host "saved $Out $($r.r)x$($r.b)"
