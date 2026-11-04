# Boris 몬스터 Blueprint 병합

작업일: 2026-11-04.

상태: 병합 완료. BP 비교·컴파일·저장, 중복 파일 제거, 테스트 참조 정리, 정규 빌드, 새 실행에서의 재로드 및 자동 테스트 검증을 마쳤다.

## 최종 대상

- 유지: `/Game/_Game/Blueprints/Characters/BP_TBMonster_Boris`
- 제거 완료: `/Game/_Game/Blueprints/Characters/BP_TBGASMonster_Boris`
- 유지할 상속 계층: `ATBMonster → BP_TBMonster → BP_TBMonster_Boris`

GAS 체력·피해·사망·Ability 초기화는 이미 `ATBMonster`에 통합되어 있다. 기존 Boris BP 원본을 최종 대상으로 유지하므로 해당 BP의 기존 배치와 스폰 참조도 유지한다.

## 비교 결과

두 BP 모두 멤버 변수 추가가 없고 EventGraph/ConstructionScript의 DSL 내용이 동일했다. 그래프는 BeginPlay·Overlap·Tick·ConstructionScript를 부모로 전달한다.

다음 기본값의 전체 속성 이름과 값도 자기 자신의 BP 경로 차이를 정규화하면 모두 동일했다.

- Actor: GAS 초기화·피해 GE·Ability 목록, AIController, 자동 빙의와 네트워크 설정 포함
- MonsterASC
- MonsterAttributes
- Mesh: Boris 메시·재질·애니메이션 설정 포함
- CharacterMovement
- CapsuleComponent

고유 기능이나 기본값 차이가 없어 추가 그래프/설정 복사가 필요하지 않았다. 비교 결과는 `Saved/Automation/MonsterGAS/BPMerge/inspection.json`, `defaults.json`, `differences.json`에 보관했다.

중복 GAS BP의 에셋 참조자는 0개였다. 기존 Boris BP의 참조자는 `/Game/Developers/Dev_Choi/Maps/L_MonsterAITest`이며, 이 맵의 기존 참조를 유지한다. 에셋 참조 교체가 필요한 맵은 발견되지 않았다.

## 변경 파일

| 파일 | 변경 |
|---|---|
| `Content/_Game/Blueprints/Characters/BP_TBMonster_Boris.uasset` | 최종 BP 컴파일·저장. 기존 상속 구조 및 기본값 유지 |
| `Content/_Game/Blueprints/Characters/BP_TBGASMonster_Boris.uasset` | 고유 기능·에셋 참조가 없는 중복 BP 제거 |
| `Source/Develrocket_TeamProj/Tests/TBMonsterGASTests.cpp` | 제거할 GAS BP를 검사 목록에서 제외. 기본 몬스터와 기존 Boris BP 검사는 유지 |
| `Docs/Boris_BP_Merge.md` | 본 보고서 신규 생성 |
| `Docs/Monster_GAS_Integration.md` | 후속 BP 병합 보고서 안내 추가 |
| `Docs/Monster_GAS_Foundation.md` | 최종 BP 병합 보고서 안내 추가 |

이번 병합에서는 TBMonster, AttributeSet, GameplayEffect, GameplayTag의 구현 코드를 수정하지 않는다. Content는 Unity Version Control 관리 대상이며 커밋·푸시·체크인을 수행하지 않는다.

## 백업·오류·검증

- `Saved/CodexBackups/BorisBPMerge/`: BP 3개, 관련 맵 2개와 테스트 소스의 작업 전 디스크 사본 및 해시 manifest.
- `Saved/CodexBackups/BorisBPMerge/EditorSavedState/`: 비교 후 두 Boris BP의 현재 에디터 상태를 저장한 사본. 원본의 미저장 설정도 보관하기 위해 별도로 생성했다.
- `Saved/Automation/MonsterGAS/BPMerge/`: 조회용 로컬 MCP 보조 코드, 비교 JSON, 최종 정리 스크립트와 검증 결과.
- 실행 중인 에디터에서 중복 BP를 삭제할 때 패키지를 해제하지 못해 삭제 API가 false를 반환했다. 에디터가 종료되지는 않았다. 새 실행의 삭제 API는 true를 반환했지만 디스크 파일이 남아 재로드 검사에 실패했다.
- 우회 처리로 에디터가 모두 종료된 상태, 에셋 참조 0개, 파일의 저장본 백업과 SHA-256 일치를 확인한 후 남은 중복 `.uasset` 하나만 제거했다. 관련 맵·외부 액터·다른 에셋은 파일 탐색기나 셸로 변경하지 않았다. 새 에디터 실행에서 에셋 레지스트리를 재검색해 제거 결과를 검증한다.
- 에디터 종료 자동 승인이 미저장 내용 손실 가능성을 이유로 처음 차단됐다. 현재 맵과 관련 BP 4개의 dirty=false 및 다른 열린 에셋이 없음을 API로 확인한 후 정상 종료 재검토를 통과했다. 사용자도 저장·종료를 승인했다. 종료되지 않은 에디터가 DLL을 사용해 한 차례 링크가 실패했으나, 정상 종료 후 정규 빌드는 성공했다.

최종 삭제·재로드 확인 결과는 `Saved/Automation/MonsterGAS/BPMerge/merge_result.json`, 자동 테스트 결과는 같은 폴더의 `index.json`에 기록한다. 검증 전 상태를 완료로 간주하지 않는다.

## 최종 검증 결과

- 정규 Development Editor 빌드 성공.
- 새 에디터 실행에서 중복 BP의 에셋 등록과 디스크 파일이 모두 없음을 확인했다. 옛 경로에 리다이렉터도 남기지 않았다.
- 기존 Boris BP를 다시 로드·컴파일하고, Actor/ASC/AttributeSet/Mesh/Movement/Capsule의 전체 조회 기본값이 병합 전과 동일함을 확인했다.
- `BlueprintCompatibility`, `InitializationDamageAndDeath` 자동 테스트 2개 성공, 실패 0. 기존 GameplayCueNotifyPaths 미설정 경고 1건이 포함됐다.
- 기본 몬스터와 기존 Boris BP를 독립 게임 월드에 실제 스폰해 전용 ASC, 초기 체력 100, 피해 40 후 60, 치명 피해 후 사망을 확인했다. 이번 BP 정리 후 2인 PIE는 재실행하지 않았다.
- `BP_TBMonster.uasset`, `L_MonsterAITest.umap`, `PCG_Test.umap`의 SHA-256은 작업 전과 동일하다. 기존 배치·맵의 참조를 바꾸거나 저장하지 않았다.
- 중복 BP의 Unity Version Control 상태가 미제출 추가 + 로컬 삭제(AD+LD)였으므로 해당 파일 한 개의 추가 예약만 취소했다. 원래 BP의 변경이나 다른 파일의 변경은 되돌리지 않았다. 체크인은 하지 않았다.
- 에디터는 저장 상태를 확인한 후 정상 종료한 상태로 두었다.

빌드/테스트 및 정리 로그는 `Saved/Logs/BorisBPMerge.log`, `Saved/Logs/BorisBPMergeTests.log`에 보관했다.
