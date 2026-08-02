// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты волны Build 1.2.2 (сокет хвата + размещаемый лут). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.Build122; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Покрывается БЕЗ PIE:
//   - контракт сокета WeaponGripSocket на ОБОИХ скелетах-дублях (есть, кость R_Hand,
//     scale=(1,1,1), трансформы совпадают) — ассетная половина контракта -normalize;
//   - арифметика поправки ножа: константы конструктора AMeleeWeapon = точная инверсия
//     сокет-трансформа этой волны (композиция даёт тождество -> нож лежит по кости);
//   - предупреждение о ПУСТОМ пикапе, размещённом на карте (LogQA Warning), и его
//     ОТСУТСТВИЕ у рантайм-спавна (DropLoot/мешок смерти наполняются ПОСЛЕ BeginPlay).
// НЕ покрывается headless (нужен PIE): фактическая поза оружия в ладони на анимируемом
//   персонаже (скелетные меши в тест-мире не грузятся) — за живым осмотром Рината/лида.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AMeleeWeapon.h"
#include "Animation/Skeleton.h"
#include "ContrarySurvivor/Actors/Pickup.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

static constexpr EAutomationTestFlags Build122TestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace Build122GripSocket
{
	// Оба скелета-дубля гуманоида (те же пути, что в AddGripSocketCommandlet.cpp).
	static const TCHAR* PlayerSkeletonPath =
		TEXT("/Game/TestContentAndCode/PreProduction/HeadAndSkeletonfbx_Head_Skeleton.HeadAndSkeletonfbx_Head_Skeleton");
	static const TCHAR* BanditSkeletonPath =
		TEXT("/Game/Characters/Shared/Humanoid/HeadAndSkeletonfbx_Head_Skeleton.HeadAndSkeletonfbx_Head_Skeleton");

	static const USkeletalMeshSocket* FindGripSocket(const USkeleton* Skeleton)
	{
		for (const USkeletalMeshSocket* Socket : Skeleton->Sockets)
		{
			if (Socket && Socket->SocketName == FName(TEXT("WeaponGripSocket")))
			{
				return Socket;
			}
		}
		return nullptr;
	}
}

// Транзиентный игровой мир (копия обвязки Build121TestWorld — она static в своём .cpp).
namespace Build122TestWorld
{
	static UWorld* Create()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
		if (!World)
		{
			return nullptr;
		}
		FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Game);
		Ctx.SetCurrentWorld(World);

		const FURL URL;
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();
		if (AWorldSettings* WorldSettings = World->GetWorldSettings())
		{
			WorldSettings->NotifyBeginPlay(); // мир без GameMode сам begun-play не ставит
		}
		return World;
	}

	static void Destroy(UWorld* World)
	{
		if (!World || !GEngine)
		{
			return;
		}
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(/*bInformEngineOfWorld=*/false);
	}
}

// ===========================================================================
// 1. Контракт сокета хвата на обоих скелетах: есть, кость R_Hand, scale=(1,1,1),
//    трансформы совпадают (иначе игрок и бандит держат оружие по-разному).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild122GripSocketAssetContractTest,
	"ContrarySurvivor.Build122.GripSocket.AssetContract", Build122TestFlags)

bool FBuild122GripSocketAssetContractTest::RunTest(const FString& Parameters)
{
	using namespace Build122GripSocket;

	USkeleton* PlayerSkeleton = LoadObject<USkeleton>(nullptr, PlayerSkeletonPath);
	USkeleton* BanditSkeleton = LoadObject<USkeleton>(nullptr, BanditSkeletonPath);
	if (!TestNotNull(TEXT("Скелет игрока (PreProduction) загрузился"), PlayerSkeleton)
		|| !TestNotNull(TEXT("Скелет бандита (Shared) загрузился"), BanditSkeleton))
	{
		return false;
	}

	const USkeletalMeshSocket* PlayerSocket = FindGripSocket(PlayerSkeleton);
	const USkeletalMeshSocket* BanditSocket = FindGripSocket(BanditSkeleton);
	if (!TestNotNull(TEXT("WeaponGripSocket есть на скелете игрока"), PlayerSocket)
		|| !TestNotNull(TEXT("WeaponGripSocket есть на скелете бандита"), BanditSocket))
	{
		return false;
	}

	TestEqual(TEXT("Сокет игрока сидит на кости R_Hand"),
		PlayerSocket->BoneName, FName(TEXT("R_Hand")));
	TestEqual(TEXT("Сокет бандита сидит на кости R_Hand"),
		BanditSocket->BoneName, FName(TEXT("R_Hand")));

	// Build 1.2.2: scale строго 1 — иначе масштабируются цифровые поправки хвата.
	TestTrue(TEXT("Scale сокета игрока = (1,1,1)"),
		PlayerSocket->RelativeScale.Equals(FVector::OneVector));
	TestTrue(TEXT("Scale сокета бандита = (1,1,1)"),
		BanditSocket->RelativeScale.Equals(FVector::OneVector));

	TestTrue(TEXT("Позиции сокета на двух скелетах совпадают"),
		PlayerSocket->RelativeLocation.Equals(BanditSocket->RelativeLocation));
	TestTrue(TEXT("Повороты сокета на двух скелетах совпадают"),
		PlayerSocket->RelativeRotation.Equals(BanditSocket->RelativeRotation));
	return true;
}

// ===========================================================================
// 2. Арифметика поправки ножа: константы AMeleeWeapon = инверсия сокет-трансформа
//    ЭТОЙ волны (литералы = поза Рината, срез 08-02). Композиция «поправка ∘ сокет»
//    обязана дать тождество: нож ложится ровно по кости R_Hand, как до правки сокета.
//    Тест про МАТЕМАТИКУ констант, не про живой ассет: будущая перенастройка сокета
//    Ринатом тест не сломает (дрейф ассетов ловят -verify и тест №1).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild122KnifeGripOffsetMathTest,
	"ContrarySurvivor.Build122.GripSocket.KnifeOffsetMath", Build122TestFlags)

bool FBuild122KnifeGripOffsetMathTest::RunTest(const FString& Parameters)
{
	// Поза сокета, подобранная Ринатом (Shared-скелет, коммит 30aedb1; срез 08-02).
	const FTransform SocketTM(
		FRotator(0.000341, -179.999728, -0.000062),
		FVector(-0.000000, -0.061311, 0.000000),
		FVector::OneVector);

	const AMeleeWeapon* KnifeCDO = GetDefault<AMeleeWeapon>();
	if (!TestNotNull(TEXT("CDO ножа доступен"), KnifeCDO))
	{
		return false;
	}
	const FTransform KnifeFixTM(
		KnifeCDO->GetGripOffsetRotation(), KnifeCDO->GetGripOffsetLocation(), FVector::OneVector);

	// Та же композиция, что в EquipWeapon: итог = ПоправкаОружия ∘ Сокет.
	const FTransform Composed = KnifeFixTM * SocketTM;
	TestTrue(FString::Printf(TEXT("Сдвиг композиции ~0 (фактически %s)"),
			*Composed.GetLocation().ToString()),
		Composed.GetLocation().IsNearlyZero(0.001));
	const double AngleDeg = FMath::RadiansToDegrees(Composed.GetRotation().GetAngle());
	TestTrue(FString::Printf(TEXT("Поворот композиции ~0 градусов (фактически %.6f)"), AngleDeg),
		AngleDeg < 0.01);

	// Пистолет — эталон сокета: у базового оружия поправка обязана быть нулевой.
	const AMasterWeapon* BaseCDO = GetDefault<AMasterWeapon>();
	TestTrue(TEXT("Поправка базового оружия (пистолета) нулевая"),
		BaseCDO->GetGripOffsetLocation().IsNearlyZero()
			&& BaseCDO->GetGripOffsetRotation().IsNearlyZero());

	// Задел на двуручное: по умолчанию сокет левой руки ПУСТ (одноручное).
	TestEqual(TEXT("LeftHandGripSocketName по умолчанию пуст"),
		BaseCDO->GetLeftHandGripSocketName(), FName(NAME_None));
	return true;
}

// ===========================================================================
// 3. Пустой РАЗМЕЩЁННЫЙ пикап предупреждает в LogQA ровно один раз; рантайм-спавн
//    (мешок смерти/DropLoot наполняются ПОСЛЕ BeginPlay) — молчит.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuild122PickupPlacedEmptyWarnTest,
	"ContrarySurvivor.Build122.Pickup.PlacedEmptyWarning", Build122TestFlags)

bool FBuild122PickupPlacedEmptyWarnTest::RunTest(const FString& Parameters)
{
	UWorld* World = Build122TestWorld::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}

	// Ровно ОДНО предупреждение: от «размещённого» пустого пикапа, не от рантайм-спавна.
	AddExpectedMessagePlain(TEXT("размещён на карте ПУСТЫМ"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, /*Occurrences=*/1);

	// «Размещённый на карте» пикап: отложенный спавн, флаг bNetStartup ДО BeginPlay —
	// то же состояние, что у актора уровня после ULevel::InitializeNetworkActors
	// (Level.cpp:3327; BeginPlay уровня идёт позже инициализации).
	APickup* Placed = World->SpawnActorDeferred<APickup>(APickup::StaticClass(), FTransform::Identity);
	if (!TestNotNull(TEXT("Отложенный спавн пикапа удался"), Placed))
	{
		Build122TestWorld::Destroy(World);
		return false;
	}
	Placed->bNetStartup = true;
	Placed->FinishSpawning(FTransform::Identity); // BeginPlay внутри — здесь ждём Warning

	// Рантайм-спавн (как DropLoot/мешок смерти): в BeginPlay пуст — предупреждать НЕЛЬЗЯ.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APickup* Runtime = World->SpawnActor<APickup>(
		APickup::StaticClass(), FVector(100.f, 0.f, 100.f), FRotator::ZeroRotator, Params);
	TestNotNull(TEXT("Рантайм-спавн пикапа удался"), Runtime);

	Build122TestWorld::Destroy(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
