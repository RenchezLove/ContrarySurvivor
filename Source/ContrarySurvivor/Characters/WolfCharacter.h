// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "WolfCharacter.generated.h"

class UStatsComponent;
class UAnimSequence;

/**
 * Волк — второй враг MVP (GDD §7.1, стайный для квеста).
 *
 * АРХИТЕКТУРА (решение Фазы 3): волк — квадрупед с ЕДИНЫМ мешем (tech-design:
 * «Звери (волки) — единый меш», НЕ модульный гуманоид). Поэтому НЕ наследует
 * AMasterHumanoidCharacter (модульная база Head/Torso/Legs), а идёт напрямую от
 * ACharacter. Общий enemy-функционал переиспользуется через КОМПОЗИЦИЮ:
 *   - UStatsComponent (HP/смерть) — как у бандита;
 *   - AEnemyAIController-логика через подкласс AWolfAIController (chase/attack).
 * AI-контроллер берёт UStatsComponent обобщённо (FindComponentByClass), без привязки
 * к классу пешки.
 *
 * МЕШ/АНИМАЦИИ (Фаза 3, добивка): на наследуемый ACharacter::GetMesh()
 * (USkeletalMeshComponent) назначается реальный скелет-меш SK_Wolf. Анимации
 * проигрываются БЕЗ editor-AnimBP — через режим Single Node
 * (USkeletalMeshComponent::PlayAnimation): по скорости Idle/Run, плюс разовый Bite
 * по сигналу AI-контроллера (PlayBiteAnimation). Это минимальный надёжный путь без
 * графа AnimBP — реальный AnimBP/стейт-машину делаем позже при необходимости.
 *
 * ЧЕРНОВЫЕ статы (draft): HP 40, скорость ~1.3× бандита. Урон укуса/дальность — на
 * AWolfAIController.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AWolfCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AWolfCharacter();

	virtual void Tick(float DeltaTime) override;

	// Разовый запуск анимации укуса (вызывается AI-контроллером в момент атаки).
	// Перебивает Idle/Run на время длительности клипа, затем авто-возврат в локомоцию.
	void PlayBiteAnimation();

protected:
	virtual void BeginPlay() override;

	// Источник истины по HP волка (как у бандита — через компонент, минуя инлайн-логику).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats", meta = (AllowPrivateAccess = "true"))
	UStatsComponent* Stats;

	// --- Анимации (Single Node, без AnimBP). Тюнингуются/переопределяются в редакторе. ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	UAnimSequence* IdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	UAnimSequence* RunAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	UAnimSequence* BiteAnim;

	// Порог скорости (см/с) для переключения Idle<->Run. DRAFT.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	float RunSpeedThreshold = 10.0f;

	// Стартовое HP волка. ЧЕРНОВОЕ значение (draft, GDD §7.1).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float WolfMaxHealth = 40.0f;

	// Множитель скорости относительно базовой скорости бандита (~600). ЧЕРНОВОЕ (draft).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float SpeedMultiplierVsBandit = 1.3f;

	// Через сколько секунд после смерти убрать тело.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
	float CorpseLifeSpan = 5.0f;

	// --- Капсула коллизии волка (квадрупед). DRAFT ---
	// Дефолт ACharacter (r34/hh88) рассчитан на стоящего ГУМАНОИДА — для волка он
	// слишком высокий (меш «парит»). Задаём размеры под квадрупеда: half-height ~ половина
	// высоты в холке (~75-80 см → hh40), radius ~ полширины корпуса. Значения чистовые
	// подберём по факту в редакторе; меш садится на дно капсулы (RelativeLocation.Z = -hh).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
	float WolfCapsuleHalfHeight = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
	float WolfCapsuleRadius = 28.0f;

	UFUNCTION()
	void HandleDeath();

public:
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "Stats")
	UStatsComponent* GetStats() const { return Stats; }

private:
	// Текущий проигрываемый локомоторный клип (чтобы не рестартить PlayAnimation каждый кадр).
	UAnimSequence* CurrentLocomotionAnim = nullptr;

	// До какого времени (GetTimeSeconds) проигрывается укус; пока активно — локомоция не трогается.
	float BiteUntilTime = -1.0f;

	// Назначить локомоторный клип (Idle/Run) если он сменился; не трогает, если идёт укус.
	void UpdateLocomotionAnimation();

	// Проиграть клип в режиме Single Node (без AnimBP).
	void PlaySingleNode(UAnimSequence* Anim, bool bLooping);
};
