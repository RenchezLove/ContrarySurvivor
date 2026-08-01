// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "WolfCharacter.generated.h"

class UStatsComponent;
class UCorpseLootComponent;
class UAnimSequence;
class AMasterInventoryItem;
class APickup;
class USoundBase;

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
	// meta DisplayPriority — поднять наши настройки наверх Details (фидбек Рината), сразу после Transform.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats", meta = (AllowPrivateAccess = "true", DisplayPriority = "1"))
	UStatsComponent* Stats;

	// --- Анимации (Single Node, без AnimBP). Тюнингуются/переопределяются в редакторе. ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (DisplayPriority = "2"))
	UAnimSequence* IdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	UAnimSequence* RunAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	UAnimSequence* BiteAnim;

	// Анимация смерти «ложится на бок» (Build 1.2, Ринат: «волки - на бок»). Проигрывается
	// один раз без цикла, поза застывает на последнем кадре. Не задана/ассета нет — прежнее
	// поведение (заморозка позы на текущем кадре).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (DisplayName = "Анимация смерти"))
	UAnimSequence* DeathAnim;

	// Порог скорости (см/с) для переключения Idle<->Run. DRAFT.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	float RunSpeedThreshold = 10.0f;

	// --- Звук атаки волка (Демо) ---
	// При укусе проигрывается СЛУЧАЙНЫЙ рык из списка. Дефолты грузятся в конструкторе
	// из /Game/Audio/Demo/wolf_growl_* через FObjectFinder; можно переопределить в BP.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (DisplayPriority = "3"))
	TArray<USoundBase*> AttackGrowlSounds;

	// Громкость рыка. Тюнингуется.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0.0"))
	float AttackGrowlVolume = 0.7f;

	// Стартовое HP волка. ЧЕРНОВОЕ значение (draft, GDD §7.1).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float WolfMaxHealth = 40.0f;

	// Множитель скорости относительно базовой скорости бандита (~600). ЧЕРНОВОЕ (draft).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (DisplayPriority = "4"))
	float SpeedMultiplierVsBandit = 1.3f;

	// Через сколько секунд после смерти убрать тело. Build 1.2.1 (ТЗ А1): труп теперь
	// ОБЫСКИВАЕТСЯ (шкура/деньги внутри), поэтому лежит дольше — дефолт 60 с (было 5).
	// Не обысканный остаток исчезает вместе с трупом.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death", meta = (ClampMin = "1.0", DisplayPriority = "5"))
	float CorpseLifeSpan = 60.0f;

	// Контейнер лута трупа (Build 1.2.1, ТЗ А1): смерть кладёт шкуру и деньги СЮДА
	// (мешок-пикап убран), игрок забирает через окно обыска «Обыскать [E]».
	// Живёт в мастер-классе волка — BP-наследники получают механизм автоматически.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
	UCorpseLootComponent* CorpseLoot;

	// --- Лут при смерти (GDD §7.8). Волк DRAFT: деньги 5-15, ниже шанс предмета, чем у бандита. ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (DisplayPriority = "6"))
	float LootMoneyMin = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	float LootMoneyMax = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LootItemDropChance = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TSubclassOf<AMasterInventoryItem> LootItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TSubclassOf<APickup> PickupClass;

	// --- Квестовый дроп (Фаза 5): волк ВСЕГДА роняет «Шкуру волка» ---
	// Класс гарантированного квестового предмета (dropChance=1.0). По умолчанию — расходник-
	// плейсхолдер (как и обычный лут), чтобы дроп работал без BP/редактора; можно переопределить
	// на реальный BP-предмет шкуры. Имя предмета задаётся QuestLootItemName.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Quest")
	TSubclassOf<AMasterInventoryItem> QuestLootItemClass;

	// СЛУЖЕБНЫЙ КЛЮЧ квестового предмета. ОБЯЗАН посимвольно совпадать с RequiredItemName
	// квеста старосты (ElderNPC.cpp:53,91) — иначе шкура не засчитается. НЕ переводить,
	// значение не менять (ADR-050, порция 0).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Quest")
	FString QuestLootItemName = TEXT("Шкура волка");

	// ПЕРЕВОДИМОЕ название той же шкуры, которое видит игрок в рюкзаке.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot|Quest")
	FText QuestLootItemText = NSLOCTEXT("Items", "WolfPelt", "Шкура волка");

	// Кладёт лут (гарантированную шкуру + деньги) В ТРУП (CorpseLoot) — Build 1.2.1,
	// ТЗ А1: мешок-пикап с трупов убран. Вызывается из HandleDeath.
	void DropLoot();

	// --- Капсула коллизии волка (квадрупед). DRAFT ---
	// Дефолт ACharacter (r34/hh88) рассчитан на стоящего ГУМАНОИДА — для волка он
	// слишком высокий (меш «парит»). Задаём размеры под квадрупеда: half-height ~ половина
	// высоты в холке (~75-80 см → hh40), radius ~ полширины корпуса. Значения чистовые
	// подберём по факту в редакторе; меш садится на дно капсулы (RelativeLocation.Z = -hh).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision", meta = (DisplayPriority = "7"))
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

	// Проиграть случайный рык атаки (Демо) в позиции волка.
	void PlayAttackSound();
};
