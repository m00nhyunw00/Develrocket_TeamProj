$ErrorActionPreference = 'Stop'

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
    throw 'This script requires an interactive Windows desktop session.'
}

if (-not ('CodexMonitorPower.DisplayOnNativeMethods' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;

namespace CodexMonitorPower
{
    public static class DisplayOnNativeMethods
    {
        [DllImport("user32.dll")]
        public static extern IntPtr GetProcessWindowStation();

        [DllImport("user32.dll")]
        public static extern IntPtr GetThreadDesktop(uint threadId);

        [DllImport("kernel32.dll")]
        public static extern uint GetCurrentThreadId();

        [DllImport("user32.dll", EntryPoint = "GetUserObjectInformationW", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern bool GetUserObjectInformation(
            IntPtr handle, int index, StringBuilder text, uint size, out uint needed);

        public static string GetObjectName(IntPtr handle)
        {
            var text = new StringBuilder(1024);
            uint needed;
            if (!GetUserObjectInformation(handle, 2, text, (uint)(text.Capacity * 2), out needed))
                throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
            return text.ToString();
        }

        [DllImport("kernel32.dll")]
        public static extern uint SetThreadExecutionState(uint flags);

        [DllImport("user32.dll", EntryPoint = "SendMessageTimeoutW", SetLastError = true)]
        public static extern IntPtr SendMessageTimeout(
            IntPtr hWnd, uint message, UIntPtr wParam, IntPtr lParam,
            uint flags, uint timeout, out UIntPtr result);
    }
}
'@
}

# 격리된 데스크톱에서 성공으로 오인하지 않도록 실제 사용자 바탕화면 확인
$windowStation = [CodexMonitorPower.DisplayOnNativeMethods]::GetObjectName(
    [CodexMonitorPower.DisplayOnNativeMethods]::GetProcessWindowStation())
$desktop = [CodexMonitorPower.DisplayOnNativeMethods]::GetObjectName(
    [CodexMonitorPower.DisplayOnNativeMethods]::GetThreadDesktop(
        [CodexMonitorPower.DisplayOnNativeMethods]::GetCurrentThreadId()))
$sessionId = [System.Diagnostics.Process]::GetCurrentProcess().SessionId
if (-not [Environment]::UserInteractive -or $sessionId -eq 0 -or
    $windowStation -ne 'WinSta0' -or $desktop -ne 'Default') {
    throw "Display-on request not sent: current desktop is $windowStation\$desktop (session $sessionId). Run through the approved tool outside the sandbox on the user's WinSta0\Default desktop."
}

# ES_DISPLAY_REQUIRED만 한 번 호출해 화면 유휴 타이머 갱신
$previousExecutionState = [CodexMonitorPower.DisplayOnNativeMethods]::SetThreadExecutionState([uint32]0x00000002)
if ($previousExecutionState -eq 0) {
    throw 'Display idle timer reset failed; display-on request was not sent.'
}

# HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, display on (-1).
$messageResult = [UIntPtr]::Zero
$sent = [CodexMonitorPower.DisplayOnNativeMethods]::SendMessageTimeout(
    [IntPtr]0xffff, [uint32]0x0112, [UIntPtr]0xf170, [IntPtr](-1),
    [uint32]0x0002, [uint32]1000, [ref]$messageResult)

if ($sent -eq [IntPtr]::Zero) {
    $nativeError = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    throw "Display-on request failed or timed out (Win32 error: $nativeError)."
}

Write-Output 'Display idle timer reset and display-on request sent from WinSta0\Default. Physical display state is unverified. Running tasks and system power policies were not changed.'
