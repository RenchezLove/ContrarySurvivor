// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Animation/AnimNotify_MeleeHit.h"
#include "AMeleeWeapon.h"
#include "ContrarySurvivor/Characters/MasterHumanoidCharacter.h"
#include "Components/SkeletalMeshComponent.h"

UAnimNotify_MeleeHit::UAnimNotify_MeleeHit()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(220, 60, 60); // красная метка на дорожке — момент попадания
#endif
}

FString UAnimNotify_MeleeHit::GetNotifyName_Implementation() const
{
	return TEXT("Удар ножом");
}

void UAnimNotify_MeleeHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	// Гуманоид модульный: монтаж играет ведущий меш, его владелец — сам персонаж.
	AMasterHumanoidCharacter* Wielder = Cast<AMasterHumanoidCharacter>(MeshComp->GetOwner());
	if (!Wielder)
	{
		return;
	}

	// Холодное оружие, которым носитель замахнулся. Обычно это оружие в руках (за время
	// замаха его могли сменить — тогда удара нет); игрок бьёт ножом и с огнестрелом в руках
	// (отдельная кнопка, 10.10.2026), поэтому нож спрашиваем у самого носителя.
	AMeleeWeapon* Melee = Wielder->GetSwingMeleeWeapon();
	if (!Melee)
	{
		return;
	}

	// Урон наносим ТОЛЬКО если этот замах начало само оружие (AMeleeWeapon::Fire). Ту же
	// анимацию удара крутит и бандит, но его урон считает ИИ отдельно — без этой проверки
	// бандит бил бы дважды.
	if (!Melee->ConsumePendingSwing())
	{
		return;
	}

	Melee->ApplyMeleeDamage();
}
