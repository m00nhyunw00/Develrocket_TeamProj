# PCG_Sample 도시 배치

2026-10-29 구현 및 에디터/PIE 검증.

## 사용

1. `/Game/Developers/Dev_Seo/PCG_Test` 맵 열기.
2. Outliner의 **PCG_Sample_City** 선택.
3. Details → City → **New Layout**: 새 배치 생성.
4. **Rebuild Same Seed**: Layout Seed의 배치를 그대로 다시 생성.
5. Play를 누르면 **Randomize On Play** 설정에 따라 게임 시작마다 새 배치 생성.

PCG 그래프는 `/Game/Developers/Dev_Seo/_PCG/_Graph/PCG_Sample`이다. 기존 PCGVolume은 비활성화되어 있다. 새 그래프는 프로젝트의 `CityPackingActor`에서 실행한다.

## 조절

| 설정 | 현재 값 | 의미 |
|---|---:|---|
| Floor | 레벨의 Floor | 배치 영역과 바닥 높이 |
| Building Gap | 250cm | 건물 경계 사이 최소 간격 |
| Edge Margin | 250cm | 바닥 가장자리 여백 |
| Max Buildings | 96 | 생성 수 상한, 실제 수는 크기와 빈 공간에 따라 결정 |
| Layout Seed | 42 | 현재 에디터 배치 재현값 |
| Randomize On Play | true | 게임 시작 시 새 Seed 사용 |

Floor/간격을 수정한 다음 New Layout 또는 Rebuild Same Seed로 반영한다. 수평 Floor와 양수 Scale을 사용한다. Floor의 Yaw 회전은 지원한다.

건물은 A~F 원본 크기를 유지하고 0/90/180/270도로 회전한다. 고정된 정사각형 칸 대신, 건물 크기에 맞는 남은 직사각형 공간을 나눠 채우는 MaxRects 방식을 사용한다. 인접한 건물 사이에는 2.5m를 확보하며, 남는 공간은 더 넓을 수 있다. 실제 외형의 오목한 부분을 사용하는 폴리곤 패킹은 아니다.

## 그래프

`City Footprint Packing → Spawn Actor → Output`

- 첫 노드는 프로젝트 C++로 추가한 PCG 노드. Floor와 건물 경계 데이터를 사용해 배치 점을 생성한다.
- `BuildingClass` metadata로 Spawn Actor에서 각 BP를 선택한다.
- BP 내부 PLA + LI 구성을 유지하며, 생성물은 PCG가 관리한다.
- `CityBounds` 컴포넌트는 C++ 생성자에서 기본 제공하고, 재생성 시 Floor 범위에 맞춘다.
- Python 도구는 제작/검증에만 사용했다. 일반 재생성과 PIE 실행에는 Python bridge가 필요하지 않다.

건물 BP의 형상이나 child actor 배치를 수정하면 첫 노드의 **Buildings → Bounds Min/Max**도 다시 측정해서 갱신해야 한다. 현재 값은 2026-10-28 원본 BP를 배치하여 측정한 값이다.

## 확인 결과

| 검사 | 결과 |
|---|---|
| Editor Seed 42 | 37개, 6종, 4방향, 겹침/간격 위반/영역 이탈 0 |
| Editor Seed 17 | 37개, 6종, 4방향, 겹침/간격 위반/영역 이탈 0 |
| New Layout | 35개, 새 Seed/배치, 검사 통과 |
| 같은 Seed 재생성 | 종류·위치·회전 signature 일치 |
| PIE 1 | Seed 706398453, 36개, 검사 통과 |
| PIE 2 | Seed 1119424466, 39개, 검사 통과 |
| PIE 간 비교 | Seed와 실제 배치 signature 모두 다름 |
| 저장 후 맵 다시 열기 | 37개 유지, Seed 42의 배치 signature 일치, 미저장 맵 없음 |

각 검사에서 실제 생성된 BP 액터 수가 계산된 수와 일치했다. 실제 액터 경계로 겹침과 간격을 검사했으며, 가장 가까운 이웃까지 거리는 모든 건물에서 약 250cm였다. 이는 프레임 성능 측정이나 모든 통로의 플레이어 이동 가능성을 검증한 결과는 아니다.

검증 JSON은 `Saved/PCGCity/`와 `Docs/PCG_Sample_Handoff_Evidence/`에 보관한다. 이전 인수인계 문서는 1개 생성 단계의 과거 기록이다.

## 환경 참고

현재 설치된 Fab 플러그인은 UBT에서 `FabToolset` ModuleRules를 찾지 못해 프로젝트 빌드를 막았다. 프로젝트의 Fab 플러그인을 비활성화하여 빌드했다. Fab을 다시 사용할 때에는 엔진/플러그인 설치 및 빌드 규칙 상태를 먼저 복구해야 한다.

재시작 시 일부 VictorianStreet 재질은 `MatLayerBlend_Standard`의 float3/float4 타입 불일치 경고를 출력했다. 이번 배치 작업에서 재질 설정은 수정하지 않았다. 체크무늬/기본 재질이 보이면 별도 재질 수정이 필요하다.

Content 폴더는 프로젝트 Git 설정에서 제외되어 Unity Version Control로 관리된다. 코드와 문서는 Git에 나타나지만 그래프/맵은 해당 에셋 관리 경로에서 별도로 확인해야 한다. 이번 작업에서 커밋/업로드는 수행하지 않았다.
