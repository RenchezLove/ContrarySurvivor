// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "Layout/Margin.h"
#include "ContrarySurvivor/Components/QuestComponent.h" // EQuestKind
#include "ContraryPdaSettings.generated.h"

class UTexture2D;

/** Чьё место на карте: от этого зависит цвет точки и подписи. */
UENUM()
enum class EPdaMapPlaceKind : uint8
{
	Friendly UMETA(DisplayName = "Мирное"),
	Enemy    UMETA(DisplayName = "База врагов"),
	Neutral  UMETA(DisplayName = "Ничьё"),
};

/** Место карты с подписью. */
USTRUCT()
struct FPdaMapPlace
{
	GENERATED_BODY()

	UPROPERTY(config, EditAnywhere, Category = "Место", meta = (DisplayName = "Подпись"))
	FText Name;

	UPROPERTY(config, EditAnywhere, Category = "Место", meta = (DisplayName = "Положение в мире (X, Y)"))
	FVector2D WorldLocation = FVector2D::ZeroVector;

	UPROPERTY(config, EditAnywhere, Category = "Место", meta = (DisplayName = "Чьё место"))
	EPdaMapPlaceKind Kind = EPdaMapPlaceKind::Neutral;
};

/**
 * Настройки карманного компьютера (КПК) и карты (слова Рината 10.10.2026: «аналог PDA из
 * Stalker… карта, сведения о квестах… основные и побочные квесты обозначаются оранжевыми и
 * синими метками и текстом»). Страница в Project Settings → Game → «Contrary Survivor: КПК и
 * карта»; значения пишутся в Config/DefaultGame.ini — меняются без кода и без правки окон.
 *
 * Цвета вида задания действуют везде разом: строка задания на экране, метка-указатель в мире,
 * список и карта в окне КПК.
 *
 * КАРТА. Картинка местности снимается одним кадром строго сверху; прямоугольник мира, который
 * она покрывает, задаётся двумя углами. Игровая камера смотрит вдоль +Y мира (поворот штанги
 * камеры 90°, APlayerCharacter::CameraBoomRotation), поэтому на экране «вверх» = +Y мира,
 * «вправо» = −X мира. Чтобы верх карты совпадал с верхом экрана в игре, левый-верхний угол
 * карты — это НАИБОЛЬШИЕ X и Y прямоугольника, правый-нижний — наименьшие.
 *
 * Границы по умолчанию — игровая зона уровня L_World_C целиком, до внешнего края тумана
 * (актор BP_WorldBorder: центр (5500, −4370), сторона 46875 см; снято с уровня 10.10.2026).
 * Невидимая стена стоит на 1400 см внутри, так что герой за край карты не выходит.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Contrary Survivor: КПК и карта"))
class CONTRARYSURVIVOR_API UContraryPdaSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }

	// Мягкий оранжевый: читается на тёмной панели и поверх игрового мира.
	UPROPERTY(config, EditAnywhere, Category = "Задания", meta = (
		DisplayName = "Цвет основных заданий"))
	FLinearColor MainQuestColor = FLinearColor(1.0f, 0.55f, 0.15f, 1.0f);

	// Мягкий синий (светлый, чтобы не тонул на тёмном фоне).
	UPROPERTY(config, EditAnywhere, Category = "Задания", meta = (
		DisplayName = "Цвет побочных заданий"))
	FLinearColor SideQuestColor = FLinearColor(0.35f, 0.65f, 1.0f, 1.0f);

	// Пусто — карта рисуется тёмным полем с сеткой и всеми отметками.
	UPROPERTY(config, EditAnywhere, Category = "Карта", meta = (
		DisplayName = "Картинка карты (снимок местности сверху)"))
	TSoftObjectPtr<UTexture2D> MapTexture;

	UPROPERTY(config, EditAnywhere, Category = "Карта", meta = (
		DisplayName = "Левый-верхний угол карты в мире (X, Y)",
		ToolTip = "Точка мира, которая попадает в левый-верхний угол картинки. При игровой камере это наибольшие X и Y прямоугольника."))
	FVector2D MapWorldTopLeft = FVector2D(28937.5, 19067.5);

	UPROPERTY(config, EditAnywhere, Category = "Карта", meta = (
		DisplayName = "Правый-нижний угол карты в мире (X, Y)",
		ToolTip = "Точка мира, которая попадает в правый-нижний угол картинки. При игровой камере это наименьшие X и Y прямоугольника."))
	FVector2D MapWorldBottomRight = FVector2D(-17937.5, -27807.5);

	// --- Окно-смартфон (слова Рината 10.10.2026: «сам виджет КПК и саму карту сделай побольше…
	// сама подложка виджета должна быть похожа на смартфон… примерно как в сталкере. Только там
	// КПК, а здесь смартфон») ---

	// Картинка смартфона, лежащего на боку, с прозрачным фоном вокруг. Пусто — окно рисуется
	// тёмной плашкой с рамкой, в том же размере и с теми же отступами экрана.
	UPROPERTY(config, EditAnywhere, Category = "Смартфон", meta = (
		DisplayName = "Картинка смартфона (подложка окна)"))
	TSoftObjectPtr<UTexture2D> PhoneTexture;

	// Где на картинке экран смартфона: отступы от краёв картинки в ДОЛЯХ её ширины (слева,
	// справа) и высоты (сверху, снизу). Всё содержимое окна рисуется строго внутри экрана.
	UPROPERTY(config, EditAnywhere, Category = "Смартфон", meta = (
		DisplayName = "Отступы экрана внутри картинки (доли: слева, сверху, справа, снизу)"))
	FMargin PhoneScreenInsets = FMargin(0.04f, 0.06f, 0.04f, 0.06f);

	UPROPERTY(config, EditAnywhere, Category = "Смартфон", meta = (ClampMin = "0.3", ClampMax = "1.0",
		DisplayName = "Высота смартфона (доля высоты экрана)"))
	float PhoneHeightFraction = 0.92f;

	// Действует, пока картинки нет; с картинкой соотношение берётся с неё.
	UPROPERTY(config, EditAnywhere, Category = "Смартфон", meta = (ClampMin = "0.5", ClampMax = "3.0",
		DisplayName = "Соотношение сторон смартфона без картинки (ширина к высоте)"))
	float PhoneAspectRatio = 1.864f;

	// Смартфон, который не помещается по ширине, уменьшается целиком.
	UPROPERTY(config, EditAnywhere, Category = "Смартфон", meta = (ClampMin = "0.0",
		DisplayName = "Поля слева и справа от смартфона, единиц раскладки"))
	float PhoneSideMargin = 24.0f;

	// --- Вид кнопок и заголовка смартфона (образец Рината 20.10.2026: кнопки меню — плашки-
	// картинки со значком слева и крупной белой подписью справа; выбранная вкладка — своя, яркая
	// плашка; у заголовка значки сигнала и батареи и тонкая светлая линия под ним; кнопки карты —
	// квадратные плашки с бирюзовой рамкой). Пусто или картинка ещё не импортирована — окно
	// рисует запасной вид кодом: бирюзовые скруглённые плашки с рамкой, без значков. ---

	// Плашка кнопок меню (вкладки и «Закрыть»). Растягивается девятью кусками: углы не
	// тянутся, края тянутся вдоль, середина — во все стороны.
	UPROPERTY(config, EditAnywhere, Category = "Смартфон: кнопки", meta = (
		DisplayName = "Плашка кнопки меню"))
	TSoftObjectPtr<UTexture2D> MenuButtonPlate;

	// Плашка выбранной вкладки (яркая).
	UPROPERTY(config, EditAnywhere, Category = "Смартфон: кнопки", meta = (
		DisplayName = "Плашка выбранной вкладки"))
	TSoftObjectPtr<UTexture2D> MenuButtonPlateActive;

	// Какая часть картинки плашки меню не тянется: доли её ширины (слева, справа) и высоты
	// (сверху, снизу). Действует на обе плашки меню.
	UPROPERTY(config, EditAnywhere, Category = "Смартфон: кнопки", meta = (
		DisplayName = "Плашка меню: нерастягиваемые края (доли: слева, сверху, справа, снизу)"))
	FMargin MenuButtonPlateMargin = FMargin(0.061f, 0.222f, 0.061f, 0.222f);

	// Квадратная плашка кнопок карты «+», «−», «Моя позиция».
	UPROPERTY(config, EditAnywhere, Category = "Смартфон: кнопки", meta = (
		DisplayName = "Плашка кнопки карты"))
	TSoftObjectPtr<UTexture2D> MapButtonPlate;

	UPROPERTY(config, EditAnywhere, Category = "Смартфон: кнопки", meta = (
		DisplayName = "Плашка карты: нерастягиваемые края (доли: слева, сверху, справа, снизу)"))
	FMargin MapButtonPlateMargin = FMargin(0.125f, 0.125f, 0.125f, 0.125f);

	// Значки слева от подписей кнопок меню. Высоту значка задаёт файл окна (размер картинки у
	// элемента значка), ширина берётся по пропорциям картинки.
	UPROPERTY(config, EditAnywhere, Category = "Смартфон: кнопки", meta = (
		DisplayName = "Значок вкладки «Задания»"))
	TSoftObjectPtr<UTexture2D> QuestsTabIcon;

	UPROPERTY(config, EditAnywhere, Category = "Смартфон: кнопки", meta = (
		DisplayName = "Значок вкладки «Карта»"))
	TSoftObjectPtr<UTexture2D> MapTabIcon;

	UPROPERTY(config, EditAnywhere, Category = "Смартфон: кнопки", meta = (
		DisplayName = "Значок кнопки «Закрыть»"))
	TSoftObjectPtr<UTexture2D> CloseIcon;

	// Значки справа от заголовка «Смартфон».
	UPROPERTY(config, EditAnywhere, Category = "Смартфон: кнопки", meta = (
		DisplayName = "Значок сигнала у заголовка"))
	TSoftObjectPtr<UTexture2D> SignalIcon;

	UPROPERTY(config, EditAnywhere, Category = "Смартфон: кнопки", meta = (
		DisplayName = "Значок батареи у заголовка"))
	TSoftObjectPtr<UTexture2D> BatteryIcon;

	// --- Приближение карты (слова Рината: «дай игроку возможность приближать и отдалять её как
	// кнопками (+ и −), так и пальцами (раздвигая)») ---

	// 1 — карта целиком; это число — самое сильное приближение.
	UPROPERTY(config, EditAnywhere, Category = "Карта", meta = (ClampMin = "1.0", ClampMax = "16.0",
		DisplayName = "Наибольшее приближение карты, раз"))
	float MapMaxZoom = 4.0f;

	// Во сколько раз меняется приближение за одно нажатие «+» или «−» и за щелчок колеса мыши.
	UPROPERTY(config, EditAnywhere, Category = "Карта", meta = (ClampMin = "1.05", ClampMax = "4.0",
		DisplayName = "Шаг приближения, раз"))
	float MapZoomStep = 1.5f;

	// --- Карта открывается по мере исследования (слова Рината 10.10.2026: «изначально на карте
	// ничего не должно быть видно, кроме деревни. Остальное — чернота… карта появляется в
	// каком-то радиусе от него, я предлагаю в радиусе видимости камеры») ---

	// Открытое хранится сеткой клеток поверх прямоугольника карты. Мельче клетка — точнее край
	// открытого, но больше запись в сохранении и работа при перерисовке. Допустимо от 8 до 512.
	UPROPERTY(config, EditAnywhere, Category = "Карта: исследование", meta = (
		DisplayName = "Размер сетки открытого (клеток по ширине и высоте)"))
	FIntPoint MapFogGridSize = FIntPoint(128, 128);

	// Сколько земли вокруг героя открывается. По умолчанию — то, что видно на экране телефона:
	// камера героя висит на штанге 3000 под углом 60° вниз, угол обзора 40° (при кадре 16:9;
	// движок держит постоянным вертикальный угол 23,1°). На земле это кусок: 804 вперёд, 634
	// назад, в ширину по 1092 в стороны на экране 16:9 и по 1365 на экране 20:9. Самая дальняя
	// видимая точка (верхний угол экрана 20:9) — в 1743 от героя; круг такого радиуса закрывает
	// всё, что игрок видел.
	UPROPERTY(config, EditAnywhere, Category = "Карта: исследование", meta = (ClampMin = "100.0",
		DisplayName = "Радиус открытия вокруг героя, см"))
	float MapRevealRadius = 1750.0f;

	// Как часто герой открывает карту вокруг себя. За четверть секунды бегом он проходит меньше
	// половины клетки сетки по умолчанию (366 см).
	UPROPERTY(config, EditAnywhere, Category = "Карта: исследование", meta = (ClampMin = "0.05",
		DisplayName = "Период открытия, сек"))
	float MapRevealInterval = 0.25f;

	// Деревня открыта с самого начала: круг вокруг зоны деревни. Зона на уровне — прямоугольник
	// с полуразмерами 1900 на 1400 (до угла 2360), круг берёт её с запасом.
	UPROPERTY(config, EditAnywhere, Category = "Карта: исследование", meta = (ClampMin = "0.0",
		DisplayName = "Радиус открытого вокруг деревни, см"))
	float VillageRevealRadius = 3000.0f;

	// Работает, только если на уровне нет зоны деревни (тогда центр берётся с неё).
	UPROPERTY(config, EditAnywhere, Category = "Карта: исследование", meta = (
		DisplayName = "Запасной центр деревни в мире (X, Y)"))
	FVector2D VillageCenterFallback = FVector2D(-3950.0, -4650.0);

	UPROPERTY(config, EditAnywhere, Category = "Карта: исследование", meta = (
		DisplayName = "Цвет неизведанного"))
	FLinearColor MapFogColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);

	// 0 — резкий край по клеткам; каждый шаг размывает край ещё примерно на клетку.
	UPROPERTY(config, EditAnywhere, Category = "Карта: исследование", meta = (ClampMin = "0", ClampMax = "8",
		DisplayName = "Мягкость края открытого"))
	int32 MapFogSoftness = 2;

	// Решение лида 10.10.2026: цель отслеживаемого задания видна и на неизведанном, как в GTA 5.
	UPROPERTY(config, EditAnywhere, Category = "Карта: исследование", meta = (
		DisplayName = "Показывать цель отслеживаемого задания на неизведанном"))
	bool bShowTrackedQuestOnUnexplored = true;

	// Места с подписями. Подпись появляется, когда открыта клетка карты под местом. Если рядом
	// с точкой из списка на уровне стоит сам объект (зона деревни, база врагов), место берётся
	// с него; точка из списка — запасной путь и способ подписать место без своего объекта.
	UPROPERTY(config, EditAnywhere, Category = "Карта: исследование", meta = (
		DisplayName = "Места с подписями"))
	TArray<FPdaMapPlace> MapPlaces;

	// Объект уровня ближе этого расстояния к точке из списка считается тем же местом.
	UPROPERTY(config, EditAnywhere, Category = "Карта: исследование", meta = (ClampMin = "0.0",
		DisplayName = "Расстояние, на котором место из списка совпадает с объектом уровня, см"))
	float MapPlaceMergeRadius = 2500.0f;

	// --- Район поиска на карте (слова Рината 11.10.2026: «на карте зеленым кругом отображается
	// примерный район нахождения логова волков (южного от деревни)… По приходу игрока в логово
	// и (или) по прошествии 3 минут пропадает и отметка района поиска, и подпись») ---

	// Метка базы врагов на уровне (QuestMarkerTag), чей район отмечает староста: южное логово.
	UPROPERTY(config, EditAnywhere, Category = "Карта: район поиска", meta = (
		DisplayName = "Метка южного логова волков на уровне"))
	FName SouthWolfDenMarkerTag = FName(TEXT("WolfDen2"));

	// Отсчёт идёт игровым временем: на паузе стоит.
	UPROPERTY(config, EditAnywhere, Category = "Карта: район поиска", meta = (ClampMin = "1.0",
		DisplayName = "Сколько держится отметка района, сек"))
	float SearchAreaSeconds = 180.0f;

	// Герой ближе этого расстояния к середине района — он «пришёл», отметка снимается.
	UPROPERTY(config, EditAnywhere, Category = "Карта: район поиска", meta = (ClampMin = "100.0",
		DisplayName = "Расстояние, на котором герой «пришёл» в район, см"))
	float SearchAreaArrivalRadius = 2500.0f;

	UContraryPdaSettings()
	{
		auto AddPlace = [this](const FText& Name, double X, double Y, EPdaMapPlaceKind Kind)
		{
			FPdaMapPlace& Place = MapPlaces.AddDefaulted_GetRef();
			Place.Name = Name;
			Place.WorldLocation = FVector2D(X, Y);
			Place.Kind = Kind;
		};
		AddPlace(NSLOCTEXT("Pda", "PlaceVillage", "Деревня"), -3950.0, -4650.0, EPdaMapPlaceKind::Friendly);
		AddPlace(NSLOCTEXT("Pda", "PlaceBanditCamp", "Лагерь бандитов"), 6940.0, 9070.0, EPdaMapPlaceKind::Enemy);
		AddPlace(NSLOCTEXT("Pda", "PlaceWolfDenEast", "Логово волков"), 13943.0, -5998.0, EPdaMapPlaceKind::Enemy);
		AddPlace(NSLOCTEXT("Pda", "PlaceWolfDenSouth", "Логово волков"), -6080.0, -13940.0, EPdaMapPlaceKind::Enemy);
		AddPlace(NSLOCTEXT("Pda", "PlaceScientistBase", "База учёных"), 8700.0, -16000.0, EPdaMapPlaceKind::Friendly);
		AddPlace(NSLOCTEXT("Pda", "PlaceAbandonedCamp", "Брошенный лагерь"), 650.0, 1400.0, EPdaMapPlaceKind::Neutral);
	}

	FLinearColor GetQuestKindColor(EQuestKind Kind) const
	{
		return Kind == EQuestKind::Side ? SideQuestColor : MainQuestColor;
	}

	static const UContraryPdaSettings* Get() { return GetDefault<UContraryPdaSettings>(); }
};
