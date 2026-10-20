$ErrorActionPreference = 'Stop'

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
    throw 'This script requires an interactive Windows desktop session.'
}

if (-not ('CodexMonitorPower.NativeMethods' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;

namespace CodexMonitorPower
{
    public static class NativeMethods
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

        [DllImport("user32.dll", EntryPoint = "SendMessageTimeoutW", SetLastError = true)]
        public static extern IntPtr SendMessageTimeout(
            IntPtr hWnd, uint message, UIntPtr wParam, IntPtr lParam,
            uint flags, uint timeout, out UIntPtr result);
    }
}
'@
}

# 격리된 데스크톱에서 성공으로 오인하지 않도록 실제 사용자 바탕화면 확인
$windowStation = [CodexMonitorPower.NativeMethods]::GetObjectName(
    [CodexMonitorPower.NativeMethods]::GetProcessWindowStation())
$desktop = [CodexMonitorPower.NativeMethods]::GetObjectName(
    [CodexMonitorPower.NativeMethods]::GetThreadDesktop(
        [CodexMonitorPower.NativeMethods]::GetCurrentThreadId()))
$sessionId = [System.Diagnostics.Process]::GetCurrentProcess().SessionId
if (-not [Environment]::UserInteractive -or $sessionId -eq 0 -or
    $windowStation -ne 'WinSta0' -or $desktop -ne 'Default') {
    throw "Display-off request not sent: current desktop is $windowStation\$desktop (session $sessionId). Run through the approved tool outside the sandbox on the user's WinSta0\Default desktop."
}

# HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, display off (2).
$messageResult = [UIntPtr]::Zero
$sent = [CodexMonitorPower.NativeMethods]::SendMessageTimeout(
    [IntPtr]0xffff, [uint32]0x0112, [UIntPtr]0xf170, [IntPtr]2,
    [uint32]0x0002, [uint32]1000, [ref]$messageResult)

if ($sent -eq [IntPtr]::Zero) {
    $nativeError = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    throw "Display-off request failed or timed out (Win32 error: $nativeError)."
}

Write-Output 'Display-off request sent from WinSta0\Default. Physical display state is unverified. No system sleep or shutdown was requested.'
