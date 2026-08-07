// Fill out your copyright notice in the Description page of Project Settings.

// PlayerCharacter.h
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MasterHumanoidCharacter.h" // InheritingFrom от AMasterHumanoidCharacter
#include "GameFramework/SpringArmComponent.h" 
#include "Camera/CameraComponent.h" 
#include "GameFramework/Controller.h" // Enhanced Input
//#include "PlayerController.h"
#include "AMasterWeapon.h"
#include "ContrarySurvivor/UI/LimpIndicatorWidget.h" // FLimpIndicatorStyle + FLimpFirstHintState (индикатор хромоты, Build 1)
#include "ContrarySurvivor/Ads/DeathLossLogic.h" // DeathLoss::FPlan (потери при смерти, Build 1.2)
#include "ContrarySurvivor/Debug/QADebug.h"      // CONTRARY_WITH_QA_CHEATS: отладочной клавиши нет в Shipping
#include "PlayerCharacter.generated.h"

class UStatsComponent;
class UQuestComponent;
class UContrarySaveGame;
class AMasterInventoryItem;
enum class EConsumableType : uint8; // тип расходника (AConsumableItem.h) — параметр GiveConsumableToBackpack
class USoundBase;
class UAudioComponent;
class UNavigationInvokerComponent;
class UDailyRewardComponent;
class UOnboardingComponent;
class UMeleeSectorIndicatorComponent;
class APickup;
struct FShopEntry;

/**
 * 
 */
UCLASS()
class CONTRARYSURVIVOR_API APlayerCharacter : public AMasterHumanoidCharacter
{
    GENERATED_BODY()

public:
    APlayerCharacter();


protected:

    // Spring Arm
    // meta DisplayPriority — поднять наши настройки наверх Details (фидбек Рината), сразу после Transform.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true", DisplayPriority = "1"))
    USpringArmComponent* SpringArmComponent;

    // Camera Component
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    UCameraComponent* CameraComponent;

    // --- Камера в стиле Last Day on Earth (#20) ---
    // Тюнингуемые из редактора параметры камеры. Применяются в конструкторе (дефолты) и
    // в BeginPlay (на случай оверрайда в BP/инстансе), чтобы можно было подбирать без кода.
    // ВАЖНО: Yaw=90 сохраняет исходную ориентацию обзора уровня (не ломает курсорный
    // прицел — он работает через deproject экрана, угол-агностичен).

    // Угол наклона камеры вниз (Pitch) и направление обзора (Yaw). DRAFT LDoE-ракурс.
    // BugReport 12: фиксированная изометрия СВЕРХУ — делаем угол круче к вертикали (-60),
    // чтобы вид был обзорнее «сверху-сбоку», как в Last Day on Earth. Yaw 90 сохранён
    // (ориентация обзора уровня). Игрок камеру НЕ вращает (bUsePawnControlRotation/inherit=false).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (DisplayPriority = "12"))
    FRotator CameraBoomRotation = FRotator(-60.0f, 90.0f, 0.0f);

    // Дистанция камеры от персонажа. Единственный источник истины по длине арма: меняется
    // ЗДЕСЬ (Camera-категория в дефолтах BP), применяется в OnConstruction (живой knob,
    // виден в редакторе сразу, не перетирается в рантайме). DRAFT, Ринат подкрутит.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (DisplayPriority = "10"))
    float CameraArmLength = 3000.0f;

    // Угол обзора камеры (перспектива). Узкий FOV ~40 даёт «сжатый» LDoE-вид.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (DisplayPriority = "11"))
    float CameraFieldOfView = 40.0f;

    // Плавное отставание камеры (lag) — оживляет движение, чтобы камера не была «приклеена».
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (DisplayPriority = "13"))
    bool bEnableCameraLag = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.0", DisplayPriority = "14"))
    float CameraLagSpeed = 7.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.0", DisplayPriority = "15"))
    float CameraLagMaxDistance = 150.0f;

    // --- «Дыхание» камеры + look-ahead по ходу движения (#28) ---
    // Эффекты ЕДВА ЗАМЕТНЫЕ (запрос Рината «чуть-чуть»; на мобиле сильное покачивание укачивает).
    // Реализованы процедурно в Tick через SpringArm->TargetOffset (world space): look-ahead по
    // вектору скорости пешки + крошечный синусный боб.

    // «Дыхание»: лёгкий постоянный вертикальный боб камеры (амплитуда в см — крошечная).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Feel", meta = (DisplayPriority = "16"))
    bool bEnableCameraBreathing = true;

    // Амплитуда дыхания (см). Держать малой (~1-2), иначе заметно/укачивает.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Feel", meta = (ClampMin = "0.0", DisplayPriority = "17"))
    float BreathingAmplitude = 1.5f;

    // Скорость дыхания (рад/сек ~ циклов). Медленно = «живо», не нервно.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Feel", meta = (ClampMin = "0.0", DisplayPriority = "18"))
    float BreathingSpeed = 1.1f;

    // Look-ahead: при движении камера чуть смещается в сторону движения.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Feel", meta = (DisplayPriority = "19"))
    bool bEnableCameraLookAhead = true;

    // Максимальное смещение look-ahead (см) — камера ведёт таргет в сторону движения, чтобы
    // игрок видел больше по ходу (BugReport 12). При отодвинутой обзорной камере 70 см незаметно,
    // поэтому заметный «лид» ~300 см. DRAFT, Ринат подкрутит (игрок всё ещё близко к центру).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Feel", meta = (ClampMin = "0.0", DisplayPriority = "20"))
    float LookAheadAmount = 300.0f;

    // Скорость плавного подмешивания look-ahead (VInterpTo). Меньше = плавнее/ленивее.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Feel", meta = (ClampMin = "0.0", DisplayPriority = "21"))
    float LookAheadInterpSpeed = 2.5f;

    // Порог скорости пешки (см/с), выше которого включается look-ahead (отсекает дрожь покоя).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Feel", meta = (ClampMin = "0.0", DisplayPriority = "22"))
    float LookAheadSpeedThreshold = 50.0f;

    // --- Боевой режим камеры (D6, ADR-035) ---
    // «Камера — ДВА режима: 1. Исследование: лёгкий look-ahead (как сейчас). 2. Бой: камера
    // ПЕРЕСТАЁТ смотреть вперёд и слегка смещается к ближайшей угрозе (или к центру угроз)…
    // БЕЗ отдаления/зума. Плавно, без рывков». Реализация: пока есть враги, ведущие бой
    // (AEnemyAIController::IsEngagingPlayer) в радиусе, look-ahead ЗАМЕЩАЕТСЯ смещением к
    // центру угроз — тот же интерполятор (CameraLookAheadOffset), переход бесшовный. Зум не трогаем.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Combat", meta = (DisplayPriority = "23"))
    bool bEnableCombatCamera = true;

    // Радиус (см), в котором ведущие бой враги считаются угрозами для смещения камеры.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Combat", meta = (ClampMin = "0.0", DisplayPriority = "24"))
    float CombatCameraThreatRadius = 2200.0f;

    // Доля вектора «игрок -> центр угроз», на которую смещается камера [0..1].
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Combat", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayPriority = "25"))
    float CombatCameraOffsetFactor = 0.45f;

    // Кламп смещения камеры к угрозам (см) — «слегка», без увода игрока из кадра.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Combat", meta = (ClampMin = "0.0", DisplayPriority = "26"))
    float CombatCameraMaxOffset = 500.0f;

    // Скорость плавного перехода камеры В боевое смещение (VInterpTo, «без рывков»).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Combat", meta = (ClampMin = "0.1", DisplayPriority = "27"))
    float CombatCameraInterpSpeed = 2.0f;

    // Скорость ВЫХОДА камеры из боя (возврат к look-ahead исследования). Отдельная и заметно
    // мягче боевого входа — фидбек Рината 07-05: «при потере противника камера смещается
    // в сторону движения достаточно резко».
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Combat", meta = (ClampMin = "0.1", DisplayPriority = "28"))
    float CombatCameraExitInterpSpeed = 1.0f;

    // --- Тряска камеры (D5) ---
    // Процедурная trauma-модель: AddCameraShake копит «травму» [0..1], затухающую со временем;
    // смещение = PerlinNoise1D * амплитуда * травма² — подмешивается в SpringArm->TargetOffset.
    // БЕЗ плагина EngineCameras (паттерны CameraShake живут в плагине) — дёшево для Android.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shake", meta = (DisplayPriority = "28"))
    bool bEnableCameraShake = true;

    // Максимальная амплитуда смещения камеры при полной травме (см, world XY).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shake", meta = (ClampMin = "0.0", DisplayPriority = "29"))
    float CameraShakeMaxAmplitude = 40.0f;

    // Частота дрожи (скорость пробега по шуму Перлина).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shake", meta = (ClampMin = "0.1", DisplayPriority = "30"))
    float CameraShakeFrequency = 14.0f;

    // Скорость затухания травмы (единиц травмы в секунду).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shake", meta = (ClampMin = "0.1", DisplayPriority = "31"))
    float CameraShakeDecay = 1.8f;

    // Травма при ПОЛУЧЕНИИ урона игроком (D5: «тряска при получении урона»). 0 = выкл.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shake", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayPriority = "32"))
    float DamageShakeTrauma = 0.5f;

    // --- Постобработка камеры (Build 1, ТЗ издателя раздел 4) ---
    // Умеренные стартовые значения, Ринат подправит. Применяются к PostProcessSettings камеры в
    // ApplyCameraSettings (OnConstruction — живой knob на размещённом экземпляре). Туман — НЕ здесь.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|PostProcess", meta = (DisplayPriority = "1"))
    bool bEnablePostProcess = true;

    // Виньетка — мягкое затемнение краёв. Умеренно (~0.35), НЕ «тоннель».
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|PostProcess", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayPriority = "2"))
    float PPVignetteIntensity = 0.35f;

    // Фиксированная экспозиция: отключает авто-адаптацию глаза (кадр не «дышит» яркостью).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|PostProcess", meta = (DisplayPriority = "3"))
    bool bPPFixedExposure = true;

    // Значение экспозиции (EV) в ручном режиме. 0 — нейтрально.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|PostProcess", meta = (DisplayPriority = "4"))
    float PPExposureCompensation = 0.0f;

    // Лёгкое зерно (film grain), очень слабое — «плёнка/потрёпанность».
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|PostProcess", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayPriority = "5"))
    float PPFilmGrainIntensity = 0.1f;

    // Насыщенность (1 — как есть, <1 — лёгкая десатурация под тон «потрёпанного новичка»).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|PostProcess", meta = (ClampMin = "0.0", ClampMax = "2.0", DisplayPriority = "6"))
    float PPSaturation = 0.92f;

    // Теплота светов [0..~0.3]: сдвигает света к тёплому (больше красного, меньше синего).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|PostProcess", meta = (ClampMin = "0.0", ClampMax = "0.3", DisplayPriority = "7"))
    float PPHighlightWarmth = 0.05f;

    // Холод теней [0..~0.3]: сдвигает тени к холодному (больше синего, меньше красного).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|PostProcess", meta = (ClampMin = "0.0", ClampMax = "0.3", DisplayPriority = "8"))
    float PPShadowCoolness = 0.05f;

    // --- Арка постобработки интро (Build 1, ТЗ раздел 4): по пути к деревне грейд плавно идёт
    // от «темнее и обесцвеченнее» к нормальному. Интерполяция альфой [0..1]: 0 = старт (значения
    // ниже), 1 = норма (значения выше). Альфу ведёт интро-последовательность (SetIntroGradeAlpha).
    // Без нового арта — только интерполяция экспозиции и насыщенности. ---

    // Экспозиция на старте интро (темнее нормы), EV.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|PostProcess|Intro", meta = (DisplayPriority = "1"))
    float PPIntroStartExposure = -1.5f;

    // Насыщенность на старте интро (сильнее обесцвечено).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|PostProcess|Intro", meta = (ClampMin = "0.0", ClampMax = "2.0", DisplayPriority = "2"))
    float PPIntroStartSaturation = 0.5f;

    // --- Хромота от низкого HP (Build 1, ТЗ издателя раздел 2) ---
    // Дёшево, БЕЗ анимации (издатель разрешил): при HP на пороге и ниже — сниженная скорость
    // ходьбы; после лечения аптечкой скорость восстанавливается — игрок физически чувствует
    // выздоровление. Порог и коэффициент — тюнинг Рината.

    // Порог здоровья (доля от максимума), НА КОТОРОМ И НИЖЕ включается хромота. 0.5 = при HP <= 50%
    // (старт игрока — ровно половина HP, поэтому «на пороге» тоже хромает — приходит раненым).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Limp", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayPriority = "1"))
    float LimpHealthFraction = 0.5f;

    // Множитель скорости ходьбы во время хромоты (0.6 = 60% скорости). Тюнинг Рината.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Limp", meta = (ClampMin = "0.1", ClampMax = "1.0", DisplayPriority = "2"))
    float LimpSpeedMultiplier = 0.6f;

    // ПУСТАЯ точка подключения под звук тяжёлого дыхания при хромоте (звук Ринат добавит позже).
    // Пусто — ничего не играет; задан — зацикливается, пока игрок хромает, и глохнет при выздоровлении.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Limp", meta = (DisplayPriority = "3"))
    USoundBase* LimpBreathingSound = nullptr;

    // --- Индикация хромоты игроку (приёмка Рината 07-27: «обозначить, что герой хромает
    // из-за низкого здоровья, а не ходит так всегда»). Кодовый UMG-виджет без .uasset. ---

    // Текст ПОСТОЯННОГО индикатора: плашка под стеком статов, видна всё время, пока игрок хромает.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Limp", meta = (DisplayPriority = "4"))
    FText LimpIndicatorText = NSLOCTEXT("LimpIndicator", "IndicatorText", "Ранен: скорость снижена");

    // РАЗОВАЯ развёрнутая подсказка (раз за игровую сессию, при первом входе в хромоту со
    // свободным управлением): объясняет причину и что скорость ВЕРНЁТСЯ после лечения.
    // Названные способы лечения сверены с кодом: аптечка лечит напрямую (AConsumableItem,
    // тип Medkit), еда и вода лечат понемногу (UStatsComponent::Food/WaterHealthRestoreAmount).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Limp", meta = (DisplayPriority = "5", MultiLine = "true"))
    FText LimpFirstHintText = NSLOCTEXT("LimpIndicator", "FirstHintText",
        "Тебя сильно потрепали: пока здоровья мало, герой хромает и идёт медленно. Подлечись — аптечкой, едой или водой — и скорость вернётся.");

    // Задержка развёрнутой подсказки после того, как управление стало свободным (сек), чтобы не
    // спорить за внимание с подсказкой движения — та всплывает ровно в момент передачи управления.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Limp", meta = (ClampMin = "0.0", DisplayPriority = "6"))
    float LimpFirstHintDelay = 2.5f;

    // Сколько секунд висит развёрнутая подсказка; затем плашка сжимается до постоянного индикатора.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Limp", meta = (ClampMin = "1.0", DisplayPriority = "7"))
    float LimpFirstHintDuration = 8.0f;

    // Стиль плашки индикатора: цвета, шрифт, позиция/отступы на экране, ширина.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Limp", meta = (DisplayPriority = "8"))
    FLimpIndicatorStyle LimpIndicatorStyle;

    // Компонент статов игрока (ADR-015) — ИСТОЧНИК ИСТИНЫ по HP/голоду/жажде/деньгам
    // (Фаза 2). Инлайн-Health базы AMasterHumanoidCharacter для игрока не используется,
    // как и у врага: TakeDamage роутится в Stats.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats", meta = (AllowPrivateAccess = "true", DisplayPriority = "2"))
    UStatsComponent* Stats;

    // Журнал квестов игрока (Фаза 5, GDD §7.7). C++-сабобъект — добавляется детерминированно,
    // без BP. Староста предлагает квест, убийства целей инкрементят прогресс, сдача даёт деньги.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest", meta = (AllowPrivateAccess = "true", DisplayPriority = "3"))
    UQuestComponent* Quests;

    // Ежедневная награда за вход (Этап F2, ADR-044 п.4). Проверка даты/начисление/окно — в
    // компоненте; числа (база/шаг/потолок) настраиваются на нём в редакторе.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retention", meta = (AllowPrivateAccess = "true", DisplayPriority = "4"))
    UDailyRewardComponent* DailyReward;

    // Онбординг первых минут (Этап F1): одноразовые контекстные подсказки. События дёргает
    // контроллер (подбор/староста/инвентарь/смерть), стартовую — сам компонент.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retention", meta = (AllowPrivateAccess = "true", DisplayPriority = "5"))
    UOnboardingComponent* Onboarding;

    // Подсветка сектора ближнего боя на земле (Build 1.1, п.5 Рината). Декаль привязана к
    // капсуле, зажигается сама, когда есть цель и в руках нож; угол и дальность берёт из
    // самого ножа. Настройки (материал, непрозрачность, выключатель) — на компоненте.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true", DisplayPriority = "6"))
    UMeleeSectorIndicatorComponent* MeleeSectorIndicator;

    // Navigation Invoker (Фаза 5): навмеш генерится ТОЛЬКО вокруг игрока и следует за ним
    // (см. DefaultEngine.ini bGenerateNavigationOnlyAroundNavigationInvokers=true). Это даёт
    // тайлы навмеша у боевых зон (база бандитов/Логово), куда бы игрок ни пришёл, и не требует
    // строить весь огромный пол — важно для Android. Радиусы задаются в конструкторе
    // (NavInvokerGenerationRadius/NavInvokerRemovalRadius) через SetGenerationRadii.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (AllowPrivateAccess = "true", DisplayPriority = "4"))
    UNavigationInvokerComponent* NavInvoker;

    // Радиус генерации тайлов навмеша вокруг игрока (см). Должен покрывать зону деревни +
    // боевые точки, когда игрок там. DRAFT-тюнинг.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (ClampMin = "0.1"))
    float NavInvokerGenerationRadius = 5000.0f;

    // Радиус удаления тайлов навмеша (см). > радиуса генерации, чтобы тайлы не «мигали»
    // на границе при движении игрока (гистерезис). DRAFT-тюнинг.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (ClampMin = "0.1"))
    float NavInvokerRemovalRadius = 7000.0f;

    // Стартовое здоровье игрока. Тюнингуемое черновое значение.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
    float PlayerMaxHealth = 100.0f;

    // Стартовые деньги НОВОГО персонажа (GDD §7.6 = 50). Применяются в BeginPlay (новый игрок)
    // через Stats->InitMoney — детерминированно, независимо от дефолта компонента/оверрайда в BP.
    // НЕ перетирают загруженный сейв: загрузка идёт только при смерти/у костра (LoadGame ->
    // RestoreState), а BeginPlay сейв не загружает.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    float StartingMoney = 50.0f;

    // Стартовые статы НОВОЙ игры (Build 1, ТЗ раздел 2: «примерно на половине»). Применяются
    // ТОЛЬКО при новой игре (сейва ещё нет): HP и голод/жажда ставятся в эту долю от максимума —
    // раненый приход (нужен для хромоты) и мягкий толчок к еде/воде. При наличии сейва не трогаются.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|New Game", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayPriority = "1"))
    float NewGameHealthFraction = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|New Game", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayPriority = "2"))
    float NewGameSurvivalFraction = 0.5f;

    // Доля MaxHealth, до которой восстанавливается HP при респауне (решение game-lead:
    // респаун = полный HP). 1.0 -> Health = MaxHealth. Остальные статы — из сейва.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayPriority = "5"))
    float RespawnHealthFraction = 1.0f;

    // Доля SurvivalMax, до которой восстанавливаются Голод/Жажда при death-респауне
    // (решение game-lead: респаун = полные статы). 1.0 -> полные. Применяется ТОЛЬКО в
    // ветке HandleDeath, не ломает обычный save/load (quit->reload восстанавливает точные значения).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RespawnSurvivalFraction = 1.0f;

    // Радиус (см), в котором точка смертельного возрождения считается «у костра» (решение
    // лида 08-06, вариант 1+3 против петли смерти в поле). Дальше от ближайшего костра —
    // значит, сейва не было (фолбэк на стартовое поле) или в слоте закреплена точка вне
    // костра (пере-сейв смерти прошлых версий) — игрок ставится к костру деревни.
    // Умолчание — от размера зоны костра: автосейв срабатывает на её краю (радиус зоны 300),
    // берём тройной запас. Только death-путь, «Продолжить» позицию сейва не проверяет.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ClampMin = "100.0", DisplayPriority = "6"))
    float RespawnNearCampfireRadius = 1000.0f;

    // --- Потери при смерти (Build 1.2, переопределение Рината поверх ТЗ издателя №1):
    // без рекламы теряется DeathConsumableLossFraction расходников и DeathMoneyLossFraction
    // денег; со «Спасти рюкзак» (просмотр ролика) — DeathRescuedLossFraction и того и
    // другого. Из потерянного DeathDropShareFraction падает мешком на месте гибели,
    // остальное исчезает. Пока экран смерти открыт — НИЧЕГО не списано; применение — в
    // Respawn(bBackpackRescued). Арифметика — DeathLoss::Compute (headless-тесты). ---

    // Доля денег, изымаемая при смерти БЕЗ просмотра рекламы (Ринат, Build 1.2: 50%;
    // прежний баланс ADR-027 был 40%). База — деньги на момент смерти (снимок HandleDeath).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DeathMoneyLossFraction = 0.50f;

    // Доля неэкипированных расходников, теряемая при смерти БЕЗ рекламы (Ринат: 70%).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DeathConsumableLossFraction = 0.70f;

    // Доля потерь (и расходников, и денег) при смерти С просмотром «Спасти рюкзак» (Ринат: 10%).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DeathRescuedLossFraction = 0.10f;

    // Какая доля ПОТЕРЯННОГО падает мешком на месте гибели (Ринат: половина). Остальное исчезает.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DeathDropShareFraction = 0.50f;

    // Build 1.2.1 (ТЗ В1, Ринат утвердил ровно 360 с): порог суммарного игрового времени,
    // после которого доступны все три rewarded-точки (рюкзак/магазин/ежедневка). Был
    // константой 15 минут в AdGatingLogic.h — теперь настраивается здесь без пересборки.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ads", meta = (ClampMin = "0.0", DisplayName = "Порог рекламы (сек игрового времени)", DisplayPriority = "1"))
    float AdMinPlaytimeSeconds = 360.0f;

    // Лимит показов «Спасти рюкзак» в календарные сутки (ТЗ №1 п.3: не более 3).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ads", meta = (ClampMin = "0"))
    int32 BackpackAdDailyLimit = 3;

    // Как часто сбрасывать накопленное игровое время в сейв (сек). Гейт «15 минут без
    // рекламы» считается по сейву + несброшенному остатку, так что точность не страдает.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ads", meta = (ClampMin = "5.0"))
    float PlaytimeFlushInterval = 60.0f;

    // --- Сейв/респаун (GDD §7.8) ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Save")
    FString SaveSlotName = TEXT("ContrarySave");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Save")
    int32 SaveUserIndex = 0;

    // Куда падает выброшенный из рюкзака предмет (мировой пикап): вперёд от игрока и вниз
    // к ногам (см). DRAFT-тюнинг (BUG3: выброс = пикап, а не Destroy).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (DisplayPriority = "6"))
    float DropForwardOffset = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    float DropDownOffset = 80.0f;

    // Build 1.2 п.6: класс пикапа, которым игрок роняет предмет из рюкзака и мешок смерти
    // (оба места спавна — одно поле). Дефолт — BP_Pickup (/Game/System): Ринат настраивает
    // его в редакторе (меш мешка, масштаб, триггер), и мешок игрока подхватывает те же
    // настройки, что дроп бандитов. Пусто/ассета нет — базовый APickup (мягкий фолбэк).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (DisplayName = "Класс пикапа (мешок)", DisplayPriority = "1"))
    TSubclassOf<APickup> PickupSpawnClass;

    // УСТАРЕЛО (Фаза 1): инлайн-поля голода/жажды. Источник истины теперь Stats.
    // Оставлены, чтобы не ломать возможные ссылки BP; не используются логикой.
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats|Deprecated")
    float Hunger;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats|Deprecated")
    float Thirst;

    // --- Аудио (Демо) ---

    // Звуки шагов: при ходьбе по земле проигрывается СЛУЧАЙНЫЙ из списка по таймеру.
    // Дефолты грузятся в конструкторе из /Game/Audio/Demo/footstep_soft_1..4 (FObjectFinder).
    // Анимаций нет — поэтому шаги по таймеру/скорости в Tick, без AnimNotify.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Footsteps", meta = (DisplayPriority = "8"))
    TArray<USoundBase*> FootstepSounds;

    // Интервал между шагами (сек) при движении. Тюнингуется.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Footsteps", meta = (ClampMin = "0.05"))
    float FootstepInterval = 0.45f;

    // Громкость шагов (тихо). Тюнингуется.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Footsteps", meta = (ClampMin = "0.0"))
    float FootstepVolume = 0.35f;

    // Порог скорости (см/с), выше которого считаем, что персонаж идёт.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Footsteps", meta = (ClampMin = "0.0"))
    float FootstepSpeedThreshold = 50.0f;

    // Фоновый эмбиент леса (зациклен, тихо). Дефолт из /Game/Audio/Demo/forest_ambience_loop.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Ambience")
    USoundBase* AmbienceSound;

    // Громкость эмбиента (фоном, негромко). Тюнингуется.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Ambience", meta = (ClampMin = "0.0"))
    float AmbienceVolume = 0.2f;

    // Выдавать ли новому игроку огнестрел на старте (Build 1.2.2, решение Рината 05-08:
    // НЕ выдавать — игрок начинает с одним ножом, а пистолет копит и покупает у торговца
    // за 150 монет). Выключатель, а не удаление класса ниже: включив галочку, стартовый
    // огнестрел возвращается для отладки и для будущих режимов, ассет BP трогать не нужно.
    // Патроны отдельно не выдаются: они приходят внутри самого пистолета (обойма + резерв),
    // поэтому без него у игрока их нет.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment", meta = (DisplayPriority = "6",
        DisplayName = "Выдавать огнестрел на старте"))
    bool bStartWithRangedWeapon = false;

    // Класс стартового оружия. Спавнится и экипируется в BeginPlay, только если включена
    // галочка выше. Значение (например, BP_Pistol) выставляется в дефолтах BP игрока.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (DisplayPriority = "7"))
    TSubclassOf<AMasterWeapon> DefaultWeaponClass;

    // Класс стартового ближнего оружия (нож). По умолчанию AMeleeWeapon (конкретный класс),
    // чтобы нож был доступен без создания нового .uasset в редакторе (Фаза 3).
    // Спавнится в BeginPlay и держится «в кобуре»; переключение — SwitchWeapon().
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    TSubclassOf<AMasterWeapon> DefaultMeleeWeaponClass;

    // Тест-комплект брони для QA-клавиши F3 (EquipTestArmor). ADR-042: автонадевание брони
    // при старте УБРАНО (игрок начинает с нулевой защитой), поэтому эти классы используются
    // ТОЛЬКО тест-клавишей. По умолчанию — полный сет Т3 (0.48 суммарно), верх прогрессии.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Armor", meta = (DisplayPriority = "8"))
    TSubclassOf<AArmor> TestHeadArmorClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Armor", meta = (DisplayPriority = "9"))
    TSubclassOf<AArmor> TestTorsoArmorClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Armor", meta = (DisplayPriority = "10"))
    TSubclassOf<AArmor> TestPantsArmorClass;

    // --- Стартовая одежда Т0 (ADR-042) ---
    // НЕ предмет: базовые меши слотов тела (нельзя снять/продать, защиты не даёт). Назначается
    // в PostInitializeComponents — ДО снимка базовых мешей CacheBaseSlotMeshes (BeginPlay базы),
    // поэтому снятие ЛЮБОЙ брони возвращает одежду Т0, а не белого манекена. Пустая ссылка или
    // недогрузившийся ассет = слот остаётся с мешем из BP (мягкий фолбэк, без краша).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body", meta = (DisplayPriority = "1"))
    TSoftObjectPtr<USkeletalMesh> StartClothHeadMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body", meta = (DisplayPriority = "2"))
    TSoftObjectPtr<USkeletalMesh> StartClothTorsoMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body", meta = (DisplayPriority = "3"))
    TSoftObjectPtr<USkeletalMesh> StartClothLegsMesh;

    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // Назначает стартовую одежду Т0 в слоты тела (ApplyStartClothing). Зовётся ЗДЕСЬ, а не в
    // BeginPlay: PostInitializeComponents идёт ПОСЛЕ применения дефолтов BP, но ДО BeginPlay
    // базы, где CacheBaseSlotMeshes снимает «базовые меши слотов» — снимок должен увидеть Т0.
    virtual void PostInitializeComponents() override;

    // Процедурные эффекты камеры (#28): дыхание + look-ahead через SpringArm->TargetOffset.
    virtual void Tick(float DeltaTime) override;

    // Build 1.2.1 (ТЗ В2): отладочная клавиша F «реклама доступна сейчас» — легаси-бинд на
    // input-компоненте пешки (QA-действия контроллера живут в его SetupInputComponent, это
    // первый бинд на самой пешке; маппинг QAUnlockAds=F — Config/DefaultInput.ini).
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // Применяет тюнингуемые knob-параметры камеры (#20) к компонентам SpringArm/Camera один раз
    // на этапе конструирования. ВАЖНО: не зовём в BeginPlay/Tick — иначе перетирались бы правки
    // и рантайм-эффекты. Источник истины по камере = Camera-категория UPROPERTY (CameraArmLength
    // и т.д.), а не «сырые» поля компонента; их и редактирует дизайнер в дефолтах BP.
    virtual void OnConstruction(const FTransform& Transform) override;

    // Применяет knob-значения камеры к SpringArm/Camera. Зовётся из конструктора (дефолты CDO)
    // и из OnConstruction (после сериализации BP-оверрайдов → они применяются и видны в редакторе).
    void ApplyCameraSettings();

    // Применяет постобработку (виньетка/экспозиция/зерно/цветокор) к PostProcessSettings камеры с
    // учётом IntroGradeAlpha (экспозиция и насыщенность интерполируются от старта интро к норме).
    void ApplyPostProcessSettings();

    // Спавнит DefaultWeaponClass и экипирует через EquipWeapon (если класс задан).
    void EquipDefaultWeapon();

    // Спавнит DefaultMeleeWeaponClass (нож) и держит «в кобуре» (скрыт, не экипирован).
    void SpawnMeleeWeapon();

public:
    // Занять пустой слот огнестрела предметом, который только что попал игроку (покупка у
    // торговца, находка в трупе или мешке). Build 1.2.2, решение Рината 05-08: на старте
    // огнестрела нет вовсе, поэтому купленный пистолет обязан занимать слот оружия — иначе
    // он лежал бы в рюкзаке мёртвым грузом и стрелять было бы нечем. Слот уже занят —
    // предмет остаётся в рюкзаке (второй ствол не отбираем). Оружие встаёт «в кобуру», как
    // нож: в руки его берёт игрок кнопкой «Оружие» (SwitchWeapon).
    // Возвращает true, если предмет забран из рюкзака в слот.
    bool TryAdoptRangedWeapon(AMasterInventoryItem* Item);

    // Страховка от «огнестрела мимо слота» (корень спама 07-08: легаси-граф BP_PlayerCharacter
    // на ReceiveBeginPlay спавнит BP_Pistol и зовёт EquipWeapon НАПРЯМУЮ — CurrentWeapon
    // становится пистолетом, а RangedWeaponInstance остаётся null; отсюда рассинхрон
    // «CurrentWeapon is ARangedWeapon, but != RangedWeaponInstance» в виджетах). Все штатные
    // C++-пути ставят слот ДО экипировки, поэтому дальнобой в руках без слота — всегда
    // артефакт. По дизайну (Build 1.2.2, решение Рината 05-08) на старте огнестрела нет —
    // артефакт снимается и уничтожается, в руки возвращается нож. Зовётся в конце BeginPlay
    // (BP-событие успевает отработать в Super::BeginPlay); публичный — для автотеста.
    void ReconcileOutOfSlotRangedWeapon();

protected:

    // Грузит меши StartCloth*Mesh (одежда Т0) и ставит их в слоты Head/Torso/Legs.
    // Мягкий фолбэк: не загрузился ассет — слот не трогаем (остаётся меш из BP).
    void ApplyStartClothing();

    // Запускает зацикленный фоновый эмбиент леса (Демо) тихо. Зовётся в BeginPlay.
    void StartAmbience();

    // Обновляет таймер шагов: при ходьбе по земле проигрывает шаги (Демо). Зовётся в Tick.
    void UpdateFootsteps(float DeltaTime);

    /**
     * Sets movement parameters
     */
    void SetUpMovement();

public:
    // Перехват стандартного пайплайна урона UE и роутинг в UStatsComponent
    // (как у AEnemyCharacter), чтобы система оружия работала без правок.
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    UFUNCTION(BlueprintPure, Category = "Stats")
    UStatsComponent* GetStats() const { return Stats; }

    UFUNCTION(BlueprintPure, Category = "Quest")
    UQuestComponent* GetQuests() const { return Quests; }

    // Build 1.2.2 (два слота оружия в инвентаре, Ринат): экземпляры живут ОБА (спавн в
    // BeginPlay), в руках один — CurrentWeapon базы. Экрану инвентаря нужны оба сразу.
    UFUNCTION(BlueprintPure, Category = "Weapon")
    AMasterWeapon* GetRangedWeaponInstance() const { return RangedWeaponInstance; }

    UFUNCTION(BlueprintPure, Category = "Weapon")
    AMasterWeapon* GetMeleeWeaponInstance() const { return MeleeWeaponInstance; }

    // Хромает ли игрок сейчас (HP на пороге LimpHealthFraction и ниже). Читает экранный
    // индикатор хромоты (ULimpIndicatorWidget) каждый тик для своей видимости.
    UFUNCTION(BlueprintPure, Category = "Movement|Limp")
    bool IsLimping() const { return bLimping; }

    // --- Экран смерти (#26) — статистика последней жизни для HUD ---

    // Сколько секунд прожил игрок в последней жизни (момент смерти − старт жизни/респаун).
    UFUNCTION(BlueprintPure, Category = "Death")
    float GetLastLifeDuration() const { return LastLifeDuration; }

    // От кого погиб (читаемое имя последнего нанёсшего урон). «Неизвестно», если урон не от врага.
    UFUNCTION(BlueprintPure, Category = "Death")
    FText GetLastDamagerName() const { return LastDamagerName; }

    // Сколько врагов убито за сессию (инкремент при смерти врага от игрока).
    UFUNCTION(BlueprintPure, Category = "Death")
    int32 GetEnemyKillCount() const { return EnemyKillCount; }

    // Засчитать убийство врага игроком (зовётся из HandleDeath врага/волка). +1 к счётчику
    // киллов + событие аналитики F3 с типом врага (Wolf/Bandit; латиницей — id событий GA).
    UFUNCTION(BlueprintCallable, Category = "Death")
    void RegisterEnemyKill(const FString& EnemyType = TEXT("Unknown"));

    // Возрождение по кнопке экрана смерти / клавише: применяет план потерь (без рекламы —
    // 70% расходников и 50% денег; bBackpackRescued=true после досмотра «Спасти рюкзак» —
    // по 10%), роняет долю потерянного мешком на месте гибели, затем респаун у костра +
    // возврат управления + скрытие экрана смерти. Клавиши Enter/Пробел зовут без аргумента.
    UFUNCTION(BlueprintCallable, Category = "Death")
    void Respawn(bool bBackpackRescued = false);

    // D5: добавить «травму» тряски камеры [0..1] (копится, затухает CameraShakeDecay).
    // Зовут: TakeDamage игрока (DamageShakeTrauma) и выстрел игрока (ARangedWeapon, лёгкая).
    UFUNCTION(BlueprintCallable, Category = "Camera|Shake")
    void AddCameraShake(float Trauma);

    // Ставит альфу арки грейда интро [0..1] и сразу применяет пост-процесс (экспозиция и
    // насыщенность интерполируются от старта интро к норме). 1 — обычная игра (по умолчанию),
    // 0 — самый тёмный/обесцвеченный старт интро. Зовёт интро-последовательность по пути к деревне.
    UFUNCTION(BlueprintCallable, Category = "Camera|PostProcess")
    void SetIntroGradeAlpha(float Alpha);

    // Переключение между дальним (пистолет) и ближним (нож) оружием.
    // Вызывается из контроллера по legacy-инпуту (DefaultInput.ini), без нового .uasset.
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void SwitchWeapon();

    // Доля денег, теряемая при смерти (для текста попапа смерти на HUD — чтобы не расходился
    // с фактическим штрафом при смене параметра). 0.50 = 50%.
    UFUNCTION(BlueprintPure, Category = "Death")
    float GetDeathMoneyLossFraction() const { return DeathMoneyLossFraction; }

    // --- Build 1.2: данные для экрана смерти (блок «Будет потеряно» + «Спасти рюкзак») ---

    // Доли потерь для текстов экрана смерти (живые из настроек, не литералы).
    UFUNCTION(BlueprintPure, Category = "Death")
    float GetDeathConsumableLossFraction() const { return DeathConsumableLossFraction; }

    UFUNCTION(BlueprintPure, Category = "Death")
    float GetDeathRescuedLossFraction() const { return DeathRescuedLossFraction; }

    // Деньги на момент смерти (снимок HandleDeath) — база плана потерь и превью.
    UFUNCTION(BlueprintPure, Category = "Death")
    float GetMoneyAtDeath() const { return MoneyAtDeath; }

    // Номер смерти за сессию (параметр аналитики ad_backpack_button_shown, ТЗ №1 п.5).
    UFUNCTION(BlueprintPure, Category = "Death")
    int32 GetDeathCountThisSession() const { return DeathCountThisSession; }

    // Кандидаты на потерю: НЕэкипированные расходники рюкзака, в порядке инвентаря
    // (тот же порядок использует применение плана — превью не разойдётся с фактом).
    TArray<AMasterInventoryItem*> GetDeathLossCandidates() const;

    // План потерь для показа и применения. bBackpackRescued=true — вариант «после ролика».
    DeathLoss::FPlan ComputeDeathLossPlan(bool bBackpackRescued) const;

    UFUNCTION(BlueprintPure, Category = "Ads")
    int32 GetBackpackAdDailyLimit() const { return BackpackAdDailyLimit; }

    // --- Build 1.2: суммарное игровое время + счётчики rewarded-рекламы (хранятся в сейве) ---

    // Суммарное игровое время профиля с установки, сек (сейв + несброшенный остаток сессии).
    // По нему работает глобальный запрет рекламы до порога AdMinPlaytimeSeconds (AdGating).
    UFUNCTION(BlueprintPure, Category = "Ads")
    float GetTotalPlayTimeSeconds() const { return SavedPlayTimeBase + UnflushedPlayTime; }

    // Порог игрового времени для рекламы (ТЗ В1) — его передают точки показа в AdGating.
    UFUNCTION(BlueprintPure, Category = "Ads")
    float GetAdMinPlaytimeSeconds() const { return AdMinPlaytimeSeconds; }

    // Использований «Спасти рюкзак» за сегодняшние календарные сутки (лимит 3/сутки).
    int32 GetBackpackAdUsesToday() const;

    // Засчитать использование «Спасти рюкзак» (пишет счётчик в сейв). Звать ПОСЛЕ досмотра.
    void RegisterBackpackAdUse();

    // Использований «Продать дороже» за сегодня (лимит настраивается в магазине, ТЗ: 4).
    int32 GetShopAdUsesToday() const;

    // Прошло ли CooldownSeconds с прошлого «Продать дороже» (ТЗ №2: 3 минуты).
    bool IsShopAdCooldownPassed(float CooldownSeconds) const;

    // Засчитать использование «Продать дороже» (счётчик + момент — в сейв).
    void RegisterShopAdUse();

    // --- Сейв/респаун API (GDD §7.8) ---

    // Сохраняет текущее состояние (статы + позиция) в слот. Вызывается костром (автосейв)
    // и доступно из UI/кнопки (ручной сейв, задел). Возвращает true при успехе.
    UFUNCTION(BlueprintCallable, Category = "Save")
    bool SaveGame();

    // Загружает сейв в текущего игрока (статы + позиция). Возвращает true, если сейв был.
    // Респаун после смерти зовёт ИМЕННО эту версию (инвентарь между чекпоинтами НЕ откатывается —
    // это текущее поведение, ломать нельзя); полное восстановление — LoadGameForContinue.
    UFUNCTION(BlueprintCallable, Category = "Save")
    bool LoadGame();

    // Б3 («Продолжить» на стартовом экране): статы+позиция (как LoadGame) ПЛЮС содержимое
    // рюкзака, экипированная броня и журнал квестов. Отдельная функция от LoadGame — иначе
    // смерть/респаун откатывали бы инвентарь до последнего автосейва костра при КАЖДОЙ смерти.
    // Возвращает true, если реальный сейв был и восстановление прошло.
    UFUNCTION(BlueprintCallable, Category = "Save")
    bool LoadGameForContinue();

    // Б3 («Новая игра» поверх существующего сейва, ТЗ издателя п.4 «честно стирает прогресс»):
    // удаляет слот сейва и приводит ЖИВЫЕ статы/позицию/накопитель времени к стартовым значениям
    // новой игры. Нужна отдельно от BeginPlay: пока сейв ещё существовал, BeginPlay применил
    // «статы как при загрузке» (полные HP/деньги), а не «половина HP» новой игры.
    UFUNCTION(BlueprintCallable, Category = "Save")
    void ResetToNewGame();

    // Есть ли сохранение в слоте С РЕАЛЬНЫМ ПРОГРЕССОМ (bHasData=true у объекта в слоте), а не
    // просто существование файла слота: FlushPlayTime создаёт файл уже через минуту игры (только
    // накопитель времени, bHasData остаётся false), поэтому «файл есть» и «есть что продолжать» —
    // РАЗНЫЕ вещи (Б3, находка лида по журналу с телефона).
    UFUNCTION(BlueprintPure, Category = "Save")
    bool HasSaveGame() const;

    // --- Этап F: доступ к слоту для полей удержания (ежедневка F2 / подсказки F1) ---

    // Грузит объект сейва из слота либо создаёт свежий (bHasData=false — такой LoadGame
    // игнорирует, позиция/статы не пострадают). Для компонентов удержания, которые правят
    // ТОЛЬКО свои поля и пишут обратно WriteSaveObject.
    UContrarySaveGame* LoadOrCreateSaveObject() const;

    // Пишет объект сейва обратно в слот. true при успехе.
    bool WriteSaveObject(UContrarySaveGame* Save) const;

    // Компонент онбординга (F1) — контроллер дёргает TryShowHint по событиям.
    UFUNCTION(BlueprintPure, Category = "Retention")
    UOnboardingComponent* GetOnboarding() const { return Onboarding; }

    // Компонент ежедневной награды (F2) — контроллер сообщает о закрытии диалога старосты
    // (Build 1: окно награды отложено до конца интро, NotifyElderDialogClosed).
    UFUNCTION(BlueprintPure, Category = "Retention")
    UDailyRewardComponent* GetDailyReward() const { return DailyReward; }

    // --- Тестирование брони без UI (Фаза 4, UI — отдельная волна) ---
    // Консольные команды (открыть консоль `~`, ввести имя). Pawn должен быть под управлением.
    //   EquipTestArmor   — (пере)спавнит и надевает тест-комплект Test*ArmorClass (дефолт — полный Т3).
    //   UnequipTestArmor — снимает броню всех слотов (возврат базовых мешей тела = одежды Т0).
    // Позволяют наблюдать смену модульного меша слота и пересчёт суммарной защиты.

    UFUNCTION(Exec, Category = "Equipment|Armor|Debug")
    void EquipTestArmor();

    UFUNCTION(Exec, Category = "Equipment|Armor|Debug")
    void UnequipTestArmor();

    // --- Действия UI-инвентаря (Фаза 4, GDD §7.4) ---
    // Высокоуровневые операции инвентаря (вызываются из AContrarySurvivorHUD по клику).
    // Знание о Stats/EquipArmor/Inventory сосредоточено здесь, у владельца предметов.

    // Использовать предмет рюкзака по клику: броня -> надеть (EquipArmor + пометить экип);
    // расходник -> применить эффект (еда/вода через Stats) и удалить из рюкзака.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void Inv_UseBackpackItem(AMasterInventoryItem* Item);

    // Выбросить предмет рюкзака (удалить из инвентаря). Экипированную броню сперва снимает.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void Inv_DropItem(AMasterInventoryItem* Item);

    // Снять броню из слота (клик по слоту paper-doll): UnequipArmor + вернуть предмет в рюкзак
    // (как неэкипированный), чтобы его было видно/можно надеть заново.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void Inv_UnequipSlot(EArmorSlot Slot);

    // Положить в рюкзак Count расходников заданного типа (еда/вода/бинт). Один вход для
    // сюжетной выдачи предметов: сам спавнит предмет, проставляет служебный ключ и
    // переводимое название из AConsumableItem и прячет визуал (предмет рюкзака — это
    // данные, а не объект на сцене). Возвращает, сколько штук реально легло в рюкзак.
    int32 GiveConsumableToBackpack(EConsumableType Type, int32 Count = 1);

    // --- Магазин торговца (Фаза 4, экономика — GDD §7.6) ---
    // Вызываются из AContrarySurvivorHUD по клику в экране магазина.

    // Купить позицию каталога (qty=1). Тонкая обёртка над Shop_BuyEntryQty — сохраняет старые
    // call-site'ы (QA-клавиши/HUD). Возвращает true при успешной покупке.
    bool Shop_BuyEntry(const FShopEntry& Entry);

    // Купить Qty единиц позиции каталога: проверяет деньги на Qty*Price, выдаёт товар (патроны —
    // в рюкзак стаком; предметы — Qty копий в рюкзак) и списывает цену. Слайдер магазина зовёт
    // это с выбранным количеством (Фаза 5, STALKER 2-стиль). Возвращает true при успехе.
    bool Shop_BuyEntryQty(const FShopEntry& Entry, int32 Qty);

    // Продать предмет рюкзака за SellPrice (целиком): экип. броню сначала снимает, удаляет
    // предмет, начисляет деньги. Для нестакающихся предметов.
    void Shop_SellItem(AMasterInventoryItem* Item, float SellPrice);

    // Продать Qty единиц из стака патронов (AAmmoItem) за UnitSellPrice за патрон. Уменьшает
    // StackCount; пустую пачку удаляет. Для прочих предметов — продаёт целиком (Qty игнор).
    void Shop_SellItemQty(AMasterInventoryItem* Item, float UnitSellPrice, int32 Qty);

    // --- Патроны как стак-предмет рюкзака (Фаза 5, STALKER 2-стиль) ---

    // Суммарно патронов во всех пачках (AAmmoItem) рюкзака.
    UFUNCTION(BlueprintPure, Category = "Inventory|Ammo")
    int32 GetReserveAmmoInInventory() const;

    // Добавить патроны в рюкзак стаком: пополняет первую неполную пачку, остаток — новой пачкой.
    UFUNCTION(BlueprintCallable, Category = "Inventory|Ammo")
    void AddAmmoToInventory(int32 Amount);

    // Изъять до Amount патронов из рюкзака (для перезарядки). Возвращает реально изъятое;
    // опустошённые пачки удаляются.
    UFUNCTION(BlueprintCallable, Category = "Inventory|Ammo")
    int32 TakeAmmoFromInventory(int32 Amount);

    // Перезарядка игрока: сперва пополняет резерв оружия из пачки патронов рюкзака, затем —
    // штатный перенос резерв->обойма (база). Переопределяет AMasterHumanoidCharacter.
    virtual void ReloadCurrentWeapon() override;

    // DEBUG-команда наполнения рюкзака тестовыми предметами (консоль `~`, ввести GiveTestItems):
    // пара расходников (еда+вода) + запасная броня головы/торса — чтобы было что
    // надевать/использовать/выбрасывать в Play без редактора и без BP-ассетов.
    UFUNCTION(Exec, Category = "Inventory|Debug")
    void GiveTestItems();

protected:
    // Смерть игрока (привязана к Stats->OnDeath): респаун на последней точке сейва
    // (костёр) + штраф «вне деревни» (ADR-027). Экипировка/оружие/квест-предметы сохраняются.
    virtual void HandleDeath() override;

    // Пересчитывает хромоту по текущему HP (Build 1): HP на пороге и ниже — сниженная скорость
    // ходьбы (+ зацикленный звук дыхания, если задан), выше — обычная. Привязана к
    // UStatsComponent::OnHealthChanged и вызывается один раз в BeginPlay. Сигнатура совпадает
    // с делегатом FOnHealthChanged (NewHealth, MaxHealth).
    UFUNCTION()
    void UpdateLimpState(float NewHealth, float InMaxHealth);

    // Ведёт экранный индикатор хромоты (Build 1, приёмка Рината 07-27): лениво создаёт кодовый
    // виджет (без .uasset), держит его в viewport и ловит момент первого показа развёрнутой
    // подсказки (свободное управление: интро передало управление, модалки закрыты).
    // Зовётся из Tick — дёшево, сплошные ранние выходы. Видимость плашки ведёт сам виджет.
    void UpdateLimpIndicator();

    // Разовая развёрнутая подсказка: показать (по таймеру задержки LimpFirstHintDelay) и по
    // истечении LimpFirstHintDuration вернуть плашке компактный текст индикатора.
    void ShowLimpFirstHint();
    void EndLimpFirstHint();

    // Build 1.2 (заменяет DropConsumablesAsBag из ADR-027): снимает из рюкзака Plan.LostItems
    // расходников (в порядке инвентаря); первые Plan.DroppedItems + Plan.DroppedMoney падают
    // возвращаемым «мешком» (APickup) на месте гибели, остальное уничтожается.
    // Квест-предметы/ресурсы/экипировка не трогаются.
    void DropDeathLoss(const FVector& DeathLoc, const DeathLoss::FPlan& Plan);

    // Build 1.2 (заменяет ApplyMoneyDeathPenalty): выставляет деньги в
    // (MoneyAtDeath - Plan.LostMoney) ПОСЛЕ загрузки сейва (иначе LoadGame перетёр бы
    // баланс) + пере-сохранение (анти-reload-эксплойт, как раньше).
    void ApplyDeathMoneyLoss(const DeathLoss::FPlan& Plan);

    // Смертельное возрождение обязано заканчиваться у костра, как обещает экран смерти
    // (решение лида 08-06, вариант 1+3): если точка после LoadGame (или стартовый фолбэк
    // без сейва) дальше RespawnNearCampfireRadius от ближайшего костра — переставляет
    // игрока к костру (в стороне от огня, лицом к нему, Z по полу). Костёр ищется по
    // классу ACampfire (не по имени экземпляра). Мир без костра — точка не меняется.
    void RelocateDeathRespawnNearCampfire();

    // Сброс накопленного игрового времени сессии в сейв (таймер PlaytimeFlushInterval).
    void FlushPlayTime();

#if CONTRARY_WITH_QA_CHEATS
    // Build 1.2.1 (ТЗ В2): ОТЛАДОЧНАЯ клавиша F — добивает накопитель игрового времени до
    // порога AdMinPlaytimeSeconds, немедленно пишет его в сейв и печатает LogQA-строку.
    // В публикационной сборке не компилируется (Б5 задания издателя).
    void OnQAUnlockAds();
#endif

    // Применяет загруженный сейв к игроку (статы + телепорт в точку респауна).
    void ApplySaveData(const UContrarySaveGame* Save);

    // Б3 (LoadGameForContinue): спавнит предметы Save->InventoryEntries в рюкзак (скрытые, как
    // остальные предметы-данные); записи с bEquipped=true, кроме этого, надеваются (EquipArmor).
    void RestoreInventoryAndArmor(const UContrarySaveGame* Save);

private:
    // Стартовый трансформ (фолбэк-точка респауна, если сейва ещё нет).
    FTransform InitialSpawnTransform;

    // --- Экран смерти (#26): трекинг текущей жизни ---
    // Момент (World time) начала текущей жизни: ставится в BeginPlay и сбрасывается в Respawn.
    float LifeStartTime = 0.0f;

    // Длительность последней жизни (сек) — фиксируется в HandleDeath для экрана смерти.
    float LastLifeDuration = 0.0f;

    // Читаемое имя последнего нанёсшего урон (для «от кого погиб»). Обновляется в TakeDamage.
    // Кто убил — ПЕРЕВОДИМЫЙ текст: игрок читает его на экране смерти каждый раз, когда
    // погибает (ADR-050). Служебное имя объекта сюда НЕ попадает: неизвестный источник
    // урона даёт «Неизвестно», а сам объект уходит в лог (протечка вида BP_..._C_2).
    FText LastDamagerName = NSLOCTEXT("Death", "KillerUnknown", "Неизвестно");

    // Счётчик убитых игроком врагов за сессию (инкремент RegisterEnemyKill).
    int32 EnemyKillCount = 0;

    // A4/ADR-027: место гибели (для «мешка» расходников) — снимок в HandleDeath ДО телепорта респауна.
    FVector DeathDropLocation = FVector::ZeroVector;

    // Build 1.2: деньги на момент смерти (снимок HandleDeath) — база плана потерь: превью
    // на экране смерти и фактическое списание считаются от ОДНОГО числа.
    float MoneyAtDeath = 0.0f;

    // Номер смерти за сессию (аналитика ТЗ №1). Инкремент в HandleDeath.
    int32 DeathCountThisSession = 0;

    // Build 1.2.1 (ТЗ Е): анимация смерти запущена (меш ушёл в Single Node) — Respawn
    // обязан вернуть режим AnimationBlueprint, иначе игрок остался бы в позе смерти.
    bool bDeathAnimPlayed = false;

    // --- Build 1.2: накопитель игрового времени (гейт «15 минут без рекламы») ---
    // База из сейва (читается в BeginPlay, обновляется при сбросе) + несброшенный остаток.
    float SavedPlayTimeBase = 0.0f;
    float UnflushedPlayTime = 0.0f;
    FTimerHandle PlaytimeFlushTimer;

    // Инстансы оружия (оба заспавнены в BeginPlay). CurrentWeapon базы указывает на активный.
    UPROPERTY()
    AMasterWeapon* RangedWeaponInstance = nullptr;

    UPROPERTY()
    AMasterWeapon* MeleeWeaponInstance = nullptr;

    // Кэш инвентаря (UInventoryComponent на базе AMasterHumanoidCharacter, защищён).
    // Доступ к нему — через каст в .cpp (Inventory protected в базе).

    // Альфа арки грейда интро [0..1]: 1 = нормальный грейд (по умолчанию, обычная игра),
    // 0 = старт интро (темнее/обесцвеченнее). Ведёт интро-последовательность (SetIntroGradeAlpha).
    float IntroGradeAlpha = 1.0f;

    // --- Рантайм-состояние процедурных эффектов камеры (#28) ---
    // Накопленное время для синусного «дыхания».
    float CameraBreathingTime = 0.0f;

    // Текущее сглаженное смещение look-ahead/боевого смещения (world XY), интерполируется в Tick.
    FVector CameraLookAheadOffset = FVector::ZeroVector;

    // Идёт возврат камеры из боя: офсет ведём мягкой CombatCameraExitInterpSpeed, пока он не
    // догонит цель исследования (иначе выход из боя дёргался бы на скорости look-ahead).
    bool bCombatCameraRecovering = false;

    // --- Рантайм тряски камеры (D5) ---
    // Текущая «травма» [0..1] (копится AddCameraShake, затухает CameraShakeDecay).
    float CameraShakeTrauma = 0.0f;

    // Накопленное время пробега по шуму Перлина (масштабируется CameraShakeFrequency).
    float CameraShakeTime = 0.0f;

    // --- Аудио-рантайм (Демо) ---
    // Накопитель времени для интервала шагов.
    float FootstepAccumulator = 0.0f;

    // Активный компонент фонового эмбиента (зациклен, не авто-уничтожается).
    UPROPERTY()
    UAudioComponent* AmbienceComponent = nullptr;

    // --- Хромота (Build 1) ---
    // Хромает ли игрок сейчас (HP на пороге и ниже) — меняем скорость/звук только на смене состояния.
    bool bLimping = false;

    // Активный зациклённый звук тяжёлого дыхания при хромоте (если LimpBreathingSound задан).
    UPROPERTY()
    UAudioComponent* LimpBreathingComponent = nullptr;

    // Экранный индикатор хромоты (кодовый UMG-виджет; создаётся лениво в UpdateLimpIndicator,
    // когда игрок впервые захромал и есть локальный контроллер с вьюпортом).
    UPROPERTY()
    TObjectPtr<ULimpIndicatorWidget> LimpIndicatorWidget;

    // «Раз за сессию» для развёрнутой подсказки — чистая логика (покрыта headless-тестом).
    FLimpFirstHintState LimpFirstHint;

    // Развёрнутая подсказка сейчас на плашке (между Show и End) — чтобы виджет, созданный
    // с опозданием, получил правильный текст.
    bool bLimpHintExpandedActive = false;

    // Один таймер на обе фазы подсказки: задержка показа, затем длительность показа.
    FTimerHandle LimpHintTimer;
};
