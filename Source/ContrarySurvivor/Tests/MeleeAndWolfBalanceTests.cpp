// Fill out your copyright notice in the Description page of Project Settings.
//
// HEADLESS Automation-тест правки баланса 08-09 (живой осмотр Рината: «сделай урон от ножа
// чуть выше, а урон по игроку от волков чуть ниже, а то он слишком быстро умирает»). Запуск:
//   UnrealEditor-Cmd "<uproject>" -ExecCmds="Automation RunTests ContrarySurvivor.MeleeWolfBalance; Quit"
//        -unattended -nopause -nosplash -stdout -nullrhi -abslog=<log>
//
// Тест держит не сами числа, а ИСХОДЫ боя — то, что игрок реально чувствует:
//   * сколько укусов волка переживает игрок со ста единицами здоровья без брони;
//   * за сколько ударов ножа падает волк.
// Числа при этом остаются редактируемыми: правку в разумных пределах тест переживёт, а вот
// возврат к прежней «мясорубке» (волк убивает за девять укусов) или случайное ослабление
// ножа поймает.
//
// Значения читаются у объектов-образцов классов, мир для этого не нужен. Урон укуса лежит
// в защищённом поле контроллера — берём его через рефлексию движка, а не ослабляем доступ
// ради теста.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AMeleeWeapon.h"
#include "ContrarySurvivor/Characters/WolfCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Controllers/WolfAIController.h"
#include "UObject/UnrealType.h"

static constexpr EAutomationTestFlags MeleeWolfBalanceTestFlags =
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

namespace MeleeWolfBalance
{
	// Сколько ударов силой Damage нужно, чтобы снять MaxHealth (последний удар добивает).
	static int32 HitsToKill(float MaxHealth, float Damage)
	{
		if (Damage <= 0.0f)
		{
			return TNumericLimits<int32>::Max();
		}
		return FMath::CeilToInt(MaxHealth / Damage);
	}

	// Значение защищённого числового поля у объекта-образца класса (через рефлексию).
	static bool TryReadFloatProperty(const UClass* Class, const TCHAR* PropertyName, float& OutValue)
	{
		const FFloatProperty* Prop = FindFProperty<FFloatProperty>(Class, FName(PropertyName));
		const UObject* Defaults = Class ? Class->GetDefaultObject() : nullptr;
		if (!Prop || !Defaults)
		{
			return false;
		}
		OutValue = Prop->GetPropertyValue_InContainer(Defaults);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMeleeAndWolfBalanceTest,
	"ContrarySurvivor.MeleeWolfBalance.KnifeBitesAndSurvival", MeleeWolfBalanceTestFlags)

bool FMeleeAndWolfBalanceTest::RunTest(const FString& Parameters)
{
	// --- Исходные числа берём из кода, а не из головы ---
	const AMeleeWeapon* Knife = GetDefault<AMeleeWeapon>();
	const UStatsComponent* Stats = GetDefault<UStatsComponent>();
	if (!TestNotNull(TEXT("Образец ножа доступен"), Knife)
		|| !TestNotNull(TEXT("Образец полосок состояния доступен"), Stats))
	{
		return false;
	}

	const float KnifeDamage = Knife->GetDamage();
	const float PlayerHealth = Stats->GetMaxHealth();

	// Здоровье волка и урон его укуса лежат в защищённых полях — читаем их через рефлексию
	// движка, а не ослабляем доступ в боевом классе ради теста.
	float WolfHealth = 0.0f;
	float WolfBite = 0.0f;
	if (!TestTrue(TEXT("Здоровье волка прочитано из его класса"),
			MeleeWolfBalance::TryReadFloatProperty(AWolfCharacter::StaticClass(), TEXT("WolfMaxHealth"), WolfHealth))
		|| !TestTrue(TEXT("Урон укуса волка прочитан из контроллера"),
			MeleeWolfBalance::TryReadFloatProperty(AWolfAIController::StaticClass(), TEXT("AttackDamage"), WolfBite)))
	{
		return false;
	}

	const int32 BitesToKillPlayer = MeleeWolfBalance::HitsToKill(PlayerHealth, WolfBite);
	const int32 HitsToKillWolf = MeleeWolfBalance::HitsToKill(WolfHealth, KnifeDamage);

	AddInfo(FString::Printf(
		TEXT("Нож %.0f урона, волк %.0f здоровья и %.0f урона за укус, игрок %.0f здоровья. Волк убивает за %d укусов, нож валит волка за %d удара(ов)."),
		KnifeDamage, WolfHealth, WolfBite, PlayerHealth, BitesToKillPlayer, HitsToKillWolf));

	// --- Что держим ---

	// Игрок без брони обязан переживать не меньше десяти укусов: до правки было девять, и
	// Ринат живьём сказал, что умирает от волков слишком быстро.
	TestTrue(FString::Printf(
		TEXT("Игрок держит не меньше 10 укусов (сейчас %d)"), BitesToKillPlayer),
		BitesToKillPlayer >= 10);

	// Но и в бессмертие не уходим: если укус ослабят до неощутимого, бой с волком превратится
	// в переминание с ноги на ногу.
	TestTrue(FString::Printf(
		TEXT("Волк остаётся опасен: убивает не больше чем за 20 укусов (сейчас %d)"), BitesToKillPlayer),
		BitesToKillPlayer <= 20);

	// Нож должен валить волка не больше чем за два удара — иначе он снова «не чувствуется».
	TestTrue(FString::Printf(
		TEXT("Нож валит волка не больше чем за 2 удара (сейчас %d)"), HitsToKillWolf),
		HitsToKillWolf <= 2);

	// Зубы волка слабее ножа: иначе зверь бьёт больнее стального клинка, и это видно игроку.
	TestTrue(FString::Printf(TEXT("Укус (%.0f) слабее удара ножом (%.0f)"), WolfBite, KnifeDamage),
		WolfBite < KnifeDamage);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
