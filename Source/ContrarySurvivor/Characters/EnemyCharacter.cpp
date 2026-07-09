// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemyCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Components/QuestComponent.h" // Фаза 5: засчёт убийства бандита в квест
#include "ContrarySurvivor/Characters/PlayerCharacter.h" // #26: счётчик киллов игрока
#include "ContrarySurvivor/Actors/Pickup.h"
#include "ContrarySurvivor/HUD/ContrarySurvivorHUD.h" // D5: всплывающие цифры урона по врагам
#include "AConsumableItem.h"
#include "APistol.h" // D1/D6: пистолет в руке бандита (дефолт SidearmWeaponClass)
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h" // GetPlayerPawn (поиск журнала квестов игрока)
#include "AIController.h"

AEnemyCharacter::AEnemyCharacter()
{
	Stats = CreateDefaultSubobject<UStatsComponent>(TEXT("StatsComponent"));

	// Лут по умолчанию: расходник + пикап без BP (editor-независимо).
	LootItemClass = AConsumableItem::StaticClass();
	PickupClass   = APickup::StaticClass();

	// D1/D6: бандит носит пистолет (визуал огнестрела ADR-035). Конкретный APistol —
	// editor-независимо; BP может переопределить/обнулить.
	SidearmWeaponClass = APistol::StaticClass();

	// Враг управляется AI-контроллером. Конкретный класс назначается в BP/дефолтах
	// (AEnemyAIController), здесь только включаем авто-поссесс при спавне/размещении.
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// ПРАВКА A: профиль капсулы Pawn по умолчанию Ignore'ит канал Visibility,
	// а скелет-меши без коллизии не ловят луч → выстрел ARangedWeapon
	// (LineTraceSingleByChannel по ECC_Visibility) проходит сквозь врага и не зовёт
	// TakeDamage. Блокируем Visibility на капсуле, чтобы враг был «простреливаемым»:
	// луч попадёт в капсулу → HitResult.GetActor()=враг → TakeDamage → UStatsComponent.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	// ПРАВКА C: стандартное для ACharacter выравнивание меша под капсулу.
	// BP врага создан заново и не унаследовал дефолтный transform GetMesh
	// (в отличие от BP игрока) → бандит выглядел перевёрнутым/криво.
	// -90 по Z ставит ноги на дно капсулы, -90 по Yaw разворачивает меш по +X.
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
	}
}

void AEnemyCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// ПРАВКА B: Torso/Legs следуют за позой Head (корневой скелет мастер-базы).
	// Делаем в PostInitializeComponents (а не в конструкторе): к этому моменту все
	// компоненты — включая дефолты, заданные в BP врага — сконструированы и
	// зарегистрированы, и линковка корректно переустанавливается на каждом спавне.
	// AnimBP на Head назначает unreal-operator в BP (контент-ассет, из C++ не ссылаемся).
	USkeletalMeshComponent* Head = GetMesh(); // == HeadMesh (базовый конструктор)
	if (Head)
	{
		if (TorsoMesh)
		{
			TorsoMesh->SetLeaderPoseComponent(Head);
		}
		if (LegsMesh)
		{
			LegsMesh->SetLeaderPoseComponent(Head);
		}
	}
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (Stats)
	{
		// UStatsComponent — источник истины по HP врага (инлайн-Health базы не используем).
		Stats->InitHealth(BanditMaxHealth, /*bSetToMax=*/true);
		Stats->OnDeath.AddDynamic(this, &AEnemyCharacter::HandleDeath);
	}

	// Скорость погони (TUNING): детерминированно поверх Super (Super выставил MaxWalkSpeed из CMC/
	// BaseWalkSpeed). Чуть выше ходьбы игрока, ниже спринта — бандит догоняет шагающего, но от
	// спринта можно оторваться. Не зависит от дефолта CMC / оверрайда BP.
	if (BanditWalkSpeed > 0.0f)
	{
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->MaxWalkSpeed = BanditWalkSpeed;
		}
	}

	// D1/D6: пистолет в руку (после инициализации статов/скорости; крепление — кость R_Hand).
	EquipSidearm();
}

void AEnemyCharacter::EquipSidearm()
{
	if (!SidearmWeaponClass)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Тот же путь, что у игрока (APlayerCharacter::EquipDefaultWeapon): спавн + EquipWeapon
	// (крепление к кости R_Hand лидер-меша, гашение коллизии оружия).
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AMasterWeapon* Sidearm = World->SpawnActor<AMasterWeapon>(
		SidearmWeaponClass, GetActorLocation(), GetActorRotation(), SpawnParams);

	if (Sidearm)
	{
		EquipWeapon(Sidearm);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: failed to spawn sidearm %s"),
			*GetName(), *SidearmWeaponClass->GetName());
	}
}

float AEnemyCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Намеренно НЕ зовём Super (инлайн-Health базы), чтобы единственным
	// источником истины по HP врага был UStatsComponent.
	if (!Stats || Stats->IsDead() || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	// GDD §7.2: броня снижает урон. ПРОЦЕНТНАЯ формула (решение Рината):
	// Final = Incoming * (1 - clamp(SumArmorFraction, 0, Cap)). У бандита брони нет
	// (сумма 0 → урон без изменений); формула общая для всех гуманоидов.
	const float Reduced = ComputeArmoredDamage(DamageAmount);

	const float Applied = Stats->ApplyDamage(Reduced);

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

	UE_LOG(LogTemp, Log, TEXT("%s took %.1f dmg (incoming %.1f, armor frac %.2f cap %.2f). Health: %.1f/%.1f"),
		*GetName(), Applied, DamageAmount, GetTotalArmorProtection(), ArmorReductionCap, Stats->GetHealth(), Stats->GetMaxHealth());

	return Applied;
}

void AEnemyCharacter::HandleDeath()
{
	UE_LOG(LogTemp, Log, TEXT("%s died."), *GetName());

	// 1) Отключаем ИИ: освобождаем контроллер, чтобы он перестал двигать/атаковать.
	if (AController* AICtrl = GetController())
	{
		AICtrl->UnPossess();
	}

	// 2) Останавливаем движение.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	// 3) Отключаем коллизию капсулы (труп не блокирует игрока/трейсы по Pawn).
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 4) Рэгдолл на основном меше (Head — корневой скелет из мастер-базы).
	//    Если у меша нет физ.ассета — ветка молча не даст эффекта, краша не будет.
	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		SkelMesh->SetCollisionObjectType(ECC_PhysicsBody);
		SkelMesh->SetAllBodiesSimulatePhysics(true);
		SkelMesh->SetSimulatePhysics(true);
		SkelMesh->WakeAllRigidBodies();
	}

	// 5) Лут: деньги + шанс предмета на земле в позиции трупа (GDD §7.8).
	DropLoot();

	// 5b) Фаза 5: засчитываем убийство бандита в kill-цель квеста игрока (тег "Bandit").
	// Журнал квестов живёт на пешке игрока (UQuestComponent).
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		if (UQuestComponent* PlayerQuests = PlayerPawn->FindComponentByClass<UQuestComponent>())
		{
			PlayerQuests->NotifyKill(FName(TEXT("Bandit")));
		}
		// #26: засчитываем убийство в счётчик киллов игрока (для экрана смерти).
		if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(PlayerPawn))
		{
			PlayerChar->RegisterEnemyKill();
		}
	}

	// 5c) D1/D6: пистолет в руке НЕ переживает труп — attach не уничтожает актор оружия
	// автоматически, без LifeSpan он остался бы висеть в мире после Destroy тела.
	if (CurrentWeapon)
	{
		CurrentWeapon->SetLifeSpan(CorpseLifeSpan);
	}

	// 6) Снимаем тело с задержкой (даём отыграть рэгдолл).
	SetLifeSpan(CorpseLifeSpan);
}

void AEnemyCharacter::DropLoot()
{
	const float Money = FMath::RoundToFloat(FMath::FRandRange(LootMoneyMin, LootMoneyMax));
	APickup::DropLoot(GetWorld(), GetActorLocation(), Money,
		LootItemClass, LootItemDropChance, PickupClass);
}
