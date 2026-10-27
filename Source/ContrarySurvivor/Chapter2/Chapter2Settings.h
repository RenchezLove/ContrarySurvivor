// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "Chapter2Settings.generated.h"

class UTexture2D;
class USoundBase;

/**
 * Глава 2 «Вера», задание «Образцы из аномалий» (утверждено Ринатом 23.10.2026, поправка
 * 25.10.2026). Все числа, надписи, картинки и звук задания — здесь, на странице настроек
 * проекта «Contrary Survivor: глава 2» (раздел [/Script/ContrarySurvivor.Chapter2Settings]
 * в Config/DefaultGame.ini). Правятся без кода.
 *
 * Тексты — дословно из РЕПЛИКИ.txt (номера в квадратных скобках у каждого поля).
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Contrary Survivor: глава 2"))
class CONTRARYSURVIVOR_API UChapter2Settings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }

	static const UChapter2Settings* Get() { return GetDefault<UChapter2Settings>(); }

	// --- Отравленная низина ---

	// Решение лида 23.10: «Без аппарата в зоне — урон каждую секунду, с аппаратом без воздуха — тоже».
	UPROPERTY(config, EditAnywhere, Category = "Низина", meta = (ClampMin = "0.0", DisplayPriority = "1",
		DisplayName = "Урон в низине без аппарата (здоровья в секунду)"))
	float ZoneDamageNoMaskPerSecond = 8.0f;

	UPROPERTY(config, EditAnywhere, Category = "Низина", meta = (ClampMin = "0.0", DisplayPriority = "2",
		DisplayName = "Урон в низине, когда воздух кончился (здоровья в секунду)"))
	float ZoneDamageNoAirPerSecond = 8.0f;

	// Насколько близко к краю низины (см) появляется кнопка «Надеть дыхательный аппарат».
	UPROPERTY(config, EditAnywhere, Category = "Низина", meta = (ClampMin = "0.0", DisplayPriority = "3",
		DisplayName = "Кнопка аппарата: расстояние до края низины (см)"))
	float MaskButtonEdgeDistance = 800.0f;

	// --- Воздух ---

	// Решение лида 23.10: «Воздуха в баллоне 3 минуты».
	UPROPERTY(config, EditAnywhere, Category = "Воздух", meta = (ClampMin = "1.0", DisplayPriority = "1",
		DisplayName = "Воздуха в полном баллоне (секунды)"))
	float AirCapacitySeconds = 180.0f;

	// Сколько секунд воздуха осталось, когда выходит сообщение [222] «Воздух на исходе».
	UPROPERTY(config, EditAnywhere, Category = "Воздух", meta = (ClampMin = "0.0", DisplayPriority = "2",
		DisplayName = "Порог «воздух на исходе» (секунды)"))
	float AirLowThresholdSeconds = 30.0f;

	// --- Детектор ---

	// Дальше этого (см) от ближайшего образца детектор молчит и полоска пуста.
	UPROPERTY(config, EditAnywhere, Category = "Детектор", meta = (ClampMin = "100.0", DisplayPriority = "1",
		DisplayName = "Дальность детектора (см)"))
	float DetectorRange = 3000.0f;

	// Писк у самого образца и на краю дальности (секунды между писками).
	UPROPERTY(config, EditAnywhere, Category = "Детектор", meta = (ClampMin = "0.05", DisplayPriority = "2",
		DisplayName = "Пауза между писками вплотную (секунды)"))
	float BeepIntervalNear = 0.15f;

	UPROPERTY(config, EditAnywhere, Category = "Детектор", meta = (ClampMin = "0.05", DisplayPriority = "3",
		DisplayName = "Пауза между писками на краю дальности (секунды)"))
	float BeepIntervalFar = 1.6f;

	// Короткий звук писка. Временно — щелчок оружия без патронов из проекта; заменить здесь.
	UPROPERTY(config, EditAnywhere, Category = "Детектор", meta = (DisplayPriority = "4",
		DisplayName = "Звук писка детектора"))
	TSoftObjectPtr<USoundBase> BeepSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/Weapons/weapon_dry_fire.weapon_dry_fire")));

	UPROPERTY(config, EditAnywhere, Category = "Детектор", meta = (ClampMin = "0.0", DisplayPriority = "5",
		DisplayName = "Громкость писка"))
	float BeepVolume = 0.6f;

	// Детектор-локатор вместо левого стика (правка Рината 25.10 вечер). Виден, пока задание
	// взято, образцы собраны не все и герой в низине или не дальше этого расстояния от её края.
	UPROPERTY(config, EditAnywhere, Category = "Детектор", meta = (ClampMin = "0.0", DisplayPriority = "6",
		DisplayName = "Локатор: показывать, когда до края низины не больше (см)"))
	float LocatorShowDistance = 1500.0f;

	// Через сколько секунд без подобранного образца на локаторе появляется полупрозрачная
	// стрелка на ближайший образец. Отсчёт идёт, пока локатор на экране; подбор образца
	// начинает его заново.
	UPROPERTY(config, EditAnywhere, Category = "Детектор", meta = (ClampMin = "0.0", DisplayPriority = "7",
		DisplayName = "Локатор: стрелка-подсказка через (секунды)"))
	float LocatorHintArrowDelay = 60.0f;

	// Во сколько раз стик больше обычного, пока он локатор («Сам стик на время поисков сделай
	// чуть больше»).
	UPROPERTY(config, EditAnywhere, Category = "Детектор", meta = (ClampMin = "1.0", ClampMax = "2.0", DisplayPriority = "8",
		DisplayName = "Локатор: увеличение стика"))
	float LocatorStickScale = 1.25f;

	// Радиус локатора, когда экранного стика нет (ПК без сенсорного слоя): круг рисуется на
	// месте стика внизу слева. Точки раскладки экрана высотой 1080.
	UPROPERTY(config, EditAnywhere, Category = "Детектор", meta = (ClampMin = "40.0", DisplayPriority = "9",
		DisplayName = "Локатор без стика: радиус"))
	float LocatorFallbackRadius = 140.0f;

	UPROPERTY(config, EditAnywhere, Category = "Детектор", meta = (DisplayPriority = "10",
		DisplayName = "Локатор без стика: центр от левого нижнего угла"))
	FVector2D LocatorFallbackOffset = FVector2D(240.0f, 240.0f);

	UPROPERTY(config, EditAnywhere, Category = "Детектор", meta = (DisplayPriority = "11", DisplayName = "Локатор: цвет волн и линии"))
	FLinearColor LocatorColor = FLinearColor(0.3f, 1.0f, 0.35f, 1.0f);

	UPROPERTY(config, EditAnywhere, Category = "Детектор", meta = (DisplayPriority = "12", DisplayName = "Локатор: цвет сигналов аномальных образований"))
	FLinearColor LocatorSignalColor = FLinearColor(0.75f, 1.0f, 0.3f, 1.0f);

	// Сколько секунд занимает один оборот линии локатора.
	UPROPERTY(config, EditAnywhere, Category = "Детектор", meta = (ClampMin = "0.3", DisplayPriority = "13",
		DisplayName = "Локатор: оборот линии (секунды)"))
	float LocatorSweepSeconds = 2.0f;

	// Прозрачность стрелки-подсказки (0 — не видна, 1 — сплошная).
	UPROPERTY(config, EditAnywhere, Category = "Детектор", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayPriority = "14",
		DisplayName = "Локатор: прозрачность стрелки"))
	float LocatorArrowOpacity = 0.45f;

	// --- Картинки ---

	// Стекло маски поверх экрана (слегка запотевшее и поцарапанное). Пусто — игра сама берёт
	// /Game/UI/Chapter2/T_MaskGlass, если такой файл есть; нет и его — стекла не видно.
	UPROPERTY(config, EditAnywhere, Category = "Картинки", meta = (DisplayPriority = "1",
		DisplayName = "Стекло маски (картинка поверх экрана)"))
	TSoftObjectPtr<UTexture2D> MaskGlassTexture;

	// Фотография героя рядом с Верой из ноутбука. Пусто — /Game/UI/Chapter2/T_PhotoHeroVera.
	UPROPERTY(config, EditAnywhere, Category = "Картинки", meta = (DisplayPriority = "2",
		DisplayName = "Фотография героя и Веры"))
	TSoftObjectPtr<UTexture2D> PhotoTexture;

	// Фотография стоит крупно по центру в верхней свободной части экрана, над окном разговора
	// (правка Рината 26.10: «Фото теперь вообще маленькое сбоку появляется»; решение лида):
	// высота — всё свободное место над окном минус отступы, но не меньше этой доли высоты
	// экрана. Если над окном места меньше, окно разговора на время показа фотографии
	// становится ниже. Пропорции фотографии сохраняются.
	UPROPERTY(config, EditAnywhere, Category = "Картинки", meta = (ClampMin = "0.1", ClampMax = "0.8", DisplayPriority = "3",
		DisplayName = "Фотография: наименьшая высота (доля высоты экрана)"))
	float PhotoMinHeightFraction = 0.35f;

	// Просвет между фотографией, краем экрана и окном разговора.
	UPROPERTY(config, EditAnywhere, Category = "Картинки", meta = (ClampMin = "0.0", DisplayPriority = "4",
		DisplayName = "Отступ фотографии от окна разговора и края экрана"))
	float PhotoMargin = 24.0f;

	// Насколько непрозрачно стекло маски (1 — как картинка, меньше — сквозь края видно больше).
	// Стекло лежит под всеми кнопками и окнами и нажатий не ловит.
	UPROPERTY(config, EditAnywhere, Category = "Картинки", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayPriority = "8",
		DisplayName = "Непрозрачность стекла маски"))
	float MaskGlassOpacity = 0.85f;

	// Сторона картинок баллона и детектора в записи задания на смартфоне.
	UPROPERTY(config, EditAnywhere, Category = "Картинки", meta = (ClampMin = "16.0", DisplayPriority = "9",
		DisplayName = "Размер картинок снаряжения в смартфоне"))
	float PdaGearImageSize = 96.0f;

	// Значок слева от надписи «Воздух» [220], он же картинка баллона в сумке и в смартфоне.
	// Пусто — /Game/UI/Chapter2/T_IconAir; нет и его — без значка.
	UPROPERTY(config, EditAnywhere, Category = "Картинки", meta = (DisplayPriority = "5",
		DisplayName = "Значок шкалы воздуха"))
	TSoftObjectPtr<UTexture2D> AirIconTexture;

	// Картинка детектора (значок предмета в сумке и фото в смартфоне). Пусто —
	// /Game/UI/Chapter2/T_IconDetector.
	UPROPERTY(config, EditAnywhere, Category = "Картинки", meta = (DisplayPriority = "6",
		DisplayName = "Значок детектора"))
	TSoftObjectPtr<UTexture2D> DetectorIconTexture;

	// Сторона значка у шкалы (в точках раскладки 1920x1080).
	UPROPERTY(config, EditAnywhere, Category = "Картинки", meta = (ClampMin = "8.0", DisplayPriority = "7",
		DisplayName = "Размер значка у шкалы"))
	float BarIconSize = 28.0f;

	// Значок предмета «Аномальное образование» в сумке (правка Рината 27.10.2026). Пусто —
	// /Game/UI/Chapter2/T_IconFormation; нет и его — плитка без картинки (с названием).
	UPROPERTY(config, EditAnywhere, Category = "Картинки", meta = (DisplayPriority = "10",
		DisplayName = "Значок аномального образования"))
	TSoftObjectPtr<UTexture2D> FormationIconTexture;

	// --- Заметки в смартфоне (правка Рината 26.10.2026) ---

	// Картинка записки наёмника: помятая бумага, к ней скрепкой приложена фотография героя
	// (в правом верхнем углу). Пусто — /Game/UI/Chapter2/T_MercenaryNote, если такой файл
	// есть; нет и его — записка рисуется светлой бумагой без картинки.
	UPROPERTY(config, EditAnywhere, Category = "Заметки", meta = (DisplayPriority = "1",
		DisplayName = "Картинка записки наёмника"))
	TSoftObjectPtr<UTexture2D> MercenaryNoteTexture;

	// Где на картинке записки лежит текст: отступы от краёв картинки в долях её ширины и
	// высоты (слева, сверху, справа, снизу). Сверху отступ больше — там фотография и скрепка. По умолчанию — свободная область картинки note_mercenary_v3 (768 на 1152): 7 %, 31 %, 7 %, 6 %.
	UPROPERTY(config, EditAnywhere, Category = "Заметки", meta = (DisplayPriority = "2",
		DisplayName = "Отступы текста записки от краёв картинки (доли)"))
	FMargin NoteTextMargins = FMargin(0.07f, 0.31f, 0.07f, 0.06f);

	// Пропорции картинки записки (ширина к высоте; у note_mercenary_v3 — 768 к 1152), пока картинки нет или её размер ещё не
	// известен.
	UPROPERTY(config, EditAnywhere, Category = "Заметки", meta = (ClampMin = "0.2", ClampMax = "3.0", DisplayPriority = "3",
		DisplayName = "Пропорции записки без картинки (ширина к высоте)"))
	float NoteFallbackAspect = 0.6667f;

	UPROPERTY(config, EditAnywhere, Category = "Заметки", meta = (DisplayPriority = "4",
		DisplayName = "Цвет текста записки"))
	FLinearColor NoteTextColor = FLinearColor(0.09f, 0.075f, 0.06f, 1.0f);

	// В точках листа шириной 600; лист целиком уменьшается под место во вкладке, поэтому кегль крупнее обычного.
	UPROPERTY(config, EditAnywhere, Category = "Заметки", meta = (ClampMin = "8", ClampMax = "48", DisplayPriority = "5",
		DisplayName = "Кегль текста записки"))
	int32 NoteTextSize = 22;

	// Шрифт «от руки». Рукописного шрифта в проекте пока нет: пусто — наклонный шрифт
	// интерфейса.
	UPROPERTY(config, EditAnywhere, Category = "Заметки", meta = (DisplayPriority = "6",
		DisplayName = "Шрифт записки (пусто — наклонный обычный)"))
	TSoftObjectPtr<UObject> NoteFont;

	// --- Экран конца главы ---

	UPROPERTY(config, EditAnywhere, Category = "Конец главы", meta = (ClampMin = "1.0", DisplayPriority = "1",
		DisplayName = "Сколько секунд виден экран конца главы"))
	float ChapterEndCardSeconds = 7.0f;

	// --- Шкалы на экране ---

	// Верх-центр экрана. Смещение — в точках раскладки 1920x1080 (правка Рината 25.10 вечер:
	// «Интерфейс балона подними чуть выше» — было 170).
	UPROPERTY(config, EditAnywhere, Category = "Шкалы", meta = (DisplayPriority = "1",
		DisplayName = "Якорь шкал (доли экрана)"))
	FVector2D BarsAnchor = FVector2D(0.5f, 0.0f);

	UPROPERTY(config, EditAnywhere, Category = "Шкалы", meta = (DisplayPriority = "2",
		DisplayName = "Смещение шкал от якоря"))
	FVector2D BarsOffset = FVector2D(0.0f, 110.0f);

	UPROPERTY(config, EditAnywhere, Category = "Шкалы", meta = (ClampMin = "60.0", DisplayPriority = "3",
		DisplayName = "Ширина шкалы"))
	float BarWidth = 320.0f;

	UPROPERTY(config, EditAnywhere, Category = "Шкалы", meta = (DisplayPriority = "4", DisplayName = "Цвет шкалы воздуха"))
	FLinearColor AirBarColor = FLinearColor(0.35f, 0.75f, 1.0f, 1.0f);

	// --- Надписи (дословно из РЕПЛИКИ.txt) ---

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[218] Кнопка у края отравленной зоны"))
	FText PutOnMaskText = NSLOCTEXT("Chapter2", "PutOnMask", "Надеть дыхательный аппарат");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[219] Кнопка снять аппарат вне зоны"))
	FText TakeOffMaskText = NSLOCTEXT("Chapter2", "TakeOffMask", "Снять дыхательный аппарат");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[220] Надпись у шкалы воздуха"))
	FText AirLabelText = NSLOCTEXT("Chapter2", "AirLabel", "Воздух");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[221] Вход в зону без аппарата"))
	FText NoMaskInZoneText = NSLOCTEXT("Chapter2", "NoMaskInZone", "Здесь нечем дышать! Наденьте дыхательный аппарат или уходите.");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[222] Воздух на исходе"))
	FText AirLowText = NSLOCTEXT("Chapter2", "AirLow", "Воздух на исходе — выбирайтесь из низины.");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[223] Воздух кончился"))
	FText AirEmptyText = NSLOCTEXT("Chapter2", "AirEmpty", "Воздух кончился!");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[224] Баллон заправлен в лагере"))
	FText AirRefilledText = NSLOCTEXT("Chapter2", "AirRefilled", "Баллон снова полон.");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[225] Надпись на детекторе"))
	FText DetectorLabelText = NSLOCTEXT("Chapter2", "DetectorLabel", "Детектор");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[253] Экран конца главы"))
	FText ChapterEndText = NSLOCTEXT("Chapter2", "ChapterEnd", "Конец первой главы. Продолжение следует…");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[230] Цель: учёный передал героя Вере"))
	FText TalkToVeraText = NSLOCTEXT("Chapter2", "TalkToVera", "Поговорить с Верой");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[231] Предмет: название баллона"))
	FText AirTankNameText = NSLOCTEXT("Chapter2", "AirTankName", "Баллон с воздухом");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (MultiLine = "true", DisplayName = "[232] Предмет: описание баллона"))
	FText AirTankDescriptionText = NSLOCTEXT("Chapter2", "AirTankDescription",
		"Баллон и маска от учёных. Нужны, чтобы дышать в отравленной низине. Пустой баллон заправляют в лагере учёных.");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[233] Предмет: название детектора"))
	FText DetectorNameText = NSLOCTEXT("Chapter2", "DetectorName", "Детектор");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (MultiLine = "true", DisplayName = "[234] Предмет: описание детектора"))
	FText DetectorDescriptionText = NSLOCTEXT("Chapter2", "DetectorDescription",
		"Прибор учёных. Ловит сигналы аномальных образований и показывает, откуда они идут.");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[235] Снаряжение нельзя продать или выбросить"))
	FText StoryItemLockedText = NSLOCTEXT("Chapter2", "StoryItemLocked", "Это снаряжение учёных: его нельзя продать или выбросить.");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (MultiLine = "true", DisplayName = "[236] Смартфон: инструкция к заданию «Образцы из аномалий»"))
	FText SamplesGuideText = NSLOCTEXT("Chapter2", "SamplesGuide",
		"Отравленная низина лежит к востоку от лагеря учёных. Перед входом наденьте маску и следите за запасом воздуха: когда он кончится, начнёт убывать здоровье. Пустой баллон заправят в лагере.\n"
		"В низине левый стик превращается в детектор. Идите туда, откуда приходят сигналы. Соберите аномальные образования и отнесите их Вере.");

	// Заметки (правка Рината 26.10.2026): вкладка смартфона, записка наёмника и фото с Верой.
	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[237] Смартфон: вкладка с записками"))
	FText NotesTabText = NSLOCTEXT("Chapter2", "NotesTab", "Заметки");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[238] Название записки наёмника"))
	FText MercenaryNoteTitleText = NSLOCTEXT("Chapter2", "MercenaryNoteTitle", "Записка наёмника");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (MultiLine = "true", DisplayName = "[239] Текст записки наёмника"))
	FText MercenaryNoteBodyText = NSLOCTEXT("Chapter2", "MercenaryNoteBody",
		"Сыч объявился у лагеря учёных. Брать только живым и целым: он мне нужен с головой, а не без неё.\n"
		"Память ему отшибло, он не помнит, от кого бежал. Пусть так и остаётся: никаких разговоров с ним, про то, где он был раньше, — ни слова.\n"
		"Учёных пока не трогать. Они сами носят нам образцы, хоть и не знают об этом.\n"
		"Фото прилагаю, чтобы не обознался.\n"
		"— К.");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[245] Записка подобрана у наёмника"))
	FText MercenaryNotePickupText = NSLOCTEXT("Chapter2", "MercenaryNotePickup",
		"Найдена записка. Прочитать её можно в смартфоне, во вкладке «Заметки».");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[246] Название заметки с фото из ноутбука"))
	FText VeraPhotoNoteTitleText = NSLOCTEXT("Chapter2", "VeraPhotoNoteTitle", "Фото из ноутбука");

	// Аномальное образование — предмет в сумке (правка Рината 27.10.2026: «Сами аномальные
	// образования сделай предметом. Этот предмет попадает в инвентарь, когда игрок берёт его»).
	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[226] Название предмета: аномальное образование"))
	FText FormationNameText = NSLOCTEXT("Chapter2", "FormationName", "Аномальное образование");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (MultiLine = "true", DisplayName = "[247] Предмет: описание аномального образования"))
	FText FormationDescriptionText = NSLOCTEXT("Chapter2", "FormationDescription",
		"Светящийся нарост из отравленной низины. Учёные Веры ждут такие образцы для исследований.");

	// СВОЯ НАДПИСЬ (в РЕПЛИКИ.txt такой нет; [235] говорит про снаряжение учёных и к образованию
	// не подходит): попытка продать, выбросить или переложить аномальное образование.
	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "Аномальное образование нельзя продать или выбросить (своя надпись, в РЕПЛИКИ.txt нет)"))
	FText FormationLockedText = NSLOCTEXT("Chapter2", "FormationLocked", "Аномальные образования нужно отнести Вере: их нельзя продать или выбросить.");

	// Чем объяснить отказ продать или выбросить сюжетный предмет с этим служебным ключом:
	// аномальное образование — своей надписью, снаряжение учёных и прочее — [235].
	static const FText& StoryItemLockedTextFor(const FString& ItemKey);

	// Картинка: поле, если задано; иначе файл по ожидаемому пути, если он есть в проекте.
	// Нет ни того, ни другого — null (картинки нет, игра не падает).
	static UTexture2D* ResolveTexture(const TSoftObjectPtr<UTexture2D>& Field, const TCHAR* ExpectedPackage);

	static constexpr const TCHAR* ExpectedMaskGlassPath = TEXT("/Game/UI/Chapter2/T_MaskGlass");
	static constexpr const TCHAR* ExpectedPhotoPath = TEXT("/Game/UI/Chapter2/T_PhotoHeroVera");
	static constexpr const TCHAR* ExpectedAirIconPath = TEXT("/Game/UI/Chapter2/T_IconAir");
	static constexpr const TCHAR* ExpectedDetectorIconPath = TEXT("/Game/UI/Chapter2/T_IconDetector");
	static constexpr const TCHAR* ExpectedMercenaryNotePath = TEXT("/Game/UI/Chapter2/T_MercenaryNote");
	static constexpr const TCHAR* ExpectedFormationIconPath = TEXT("/Game/UI/Chapter2/T_IconFormation");
};
