// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ContrarySurvivor/Debug/QADebug.h" // CONTRARY_WITH_QA_CHEATS

/**
 * МЕНЮ ОТЛАДКИ (Ринат 09.10.2026: «при нажатии клавиши "ъ" справа выезжало меню с кнопками и
 * тумблерами: god mode, 100 hp, заморозить всех врагов, телепорт на базу бандитов, учёных,
 * деревню, логово волков 1 и 2»).
 *
 * Всё содержимое файла — под выключателем CONTRARY_WITH_QA_CHEATS: в сборке для магазина
 * (Shipping) меню НЕТ ВОВСЕ, оно не компилируется (как и остальные отладочные клавиши).
 * Панель собрана кодом на Slate — виджета-чертежа в редакторе нет и не нужно.
 *
 * Две части:
 *  - ContraryDebugTools — действия меню без интерфейса (точки телепорта, телепорт, выдача
 *    оружия, убийство врагов рядом). Их же проверяют автотесты;
 *  - SContraryDebugMenu — сама выезжающая панель. Создаёт и показывает её контроллер игрока
 *    (AContrarySurvivorPlayerController::OnQAToggleDebugMenu, клавиша «]» — она же русская «ъ»).
 */
#if CONTRARY_WITH_QA_CHEATS

#include "Animation/CurveSequence.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class AActor;
class APawn;
class APlayerCharacter;
class ARangedWeapon;
class AContrarySurvivorPlayerController;
class SVerticalBox;

// Точка телепорта для кнопки меню.
struct FContraryDebugTeleportPoint
{
	// Подпись кнопки («Деревня», «Логово волков 1», «Scientists»).
	FString Label;

	// Куда ставить героя (рядом с этой точкой, на землю).
	FVector Location = FVector::ZeroVector;
};

namespace ContraryDebugTools
{
	// Приставка метки актора, который становится кнопкой телепорта: актор с меткой
	// DebugTP_Scientists даёт кнопку «Scientists». Новые точки добавляются на уровне без кода.
	extern const TCHAR* const TeleportTagPrefix;

	// Все точки телепорта уровня: деревня (зона деревни), базы врагов (по одной на базу,
	// одинаковые названия нумеруются: «Логово волков 1», «Логово волков 2») и акторы с
	// меткой DebugTP_<Имя>.
	void CollectTeleportPoints(UWorld* World, TArray<FContraryDebugTeleportPoint>& OutPoints);

	// Поставить пешку рядом с точкой: на землю и не внутрь преграды (пробует несколько мест
	// вокруг точки). false — подходящего места не нашлось, пешка осталась где была.
	bool TeleportPawnNear(APawn* Pawn, const FVector& Point);

	// Выдать огнестрел сразу в слот и в руки (занятый слот — прежний ствол уходит в сумку) и
	// патроны его вида в сумку. PreferredRow — строка таблицы предметов (чертёж с настройками
	// из редактора); строки или её класса нет — берётся FallbackClass.
	ARangedWeapon* GiveFirearm(APlayerCharacter* Player, FName PreferredRow,
		TSubclassOf<ARangedWeapon> FallbackClass, int32 ExtraAmmo);

	// Убить всех врагов в радиусе от игрока штатным уроном (лут и задания срабатывают как
	// обычно). Возвращает число убитых.
	int32 KillEnemiesNear(APlayerCharacter* Player, float Radius);
}

class SContraryDebugMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SContraryDebugMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AContrarySurvivorPlayerController>, Controller)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// Показать (панель выезжает справа) или спрятать (уезжает обратно). При показе список
	// точек телепорта собирается заново — уровень мог смениться.
	void SetOpen(bool bInOpen);

	bool IsOpen() const { return bOpen; }

private:
	// Пересобирает содержимое панели (тумблеры, кнопки, точки телепорта).
	void RebuildContent();

	APlayerCharacter* GetPlayer() const;

	// Сдвиг панели за правый край экрана по ходу анимации.
	TOptional<FSlateRenderTransform> GetSlideTransform() const;
	EVisibility GetPanelVisibility() const;

	TWeakObjectPtr<AContrarySurvivorPlayerController> Controller;
	TSharedPtr<SVerticalBox> ContentBox;
	FCurveSequence SlideAnimation;
	bool bOpen = false;
};

#endif // CONTRARY_WITH_QA_CHEATS
