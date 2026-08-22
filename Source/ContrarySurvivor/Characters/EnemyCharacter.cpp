// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemyCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Components/CorpseLootComponent.h" // Build 1.2.1 (А1): лут в трупе
#include "ContrarySurvivor/Components/QuestComponent.h" // Фаза 5: засчёт убийства бандита в квест
#include "ContrarySurvivor/Characters/PlayerCharacter.h" // #26: счётчик киллов игрока
#include "ContrarySurvivor/Actors/Pickup.h"
#include "ContrarySurvivor/HUD/ContrarySurvivorHUD.h" // D5: всплывающие цифры урона по врагам
#include "ContrarySurvivor/Debug/QADebug.h" // force-drop (Z) + QA-лог дропа
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

	// Build 1.2.1 (ТЗ А1): контейнер лута трупа в МАСТЕР-классе — BP-наследники получают
	// обыск автоматически. Наполняется в DropLoot (из HandleDeath).
	CorpseLoot = CreateDefaultSubobject<UCorpseLootComponent>(TEXT("CorpseLoot"));
	if (CorpseLoot)
	{
		// ADR-076 п.2: имя в перечне обыскиваемых («Труп бандита; Мешок»). Правится в BP врага.
		CorpseLoot->SearchObjectName = NSLOCTEXT("CorpseLoot", "BanditCorpseName", "Труп бандита");
	}

	// Лут по умолчанию (editor-независимо): пикап без BP + таблица расходников Консервы/
	// Вода/Бинт с равновероятным выбором (Ринат 07-17). Имена — из единого источника
	// AConsumableItem::GetDefaultDisplayName (задел под FText-локализацию, ADR-041).
	PickupClass = APickup::StaticClass();
	const EConsumableType DefaultLootTypes[] =
		{ EConsumableType::Food, EConsumableType::Water, EConsumableType::Medkit };
	for (const EConsumableType Type : DefaultLootTypes)
	{
		FBanditLootEntry Entry;
		Entry.DisplayName = AConsumableItem::GetDefaultDisplayName(Type);
		Entry.DisplayText = AConsumableItem::GetDefaultDisplayText(Type);
		Entry.ItemClass = AConsumableItem::StaticClass();
		Entry.ConsumableType = Type;
		LootTable.Add(Entry);
	}

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

	// 4) Анимация смерти «ложится на спину» (Build 1.2, поле в мастер-базе — одна на всех
	//    гуманоидов). Ассета нет — откат на прежний рэгдолл (который у текущего меша без
	//    физ.ассета молча не работал — Ринат на приёмке 08-01: «Сейчас бандит не падает.
	//    Видимо физики нет» — потому и появилась анимация).
	if (!PlayDeathAnimationIfSet())
	{
		if (USkeletalMeshComponent* SkelMesh = GetMesh())
		{
			SkelMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			SkelMesh->SetCollisionObjectType(ECC_PhysicsBody);
			SkelMesh->SetAllBodiesSimulatePhysics(true);
			SkelMesh->SetSimulatePhysics(true);
			SkelMesh->WakeAllRigidBodies();
		}
	}

	// 5) Лут: деньги + шанс предмета на земле в позиции трупа (GDD §7.8).
	DropLoot();

	// 5b) Фаза 5: засчитываем убийство в kill-цель квеста игрока. Тег — поле QuestKillTag
	// (ADR-075: настраивается в BP, дефолт "Bandit"); пустой тег = в квесты не идёт.
	// Журнал квестов живёт на пешке игрока (UQuestComponent).
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		if (!QuestKillTag.IsNone())
		{
			if (UQuestComponent* PlayerQuests = PlayerPawn->FindComponentByClass<UQuestComponent>())
			{
				PlayerQuests->NotifyKill(QuestKillTag);
			}
		}
		// #26: засчитываем убийство в счётчик киллов игрока (для экрана смерти).
		// Тип врага для события аналитики F3 — тот же QuestKillTag (единый источник).
		if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(PlayerPawn))
		{
			PlayerChar->RegisterEnemyKill(QuestKillTag.IsNone() ? TEXT("Unknown") : QuestKillTag.ToString());
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
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Money = FMath::RoundToFloat(FMath::FRandRange(LootMoneyMin, LootMoneyMax));
	const FVector Loc = GetActorLocation();

	// Шанс расходников бросаем ЗДЕСЬ, а не в APickup::DropLoot: при удаче падает 1-2 предмета
	// из LootTable (Ринат 07-17), а статический хелпер несёт максимум один предмет.
	// QA force-drop (клавиша Z) поднимает шанс до 100% — как в APickup::DropLoot.
	const float EffectiveChance = FQADebug::bForceDrop ? 1.0f : LootItemDropChance;
	const float Roll = FMath::FRand();
	const bool bChanceHit = (LootTable.Num() > 0) && (Roll <= EffectiveChance);

	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	TArray<AMasterInventoryItem*> Items;
	FString ItemNamesLog;
	if (bChanceHit)
	{
		// Каждая единица выбирается из таблицы равновероятно и НЕЗАВИСИМО (повторы допустимы).
		const int32 Count = FMath::RandRange(
			LootItemCountMin, FMath::Max(LootItemCountMin, LootItemCountMax));
		for (int32 i = 0; i < Count; ++i)
		{
			const FBanditLootEntry& Entry = LootTable[FMath::RandRange(0, LootTable.Num() - 1)];
			UClass* ItemClass = Entry.ItemClass ? *Entry.ItemClass : AConsumableItem::StaticClass();
			AMasterInventoryItem* Item = World->SpawnActor<AMasterInventoryItem>(
				ItemClass, Loc, FRotator::ZeroRotator, Sp);
			if (!Item)
			{
				continue;
			}
			// Предмет лута — данные рюкзака, не объект сцены (тот же приём, что в APickup::DropLoot).
			Item->SetActorHiddenInGame(true);
			Item->SetActorEnableCollision(false);
			// Служебный ключ; пустое поле таблицы -> ключ по типу расходника.
			Item->ItemName = !Entry.DisplayName.IsEmpty()
				? Entry.DisplayName
				: AConsumableItem::GetDefaultDisplayName(Entry.ConsumableType);
			// Переводимое название рядом с ключом: класс AConsumableItem один на воду,
			// консервы и бинт, поэтому название задаётся здесь, а не в конструкторе класса.
			Item->ItemDisplayText = !Entry.DisplayText.IsEmpty()
				? Entry.DisplayText
				: AConsumableItem::GetDefaultDisplayText(Entry.ConsumableType);
			if (AConsumableItem* Cons = Cast<AConsumableItem>(Item))
			{
				Cons->ConsumableType = Entry.ConsumableType;
			}
			Items.Add(Item);
			ItemNamesLog += (ItemNamesLog.IsEmpty() ? TEXT("") : TEXT(", ")) + Item->ItemName;
		}
	}

	// Build 1.2.1 (ТЗ А1): мешок-пикап с трупов УБРАН — деньги и предметы остаются В ТРУПЕ
	// (CorpseLoot), игрок забирает их через окно обыска («Обыскать [E]», частичный обыск
	// штатен). Не забранное исчезает вместе с трупом (CorpseLifeSpan).
	if (CorpseLoot)
	{
		CorpseLoot->InitLoot(Money, Items);
	}
	else
	{
		// Контейнера нет (не должно случаться: создаётся в конструкторе) — не оставляем
		// висящие скрытые предметы в мире.
		for (AMasterInventoryItem* It : Items)
		{
			if (IsValid(It)) { It->Destroy(); }
		}
		Items.Reset();
	}

	// QA-инструментирование: что легло в труп при смерти бандита (формат DROPLOOT сохранён).
	FQADebug::QA(World, FString::Printf(
		TEXT("QA: DROPLOOT money=%.0f chance=%.2f roll=%.2f hit=%s items=%d [%s] -> corpse"),
		Money, EffectiveChance, Roll, bChanceHit ? TEXT("YES") : TEXT("no"),
		Items.Num(), *ItemNamesLog), /*bScreen=*/true);
}
