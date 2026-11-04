# TBMonster GAS 통합

> 이 문서는 C++ 클래스 통합 시점의 기록이다. 후속 Boris BP 정리의 최종 상태는 [Boris BP 병합 보고](Boris_BP_Merge.md)를 따른다.

작업일: 2026-11-04. 사용자의 통합 요청에 따라 `TBGASMonster`의 기능을 `TBMonster`로 옮겼다.

## 현재 구조와 영향

`ATBCharacterBase → ATBMonster → 몬스터 Blueprint` 구조로 정리했다. 별도의 `ATBGASMonster` 네이티브 클래스는 제거했다.

`TBMonster`가 `UTBAbilitySystemComponent`, `UTBMonsterAttributeSet`, 초기 체력 GE, 피해 GE, 사망 처리, 초기 Ability 목록을 직접 관리한다. 따라서 기존 `TBMonster` 기반 BP와 해당 BP로 스폰하거나 배치한 몬스터 전체에 GAS 기반 체력·피해·사망 기능이 적용된다. 플레이어 및 공용 캐릭터 코드는 수정하지 않았다.

기존 이동·메시·AIController·자동 빙의 기본 설정은 유지했다. 기존 MeleeAbilityClass와 StartupAbilities를 같은 초기화 경로에서 한 번만 부여하며, 부여한 Ability는 정리 경로에서도 함께 관리한다.

## 이번 통합에서 수정·제거한 파일

| 파일 | 변경 |
|---|---|
| `Source/Develrocket_TeamProj/Character/Monster/TBMonster.h` | GAS 설정, 체력/피해/사망 API와 복제 상태를 통합. MonsterASC 타입을 프로젝트 ASC로 변경 |
| `Source/Develrocket_TeamProj/Character/Monster/TBMonster.cpp` | ASC/AttributeSet 생성, ActorInfo, 초기 GE/Ability, 피해·사망·복제·종료 처리 통합 |
| `Source/Develrocket_TeamProj/Character/Monster/TBGASMonster.h` | 제거. 통합 전 파일은 Saved 백업에 보관 |
| `Source/Develrocket_TeamProj/Character/Monster/TBGASMonster.cpp` | 제거. 통합 전 파일은 Saved 백업에 보관 |
| `Source/Develrocket_TeamProj/Tests/TBMonsterGASTests.cpp` | 네이티브 테스트 대상을 TBMonster로 변경. 기존 BP 3개의 로드·ASC·체력·피해·사망 및 옛 클래스 참조 전환 검사 추가 |
| `Config/DefaultEngine.ini` | 옛 TBGASMonster 클래스 및 사망 델리게이트 서명에 대한 Core Redirect 2줄 추가 |
| `Content/_Game/Blueprints/Characters/BP_TBGASMonster_Boris.uasset` | TBMonster를 직접 부모로 참조하도록 컴파일·저장. 에셋 경로와 이름은 유지 |
| `Docs/Monster_GAS_Foundation.md` | 이전 분리 구현 기록임을 명시하고 현재 통합 보고서 링크 추가 |
| `Docs/Monster_GAS_Integration.md` | 현재 통합 보고서 신규 생성 |

Core Redirect는 옛 클래스 이름으로 저장된 BP/클래스/함수 참조를 새 클래스로 연결하기 위한 설정이다. 남아 있는 옛 이름 문자열은 별도 구현 클래스를 뜻하지 않는다.

## 기존에 만든 별도 GAS 파일

다음 6개 소스는 별도 책임을 유지하며 이번 통합에서는 수정하지 않았다.

- `AbilitySystem/Attributes/TBMonsterAttributeSet.h/.cpp`: Health, MaxHealth, IncomingDamage, 체력 제한과 복제.
- `AbilitySystem/Effects/TBMonsterGameplayEffects.h/.cpp`: 초기화·피해·사망 효과.
- `AbilitySystem/TBMonsterGameplayTags.h/.cpp`: `Data.Damage` 태그.

위 경로는 `Source/Develrocket_TeamProj` 기준이다. 기능 사용은 몬스터 서버에서 `ApplyGASDamage(Damage, DamageSource)` 호출, BP 기본값에서 StartupAbilities/InitialAttributesEffect 설정, 사망 연출을 On Death/OnMonsterDied에 연결하는 방식이다. ApplyGASDamage는 RPC가 아니며 클라이언트 호출을 서버로 전달하지 않는다.

## 검증 기록

- 정규 Development Editor 컴파일·링크 성공. 실행 중인 에디터가 All Saved인 상태를 확인해 정상 종료한 후 빌드했다.
- 통합 후 에셋 검증 성공: `Saved/Automation/MonsterGAS/Integration/Assets.json`. BP 세 개 모두 전용 ASC/AttributeSet을 사용한다. 기존 BP 두 개와 PCG_Test 맵의 파일 해시는 통합 직전과 동일하다.
- 통합 후 자동 테스트 2개 성공, 실패 0: `Saved/Automation/MonsterGAS/Integration/index.json`. `InitializationDamageAndDeath`, `BlueprintCompatibility`를 실행했다. GameplayCueNotifyPaths 미설정 경고가 1건 포함됐다.
- 검증 범위: 초기화·피해·사망·재빙의·Ability 중복 방지, 기존 몬스터 BP 3개의 새 ASC와 기본 체력/피해/사망, 기존 클래스 이름 전환.
- 최초 분리 구현 당시에는 실제 PCG_Test 맵의 2인 PIE에서 100→60→0 체력·사망 복제와 클라이언트 직접 피해 거부를 확인했다. 이번 통합 후에는 해당 PIE 검증을 다시 실행하지 않았다. 이전 결과를 이번 통합의 네트워크 실행 증거로 간주하지 않는다.

공격 패턴·무기 판정 연결·보스 한 마리 스폰 규칙·체력 UI·사망 애니메이션은 이번 통합 범위에 추가하지 않았다.

## 백업과 보조 파일

- `Saved/CodexBackups/MonsterGAS_Integration/`: 통합 직전 소스, 설정, 보고서, 몬스터 BP 3개와 PCG_Test 맵 백업. `manifest.json`에 각 파일 해시 기록.
- `Saved/Automation/MonsterGAS/integrate_assets.py`: 새 BP 부모 전환/저장, 기존 BP 컴파일/타입 확인, 원본 BP·맵 해시 비교를 위한 보조 스크립트.
- `Saved/Logs/MonsterGASIntegrationAssets.log`, `Saved/Logs/MonsterGASIntegrationTests.log`: 이번 검증 로그.

Content는 Unity Version Control 관리 대상이다. 커밋·푸시·체크인은 수행하지 않았다.
