---
name: turn-on-monitors
description: 사용자가 Windows의 꺼진 모니터 화면을 켜도록 요청하면 기본 Windows API로 연결된 모든 디스플레이에 화면 켜짐을 요청한다. 실행 중인 Codex·빌드·Unreal·Rider 작업과 시스템 전원 정책은 유지한다. 스킬 작성·설명 요청만으로 실행하지 않는다.
---

# Windows 모니터 화면 켜기

[turn-off-monitors](../turn-off-monitors/SKILL.md)로 껐거나 Windows 디스플레이 절전 상태인 모든 모니터에 화면 켜짐을 요청한다. 물리적으로 전원이 꺼진 모니터, 전원이 꺼진 컴퓨터 또는 시스템 절전 중인 컴퓨터를 깨우는 기능은 아니다. 잠금 해제·로그인이나 프로그램 시작·종료도 수행하지 않는다.

## 대상과 실행 조건

- Windows의 실제 사용자 데스크톱 세션에서 실행한다. Linux·WSL·컨테이너·서비스 세션의 화면 제어를 실제 Windows 디스플레이 제어로 간주하지 않는다. 연결된 Windows 실행 도구와 대상 사용자 세션을 확인한다.
- 원격 컴퓨터라면 컴퓨터와 사용자 세션을 확인한다. RDP 가상 디스플레이와 콘솔 물리 모니터는 다를 수 있다. 콘솔 모니터를 켜려면 해당 콘솔 세션에서 실행한다. 대상이 불명확할 때만 확인을 요청한다.
- 실행 전 화면 켜기를 요청한다고 짧게 알린다. 명확한 화면 켜기 요청에는 추가 승인을 요구하지 않는다. 번들 PowerShell 스크립트를 사용하며 외부 프로그램 설치나 관리자 권한은 기본적으로 필요하지 않다.

## 실행

저장소 루트에서 설치된 PowerShell 7(`pwsh.exe`)로 실행한다. 실제 경로는 현재 체크아웃에 맞게 지정한다. 이미 PowerShell 7에서 실행 중이면 `& ".agents\skills\turn-on-monitors\scripts\turn-on-monitors.ps1"`로 직접 호출해도 된다. Windows PowerShell 5.1(`powershell.exe`)은 실행 정책이 별도로 적용되므로 고정 호출하지 않는다. `pwsh.exe`가 PATH에 없으면 현재 실행 중인 PowerShell 7의 `$PSHOME\pwsh.exe` 경로를 사용한다.

```powershell
pwsh.exe -NoProfile -NonInteractive -File ".agents\skills\turn-on-monitors\scripts\turn-on-monitors.ps1"
```

- Codex Windows 샌드박스에서는 명령이 실제 바탕화면과 다른 `CodexSandboxDesktop`에서 실행될 수 있다. 같은 세션 ID이거나 `UserInteractive=True`인 것만으로 실제 바탕화면이라고 판단하지 않는다. 이 환경에서는 `exec_command`의 `sandbox_permissions="require_escalated"`로 아래 스크립트의 사용자 데스크톱 실행을 요청하고 도구의 승인 결과를 따른다. 스크립트는 `WinSta0\Default`인지 확인하고 다른 데스크톱에서는 메시지를 보내기 전에 오류로 종료한다. 데스크톱 전환이나 보안 설정 변경으로 우회하지 않는다.
- 실행 정책을 우회하거나 변경하지 않는다. 조직 정책으로 차단되면 원인을 보고한다.
- 실제 사용자 바탕화면 검사 후 `SetThreadExecutionState(ES_DISPLAY_REQUIRED)`를 한 번 호출해 화면 유휴 타이머를 갱신한다. 켜기 메시지만 보내고 끝내지 않는다. `ES_CONTINUOUS`나 `ES_SYSTEM_REQUIRED`는 추가하지 않으며, 상시 화면 켜짐 유지나 시스템 절전 방지를 설정하지 않는다. API 반환값이 0이면 실패로 보고한다. [Microsoft API 문서](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setthreadexecutionstate).
- `user32.dll`의 `SendMessageTimeoutW`로 `HWND_BROADCAST`에 `WM_SYSCOMMAND / SC_MONITORPOWER` 메시지와 `lParam=-1`을 보내 전체 디스플레이 전원 켜짐을 요청한다. 화면 끄기 스킬의 `lParam=2`에 대응한다.
- 타임아웃이 있는 일회성 전송 뒤 PowerShell이 종료된다. 브로드캐스트 타임아웃은 대상 창마다 적용될 수 있으므로 전체 호출이 지정한 1초보다 길어질 수 있다. 상시 실행 프로세스, 반복 전송, 키보드·마우스 입력 주입은 만들지 않는다.
- Codex 절전 방지, Windows 자동 화면 꺼짐·절전 정책, 기존 작업은 변경하지 않는다. 이 요청이 화면을 계속 켜 두는 것은 아니다.

## 확인과 제한

- 종료 코드와 출력으로 API 요청 결과를 확인한다. 요청 성공과 모든 물리 모니터가 실제로 켜졌다는 관찰을 구분한다. 드라이버·모니터·세션에 따라 직접 전원 켜짐 요청의 효과가 다를 수 있다.
- 켜지지 않으면 세션과 디스플레이 상태를 확인하고, 사용자가 키보드·마우스로 깨울 수 있음을 안내한다. 요청 없이 입력을 주입하거나 전원 정책을 변경하지 않는다.
- `SetSuspendState`, `shutdown.exe`, 프로세스 종료·잠금 해제를 사용하지 않는다. 이 스크립트 자체는 컴퓨터 절전·최대 절전·종료나 작업 중단을 요청하지 않는다.
- Windows에서 실제 실행하지 않았다면 화면 켜짐을 검증했다고 쓰지 않는다. 완료 보고에는 요청 결과와 남은 확인 사항만 간결하게 적는다.
