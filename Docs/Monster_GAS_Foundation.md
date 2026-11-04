# 몬스터 GAS 기반 구현 보고

> 후속 Boris BP 정리의 최종 상태는 [Boris BP 병합 보고](Boris_BP_Merge.md)를 따른다.

> 이 문서는 최초 분리 구현 단계의 기록이다. 이후 사용자 요청으로 기능을 `TBMonster`에 통합했다. 현재 구조와 최종 변경 목록은 [몬스터 GAS 통합 보고](Monster_GAS_Integration.md)를 따른다.

작업일: 2026-11-04. 범위: 체력, 피해, 사망, 초기 Ability 부여.

## 변경 대상과 영향

기존 `ATBMonster`, `ATBCharacterBase`, `UTBAbilitySystemComponent`, 플레이어 AttributeSet, 공용 `TBGameplayTags`의 최종 소스 변경은 없다. 처음에 수정했던 `TBMonster.h/.cpp`, `TBGameplayTags.h/.cpp`는 작업 시작 당시 내용으로 복원했다. Git의 기존 추적 파일 차이가 없는 상태를 확인했다.

새 `ATBGASMonster : ATBMonster`와 새 BP에만 기능을 추가했다. 기존 몬스터 BP와 레벨에 배치된 기존 몬스터는 자동으로 교체되지 않는다.

## 새 소스 파일 전체

경로 기준: `Source/Develrocket_TeamProj/`.

| 파일 | 내용 |
|---|---|
| `Character/Monster/TBGASMonster.h` | 체력 조회, 서버 피해 함수, 사망 이벤트, 초기 GE/Ability 설정, 사망 상태 복제 선언 |
| `Character/Monster/TBGASMonster.cpp` | 기존 MonsterASC를 프로젝트 ASC 타입으로 생성, ActorInfo 연결, 서버 초기화, 피해 GE 적용, 사망·AI 정지·이동/충돌 차단 |
| `AbilitySystem/Attributes/TBMonsterAttributeSet.h` | Health, MaxHealth, IncomingDamage와 RepNotify 선언 |
| `AbilitySystem/Attributes/TBMonsterAttributeSet.cpp` | 기본 체력 100, 체력 제한, 피해 계산, Health/MaxHealth 복제, 체력 소진 통지 |
| `AbilitySystem/Effects/TBMonsterGameplayEffects.h` | 초기화·피해·사망 GE 클래스 선언 |
| `AbilitySystem/Effects/TBMonsterGameplayEffects.cpp` | 초기 체력/최대 체력 설정, SetByCaller 피해, State.Dead를 부여하는 지속 효과 |
| `AbilitySystem/TBMonsterGameplayTags.h` | 몬스터 피해용 네이티브 태그 선언 |
| `AbilitySystem/TBMonsterGameplayTags.cpp` | `TBMonsterGameplayTags::Data_Damage` → `Data.Damage` 등록 |
| `Tests/TBMonsterGASTests.cpp` | 독립 게임 월드에서 초기화·피해·회복 제한·재빙의·Ability 중복·사망을 검증하는 자동 테스트 |

소스 9개를 새로 만들었다. `Build.cs`, `.uproject`, 프로젝트 공용 설정은 수정하지 않았다.

## 기존 에셋과 로컬 설정 변경

- `Content/Developers/Dev_Seo/PCG_Test.umap`: 원본 맵에서 검증용 몬스터 한 개를 임시 배치해 PIE를 실행했다. 검증용 액터가 저장된 상태를 별도 보관한 뒤, 사용자의 저장 승인에 따라 해당 액터만 제거해 최종 저장했다. 저장한 맵을 다시 열어 검증용 액터가 0개임을 확인했다. 맵의 최종 파일 해시는 작업 시작 시 저장본과 다르므로 변경된 에셋에 포함한다. 기존 몬스터를 새 GAS 몬스터로 일괄 교체하지 않았다.
- `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`: 검증 중 Listen Server / 2인으로 바뀌었던 PIE 설정을 기존 Standalone / 1인으로 복원했다. 테스트 직후 설정 사본은 `Saved/Automation/MonsterGAS/PIESettingsAfterTest.ini`에 보관했다.
- 최종 맵 정리 검증: `Saved/Automation/MonsterGAS/FinalMapCleanup.json`, `success: true`, 저장 후 재열기 시 `remaining_test_actors_after_reopen: []`.

## 새 에셋

`/Game/_Game/Blueprints/Characters/BP_TBGASMonster_Boris`

- 기존 `BP_TBMonster_Boris`를 복제하고, 새 BP의 부모만 `ATBGASMonster`로 변경했다.
- 새 BP의 MonsterASC 실제 타입이 `UTBAbilitySystemComponent`임을 확인하고 컴파일·저장했다.
- Boris 메시와 기존 `BP_TBMonsterAIController` 설정을 유지한다. 원본의 Anim Class가 None이어서 새 BP도 None이다.
- 복제 전후 원본 BP 파일의 SHA-256이 동일함을 확인했다.
- 실제 파일: `Content/_Game/Blueprints/Characters/BP_TBGASMonster_Boris.uasset`.
- Content는 Git 제외 대상이며 프로젝트의 Unity Version Control 관리 대상이다. 체크인하지 않았다.

## 사용과 흐름

1. 새 BP를 배치하거나 서버에서 스폰한다. 기존 BP를 사용하면 새 GAS 기능은 적용되지 않는다.
2. 부모의 MonsterASC 생성 시 `SetDefaultSubobjectClass`로 타입을 `UTBAbilitySystemComponent`로 지정한다. 따라서 공용 `GetTBAbilitySystemComponent()`가 새 몬스터에서 정상 동작한다.
3. 서버의 PossessedBy/BeginPlay에서 ActorInfo를 연결하고 초기 GE를 한 번 적용한 뒤 StartupAbilities를 부여한다. 클라이언트는 ActorInfo만 연결하고 수치를 복제받는다.
4. 공격 판정이 성공한 **서버 코드**에서 몬스터의 `ApplyGASDamage(Damage, DamageSource)`를 호출한다. 이 함수는 RPC가 아니며 클라이언트 호출을 서버에 전달하지 않는다.
5. 피해 GE가 `IncomingDamage`를 전달하면 AttributeSet이 Health를 차감하고 메타 피해 값을 0으로 초기화한다. 음수·0·비정상 피해는 거부한다.
6. Health가 0이면 서버가 사망을 확정한다. State.Dead 부여, Ability 취소와 전투 태그 차단, AI 논리/이동 정지, 이동/충돌 비활성화를 수행한다.
7. 복제되는 bIsDead의 OnRep가 클라이언트 사망 처리를 실행한다. BP의 `On Death`와 `OnMonsterDied`에 연출/후속 처리를 연결할 수 있다. 호출은 서버와 각 클라이언트에서 발생하므로 보상·진행 처리는 권한을 검사한다.

기본 Health/MaxHealth는 100이다. 다른 시작 수치가 필요하면 `TBGameplayEffect_MonsterInitialize`를 부모로 GE BP를 만들고 MaxHealth/Health Modifier를 설정한 뒤 새 몬스터 BP의 `Initial Attributes Effect`에 지정한다. Startup Abilities 배열에 몬스터용 GA 클래스를 설정한다. 기존 Melee Ability Class도 초기 목록에 포함하며 동일 클래스는 중복 부여하지 않는다.

사망 이후 일반 회복 GE로 부활하지 않는다. 자동 Destroy, 사망 애니메이션, 보스 UI, 공격/피격 판정, 플레이어 무기 연결, 보스 한 마리 스폰 규칙, 부활/풀링은 이번 범위에 추가하지 않았다. 초기 Ability 목록은 빈 상태이며 플레이어용 GA를 임의로 몬스터에 연결하지 않았다.

## 검증

- UE 5.8.4 Development Editor 정규 컴파일·링크 성공. 에디터를 닫은 상태에서 `-NoHotReloadFromIDE -NoLiveCoding`으로 빌드했다.
- `Project.GAS.Monster.InitializationDamageAndDeath`: 성공, 오류 0. 기존 프로젝트의 GameplayCueNotifyPaths 미설정 경고 1건이 기록됐다.
- 확인 항목: 전용 ASC 타입과 ActorInfo, 초기 100/100, Ability 중복 방지, 피해 40 후 체력 60, 음수/0 피해 거부, 재빙의 시 체력/Ability 유지, 회복 상한, MaxHealth 감소 시 Health 제한, 치명 피해 후 0/사망 태그/전투 차단/이동·충돌 정지, 사망 후 피해 거부 및 회복에 의한 부활 방지.
- 자동 테스트는 네이티브 몬스터의 서버 동작 검증이다.
- 실제 `PCG_Test` 원본 맵에서 새 Boris BP를 사용한 Listen Server + Client 2인 PIE 검증도 통과했다. 양쪽에서 Health `100 → 60 → 0`, 사망 상태, 이동 모드 None, 캡슐/메시 충돌 NoCollision을 확인했다. 클라이언트의 직접 피해 호출이 거부됨도 확인했다. 결과: `Saved/Automation/MonsterGAS/PIEReplication.json`.
- 공격 판정이나 사망 애니메이션을 연결한 실제 보스 전투는 아직 검증 대상이 아니다.
- 결과: `Saved/Automation/MonsterGAS/index.json`, `Saved/Logs/MonsterGASAutomation.log`.
- 새 BP 생성/타입/설정/원본 해시 검증: `Saved/Automation/MonsterGAS/BorisAsset.json`.

## 작업 중 오류와 보관 파일

새 리플렉션 타입을 Live Coding으로 반영하던 중 에디터가 종료됐다. 이후 정규 빌드로 전환했다. 별도 무인 검증에서 사망 GE 컴포넌트의 생성 방식 오류도 발견해 `CreateDefaultSubobject`로 수정했고, 이후 빌드와 자동 테스트가 통과했다.

종료 당시 `PCG_Test`가 미저장 상태였다. 다음 파일을 별도로 보관했고, 사용자의 원본 맵 사용 지시 및 최종 저장 승인에 따라 디스크 원본에서 검증과 정리를 마쳤다. 자동 저장 이후부터 비정상 종료 전까지의 미저장 변경은 복구됐다고 보장할 수 없다.

| 보관 파일 | 내용 |
|---|---|
| `Saved/CodexBackups/MonsterGAS_Recovery/PCG_Test_Disk.umap` | 2026-11-04 10:43:48 디스크 저장본 |
| `Saved/CodexBackups/MonsterGAS_Recovery/PCG_Test_Autosave_1058.umap` | 2026-11-04 10:58:03 최신 자동 저장본 |
| `Saved/CodexBackups/MonsterGAS_Recovery/PCG_Test_BeforeCleanup.umap` | 사용자 승인 후 테스트 액터 제거·저장 직전 맵 보관본 |
| `Saved/CodexBackups/MonsterGAS_AdditiveRefactor/TBMonster.h` | 새 클래스로 옮기기 전 작업 중간본 |
| `Saved/CodexBackups/MonsterGAS_AdditiveRefactor/TBMonster.cpp` | 새 클래스로 옮기기 전 작업 중간본 |
| `Saved/CodexBackups/MonsterGAS_AdditiveRefactor/TBGameplayTags.h` | 몬스터 전용 태그로 분리하기 전 작업 중간본 |
| `Saved/CodexBackups/MonsterGAS_AdditiveRefactor/TBGameplayTags.cpp` | 몬스터 전용 태그로 분리하기 전 작업 중간본 |

`Saved/Automation/MonsterGAS/PIESettingsBefore.json`에 PIE 확인 전 읽은 클라이언트 수/단일 프로세스/별도 서버 설정도 기록했다. 최종 정리용 보조 스크립트는 `Saved/Automation/MonsterGAS/finalize_map.py`, 저장·재열기 확인 결과는 `Saved/Automation/MonsterGAS/FinalMapCleanup.json`, 해당 실행 로그는 `Saved/Logs/MonsterGASFinalize.log`에 남긴다. 컴파일 산출물은 Binaries/Intermediate에, 실행 로그/자동 테스트 결과/충돌 기록은 Saved에 생성됐다. 소스·에셋 외의 작업 보조 산출물은 배포 기능에 포함되지 않는다.

이 보고서 자체의 신규 파일은 `Docs/Monster_GAS_Foundation.md`이다. 커밋·푸시·Unity Version Control 체크인은 수행하지 않았다.
