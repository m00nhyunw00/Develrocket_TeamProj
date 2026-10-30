# 블록 단위 건물 패킹 구현 및 검증

작성일: 2026-10-30

## 구현

- `Source/Develrocket_TeamProj/PCG/CityBlockPacking.h/.cpp`: `UCityBlockPackingSettings` / `City Block Packing` 노드.
- `CityPackingAlgorithm.h`: 기존 MaxRects 계산을 공용 코드로 이동. 기존 `CityPacking.cpp`도 이를 사용하며, 기존 클리어런스·재현 테스트를 통과했다.
- `Blocks` 입력은 수평·단위 Scale의 블록 포인트, `Buildings` 입력은 카탈로그 Attribute Set이다.
- 필수 카탈로그 속성: `BuildingClass`, `BoundsMin`, `BoundsMax`, `GroundOffsetZ`. `Weight`는 기본값 1이며 0 이하는 제외한다. 양수 값은 배치 점수와 반복 선택 페널티의 상대 선호도에 사용한다. 정확한 확률 분포를 보장하는 가중 추첨은 아니다.
- 카탈로그 정렬을 고정하고 PCG 실행 시드와 `BlockID`를 결합하여 블록별 패킹 시드를 만든다. 안정적인 재현에는 같은 BlockID, 카탈로그, 설정도 필요하다.
- 0/90/180/270도 회전, 회전된 비대칭 피벗의 XY 보정, 사용자 지정 GroundOffsetZ 적용을 수행한다.
- `Out`은 생성용 피벗 Transform과 실제 건물 Bounds를 가진 포인트다. `BuildingClass`, `BlockID`, `LotID`, `BlockSeed`, `SelectionSeed`, `QuarterTurns`, `BlockCoverage`를 포함한다.
- `Footprints`는 각 배치의 XY 점유 사각형을 표면에 얇게 표시하기 위한 별도 포인트 출력이다. 남은 공간 데이터는 아니다.
- `GroundOffsetZ`를 적용한 최종 Transform이므로 이후 기존 Add/XY Subtract 보정을 다시 적용하면 안 된다.
- 건물 전체를 새로 생성하거나 건물 메시를 변형하는 노드가 아니다. 카탈로그의 고정 크기 건물을 블록 내부에 배치한다.
- 상위 블록·도로는 현재 기존 분할 규칙을 유지한다. 건물에 맞춰 바뀌는 것은 블록 내부 점유 영역이다.

## 준비한 그래프

`/Game/Developers/Dev_Seo/_PCG/_Hierarchical/PCG_CityGen/PCG_Block_Packing`

- `01_BuildingCatalog`: DT_CityBuildings, Attribute Set.
- `02_BlockPacking`: 간격 250cm, EdgeInset 0, LayoutAttempts 8, 90도 회전 허용.
- `MaxBuildingsPerBlock`은 실제 그래프에서 1로 두었다. 포인트 검증 후 상한을 늘린다. 상한이므로 해당 개수까지 반드시 생성하는 설정은 아니다.
- `03_SpawnPackedBuildings`: No Merging, Spawn By Attribute BuildingClass, 템플릿 편집 꺼짐.
- 노드와 내부값은 Codex, 선 연결은 사용자 담당이라는 현재 합의를 유지했다.
- 마지막 확인 시 사용자가 Input → Blocks 및 카탈로그 → Buildings 연결을 진행한 상태였다. 02의 Out은 아직 연결되지 않았다. 03 → Output은 연결돼 있었다. 다음 작업에서는 현재 연결을 다시 읽는다.

권장 다음 확인:

1. 먼저 기존 03 → Output 연결을 끊고 02의 Out → Output으로 직접 연결하여 포인트만 검사한다.
2. 상위 City Master의 블록 Loop에서 새 PCG_Block_Packing을 사용한다. 고정 필지를 만드는 PCG_Block_Layout을 거치지 않아야 원래 65m 블록을 받는다.
3. 블록당 상한 1에서 출력 4개 확인 후 8 등으로 늘려 결과를 확인한다.
4. 이후 02의 Out → 03의 In → Output으로 바꾸어 건물을 생성한다. Footprints는 진단용으로 별도 연결 가능하다.

## 실제 검증

- UBT 로그 `C:/Users/user/AppData/Local/UnrealBuildTool/Log.txt`: CityBlockPacking.cpp, CityPacking.cpp, generated module 컴파일 및 `Result: Succeeded`.
- 에디터 로그: Live Coding 성공 및 신규 CityBlockPackingSettings 등록.
- Rider build_solution_state는 진단 없이 실패로 반환했지만, 실제 UBT·에디터·클래스 조회에서 성공을 확인했다. 현재 반영 방식은 Live Coding이다. 에디터 재시작/패키징 전에는 정규 Editor 빌드가 필요하다.
- Automation `Project.PCG.CityBlockPacking.RotatedPivotAndGround`: 성공. 비대칭 Bounds, 네 회전, 이동·회전된 블록 기준에서 접지 높이 및 회전 경계 확인.
- Automation `Project.PCG.CityPacking.ClearanceAndBounds`: 성공. 기존 패킹 경계·간격·시드 재현 회귀 검사.
- 별도 transient 그래프와 임시 PCG Volume으로 실제 PCG 노드를 실행했다. Spawn Actor 노드는 없었다.
- 테스트 입력은 현재 도시와 같은 65m 블록 4개, 제공된 A/B 카탈로그다.
- 상한 1: 총 4포인트, 각 블록 1개.
- 상한 8: 총 12포인트, 각 블록 3개. A 4개 / B 8개.
- 모든 배치의 회전된 XY Bounds가 해당 블록 내부에 있고, 같은 블록 내 건물 사이 250cm 이상 간격을 만족했다.
- 카탈로그 GroundOffsetZ를 접지 기준으로 사용했을 때 표면 Z 일치를 확인했다. 이는 새로운 시각적 접지면 측정이 아니다.
- 같은 시드 42 재생성: Transform·회전·카탈로그 크기·보정값 일치.
- 시드 43: 다른 배치 확인, 동일한 경계·간격 검사 통과.
- 실제 새 패킹 방식의 BP 생성/PIE/멀티플레이 검증은 아직 하지 않았다.
- 임시 테스트 액터 제거 확인. 기존 생산 그래프의 B 건물 8개는 유지됐다.

증거:

- `Saved/PCGCity/validate_block_packing_20261030.py`
- `Saved/PCGCity/block_packing_validation_20261030.json`

## 제약

- 박스 Bounds 기반 보수적인 패킹이다. 비정형 벽면 사이의 정확한 거리가 아니다.
- 전체 건물 Bounds에는 접지면 아래 구조가 포함될 수 있다. 접지 보정은 GroundOffsetZ를 별도로 사용한다.
- 임의 각도 회전·비수평 블록·비단위 Scale는 지원하지 않는다. Floor 크기 Scale은 상위 Apply Scale To Bounds로 처리한다.
- 도로 방향에 대한 출입구 정렬, 잔여 공간 장식, 건물에 따라 상위 블록 크기 자체를 바꾸는 기능은 이번 구현에 포함되지 않는다.
