// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_MeleeHit.generated.h"

class UAnimSequenceBase;
class USkeletalMeshComponent;

/**
 * Уведомление «нож достал цель» — метка на дорожке анимации замаха (Build 1.1).
 *
 * ЗАЧЕМ. Без него урон ближнего боя наносится в момент НАЖАТИЯ КНОПКИ, то есть до того, как
 * персонаж успел взмахнуть. Игрок видит, что удар прилетел раньше движения. С этой меткой урон
 * считается ровно на том кадре анимации, где лезвие проходит через сектор — «в этом же секторе
 * должен наноситься урон» (формулировка Рината, пункт 5 плана Build 1.1).
 *
 * ПОЧЕМУ ОТДЕЛЬНЫЙ КЛАСС, А НЕ ИМЕНОВАННОЕ СОБЫТИЕ. Факт от оператора: скриптом можно поставить
 * на дорожку уведомление любого КЛАССА, а вот задать ИМЯ безымянному уведомлению из скрипта
 * нельзя — массив закрыт на запись. То есть метки этого класса оператор расставит по кадрам
 * автоматически, а именованные пришлось бы расставлять мышкой.
 *
 * ЧТО ДЕЛАЕТ. Находит владельца меша, берёт его экипированное оружие и, если это холодное
 * оружие, просит его нанести урон по сектору (AMeleeWeapon::ApplyMeleeDamage). Оружие сменили
 * посреди замаха или в руках пистолет — урона нет, без крашей.
 */
UCLASS(const, hidecategories = Object, collapsecategories, meta = (DisplayName = "Удар холодным оружием"))
class CONTRARYSURVIVOR_API UAnimNotify_MeleeHit : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_MeleeHit();

	//~ Begin UAnimNotify interface
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	//~ End UAnimNotify interface
};
