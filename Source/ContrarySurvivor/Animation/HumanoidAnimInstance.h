// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HumanoidAnimInstance.generated.h"

class AMasterHumanoidCharacter;

/**
 * Родительский класс анимационного блюпринта гуманоидов (ABP_HumanoidCharacter), Build 1.1.
 *
 * ЗАЧЕМ. Поза прицеливания должна подмешиваться к ходьбе только тогда, когда персонаж реально
 * целится: в руках дальнобойное оружие и есть цель. Решение о том, целимся мы или нет, и
 * плавность перехода — это игровая логика, ей место в C++, а не в графе мышкой. Граф берёт
 * отсюда ОДНО число — AimBlendWeight, и подаёт его в вес узла послойного смешивания.
 *
 * КАК ГРАФ ЕГО БЕРЁТ. Коммандлет `-run=PatchAnimBp -aim` ставит в граф узел послойного
 * смешивания и подключает к его входу веса узел чтения переменной AimBlendWeight. То есть
 * связь «код → граф» — обычная переменная, а не скрытая магия; её видно, открыв граф мышкой.
 *
 * ПОЧЕМУ ВЕС, А НЕ ПЕРЕКЛЮЧАТЕЛЬ. Резкое включение позы дёргает корпус. Здесь вес плавно
 * ползёт к 1 при прицеливании и к 0 при отпускании, скорости настраиваются раздельно
 * (вход в позу обычно чуть быстрее выхода).
 */
UCLASS()
class CONTRARYSURVIVOR_API UHumanoidAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UHumanoidAnimInstance();

	// Сила наложения позы прицеливания прямо сейчас: 0 — позы нет совсем, 1 — поза наложена
	// полностью. Это значение читает граф анимации. Считается каждый кадр, руками не ставится.
	UPROPERTY(BlueprintReadOnly, Category = "Прицеливание", meta = (DisplayName = "Сила позы прицеливания"))
	float AimBlendWeight = 0.0f;

	// --- Настройки Рината (EditAnywhere, наверх Details) ---

	// Общий выключатель наложения позы прицеливания.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Прицеливание", meta = (DisplayName = "Включить позу прицеливания", DisplayPriority = "1"))
	bool bEnableAimPose = true;

	// Насколько сильно поза накладывается в полностью прицеленном состоянии. 1 — как её сделал
	// моделлер, меньше — приглушённее. Это потолок, до которого дотягивается сила наложения.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Прицеливание", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayName = "Сила наложения", DisplayPriority = "2"))
	float AimPoseStrength = 1.0f;

	// Скорость входа в позу (единиц силы в секунду). Больше — быстрее вскидывает оружие.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Прицеливание", meta = (ClampMin = "0.1", DisplayName = "Скорость входа в позу", DisplayPriority = "3"))
	float AimBlendInSpeed = 8.0f;

	// Скорость выхода из позы (единиц силы в секунду). Меньше — дольше держит стойку после боя.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Прицеливание", meta = (ClampMin = "0.1", DisplayName = "Скорость выхода из позы", DisplayPriority = "4"))
	float AimBlendOutSpeed = 5.0f;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// Целится ли персонаж прямо сейчас: в руках дальнобойное оружие И у него есть захваченная
	// цель. Признак цели берём у самого оружия (ARangedWeapon::HasTarget) — он одинаково
	// работает и у игрока, и у бандита, потому что цель оружию ставят оба пути стрельбы.
	virtual bool IsAiming() const;

	// Владелец меша как гуманоид (или nullptr, если анимация играет не на нашем персонаже).
	AMasterHumanoidCharacter* GetHumanoidOwner() const;
};
