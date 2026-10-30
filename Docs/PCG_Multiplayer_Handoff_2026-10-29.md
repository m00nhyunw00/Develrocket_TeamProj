# PCG 멀티플레이 시드 동기화 인수인계

작성일: 2026-10-29

## 현재 요청과 작업 범위

- 사용자 목적: 멀티플레이에서 PCG 도시가 서버와 클라이언트에 동일하게 생성되도록 구현.
- 사용자가 Rider에서 직접 코드를 수정하고, Codex가 저장된 소스를 재검토하는 방식으로 진행했다.
- 마지막 재점검에서는 코드를 수정하지 않았다. 이번 작업은 인수인계 문서 작성이다.
- 다음 작업도 현재 소스를 먼저 확인하고 진행한다. 이 문서를 근거로 이미 구현·검증이 끝났다고 판단하지 않는다.

## 프로젝트와 대상

- 프로젝트: `C:\Users\user\Documents\Unreal Projects\Develrocket_TeamProj`
- 엔진: Unreal Engine 5.8
- 헤더: `Source/Develrocket_TeamProj/PCG/CityPacking.h`
- 구현: `Source/Develrocket_TeamProj/PCG/CityPacking.cpp`
- 기존 맥락의 그래프: `/Game/Developers/Dev_Seo/_PCG/_Graph/PCG_Sample`
- 기존 맥락의 맵: `/Game/Developers/Dev_Seo/PCG_Test`
- 그래프·맵 경로는 이전 작업 맥락이다. 이번 소스 재점검에서 에디터의 현재 열린 맵이나 배치 상태를 확인하지 않았다.

## 현재 소스에서 확인한 구조

1. `ACityPackingActor` 생성자에서 `bReplicates = true`, `bAlwaysRelevant = true` 설정.
2. PCG 컴포넌트 생성 트리거는 `GenerateOnDemand`.
3. `FCityLayoutState`에 `Seed`와 `Revision`이 있으며, 액터의 `LayoutState`를 `ReplicatedUsing=OnRep_LayoutState`로 선언했다.
4. `GetLifetimeReplicatedProps()`에서 `DOREPLIFETIME(ACityPackingActor, LayoutState)` 등록.
5. 서버 `BeginPlay()`에서 `GenerateNewLayoutOnServer()` 호출. 이 함수는 권한을 검사하고 새 GUID 해시로 시드를 만든 뒤 Revision 증가, 서버 로컬 적용, `ForceNetUpdate()`를 수행한다.
6. 클라이언트는 `OnRep_LayoutState()`와 `BeginPlay()`에서 `ApplyLayoutState()`를 호출한다. BeginPlay 이전 또는 Revision 0이면 적용을 보류한다.
7. `ApplyLayoutState()`는 복제받은 Seed를 `LayoutSeed`에 복사한 뒤 `RebuildSameSeed()` 호출.
8. `RebuildSameSeed()`는 Floor 기준 경계 갱신, 기존 PCG 정리, 컴포넌트 Seed 설정, `GenerateLocal(true)` 호출.
9. 사용자 수정으로 `NewLayout()`은 게임 월드에서 `GenerateNewLayoutOnServer()`를 호출하도록 변경됐다. 클라이언트 호출은 내부 권한 검사에서 무시된다. 이 함수 자체는 Server RPC가 아니다.
10. 패킹 PCG Element는 `FRandomStream Random(Owner->LayoutSeed);`를 사용한다. 해당 범위에는 `ACityPackingActor* Owner` 선언이 있으므로 이 표현 자체는 유효하다. 실행마다 새 GUID 시드를 만들던 기존 코드는 주석 처리됐다.

현재 방향은 서버가 시드·Revision을 복제하고, 각 머신이 동일한 입력으로 로컬 PCG를 생성하는 방식이다. 건물 액터 전체를 서버에서 복제하는 방식과 혼동하지 않는다.

## 남은 문제와 수정 우선순위

### 1. 생성 중 수신한 새 상태의 생성 요청이 유실될 수 있음

- 위치: `CityPacking.cpp`의 `RebuildSameSeed()` 시작 부분, 재점검 당시 47행.
- 현재 `PCGComponent->IsGenerating()`이면 바로 반환하며 대기 상태나 재시도가 없다.
- 재현 가능한 흐름: 클라이언트가 Revision 1을 생성 중 → 서버의 Revision 2 수신 → `LayoutSeed`만 새 값으로 변경 → 재생성은 조기 반환 → Revision 2로 다시 생성되지 않을 수 있다.
- 다음 수정 방향: 최신 수신 상태를 보관하고, 현재 생성이 끝나면 최신 Revision을 적용한다. 완료 델리게이트/API는 로컬 UE 5.8 PCG 소스에서 확인한다.
- 생성 중 사용하는 시드를 고정하고, 새 요청과 진행 중인 요청의 Revision을 구분한다. 적용 완료 상태는 실제 생성 완료와 구분 없이 먼저 확정하지 않는다.
- 빠른 연속 재생성, 늦게 접속한 클라이언트를 포함해 검증한다.

### 2. `bRandomizeOnPlay` 옵션이 무시됨

- 위치: `CityPacking.cpp`의 `BeginPlay()`와 `GenerateNewLayoutOnServer()`, 재점검 당시 84행 및 71행부터.
- 서버 BeginPlay가 항상 새 GUID 기반 시드를 생성한다. 따라서 옵션을 꺼도 고정된 `LayoutSeed`로 시작하지 않는다.
- 다음 수정 방향: 시작 시 서버가 옵션에 따라 랜덤 시드 또는 설정된 `LayoutSeed`를 선택하고, 두 경우 모두 LayoutState의 Revision을 유효한 값으로 갱신해 복제한다.
- 수동 `NewLayout()`의 새 랜덤 배치 동작과 시작 시 옵션을 구분한다.

### 추가 정리 및 확인

- `bReuseSeedNextGeneration`은 현재 true/false 대입만 남아 있으며, 기존 새 시드 생성 분기는 주석 처리됐다. 관련 주석과 플래그 정리는 기능 수정 후 검토한다.
- `Math/RandomStream.h`, `Engine/World.h` 직접 include 여부를 검토한다. 현재 간접 include로 컴파일될 가능성이 있으므로 빌드하지 않고 확정적인 컴파일 오류라고 말하지 않는다.
- 같은 시드만으로 동일 배치를 보장하려면 양쪽의 Floor 변환, 건물 목록 및 순서, 건물 bounds, 간격, 기타 그래프 입력도 같아야 한다. 다르다는 증거 없이 모두 복제해야 한다고 단정하지 않는다.
- 생성되는 건물 BP의 Replicates 여부 및 중복 생성 가능성은 에디터에서 별도 확인해야 한다. 현재 중복 생성이 관측됐다는 뜻은 아니다.
- 클라이언트가 새 배치를 요청해야 하는 요구가 추가되면 소유권이 있는 PlayerController 등의 Server RPC 경로를 설계한다. 현재 클라이언트 `NewLayout()` 호출은 서버에 전달되지 않는다.

## 검증 상태

- 수행: 저장된 `CityPacking.h/.cpp` 소스 재검토.
- 기본 시드 복제 연결 및 `NewLayout()`의 서버 경로 수정은 소스에서 확인했다.
- 미수행: 이번 변경의 빌드, PIE 서버·클라이언트 실행, 늦은 접속, 연속 재생성, 실제 배치 비교.
- 따라서 “멀티플레이 동기화 해결 완료” 상태가 아니다.
- 문서 작성 전 Git에서 위 두 소스 파일이 수정 상태임을 확인했다. 사용자의 변경을 되돌리거나 덮어쓰지 않는다. 커밋·푸시는 요청받지 않았다.

## 다음 작업 순서

1. 현재 소스와 Git diff를 다시 읽어 사용자 추가 수정 여부 확인.
2. 생성 완료 이벤트를 확인하고 최신 Revision 대기·재적용 처리 구현.
3. 시작 시 `bRandomizeOnPlay` 반영.
4. 적절한 Editor 타깃 빌드로 컴파일 확인. 실행 중인 Editor/Live Coding과의 DLL 충돌을 기능 오류와 구분.
5. 서버와 클라이언트에서 Seed/Revision, 건물 개수와 클래스·Transform 목록을 비교. 기존 로그에는 Seed/개수/간격/커버리지가 있지만, 같은 개수만으로 같은 배치가 입증되지는 않는다.
6. 최초 시작, 연속 재생성, 생성 중 상태 갱신, 늦은 접속, Randomize 옵션 꺼짐을 각각 확인하고 실제 실행한 검증만 보고.

## 대화 주의사항

- 사용자에게는 한국어로 간결하고 구체적으로 설명한다.
- 검토 요청을 임의의 수정 요청으로 바꾸지 않는다.
- 소스 확인, 빌드 성공, 에디터 상태, 실제 멀티플레이 검증을 구분해서 보고한다.
- 이전 답변의 BeginPlay 링크에 프로젝트 경로 일부가 누락됐었다. 이 문서의 프로젝트 루트와 실제 소스 경로를 기준으로 접근한다.
