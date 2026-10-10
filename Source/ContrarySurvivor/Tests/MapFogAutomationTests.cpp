// Fill out your copyright notice in the Description page of Project Settings.

// Карта КПК открывается по мере исследования (слова Рината 10.10.2026: «изначально на карте
// ничего не должно быть видно, кроме деревни. Остальное — чернота, неизведанные территории…
// игрок пошёл от деревни в сторону неизведанных территорий и карта появляется в каком-то
// радиусе от него»). Проверяются правила сетки открытого, память героя, сохранение и то, что
// окно показывает подписи мест только на открытом.
//
// Настройки карты на время каждой проверки ставятся известными (углы карты в боевом конфиге
// правит лид — от них проверки зависеть не должны) и возвращаются обратно. Слот сохранения
// ИЗОЛИРОВАННЫЙ (ASaveTestPlayerCharacter) — боевой слот не трогается.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Actors/Campfire.h"
#include "ContrarySurvivor/Actors/VillageZone.h"
#include "ContrarySurvivor/Components/MapExplorationComponent.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Data/ContraryPdaSettings.h"
#include "ContrarySurvivor/Debug/ContraryDebugMenu.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/UI/PdaMapFog.h"
#include "ContrarySurvivor/UI/PdaScreenWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"

static constexpr EAutomationTestFlags MapFogTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace MapFogTest
{
	const FVector2D VillageXY(-3950.0, -4650.0);
	const FVector2D WolfDenXY(13943.0, -5998.0);
	const FVector2D SecondDenXY(-6080.0, -13940.0);
	const FVector2D BanditCampXY(6940.0, 9070.0);

	// Транзиентный игровой мир (та же обвязка, что в PdaAutomationTests.cpp).
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
			WorldSettings->NotifyBeginPlay();
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

	template <typename T>
	static T* Spawn(UWorld* World, const FVector& Loc)
	{
		if (!World)
		{
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<T>(T::StaticClass(), Loc, FRotator::ZeroRotator, Params);
	}

	static void DeleteSlot(const FString& Slot)
	{
		if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
		{
			UGameplayStatics::DeleteGameInSlot(Slot, 0);
		}
	}

	// Известные настройки карты на время проверки; прежние возвращаются при выходе.
	struct FScopedMapSettings
	{
		UContraryPdaSettings* Settings;
		FVector2D TopLeft, BottomRight, VillageFallback;
		FIntPoint Grid;
		float RevealRadius, VillageRadius, MergeRadius;
		int32 Softness;
		bool bTrackedOnUnexplored;
		TArray<FPdaMapPlace> Places;

		FScopedMapSettings()
			: Settings(GetMutableDefault<UContraryPdaSettings>())
		{
			TopLeft = Settings->MapWorldTopLeft;
			BottomRight = Settings->MapWorldBottomRight;
			VillageFallback = Settings->VillageCenterFallback;
			Grid = Settings->MapFogGridSize;
			RevealRadius = Settings->MapRevealRadius;
			VillageRadius = Settings->VillageRevealRadius;
			MergeRadius = Settings->MapPlaceMergeRadius;
			Softness = Settings->MapFogSoftness;
			bTrackedOnUnexplored = Settings->bShowTrackedQuestOnUnexplored;
			Places = Settings->MapPlaces;

			// Игровая зона уровня: квадрат 46875 см, клетка сетки 128 на 128 — 366 см.
			Settings->MapWorldTopLeft = FVector2D(28937.5, 19067.5);
			Settings->MapWorldBottomRight = FVector2D(-17937.5, -27807.5);
			Settings->VillageCenterFallback = VillageXY;
			Settings->MapFogGridSize = FIntPoint(128, 128);
			Settings->MapRevealRadius = 1750.0f;
			Settings->VillageRevealRadius = 3000.0f;
			Settings->MapPlaceMergeRadius = 2500.0f;
			Settings->MapFogSoftness = 2;
			Settings->bShowTrackedQuestOnUnexplored = true;
			Settings->MapPlaces.Reset();
			AddPlace(TEXT("Деревня"), VillageXY, EPdaMapPlaceKind::Friendly);
			AddPlace(TEXT("Лагерь бандитов"), BanditCampXY, EPdaMapPlaceKind::Enemy);
			AddPlace(TEXT("Логово волков"), WolfDenXY, EPdaMapPlaceKind::Enemy);
			AddPlace(TEXT("Логово волков"), SecondDenXY, EPdaMapPlaceKind::Enemy);
		}

		~FScopedMapSettings()
		{
			Settings->MapWorldTopLeft = TopLeft;
			Settings->MapWorldBottomRight = BottomRight;
			Settings->VillageCenterFallback = VillageFallback;
			Settings->MapFogGridSize = Grid;
			Settings->MapRevealRadius = RevealRadius;
			Settings->VillageRevealRadius = VillageRadius;
			Settings->MapPlaceMergeRadius = MergeRadius;
			Settings->MapFogSoftness = Softness;
			Settings->bShowTrackedQuestOnUnexplored = bTrackedOnUnexplored;
			Settings->MapPlaces = Places;
		}

		void AddPlace(const TCHAR* Name, const FVector2D& Location, EPdaMapPlaceKind Kind)
		{
			FPdaMapPlace& Place = Settings->MapPlaces.AddDefaulted_GetRef();
			Place.Name = FText::FromString(Name);
			Place.WorldLocation = Location;
			Place.Kind = Kind;
		}
	};

	static void MoveTo(AActor* Actor, const FVector2D& XY)
	{
		Actor->SetActorLocation(FVector(XY.X, XY.Y, 100.0), /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	}

	static int32 CountLabel(const UPdaScreenWidget* Window, const TCHAR* Label)
	{
		int32 Count = 0;
		for (const FString& Shown : Window->GetShownPlaceLabels())
		{
			Count += (Shown == Label) ? 1 : 0;
		}
		return Count;
	}
}

// ===========================================================================
// 1. Сетка открытого: круг открывается, открытое не закрывается, запись сжимается и читается.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapFogGridRulesTest,
	"ContrarySurvivor.Pda.MapFog.CircleOpensAndStaysOpen", MapFogTestFlags)

bool FMapFogGridRulesTest::RunTest(const FString& Parameters)
{
	FPdaMapFog Fog;
	Fog.Init(128, 128);
	TestEqual(TEXT("Новая сетка закрыта целиком"), Fog.CountRevealed(), 0);

	// Круг радиусом в десятую долю карты (12,8 клетки) в середине.
	const FVector2D Center(0.5, 0.5), Radius(0.1, 0.1);
	TestTrue(TEXT("Первое открытие что-то открыло"), Fog.RevealCircle(Center, Radius));
	TestTrue(TEXT("Середина круга открыта"), Fog.IsRevealedAt(Center));
	TestTrue(TEXT("Точка внутри круга открыта"), Fog.IsRevealedAt(FVector2D(0.58, 0.5)));
	TestTrue(TEXT("Точка внутри круга по другой оси открыта"), Fog.IsRevealedAt(FVector2D(0.5, 0.42)));
	TestFalse(TEXT("Точка за кругом закрыта"), Fog.IsRevealedAt(FVector2D(0.63, 0.5)));
	TestFalse(TEXT("Угол квадрата вокруг круга закрыт"), Fog.IsRevealedAt(FVector2D(0.59, 0.59)));
	const int32 Opened = Fog.CountRevealed();
	// Площадь круга: 3,14 * 12,8 * 12,8 = 515 клеток.
	TestTrue(*FString::Printf(TEXT("Открыто около 515 клеток (вышло %d)"), Opened), Opened > 470 && Opened < 560);

	TestFalse(TEXT("Повторное открытие того же круга ничего не меняет"), Fog.RevealCircle(Center, Radius));
	TestTrue(TEXT("Соседний круг открывает новое"), Fog.RevealCircle(FVector2D(0.6, 0.5), Radius));
	TestTrue(TEXT("Открытое раньше осталось открытым"), Fog.IsRevealedAt(FVector2D(0.42, 0.5)));
	TestTrue(TEXT("Открытого стало больше"), Fog.CountRevealed() > Opened);

	// Точка за краем карты прижимается к краю и открывает крайние клетки.
	TestTrue(TEXT("Круг у края открывает крайние клетки"), Fog.RevealCircle(FVector2D(1.0, 0.0), Radius));
	TestTrue(TEXT("Крайняя клетка открыта"), Fog.IsCellRevealed(127, 0));

	// Запись и чтение.
	TArray<uint8> Packed;
	Fog.Pack(Packed);
	TestTrue(*FString::Printf(TEXT("Запись компактна: %d байт вместо 2048"), Packed.Num()), Packed.Num() > 0 && Packed.Num() < 400);
	FPdaMapFog Read;
	TestTrue(TEXT("Запись читается"), Read.Unpack(128, 128, Packed));
	TestTrue(TEXT("Прочитанное совпало с записанным"), Read == Fog);

	FPdaMapFog Bad;
	Bad.Init(16, 16);
	TestFalse(TEXT("Запись с чужим размером сетки отвергается"), Bad.Unpack(64, 64, Packed));
	TestEqual(TEXT("Отвергнутая запись сетку не портит"), Bad.GetWidth(), 16);
	TArray<uint8> Torn = Packed;
	Torn.SetNum(Packed.Num() / 2);
	TestFalse(TEXT("Оборванная запись отвергается"), Bad.Unpack(128, 128, Torn));

	// Смена размера сетки: открытое переносится.
	FPdaMapFog Resized = Fog;
	Resized.ResampleTo(64, 64);
	TestEqual(TEXT("Сетка стала 64 на 64"), Resized.GetWidth(), 64);
	TestTrue(TEXT("После смены размера середина открыта"), Resized.IsRevealedAt(Center));
	TestFalse(TEXT("После смены размера дальний угол закрыт"), Resized.IsRevealedAt(FVector2D(0.1, 0.9)));

	// Маска черноты: внутри открытого прозрачно, вдали черно, на границе — плавный переход.
	TArray<uint8> Alpha;
	Fog.BuildAlpha(Alpha, 2);
	TestEqual(TEXT("Маска — по байту на клетку"), Alpha.Num(), 128 * 128);
	if (Alpha.Num() == 128 * 128)
	{
		TestEqual(TEXT("В середине открытого черноты нет"), static_cast<int32>(Alpha[64 * 128 + 64]), 0);
		TestEqual(TEXT("Вдали от открытого чернота полная"), static_cast<int32>(Alpha[110 * 128 + 20]), 255);
		int32 Soft = 0;
		for (const uint8 Value : Alpha)
		{
			Soft += (Value > 0 && Value < 255) ? 1 : 0;
		}
		TestTrue(*FString::Printf(TEXT("На границе есть плавный переход (%d клеток)"), Soft), Soft > 50);

		TArray<uint8> Hard;
		Fog.BuildAlpha(Hard, 0);
		int32 HardSoft = 0;
		for (const uint8 Value : Hard)
		{
			HardSoft += (Value > 0 && Value < 255) ? 1 : 0;
		}
		TestEqual(TEXT("Без размытия переходных клеток нет"), HardSoft, 0);
	}

	// Стрелка героя у края поля карты прижимается внутрь, а не уходит за край.
	const FVector2D AreaMin(100.0, 50.0), AreaMax(700.0, 650.0);
	TestTrue(TEXT("Стрелка в середине поля не двигается"),
		UPdaScreenWidget::ClampArrowCenter(FVector2D(400.0, 300.0), AreaMin, AreaMax, 14.0).Equals(FVector2D(400.0, 300.0)));
	TestTrue(TEXT("Стрелка у правого-нижнего края прижата внутрь"),
		UPdaScreenWidget::ClampArrowCenter(FVector2D(700.0, 650.0), AreaMin, AreaMax, 14.0).Equals(FVector2D(686.0, 636.0)));
	TestTrue(TEXT("Стрелка у левого-верхнего края прижата внутрь"),
		UPdaScreenWidget::ClampArrowCenter(FVector2D(100.0, 50.0), AreaMin, AreaMax, 14.0).Equals(FVector2D(114.0, 64.0)));
	return true;
}

// ===========================================================================
// 2. На старте открыта только деревня; герой открывает круг вокруг себя.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapFogVillageAndHeroTest,
	"ContrarySurvivor.Pda.MapFog.VillageAtStartAndHeroOpensAround", MapFogTestFlags)

bool FMapFogVillageAndHeroTest::RunTest(const FString& Parameters)
{
	MapFogTest::FScopedMapSettings Scoped;
	UWorld* World = MapFogTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	ASaveTestPlayerCharacter* Player = MapFogTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(-3950.0, -4650.0, 100.0));
	UMapExplorationComponent* Exploration = Player ? Player->GetMapExploration() : nullptr;
	if (!TestNotNull(TEXT("У героя есть память о карте"), Exploration))
	{
		MapFogTest::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(TEXT("ContraryMapFogStartQA"));

	// Начало игры: зоны деревни на уровне нет — центр из настроек.
	Exploration->ResetToVillage();
	TestTrue(TEXT("Центр деревни открыт"), Exploration->IsWorldLocationRevealed(MapFogTest::VillageXY));
	TestTrue(TEXT("Край деревни (2400 от центра) открыт"), Exploration->IsWorldLocationRevealed(MapFogTest::VillageXY + FVector2D(2400.0, 0.0)));
	TestFalse(TEXT("За деревней (4000 от центра) закрыто"), Exploration->IsWorldLocationRevealed(MapFogTest::VillageXY + FVector2D(4000.0, 0.0)));
	TestFalse(TEXT("Логово волков закрыто"), Exploration->IsWorldLocationRevealed(MapFogTest::WolfDenXY));
	TestFalse(TEXT("Лагерь бандитов закрыт"), Exploration->IsWorldLocationRevealed(MapFogTest::BanditCampXY));
	const int32 VillageCells = Exploration->GetFog().CountRevealed();
	// Круг радиусом 3000 при клетке 366: 3,14 * 8,19 * 8,19 = 211 клеток.
	TestTrue(*FString::Printf(TEXT("Открыто около 211 клеток деревни (вышло %d)"), VillageCells), VillageCells > 180 && VillageCells < 245);

	// Герой пришёл к логову волков.
	const int32 RevisionBefore = Exploration->GetRevision();
	MapFogTest::MoveTo(Player, MapFogTest::WolfDenXY);
	TestTrue(TEXT("Герой открыл новое у логова"), Exploration->RevealAroundOwner());
	TestTrue(TEXT("Счётчик изменений вырос"), Exploration->GetRevision() > RevisionBefore);
	TestTrue(TEXT("Логово волков открыто"), Exploration->IsWorldLocationRevealed(MapFogTest::WolfDenXY));
	TestTrue(TEXT("В 1200 от героя открыто"), Exploration->IsWorldLocationRevealed(MapFogTest::WolfDenXY + FVector2D(0.0, 1200.0)));
	TestFalse(TEXT("В 2500 от героя закрыто"), Exploration->IsWorldLocationRevealed(MapFogTest::WolfDenXY + FVector2D(0.0, 2500.0)));
	TestTrue(TEXT("Деревня осталась открытой"), Exploration->IsWorldLocationRevealed(MapFogTest::VillageXY));
	const int32 RevisionAfter = Exploration->GetRevision();
	TestFalse(TEXT("Стоя на месте, герой нового не открывает"), Exploration->RevealAroundOwner());
	TestEqual(TEXT("Счётчик изменений на месте не растёт"), Exploration->GetRevision(), RevisionAfter);

	// Меню отладки.
	TestTrue(TEXT("Кнопка «открыть всю карту» отработала"), ContraryDebugTools::RevealWholeMap(Player));
	TestEqual(TEXT("Открыты все клетки"), Exploration->GetFog().CountRevealed(), 128 * 128);
	TestTrue(TEXT("Кнопка «закрыть заново» отработала"), ContraryDebugTools::HideMapAgain(Player));
	TestEqual(TEXT("После «закрыть заново» открыта одна деревня"), Exploration->GetFog().CountRevealed(), VillageCells);
	TestFalse(TEXT("Кнопки без героя не падают"), ContraryDebugTools::RevealWholeMap(nullptr));

	// Зона деревни на уровне: центр берётся с неё, а не из настроек.
	const FVector2D ZoneXY(15000.0, 12000.0);
	AVillageZone* Zone = MapFogTest::Spawn<AVillageZone>(World, FVector(ZoneXY.X, ZoneXY.Y, 0.0));
	if (TestNotNull(TEXT("Зона деревни заспавнена"), Zone))
	{
		TestTrue(TEXT("Центр деревни взят с зоны"), Exploration->FindVillageCenter().Equals(ZoneXY));
		Exploration->ResetToVillage();
		TestTrue(TEXT("Открыто вокруг зоны деревни"), Exploration->IsWorldLocationRevealed(ZoneXY));
		TestFalse(TEXT("Точка из настроек при живой зоне закрыта"), Exploration->IsWorldLocationRevealed(MapFogTest::VillageXY));
	}

	MapFogTest::Destroy(World);
	MapFogTest::DeleteSlot(TEXT("ContraryMapFogStartQA"));
	return true;
}

// ===========================================================================
// 3. Открытое переживает сохранение и «Продолжить»; «Новая игра» оставляет одну деревню;
//    сохранение без этих данных — деревня и место, где герой стоит.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapFogSaveTest,
	"ContrarySurvivor.Pda.MapFog.SurvivesSaveAndContinue", MapFogTestFlags)

bool FMapFogSaveTest::RunTest(const FString& Parameters)
{
	MapFogTest::FScopedMapSettings Scoped;
	const FString Slot = TEXT("ContraryMapFogSaveQA");
	MapFogTest::DeleteSlot(Slot);

	FPdaMapFog SavedFog;

	// Сессия 1: герой сходил к логову волков и сохранился.
	{
		UWorld* World = MapFogTest::Create();
		if (!TestNotNull(TEXT("Мир сессии 1 создан"), World))
		{
			return false;
		}
		ASaveTestPlayerCharacter* P1 = MapFogTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(-3950.0, -4650.0, 100.0));
		if (TestNotNull(TEXT("Игрок сессии 1 заспавнен"), P1) && TestNotNull(TEXT("Память о карте есть"), P1->GetMapExploration()))
		{
			P1->UseTestSaveSlot(Slot);
			P1->GetMapExploration()->ResetToVillage();
			P1->GetMapExploration()->RevealAroundWorld(MapFogTest::WolfDenXY, 1750.0f);
			SavedFog = P1->GetMapExploration()->GetFog();
			TestTrue(TEXT("Сохранение сессии 1 записано"), P1->SaveGame());

			if (const UContrarySaveGame* Save = P1->LoadOrCreateSaveObject())
			{
				TestEqual(TEXT("В сохранении записана ширина сетки"), Save->MapFogWidth, 128);
				TestEqual(TEXT("В сохранении записана высота сетки"), Save->MapFogHeight, 128);
				TestTrue(*FString::Printf(TEXT("Запись открытого компактна: %d байт"), Save->MapFogRuns.Num()),
					Save->MapFogRuns.Num() > 0 && Save->MapFogRuns.Num() < 300);
			}
		}
		MapFogTest::Destroy(World);
	}

	// Сессия 2: «Продолжить» возвращает открытое; «Новая игра» оставляет одну деревню.
	{
		UWorld* World = MapFogTest::Create();
		if (!TestNotNull(TEXT("Мир сессии 2 создан"), World))
		{
			return false;
		}
		ASaveTestPlayerCharacter* P2 = MapFogTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(-3950.0, -4650.0, 100.0));
		UMapExplorationComponent* Exploration = P2 ? P2->GetMapExploration() : nullptr;
		if (TestNotNull(TEXT("Игрок сессии 2 заспавнен"), Exploration))
		{
			P2->UseTestSaveSlot(Slot);
			Exploration->ResetToVillage();
			TestFalse(TEXT("До загрузки логово закрыто"), Exploration->IsWorldLocationRevealed(MapFogTest::WolfDenXY));

			TestTrue(TEXT("«Продолжить» сессии 2 отработал"), P2->LoadGameForContinue());
			TestTrue(TEXT("Логово волков открыто после загрузки"), Exploration->IsWorldLocationRevealed(MapFogTest::WolfDenXY));
			TestTrue(TEXT("Деревня открыта после загрузки"), Exploration->IsWorldLocationRevealed(MapFogTest::VillageXY));
			TestFalse(TEXT("Лагерь бандитов по-прежнему закрыт"), Exploration->IsWorldLocationRevealed(MapFogTest::BanditCampXY));
			TestTrue(TEXT("Открытое совпало с сохранённым клетка в клетку"), Exploration->GetFog() == SavedFog);

			// Сохранение, записанное до появления карты-исследования: стираем данные и читаем
			// заново. Герой при этом стоит у второго логова.
			if (UContrarySaveGame* Save = P2->LoadOrCreateSaveObject())
			{
				Save->MapFogWidth = 0;
				Save->MapFogHeight = 0;
				Save->MapFogRuns.Reset();
				P2->WriteSaveObject(Save);

				MapFogTest::MoveTo(P2, MapFogTest::SecondDenXY);
				Exploration->RestoreFromSave(Save);
				TestTrue(TEXT("Старое сохранение: деревня открыта"), Exploration->IsWorldLocationRevealed(MapFogTest::VillageXY));
				TestTrue(TEXT("Старое сохранение: место героя открыто"), Exploration->IsWorldLocationRevealed(MapFogTest::SecondDenXY));
				TestFalse(TEXT("Старое сохранение: логово волков закрыто"), Exploration->IsWorldLocationRevealed(MapFogTest::WolfDenXY));
			}
			TestTrue(TEXT("Старое сохранение загружается через «Продолжить»"), P2->LoadGameForContinue());
			TestTrue(TEXT("После него деревня открыта"), Exploration->IsWorldLocationRevealed(MapFogTest::VillageXY));
			TestFalse(TEXT("После него логово волков закрыто"), Exploration->IsWorldLocationRevealed(MapFogTest::WolfDenXY));

			// Испорченная запись читается так же, как её отсутствие.
			if (UContrarySaveGame* Save = P2->LoadOrCreateSaveObject())
			{
				Save->MapFogWidth = 128;
				Save->MapFogHeight = 128;
				Save->MapFogRuns = { 0x05, 0x03 };
				Exploration->RevealAll();
				Exploration->RestoreFromSave(Save);
				TestFalse(TEXT("Испорченная запись: карта не остаётся открытой целиком"), Exploration->IsWorldLocationRevealed(MapFogTest::BanditCampXY));
				TestTrue(TEXT("Испорченная запись: деревня открыта"), Exploration->IsWorldLocationRevealed(MapFogTest::VillageXY));
			}

			// «Новая игра».
			Exploration->RevealAll();
			P2->ResetToNewGame();
			TestTrue(TEXT("Новая игра: деревня открыта"), Exploration->IsWorldLocationRevealed(MapFogTest::VillageXY));
			TestFalse(TEXT("Новая игра: логово волков закрыто"), Exploration->IsWorldLocationRevealed(MapFogTest::WolfDenXY));
			TestFalse(TEXT("Новая игра: лагерь бандитов закрыт"), Exploration->IsWorldLocationRevealed(MapFogTest::BanditCampXY));
		}
		MapFogTest::Destroy(World);
	}

	MapFogTest::DeleteSlot(Slot);
	return true;
}

// ===========================================================================
// 4. Окно: подписи мест и отметки появляются только на открытом; цель отслеживаемого
//    задания видна и на неизведанном (по галочке); маска черноты не считается зря.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapFogWindowTest,
	"ContrarySurvivor.Pda.MapFog.PlaceLabelAppearsWhenPlaceIsOpened", MapFogTestFlags)

bool FMapFogWindowTest::RunTest(const FString& Parameters)
{
	MapFogTest::FScopedMapSettings Scoped;
	UWorld* World = MapFogTest::Create();
	if (!TestNotNull(TEXT("Тестовый мир создан"), World))
	{
		return false;
	}
	ASaveTestPlayerCharacter* Player = MapFogTest::Spawn<ASaveTestPlayerCharacter>(World, FVector(-3950.0, -4650.0, 100.0));
	UMapExplorationComponent* Exploration = Player ? Player->GetMapExploration() : nullptr;
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	// Костры: один в деревне, один у логова волков (он же цель отслеживаемого задания),
	// один у лагеря бандитов (цель неотслеживаемого задания).
	ACampfire* VillageFire = MapFogTest::Spawn<ACampfire>(World, FVector(-3880.0, -4320.0, 0.0));
	ACampfire* DenFire = MapFogTest::Spawn<ACampfire>(World, FVector(MapFogTest::WolfDenXY.X, MapFogTest::WolfDenXY.Y, 0.0));
	ACampfire* CampFire = MapFogTest::Spawn<ACampfire>(World, FVector(MapFogTest::BanditCampXY.X, MapFogTest::BanditCampXY.Y, 0.0));
	// Зона деревни стоит рядом с точкой «Деревня» из списка мест — это одно место, а не два.
	AVillageZone* Zone = MapFogTest::Spawn<AVillageZone>(World, FVector(-3900.0, -4600.0, 0.0));
	if (!Exploration || !Quests || !VillageFire || !DenFire || !CampFire || !Zone)
	{
		AddError(TEXT("Не заспавнены герой, костры и зона деревни"));
		MapFogTest::Destroy(World);
		return false;
	}
	Player->UseTestSaveSlot(TEXT("ContraryMapFogWindowQA"));
	DenFire->Tags.Add(FName(TEXT("FogTargetDen")));
	CampFire->Tags.Add(FName(TEXT("FogTargetCamp")));

	auto MakeQuest = [](const TCHAR* Id, const TCHAR* TargetTag)
	{
		FQuest Quest;
		Quest.QuestId = FName(Id);
		Quest.Title = FText::FromString(FString::Printf(TEXT("QA %s"), Id));
		Quest.Kind = EQuestKind::Main;
		Quest.TargetCount = 3; // цель не выполнена — задание идёт и ведёт к цели
		Quest.RequiredItemCount = 0;
		Quest.RewardMoney = 0.0f;
		Quest.State = EQuestState::Active;
		Quest.MapMarkerTag = FName(TargetTag);
		return Quest;
	};
	const FQuest Tracked = MakeQuest(TEXT("FogTracked"), TEXT("FogTargetDen"));
	const FQuest Other = MakeQuest(TEXT("FogOther"), TEXT("FogTargetCamp"));
	Quests->RestoreQuests({ Tracked, Other }, Tracked.QuestId);

	Exploration->ResetToVillage();
	UPdaScreenWidget* Window = CreateWidget<UPdaScreenWidget>(World, UPdaScreenWidget::StaticClass());
	if (!TestNotNull(TEXT("Окно создалось"), Window))
	{
		MapFogTest::Destroy(World);
		return false;
	}

	// --- Начало игры: видна одна деревня ---
	Window->InitPda(Player);
	TestEqual(TEXT("Подпись «Деревня» одна (зона и точка списка — одно место)"), MapFogTest::CountLabel(Window, TEXT("Деревня")), 1);
	TestEqual(TEXT("Подписи «Логово волков» нет"), MapFogTest::CountLabel(Window, TEXT("Логово волков")), 0);
	TestEqual(TEXT("Подписи «Лагерь бандитов» нет"), MapFogTest::CountLabel(Window, TEXT("Лагерь бандитов")), 0);
	TestEqual(TEXT("На карте только деревня и её костёр"), Window->GetPlaceMapMarkerCount(), 2);
	TestEqual(TEXT("На неизведанном видна только цель отслеживаемого задания"), Window->GetQuestMapMarkerCount(), 1);
	TestEqual(TEXT("Маска черноты построена по клеткам сетки"), Window->GetFogAlpha().Num(), 128 * 128);

	// Ничего не изменилось — маска заново не считается.
	const int32 Rebuilds = Window->GetFogRebuildCount();
	Window->RefreshNow();
	Window->RefreshNow();
	TestEqual(TEXT("Маска не пересчитывается, пока открытое не менялось"), Window->GetFogRebuildCount(), Rebuilds);

	// Галочка выключена — на неизведанном нет и цели отслеживаемого задания.
	Scoped.Settings->bShowTrackedQuestOnUnexplored = false;
	Window->RefreshNow();
	TestEqual(TEXT("Без галочки цель на неизведанном не видна"), Window->GetQuestMapMarkerCount(), 0);
	Scoped.Settings->bShowTrackedQuestOnUnexplored = true;

	// --- Герой побывал у логова волков ---
	MapFogTest::MoveTo(Player, MapFogTest::WolfDenXY);
	TestTrue(TEXT("Герой открыл логово"), Exploration->RevealAroundOwner());
	Window->RefreshNow();
	TestEqual(TEXT("Появилась подпись «Логово волков» — одна (второе логово закрыто)"), MapFogTest::CountLabel(Window, TEXT("Логово волков")), 1);
	TestEqual(TEXT("Подпись «Деревня» осталась"), MapFogTest::CountLabel(Window, TEXT("Деревня")), 1);
	TestEqual(TEXT("Подписи «Лагерь бандитов» по-прежнему нет"), MapFogTest::CountLabel(Window, TEXT("Лагерь бандитов")), 0);
	TestEqual(TEXT("На карте деревня, логово и два костра"), Window->GetPlaceMapMarkerCount(), 4);
	TestEqual(TEXT("Маска пересчитана один раз"), Window->GetFogRebuildCount(), Rebuilds + 1);
	TestEqual(TEXT("Цель неотслеживаемого задания на неизведанном не видна"), Window->GetQuestMapMarkerCount(), 1);

	// --- Герой дошёл до лагеря бандитов: видна и цель второго задания ---
	Exploration->RevealAroundWorld(MapFogTest::BanditCampXY, 1750.0f);
	Window->RefreshNow();
	TestEqual(TEXT("Появилась подпись «Лагерь бандитов»"), MapFogTest::CountLabel(Window, TEXT("Лагерь бандитов")), 1);
	TestEqual(TEXT("Видны цели обоих заданий"), Window->GetQuestMapMarkerCount(), 2);

	// --- Вся карта открыта: подписаны оба логова ---
	Exploration->RevealAll();
	Window->RefreshNow();
	TestEqual(TEXT("На открытой карте подписаны оба логова"), MapFogTest::CountLabel(Window, TEXT("Логово волков")), 2);

	MapFogTest::Destroy(World);
	MapFogTest::DeleteSlot(TEXT("ContraryMapFogWindowQA"));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
