#include "AbilitySystem/Abilities/Player/TBGameplayAbility_Reload.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/TBPlayerAttributeSet.h"
#include "GameplayEffect.h"
#include "TBGameplayTags.h"

UTBGameplayAbility_Reload::UTBGameplayAbility_Reload()
{
    SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat_Reload));

    // 소유 클라이언트에서 몽타주 예측 재생
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // 재장전 완료 시 서버에서 비용 적용
    bCommitOnActivate = false;

    // 재장전의 상태 및 실행 제한
    ActivationOwnedTags.AddTag(TBGameplayTags::State_Combat);

    ActivationBlockedTags.AddTag(TBGameplayTags::State_Combat);
    ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
    ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
}

bool UTBGameplayAbility_Reload::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
    const UAbilitySystemComponent* ASC =
        ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

    const UTBPlayerAttributeSet* Attributes =
        IsValid(ASC) ? ASC->GetSet<UTBPlayerAttributeSet>() : nullptr;

    if (!Attributes || !GetCostGameplayEffect())
    {
        return false;
    }

    // 현재 탄창에서 추가로 채울 수 있는 양 계산
    const float MissingAmmo =
        Attributes->GetMaxLoadedBlood() - Attributes->GetLoadedBlood();

    // 탄창에 빈 공간이 있고 보유 혈액이 있을 때 허용
    return MissingAmmo > KINDA_SMALL_NUMBER
        && Attributes->GetBlood() > KINDA_SMALL_NUMBER;
}

void UTBGameplayAbility_Reload::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    // 재장전 자원 변경은 서버에서만 확정
    if (!ActorInfo || !ActorInfo->IsNetAuthority())
    {
        return;
    }

    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

    const UTBPlayerAttributeSet* Attributes =
        IsValid(ASC) ? ASC->GetSet<UTBPlayerAttributeSet>() : nullptr;

    const UGameplayEffect* ReloadEffect = GetCostGameplayEffect();

    if (!Attributes || !ReloadEffect)
    {
        return;
    }

    // 재장전 완료 시점의 탄창 부족량 계산
    const float MissingAmmo =
        Attributes->GetMaxLoadedBlood() - Attributes->GetLoadedBlood();

    // 보유 혈액을 초과하지 않는 범위에서 충전량 결정
    const float ReloadAmount =
        FMath::Min(MissingAmmo, Attributes->GetBlood());

    if (ReloadAmount <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    // 이번 재장전에 사용할 GE Spec 생성
    FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(ReloadEffect->GetClass(), GetAbilityLevel(Handle, ActorInfo), MakeEffectContext(Handle, ActorInfo));

    if (!Spec.IsValid())
    {
        return;
    }

    // 보유 혈액 소모 및 탄창 충전량 전달
    Spec.Data->SetSetByCallerMagnitude(TBGameplayTags::Data_Reload_BloodCost, -ReloadAmount);
    Spec.Data->SetSetByCallerMagnitude(TBGameplayTags::Data_Reload_LoadedGain, ReloadAmount);

    const FActiveGameplayEffectHandle AppliedHandle =
        ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

    // 재장전 적용 성공 시 실제 장전량 저장
    if (AppliedHandle.WasSuccessfullyApplied())
    {
        ASC->SetNumericAttributeBase(UTBPlayerAttributeSet::GetAmmoAtLastReloadAttribute(), Attributes->GetLoadedBlood());
    }
}

void UTBGameplayAbility_Reload::HandleActionMontageCompleted()
{
    // 클라이언트는 서버의 재장전 확정 및 종료를 대기
    if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
    {
        return;
    }

    // 완료 시 비용 및 충전 상태 재검사
    if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    // 공통 정상 종료 처리
    Super::HandleActionMontageCompleted();
}