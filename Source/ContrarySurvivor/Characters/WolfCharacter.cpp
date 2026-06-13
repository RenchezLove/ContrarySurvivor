// Fill out your copyright notice in the Description page of Project Settings.

#include "WolfCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Controllers/WolfAIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

// Базовая скорость бандита (дефолт CharacterMovementComponent::MaxWalkSpeed UE = 600).
// Используется как опорная для множителя скорости волка.
static constexpr float BanditBaseWalkSpeed = 600.0f;

AWolfCharacter::AWolfCharacter()
{
	// Тик нужен для смены Idle/Run по скорости (Single Node анимации без AnimBP).
	PrimaryActorTick.bCanEverTick = true;

	Stats = CreateDefaultSubobject<UStatsComponent>(TEXT("StatsComponent"));

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
		// Выравнивание меша под капсулу: Z = -(half-height) ставит лапы на дно капсулы
		// (после re-import волк ориентирован как гуманоид: forward=-Y, up=+Z, лапы на Z0).
		// Yaw -90 разворачивает по +X (волк смотрит вперёд) — ВЕРНО, не менять. Pitch/Roll 0.
		// Z вычисляем от реального half-height капсулы, а не магической константой.
		MeshComp->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -WolfCapsuleHalfHeight), FRotator(0.f, -90.f, 0.f));
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

float AWolfCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Источник истины по HP волка — UStatsComponent (как у бандита). Брони у волка нет.
	if (!Stats || Stats->IsDead() || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	const float Applied = Stats->ApplyDamage(DamageAmount);

	UE_LOG(LogTemp, Log, TEXT("%s (wolf) took %.1f damage. Health: %.1f/%.1f"),
		*GetName(), Applied, Stats->GetHealth(), Stats->GetMaxHealth());

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

	// Замораживаем позу на последнем кадре (Single Node), рэгдолл не используем
	// (физ-ассет может отсутствовать). Тело снимается с задержкой.
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->bPauseAnims = true;
	}

	SetLifeSpan(CorpseLifeSpan);
}
