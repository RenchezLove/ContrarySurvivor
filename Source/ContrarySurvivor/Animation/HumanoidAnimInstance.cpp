// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Animation/HumanoidAnimInstance.h"
#include "ContrarySurvivor/Characters/MasterHumanoidCharacter.h"

UHumanoidAnimInstance::UHumanoidAnimInstance()
{
}

AMasterHumanoidCharacter* UHumanoidAnimInstance::GetHumanoidOwner() const
{
	// TryGetPawnOwner отдаёт пешку, на меше которой играет эта анимация.
	return Cast<AMasterHumanoidCharacter>(TryGetPawnOwner());
}

bool UHumanoidAnimInstance::IsAiming() const
{
	if (!bEnableAimPose)
	{
		return false;
	}
	const AMasterHumanoidCharacter* Owner = GetHumanoidOwner();
	return Owner && Owner->IsAimingAtTarget();
}

void UHumanoidAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Плавно ползём к нужной силе наложения: резкое включение позы дёргало бы корпус.
	// Вход и выход настраиваются раздельно — обычно оружие вскидывают быстрее, чем опускают.
	const bool bAiming = IsAiming();
	const float Target = bAiming ? FMath::Clamp(AimPoseStrength, 0.0f, 1.0f) : 0.0f;
	const float Speed = bAiming ? AimBlendInSpeed : AimBlendOutSpeed;

	AimBlendWeight = FMath::FInterpConstantTo(AimBlendWeight, Target, DeltaSeconds, FMath::Max(0.1f, Speed));
	AimBlendWeight = FMath::Clamp(AimBlendWeight, 0.0f, 1.0f);
}
