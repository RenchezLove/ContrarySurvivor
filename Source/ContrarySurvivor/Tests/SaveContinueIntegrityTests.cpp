// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS-тест КРИТИЧЕСКОГО бага сейвов (ADR-076 п.1, живой тест Рината 22.08):
// «сохранился → вышел → зашёл → вышел → зашёл» — второй вход стирал сохранение (рюкзак пуст,
// сюжет сначала), но интро не проигрывалось (флаги выживали через CopyRetentionData).
//
// КОРЕНЬ (по коду): LoadGameForContinue телепортировал игрока в зону костра ДО восстановления
// рюкзака/журнала; телепорт синхронно жал триггер костра, и автосейв писал в слот пустое
// состояние. Тест воспроизводит цикл двух «перезапусков» (свежий актор игрока = новый запуск
// процесса: память пустая, слот на диске общий) и требует, чтобы слот пережил оба.
//
// Слот ИЗОЛИРОВАННЫЙ (ASaveTestPlayerCharacter::UseTestSaveSlot) — боевой ContrarySave не
// трогается. Запуск (гоняет game-lead/qa, НЕ cpp-dev — правило Рината):
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.SaveIntegrity; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SaveTestPlayerCharacter.h"
#include "ContrarySurvivor/Actors/Campfire.h"
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "UInventoryComponent.h"
#include "AQuestItem.h"
#include "APistol.h"       // ствол в слоте переживает перезапуск (фикс 23.08)
#include "ARangedWeapon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h" // рефлексия: обнулить антиспам автосейва костра

static constexpr EAutomationTestFlags SaveIntegrityTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// Транзиентный игровой мир (копия обвязки CombatAutomationTests.cpp — статики свои на файл).
namespace SaveIntegrityTestWorld
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
}

// ===========================================================================
// Два «перезапуска» подряд не стирают сохранение (ADR-076 п.1)
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveContinueTwiceKeepsProgressTest,
	"ContrarySurvivor.SaveIntegrity.ContinueTwiceKeepsProgress", SaveIntegrityTestFlags)

bool FSaveContinueTwiceKeepsProgressTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContrarySaveWipeQA");
	if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		UGameplayStatics::DeleteGameInSlot(Slot, 0); // хвост прошлого прогона
	}

	UWorld* World = SaveIntegrityTestWorld::Create();
	if (!World)
	{
		AddError(TEXT("Не создан тестовый мир"));
		return false;
	}

	const FVector CampfireLoc(0.0f, 0.0f, 100.0f);
	const FVector FarLoc(5000.0f, 0.0f, 100.0f);
	ACampfire* Campfire = SaveIntegrityTestWorld::Spawn<ACampfire>(World, CampfireLoc);
	if (!Campfire)
	{
		AddError(TEXT("Не заспавнен костёр"));
		SaveIntegrityTestWorld::Destroy(World);
		return false;
	}

	// Антиспам автосейва (2 с) — в ноль: все три «входа в зону» происходят в одном мировом
	// времени теста, а в жизни между ними целые сессии. Поле protected — ставим рефлексией.
	if (FFloatProperty* CooldownProp = FindFProperty<FFloatProperty>(ACampfire::StaticClass(), TEXT("AutoSaveCooldown")))
	{
		CooldownProp->SetPropertyValue_InContainer(Campfire, 0.0f);
	}
	else
	{
		AddError(TEXT("Нет свойства AutoSaveCooldown"));
	}

	// --- СЕССИЯ 1: живой прогресс, сохранение у костра, «выход» ---
	{
		ASaveTestPlayerCharacter* P1 = SaveIntegrityTestWorld::Spawn<ASaveTestPlayerCharacter>(World, FarLoc);
		if (!P1)
		{
			AddError(TEXT("Не заспавнен игрок сессии 1"));
			SaveIntegrityTestWorld::Destroy(World);
			return false;
		}
		P1->UseTestSaveSlot(Slot);

		// Прогресс: деньги, предмет в рюкзаке, квест в журнале.
		if (UStatsComponent* Stats = P1->FindComponentByClass<UStatsComponent>())
		{
			Stats->AddMoney(100.0f); // стартовые 50 -> 150
		}
		if (UInventoryComponent* Inv = P1->GetInventory())
		{
			FActorSpawnParameters Sp;
			Sp.Owner = P1;
			Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AQuestItem* Pelt = World->SpawnActor<AQuestItem>(AQuestItem::StaticClass(), FarLoc, FRotator::ZeroRotator, Sp))
			{
				Pelt->ItemName = TEXT("Шкура волка");
				Pelt->StackCount = 2;
				Pelt->SetActorHiddenInGame(true);
				Pelt->SetActorEnableCollision(false);
				Inv->AddItem(Pelt);
			}
		}
		if (UQuestComponent* Quests = P1->FindComponentByClass<UQuestComponent>())
		{
			FQuest Q;
			Q.QuestId = FName(TEXT("QAWipeQuest"));
			Q.Title = FText::FromString(TEXT("Тестовый квест"));
			Q.TargetCount = 5;
			Q.KillTargetTag = FName(TEXT("Wolf"));
			Quests->OfferQuest(Q);
			Quests->AcceptQuest(Q.QuestId);
		}

		// Игрок сам приходит к костру — телепорт в зону жмёт триггер, автосейв пишет ПОЛНОЕ
		// состояние (это же — проверка, что overlap-механика в headless-мире реально работает:
		// без неё тест не имел бы права зеленеть).
		P1->SetActorLocation(CampfireLoc + FVector(100.0f, 0.0f, 0.0f), /*bSweep=*/false,
			nullptr, ETeleportType::TeleportPhysics);
		TestTrue(TEXT("Автосейв костра от входа в зону сработал (триггер жив в headless-мире)"),
			UGameplayStatics::DoesSaveGameExist(Slot, 0));

		// Дублируем явным сохранением — детерминизм не зависит от порядка overlap-событий.
		TestTrue(TEXT("Сохранение у костра прошло"), P1->SaveGame());

		// Слот на диске богатый.
		if (const UContrarySaveGame* Disk = Cast<UContrarySaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)))
		{
			TestTrue(TEXT("Слот после сессии 1: рюкзак не пуст"), Disk->InventoryEntries.Num() > 0);
			TestTrue(TEXT("Слот после сессии 1: журнал не пуст"), Disk->Quests.Num() > 0);
			TestEqual(TEXT("Слот после сессии 1: деньги"), Disk->Money, 150.0f);
		}
		else
		{
			AddError(TEXT("Слот после сессии 1 не читается"));
		}

		P1->Destroy(); // «выход из игры»
	}

	// --- СЕССИЯ 2 (перезапуск №1): свежий игрок, «Продолжить» ---
	{
		ASaveTestPlayerCharacter* P2 = SaveIntegrityTestWorld::Spawn<ASaveTestPlayerCharacter>(World, FarLoc);
		if (!P2)
		{
			AddError(TEXT("Не заспавнен игрок сессии 2"));
			SaveIntegrityTestWorld::Destroy(World);
			return false;
		}
		P2->UseTestSaveSlot(Slot);
		TestTrue(TEXT("Перезапуск №1: «Продолжить» отработал"), P2->LoadGameForContinue());

		// Восстановление телепортировало игрока к сейв-точке — В ЗОНУ костра (механизм бага задет).
		TestTrue(TEXT("Перезапуск №1: игрок в зоне костра после «Продолжить»"),
			FVector::Dist2D(P2->GetActorLocation(), CampfireLoc) <= Campfire->GetSafeZoneRadius() + 1.0f);

		// Память восстановлена.
		TestTrue(TEXT("Перезапуск №1: рюкзак восстановлен"),
			P2->GetInventory() && P2->GetInventory()->GetInventoryItems().Num() > 0);
		if (const UQuestComponent* Quests = P2->FindComponentByClass<UQuestComponent>())
		{
			TestTrue(TEXT("Перезапуск №1: журнал восстановлен"), Quests->GetQuests().Num() > 0);
		}

		// ⛔ ГЛАВНАЯ ПРОВЕРКА БАГА: слот на диске ПОСЛЕ «Продолжить» всё ещё богатый.
		// До фикса телепорт в зону костра затирал его пустым состоянием ровно здесь.
		if (const UContrarySaveGame* Disk = Cast<UContrarySaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)))
		{
			TestTrue(TEXT("Слот ПОСЛЕ перезапуска №1: рюкзак НЕ стёрт (корень ADR-076 п.1)"),
				Disk->InventoryEntries.Num() > 0);
			TestTrue(TEXT("Слот ПОСЛЕ перезапуска №1: журнал НЕ стёрт"), Disk->Quests.Num() > 0);
			TestEqual(TEXT("Слот ПОСЛЕ перезапуска №1: деньги целы"), Disk->Money, 150.0f);
		}
		else
		{
			AddError(TEXT("Слот после перезапуска №1 не читается"));
		}

		P2->Destroy(); // «выход из игры»
	}

	// --- СЕССИЯ 3 (перезапуск №2): у Рината здесь всё пропадало ---
	{
		ASaveTestPlayerCharacter* P3 = SaveIntegrityTestWorld::Spawn<ASaveTestPlayerCharacter>(World, FarLoc);
		if (!P3)
		{
			AddError(TEXT("Не заспавнен игрок сессии 3"));
			SaveIntegrityTestWorld::Destroy(World);
			return false;
		}
		P3->UseTestSaveSlot(Slot);
		TestTrue(TEXT("Перезапуск №2: «Продолжить» отработал"), P3->LoadGameForContinue());

		TestTrue(TEXT("Перезапуск №2: рюкзак ЖИВ (симптом Рината закрыт)"),
			P3->GetInventory() && P3->GetInventory()->GetInventoryItems().Num() > 0);
		if (const UQuestComponent* Quests = P3->FindComponentByClass<UQuestComponent>())
		{
			TestTrue(TEXT("Перезапуск №2: журнал ЖИВ"), Quests->GetQuests().Num() > 0);
		}
		if (const UStatsComponent* Stats = P3->FindComponentByClass<UStatsComponent>())
		{
			TestEqual(TEXT("Перезапуск №2: деньги целы"), Stats->GetMoney(), 150.0f);
		}

		P3->Destroy();
	}

	// Уборка: тестовый слот с диска.
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	SaveIntegrityTestWorld::Destroy(World);
	return true;
}

// ===========================================================================
// Ствол В СЛОТЕ ОРУЖИЯ переживает перезапуск (фикс 23.08, решение лида: «та же
// категория, что критбаг сейвов» — взятый в слот пистолет раньше не попадал в
// сейв вовсе и терялся; возможно, ровно так пропал пистолет Рината)
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveSlotFirearmSurvivesContinueTest,
	"ContrarySurvivor.SaveIntegrity.SlotFirearmSurvivesContinue", SaveIntegrityTestFlags)

bool FSaveSlotFirearmSurvivesContinueTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("ContrarySlotGunQA");
	if (UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		UGameplayStatics::DeleteGameInSlot(Slot, 0);
	}

	UWorld* World = SaveIntegrityTestWorld::Create();
	if (!World)
	{
		AddError(TEXT("Не создан тестовый мир"));
		return false;
	}
	const FVector CampfireLoc(0.0f, 0.0f, 100.0f);
	ACampfire* Campfire = SaveIntegrityTestWorld::Spawn<ACampfire>(World, CampfireLoc);
	if (Campfire)
	{
		if (FFloatProperty* CooldownProp = FindFProperty<FFloatProperty>(ACampfire::StaticClass(), TEXT("AutoSaveCooldown")))
		{
			CooldownProp->SetPropertyValue_InContainer(Campfire, 0.0f);
		}
	}

	// Сессия 1: пистолет покупным путём (в рюкзак -> клик по плитке -> слот) и сейв у костра.
	{
		ASaveTestPlayerCharacter* P1 = SaveIntegrityTestWorld::Spawn<ASaveTestPlayerCharacter>(
			World, FVector(4000.0f, 0.0f, 100.0f));
		if (!P1)
		{
			AddError(TEXT("Не заспавнен игрок сессии 1"));
			SaveIntegrityTestWorld::Destroy(World);
			return false;
		}
		P1->UseTestSaveSlot(Slot);

		FActorSpawnParameters Sp;
		Sp.Owner = P1;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APistol* Pistol = World->SpawnActor<APistol>(APistol::StaticClass(),
			P1->GetActorLocation(), FRotator::ZeroRotator, Sp);
		if (!Pistol || !P1->GetInventory())
		{
			AddError(TEXT("Не заспавнен пистолет / нет рюкзака"));
			SaveIntegrityTestWorld::Destroy(World);
			return false;
		}
		Pistol->SetActorHiddenInGame(true);
		Pistol->SetActorEnableCollision(false);
		P1->GetInventory()->AddItem(Pistol);
		TestTrue(TEXT("Пистолет взят в слот (как кликом по плитке)"), P1->TryAdoptRangedWeapon(Pistol));
		TestNotNull(TEXT("Слот занят"), P1->GetRangedWeaponInstance());
		// Единая бухгалтерия (п.6): резерв взятого ствола уехал видимой пачкой в рюкзак.
		if (ARangedWeapon* Adopted = Cast<ARangedWeapon>(P1->GetRangedWeaponInstance()))
		{
			TestEqual(TEXT("Резерв ствола после взятия пуст"), Adopted->GetCurrentAmmoReserve(), 0);
		}
		TestTrue(TEXT("Патроны ствола лежат пачкой в рюкзаке"), P1->GetReserveAmmoInInventory() > 0);

		P1->SetActorLocation(CampfireLoc + FVector(100.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		TestTrue(TEXT("Сохранение у костра прошло"), P1->SaveGame());

		// Слот на диске: запись ствола с признаком «в слоте» существует.
		if (const UContrarySaveGame* Disk = Cast<UContrarySaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)))
		{
			bool bSlotEntryFound = false;
			for (const FSavedInventoryEntry& Entry : Disk->InventoryEntries)
			{
				if (Entry.bEquipped && Entry.ClassPath.Contains(TEXT("Pistol")))
				{
					bSlotEntryFound = true;
					break;
				}
			}
			TestTrue(TEXT("В сейве есть запись ствола слота (bEquipped, класс пистолета)"), bSlotEntryFound);
		}
		else
		{
			AddError(TEXT("Слот сессии 1 не читается"));
		}

		P1->Destroy(); // «выход из игры»
	}

	// Сессия 2 (перезапуск): «Продолжить» возвращает ствол В СЛОТ с доступными патронами.
	{
		ASaveTestPlayerCharacter* P2 = SaveIntegrityTestWorld::Spawn<ASaveTestPlayerCharacter>(
			World, FVector(4000.0f, 0.0f, 100.0f));
		if (!P2)
		{
			AddError(TEXT("Не заспавнен игрок сессии 2"));
			SaveIntegrityTestWorld::Destroy(World);
			return false;
		}
		P2->UseTestSaveSlot(Slot);
		TestTrue(TEXT("«Продолжить» отработал"), P2->LoadGameForContinue());

		TestNotNull(TEXT("Ствол ВЕРНУЛСЯ В СЛОТ после перезапуска (раньше терялся)"),
			P2->GetRangedWeaponInstance());
		TestTrue(TEXT("Патроны доступны пачкой в рюкзаке"), P2->GetReserveAmmoInInventory() > 0);
		if (ARangedWeapon* Restored = Cast<ARangedWeapon>(P2->GetRangedWeaponInstance()))
		{
			TestEqual(TEXT("Резерв восстановленного ствола пуст (единая бухгалтерия)"),
				Restored->GetCurrentAmmoReserve(), 0);
			TestTrue(TEXT("Обойма восстановленного ствола не пуста (полная обойма конструктора)"),
				Restored->GetCurrentAmmoInClip() > 0);
		}

		P2->Destroy();
	}

	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	SaveIntegrityTestWorld::Destroy(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
