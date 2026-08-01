// Fill out your copyright notice in the Description page of Project Settings.

#include "WolfCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Components/CorpseLootComponent.h" // Build 1.2.1 (А1): лут в трупе
#include "ContrarySurvivor/Components/QuestComponent.h" // Фаза 5: засчёт убийства волка в квест
#include "ContrarySurvivor/Characters/PlayerCharacter.h" // #26: счётчик киллов игрока
#include "ContrarySurvivor/Controllers/WolfAIController.h"
#include "ContrarySurvivor/Actors/Pickup.h"
#include "ContrarySurvivor/HUD/ContrarySurvivorHUD.h" // D5: всплывающие цифры урона по врагам
#include "ContrarySurvivor/Debug/QADebug.h" // QA-лог гарантированного дропа шкуры
#include "AConsumableItem.h"
#include "AQuestItem.h" // Фаза 5: «Шкура волка» — квест-предмет (категория Quest, не теряется при смерти)
#include "Kismet/GameplayStatics.h" // GetPlayerPawn (поиск журнала квестов игрока)
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

// Базовая скорость бандита (дефолт CharacterMovementComponent::MaxWalkSpeed UE = 600).
// Используется как опорная для множителя скорости волка.
static constexpr float BanditBaseWalkSpeed = 600.0f;

AWolfCharacter::AWolfCharacter()
{
	// Тик нужен для смены Idle/Run по скорости (Single Node анимации без AnimBP).
	PrimaryActorTick.bCanEverTick = true;

	Stats = CreateDefaultSubobject<UStatsComponent>(TEXT("StatsComponent"));

	// Build 1.2.1 (ТЗ А1): контейнер лута трупа в мастер-классе волка — BP-наследники
	// получают обыск автоматически. Наполняется в DropLoot (из HandleDeath).
	CorpseLoot = CreateDefaultSubobject<UCorpseLootComponent>(TEXT("CorpseLoot"));

	// Лут по умолчанию (editor-независимо): расходник + базовый пикап.
	LootItemClass = AConsumableItem::StaticClass();
	PickupClass   = APickup::StaticClass();

	// Квестовый дроп: гарантированная «Шкура волка». КВЕСТ-предмет (AQuestItem, категория Quest):
	// не теряется при смерти игрока и не «съедается» из рюкзака; изымается при сдаче квеста старосте.
	// Тюнингуется на BP шкуры при необходимости.
	QuestLootItemClass = AQuestItem::StaticClass();

	// AI: волк управляется AWolfAIController (chase/attack), авто-поссесс при спавне.
	AIControllerClass = AWolfAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Капсула: переопределяем дефолт ACharacter (r34/hh88 — под гуманоида) на размеры
	// квадрупеда (DRAFT, см. WolfCapsule* в .h). InitCapsuleSize гарантирует hh>=radius.
	// Также блокируем Visibility, чтобы LineTrace дальнобоя игрока (ECC_Visibility)
	// попадал по волку (та же причина, что в фиксе боя бандита, Фаза 2).
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->InitCapsuleSize(WolfCapsuleRadius, WolfCapsuleHalfHeight);
		Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	// Реальный скелет-меш волка на наследуемый ACharacter::GetMesh() (Фаза 3).
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> WolfMeshAsset(TEXT("/Game/Characters/Wolf/SK_Wolf.SK_Wolf"));
		if (WolfMeshAsset.Succeeded())
		{
			MeshComp->SetSkeletalMeshAsset(WolfMeshAsset.Object);
		}
		// Выравнивание меша под капсулу: Z = -(half-height) ставит лапы на дно капсулы.
		// После re-export с ИСХОДНОЙ ориентацией волк forward=+Y (гуманоид forward=-Y),
		// т.е. развёрнут на 180° относительно гуманоида. Поэтому Yaw = +90 (было -90; +90 = -90+180):
		// волк смотрит вперёд по +X. Это rigid-поворот компонента — скиннинг/анимации не ломает. Pitch/Roll 0.
		// Z вычисляем от реального half-height капсулы, а не магической константой.
		MeshComp->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -WolfCapsuleHalfHeight), FRotator(0.f, 90.f, 0.f));
		// Меш не несёт коллизию — её держит капсула.
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Анимации волка (Single Node, без AnimBP). FObjectFinder загружает ассеты в дефолтах;
	// можно переопределить в BP-наследнике.
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleAsset(TEXT("/Game/Characters/Wolf/Anim_Wolf_Idle.Anim_Wolf_Idle"));
	if (IdleAsset.Succeeded()) { IdleAnim = IdleAsset.Object; }
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunAsset(TEXT("/Game/Characters/Wolf/Anim_Wolf_Run.Anim_Wolf_Run"));
	if (RunAsset.Succeeded()) { RunAnim = RunAsset.Object; }
	static ConstructorHelpers::FObjectFinder<UAnimSequence> BiteAsset(TEXT("/Game/Characters/Wolf/Anim_Wolf_Bite.Anim_Wolf_Bite"));
	if (BiteAsset.Succeeded()) { BiteAnim = BiteAsset.Object; }
	// Build 1.2: смерть «на бок». До импорта ассета Finder мягко промахивается (поле null,
	// прежнее поведение) — путь согласован с импортом моделлера этой же ночи.
	static ConstructorHelpers::FObjectFinder<UAnimSequence> DeathAsset(TEXT("/Game/Characters/Wolf/Anim_Wolf_Death.Anim_Wolf_Death"));
	if (DeathAsset.Succeeded()) { DeathAnim = DeathAsset.Object; }

	// Звуки атаки волка (Демо): набор рыков, при укусе выбирается случайный.
	static ConstructorHelpers::FObjectFinder<USoundBase> Growl1(TEXT("/Game/Audio/Demo/wolf_growl_monster1.wolf_growl_monster1"));
	if (Growl1.Succeeded()) { AttackGrowlSounds.Add(Growl1.Object); }
	static ConstructorHelpers::FObjectFinder<USoundBase> Growl2(TEXT("/Game/Audio/Demo/wolf_growl_monster2.wolf_growl_monster2"));
	if (Growl2.Succeeded()) { AttackGrowlSounds.Add(Growl2.Object); }
	static ConstructorHelpers::FObjectFinder<USoundBase> Growl3(TEXT("/Game/Audio/Demo/wolf_growl_wolfman.wolf_growl_wolfman"));
	if (Growl3.Succeeded()) { AttackGrowlSounds.Add(Growl3.Object); }

	// Скорость ~1.3× бандита (draft).
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = BanditBaseWalkSpeed * SpeedMultiplierVsBandit;
	}
}

void AWolfCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (Stats)
	{
		Stats->InitHealth(WolfMaxHealth, /*bSetToMax=*/true);
		Stats->OnDeath.AddDynamic(this, &AWolfCharacter::HandleDeath);
	}

	// Стартовый клип — Idle (Single Node), чтобы волк не стоял в T-позе.
	UpdateLocomotionAnimation();
}

void AWolfCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Пока проигрывается укус — не перебиваем его локомоцией.
	const UWorld* World = GetWorld();
	if (World && World->GetTimeSeconds() < BiteUntilTime)
	{
		return;
	}

	UpdateLocomotionAnimation();
}

void AWolfCharacter::UpdateLocomotionAnimation()
{
	// Мёртвый волк не анимируется (труп удалится по таймеру).
	if (Stats && Stats->IsDead())
	{
		return;
	}

	const float Speed = GetVelocity().Size2D();
	UAnimSequence* Desired = (Speed > RunSpeedThreshold) ? RunAnim : IdleAnim;

	if (!Desired || Desired == CurrentLocomotionAnim)
	{
		return; // нечего менять (либо ассет не задан, либо уже играет)
	}

	CurrentLocomotionAnim = Desired;
	PlaySingleNode(Desired, /*bLooping=*/true);
}

void AWolfCharacter::PlayBiteAnimation()
{
	// Рык атаки (Демо) — на каждый укус, независимо от наличия аним-клипа.
	PlayAttackSound();

	if (!BiteAnim)
	{
		return;
	}

	PlaySingleNode(BiteAnim, /*bLooping=*/false);

	// Заблокировать смену локомоции на длительность клипа укуса.
	if (const UWorld* World = GetWorld())
	{
		BiteUntilTime = World->GetTimeSeconds() + BiteAnim->GetPlayLength();
	}
	// Сбросить кэш локомоции, чтобы после укуса клип переустановился.
	CurrentLocomotionAnim = nullptr;
}

void AWolfCharacter::PlaySingleNode(UAnimSequence* Anim, bool bLooping)
{
	if (!Anim)
	{
		return;
	}
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		// PlayAnimation переводит компонент в режим Single Node и проигрывает клип
		// без необходимости в AnimBP/AnimInstance-классе.
		MeshComp->PlayAnimation(Anim, bLooping);
	}
}

void AWolfCharacter::PlayAttackSound()
{
	if (AttackGrowlSounds.Num() == 0)
	{
		return;
	}
	const int32 Index = FMath::RandRange(0, AttackGrowlSounds.Num() - 1);
	if (USoundBase* Growl = AttackGrowlSounds[Index])
	{
		UGameplayStatics::PlaySoundAtLocation(this, Growl, GetActorLocation(), AttackGrowlVolume);
	}
}

float AWolfCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Источник истины по HP волка — UStatsComponent (как у бандита). Брони у волка нет.
	if (!Stats || Stats->IsDead() || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	const float Applied = Stats->ApplyDamage(DamageAmount);

	// Звук боли (Демо) — только от боевого урона (эта точка).
	if (Applied > 0.0f)
	{
		Stats->PlayHurtSound();

		// D5: всплывающая цифра урона над врагом (только по врагам — решение Рината).
		if (APlayerController* PC0 = UGameplayStatics::GetPlayerController(GetWorld(), 0))
		{
			if (AContrarySurvivorHUD* HUD = Cast<AContrarySurvivorHUD>(PC0->GetHUD()))
			{
				HUD->AddDamageNumber(GetActorLocation(), Applied);
			}
		}
	}

	// Лог показывает И запрошенный (requested), И реально применённый (applied) урон: на
	// добивающем хите applied < requested (клампится остатком HP) — «took 5.0» на запросе 25
	// больше не путает. Формат согласован с логами бандита/игрока.
	UE_LOG(LogTemp, Log, TEXT("%s (wolf) took %.1f dmg (requested %.1f). Health: %.1f/%.1f"),
		*GetName(), Applied, DamageAmount, Stats->GetHealth(), Stats->GetMaxHealth());

	return Applied;
}

void AWolfCharacter::HandleDeath()
{
	UE_LOG(LogTemp, Log, TEXT("%s (wolf) died."), *GetName());

	if (AController* AICtrl = GetController())
	{
		AICtrl->UnPossess();
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Анимация смерти «ложится на бок» (Build 1.2); ассета нет — прежнее поведение:
	// заморозка позы на текущем кадре (Single Node). Рэгдолл не используем.
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (DeathAnim)
		{
			// Без цикла: поза застывает на последнем кадре (лежит на боку).
			MeshComp->PlayAnimation(DeathAnim, /*bLooping=*/false);
			UE_LOG(LogTemp, Log, TEXT("%s: анимация смерти волка '%s' запущена."),
				*GetName(), *DeathAnim->GetName());
		}
		else
		{
			MeshComp->bPauseAnims = true;
		}
	}

	// Лут волка (деньги + шанс предмета) в позиции трупа (GDD §7.8).
	DropLoot();

	// Фаза 5: засчитываем убийство волка в Kill-квест игрока (если есть Active с тегом "Wolf").
	// Журнал квестов живёт на пешке игрока (UQuestComponent). Тег цели — "Wolf".
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		if (UQuestComponent* PlayerQuests = PlayerPawn->FindComponentByClass<UQuestComponent>())
		{
			PlayerQuests->NotifyKill(FName(TEXT("Wolf")));
		}
		// #26: засчитываем убийство в счётчик киллов игрока (для экрана смерти).
		// Тип "Wolf" — для события аналитики F3 (латиницей, как теги квестов).
		if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(PlayerPawn))
		{
			PlayerChar->RegisterEnemyKill(TEXT("Wolf"));
		}
	}

	SetLifeSpan(CorpseLifeSpan);
}

void AWolfCharacter::DropLoot()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Money = FMath::RoundToFloat(FMath::FRandRange(LootMoneyMin, LootMoneyMax));

	// КВЕСТОВЫЙ ДРОП (Фаза 5): «Шкура волка» гарантированно + деньги. Build 1.2.1 (ТЗ А1):
	// мешок-пикап убран — шкура и деньги остаются В ТРУПЕ (CorpseLoot), игрок забирает их
	// окном обыска. Имя/название задаются ЯВНО на экземпляре (класс AQuestItem общий на
	// шкуру и ноутбук — конструктор различить их не может).
	TArray<AMasterInventoryItem*> Loot;
	if (QuestLootItemClass)
	{
		FActorSpawnParameters Sp;
		Sp.Owner = this;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AMasterInventoryItem* Pelt = World->SpawnActor<AMasterInventoryItem>(
			QuestLootItemClass, GetActorLocation(), FRotator::ZeroRotator, Sp);
		if (Pelt)
		{
			// Предмет лута — данные рюкзака, не объект сцены (как в APickup::DropLoot).
			Pelt->SetActorHiddenInGame(true);
			Pelt->SetActorEnableCollision(false);
			Pelt->ItemName = QuestLootItemName;
			Pelt->ItemDisplayText = QuestLootItemText;
			Loot.Add(Pelt);
		}
	}

	if (CorpseLoot)
	{
		CorpseLoot->InitLoot(Money, Loot);
	}
	else
	{
		for (AMasterInventoryItem* It : Loot)
		{
			if (IsValid(It)) { It->Destroy(); }
		}
	}

	FQADebug::QA(World, FString::Printf(
		TEXT("QA: wolf loot = '%s' (guaranteed, money=%.0f) -> corpse"), *QuestLootItemName, Money),
		/*bScreen=*/true);
}
