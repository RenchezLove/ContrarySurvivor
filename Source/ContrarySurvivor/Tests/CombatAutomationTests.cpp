// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тесты боевого кора (CU-free регрессия). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.Combat; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Покрывают логику, которой НЕ нужен навмеш: UStatsComponent (урон/смерть/восстановление),
// числа урона оружия и брони, резист брони, смерть/респаун игрока (стат-форс), квест-стейт+награда.
// НЕ покрывается headless (нужен PIE/контроллер/HUD/навмеш — помечено в тестах/отчёте):
//   - bDeathScreen гейтит инпут + возврат инпута (логика контроллера+HUD);
//   - погоня по навмешу (cs.TestWolfChase в PIE).
//
// Пороги (HP/урон/резист/награда) взяты из РЕАЛЬНОГО кода (StatsComponent/APistol/AMeleeWeapon/
// AArmor/EnemyCharacter/QuestComponent), не выдуманы.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CombatTestProbe.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Characters/EnemyCharacter.h"
#include "APistol.h"
#include "AMeleeWeapon.h"
#include "AHeadArmor.h"
#include "ATorsoArmor.h"
#include "APantsArmor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h" // FURL
#include "GameFramework/Actor.h"

// Контекст всех приложений (editor/client/commandlet) + продуктовый фильтр (это игровые тесты).
static constexpr EAutomationTestFlags CombatTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

// ---------------------------------------------------------------------------
// Вспомогательное: транзиентный игровой мир для тестов, требующих спавна акторов
// (резист брони, респаун игрока, квест-награда). Логики навмеша не задействуют.
// ---------------------------------------------------------------------------
namespace CombatTestWorld
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

	// Спавн актора с прогоном BeginPlay в безопасной точке (над полом нет — мир пустой).
	template <typename T>
	static T* Spawn(UWorld* World, const FVector& Loc = FVector(0.f, 0.f, 100.f))
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
// 1. UStatsComponent — урон клампится по HP, OnDeath на 0, dead игнорирует урон
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatStatsDamageDeathTest,
	"ContrarySurvivor.Combat.Stats.DamageClampAndDeath", CombatTestFlags)
bool FCombatStatsDamageDeathTest::RunTest(const FString& Parameters)
{
	UStatsComponent* Stats = NewObject<UStatsComponent>();
	TestNotNull(TEXT("Stats created"), Stats);
	if (!Stats) { return false; }

	UCombatTestProbe* Probe = NewObject<UCombatTestProbe>();
	Stats->OnDeath.AddDynamic(Probe, &UCombatTestProbe::HandleDeath);

	Stats->InitHealth(100.0f, /*bSetToMax=*/true);
	TestEqual(TEXT("Health = 100 after init"), Stats->GetHealth(), 100.0f);
	TestFalse(TEXT("Not dead after init"), Stats->IsDead());

	// Частичный урон: applied == requested, HP убывает.
	const float Applied1 = Stats->ApplyDamage(30.0f);
	TestEqual(TEXT("Applied 30 of 30"), Applied1, 30.0f);
	TestEqual(TEXT("Health 70"), Stats->GetHealth(), 70.0f);
	TestFalse(TEXT("Still alive at 70"), Stats->IsDead());

	// Смертельный урон: HP не уходит в минус (кламп в 0), OnDeath срабатывает один раз.
	const float Applied2 = Stats->ApplyDamage(1000.0f);
	TestEqual(TEXT("Applied clamped to remaining 70"), Applied2, 70.0f);
	TestEqual(TEXT("Health clamped to 0 (no negative)"), Stats->GetHealth(), 0.0f);
	TestTrue(TEXT("Dead at 0 HP"), Stats->IsDead());
	TestEqual(TEXT("OnDeath fired exactly once"), Probe->DeathCount, 1);

	// Урон по мёртвому игнорируется (0 applied, OnDeath не повторяется).
	const float Applied3 = Stats->ApplyDamage(10.0f);
	TestEqual(TEXT("Dead ignores further damage"), Applied3, 0.0f);
	TestEqual(TEXT("OnDeath still once"), Probe->DeathCount, 1);

	return true;
}

// ===========================================================================
// 2. UStatsComponent — RestoreState снимает смерть и восстанавливает статы
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatStatsRestoreTest,
	"ContrarySurvivor.Combat.Stats.RestoreStateClearsDeath", CombatTestFlags)
bool FCombatStatsRestoreTest::RunTest(const FString& Parameters)
{
	UStatsComponent* Stats = NewObject<UStatsComponent>();
	if (!Stats) { return false; }

	Stats->InitHealth(100.0f, true);
	Stats->ApplyDamage(1000.0f); // убиваем
	TestTrue(TEXT("Dead before restore"), Stats->IsDead());

	Stats->RestoreState(/*HP=*/100.0f, /*Hunger=*/80.0f, /*Thirst=*/90.0f, /*Money=*/50.0f);
	TestFalse(TEXT("Alive after restore"), Stats->IsDead());
	TestEqual(TEXT("HP restored"), Stats->GetHealth(), 100.0f);
	TestEqual(TEXT("Hunger restored"), Stats->GetHunger(), 80.0f);
	TestEqual(TEXT("Thirst restored"), Stats->GetThirst(), 90.0f);
	TestEqual(TEXT("Money restored"), Stats->GetMoney(), 50.0f);

	return true;
}

// ===========================================================================
// 3. UStatsComponent — добивающий хит: applied клампится остатком (requested vs applied)
//    Моделирует пистолетный выстрел (25) по врагу-волку с остатком HP < 25.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFinishingBlowTest,
	"ContrarySurvivor.Combat.Stats.FinishingBlowClamp", CombatTestFlags)
bool FCombatFinishingBlowTest::RunTest(const FString& Parameters)
{
	UStatsComponent* Stats = NewObject<UStatsComponent>();
	if (!Stats) { return false; }

	Stats->InitHealth(40.0f, true); // HP волка
	const float Shot1 = Stats->ApplyDamage(25.0f);
	TestEqual(TEXT("First shot applies full 25"), Shot1, 25.0f);
	TestEqual(TEXT("HP 15 after first shot"), Stats->GetHealth(), 15.0f);

	// Добивающий выстрел: requested 25, applied клампится остатком 15.
	const float Shot2 = Stats->ApplyDamage(25.0f);
	TestEqual(TEXT("Finishing shot applied clamps to remaining 15 (requested 25)"), Shot2, 15.0f);
	TestEqual(TEXT("HP 0 after finishing shot"), Stats->GetHealth(), 0.0f);
	TestTrue(TEXT("Dead after finishing shot"), Stats->IsDead());

	return true;
}

// ===========================================================================
// 4. Урон оружия — дефолты CDO: пистолет 25, нож 35 (числа из APistol/AMeleeWeapon ctor)
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatWeaponDamageTest,
	"ContrarySurvivor.Combat.Weapon.DamageDefaults", CombatTestFlags)
bool FCombatWeaponDamageTest::RunTest(const FString& Parameters)
{
	const APistol* PistolCDO = GetDefault<APistol>();
	const AMeleeWeapon* KnifeCDO = GetDefault<AMeleeWeapon>();
	TestNotNull(TEXT("Pistol CDO"), PistolCDO);
	TestNotNull(TEXT("Knife CDO"), KnifeCDO);
	if (!PistolCDO || !KnifeCDO) { return false; }

	TestEqual(TEXT("Pistol damage 25"), PistolCDO->GetDamage(), 25.0f);
	TestEqual(TEXT("Knife damage 35"), KnifeCDO->GetDamage(), 35.0f);

	return true;
}

// ===========================================================================
// 5. Броня — значения слотов (CDO) + резист ComputeArmoredDamage (50% и кап 75%)
//    Резист тестируется на реальном AEnemyCharacter (наследует EquipArmor/ComputeArmoredDamage).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatArmorResistTest,
	"ContrarySurvivor.Combat.Armor.ProtectionAndResist", CombatTestFlags)
bool FCombatArmorResistTest::RunTest(const FString& Parameters)
{
	// (a) Дефолты долей слотов из CDO (числа из ctor AHeadArmor/ATorsoArmor/APantsArmor).
	TestEqual(TEXT("Head armor 0.10"), GetDefault<AHeadArmor>()->GetArmorProtection(), 0.10f);
	TestEqual(TEXT("Torso armor 0.25"), GetDefault<ATorsoArmor>()->GetArmorProtection(), 0.25f);
	TestEqual(TEXT("Pants armor 0.15"), GetDefault<APantsArmor>()->GetArmorProtection(), 0.15f);

	// (b) Формула резиста на живом персонаже в транзиентном мире.
	UWorld* World = CombatTestWorld::Create();
	TestNotNull(TEXT("Test world"), World);
	if (!World) { return false; }

	bool bOk = true;
	{
		AEnemyCharacter* Enemy = CombatTestWorld::Spawn<AEnemyCharacter>(World);
		TestNotNull(TEXT("Enemy spawned"), Enemy);
		if (Enemy)
		{
			// Без брони: урон проходит без снижения.
			TestEqual(TEXT("No armor -> full 100"), Enemy->ComputeArmoredDamage(100.0f), 100.0f);

			// Сумма 0.10+0.25+0.15 = 0.50 -> урон 100 * (1-0.50) = 50.
			AHeadArmor* Head = CombatTestWorld::Spawn<AHeadArmor>(World);
			ATorsoArmor* Torso = CombatTestWorld::Spawn<ATorsoArmor>(World);
			APantsArmor* Pants = CombatTestWorld::Spawn<APantsArmor>(World);
			if (Head && Torso && Pants)
			{
				Enemy->EquipArmor(Head);
				Enemy->EquipArmor(Torso);
				Enemy->EquipArmor(Pants);
				TestEqual(TEXT("Armor 0.50 -> 50 dmg"), Enemy->ComputeArmoredDamage(100.0f), 50.0f);

				// Кап 0.75: завышаем долю торса до 0.9 (сумма > кап) -> урон не падает ниже 25.
				Torso->ArmorProtection = 0.9f;
				TestEqual(TEXT("Resist capped at 0.75 -> 25 dmg"), Enemy->ComputeArmoredDamage(100.0f), 25.0f);
			}
			else
			{
				AddError(TEXT("Armor spawn failed"));
				bOk = false;
			}
		}
		else
		{
			bOk = false;
		}
	}

	CombatTestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 6. Смерть/респаун игрока (стат-форс). bDeathScreen-гейт инпута — PIE-only (нужен контроллер/HUD).
//    Здесь проверяем headless-часть: смерть через Stats, затем Respawn() форсит HP/Hunger/Thirst.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatPlayerRespawnTest,
	"ContrarySurvivor.Combat.Player.DeathRespawnRestoresStats", CombatTestFlags)
bool FCombatPlayerRespawnTest::RunTest(const FString& Parameters)
{
	UWorld* World = CombatTestWorld::Create();
	TestNotNull(TEXT("Test world"), World);
	if (!World) { return false; }

	bool bOk = true;
	{
		APlayerCharacter* Player = CombatTestWorld::Spawn<APlayerCharacter>(World);
		TestNotNull(TEXT("Player spawned"), Player);
		UStatsComponent* Stats = Player ? Player->GetStats() : nullptr;
		TestNotNull(TEXT("Player stats"), Stats);

		if (Player && Stats)
		{
			const float MaxHP = Stats->GetMaxHealth();
			const float SurvMax = Stats->GetSurvivalMax();

			// Подводим к смерти: летальный урон через Stats (как боевой пайплайн).
			Stats->ApplyDamage(MaxHP + 1000.0f);
			TestTrue(TEXT("Player dead after lethal damage"), Stats->IsDead());

			// Респаун: должен снять смерть и форснуть полные HP/Hunger/Thirst.
			Player->Respawn();
			TestFalse(TEXT("Player alive after respawn"), Stats->IsDead());
			TestEqual(TEXT("HP forced to max on respawn"), Stats->GetHealth(), MaxHP);
			TestEqual(TEXT("Hunger forced to max on respawn"), Stats->GetHunger(), SurvMax);
			TestEqual(TEXT("Thirst forced to max on respawn"), Stats->GetThirst(), SurvMax);
		}
		else
		{
			bOk = false;
		}
	}

	CombatTestWorld::Destroy(World);
	return bOk;
}

// ===========================================================================
// 7. Квест: offer -> accept -> kill-прогресс -> Completed -> turn-in -> TurnedIn + награда.
//    Используем настоящего игрока (несёт UQuestComponent + UStatsComponent владельца).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatQuestRewardTest,
	"ContrarySurvivor.Combat.Quest.AcceptCompleteTurnInReward", CombatTestFlags)
bool FCombatQuestRewardTest::RunTest(const FString& Parameters)
{
	UWorld* World = CombatTestWorld::Create();
	TestNotNull(TEXT("Test world"), World);
	if (!World) { return false; }

	bool bOk = true;
	{
		APlayerCharacter* Player = CombatTestWorld::Spawn<APlayerCharacter>(World);
		UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
		UStatsComponent* Stats = Player ? Player->GetStats() : nullptr;
		TestNotNull(TEXT("Player"), Player);
		TestNotNull(TEXT("Quests"), Quests);
		TestNotNull(TEXT("Stats"), Stats);

		if (Player && Quests && Stats)
		{
			const float MoneyBefore = Stats->GetMoney();

			// Kill-only квест: убить 1 «Wolf», награда 150.
			FQuest Q;
			Q.QuestId = FName(TEXT("TestKillWolf"));
			Q.Title = TEXT("Test");
			Q.Type = EQuestType::Kill;
			Q.KillTargetTag = FName(TEXT("Wolf"));
			Q.TargetCount = 1;
			Q.RequiredItemCount = 0;
			Q.RewardMoney = 150.0f;

			Quests->OfferQuest(Q);
			const FQuest* Offered = Quests->FindQuest(Q.QuestId);
			TestNotNull(TEXT("Quest offered"), Offered);
			if (Offered) { TestTrue(TEXT("Offered = NotStarted"), Offered->State == EQuestState::NotStarted); }

			TestTrue(TEXT("Accept succeeds"), Quests->AcceptQuest(Q.QuestId));
			const FQuest* Active = Quests->FindQuest(Q.QuestId);
			if (Active) { TestTrue(TEXT("Accepted = Active"), Active->State == EQuestState::Active); }

			// Убийство цели -> прогресс -> Completed.
			Quests->NotifyKill(FName(TEXT("Wolf")));
			const FQuest* Done = Quests->FindQuest(Q.QuestId);
			if (Done) { TestTrue(TEXT("Kill -> Completed"), Done->State == EQuestState::Completed); }

			// Сдача -> TurnedIn + награда начислена владельцу (игроку).
			TestTrue(TEXT("Turn-in succeeds"), Quests->TurnInQuest(Q.QuestId));
			const FQuest* TurnedIn = Quests->FindQuest(Q.QuestId);
			if (TurnedIn) { TestTrue(TEXT("State TurnedIn"), TurnedIn->State == EQuestState::TurnedIn); }
			TestEqual(TEXT("Reward +150 to owner money"), Stats->GetMoney(), MoneyBefore + 150.0f);
		}
		else
		{
			bOk = false;
		}
	}

	CombatTestWorld::Destroy(World);
	return bOk;
}

#endif // WITH_DEV_AUTOMATION_TESTS
