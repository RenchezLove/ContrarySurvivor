// Fill out your copyright notice in the Description page of Project Settings.


// PlayerCharacter.cpp
// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "Engine/Scene.h" // FPostProcessSettings / AEM_Manual (постобработка камеры, Build 1)
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Controller.h" // Enhanced Input
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Components/MeleeSectorIndicatorComponent.h" // Build 1.1: подсветка сектора ножа
#include "ContrarySurvivor/Retention/DailyRewardComponent.h" // Этап F2: ежедневная награда
#include "ContrarySurvivor/Retention/OnboardingComponent.h"  // Этап F1: онбординг-подсказки
#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h"   // Этап F3: события аналитики
#include "ContrarySurvivor/Save/ContrarySaveGame.h"
#include "ContrarySurvivor/Subsystems/SpawnPlacementUtils.h"
#include "Components/CapsuleComponent.h"
#include "UInventoryComponent.h"
#include "AMasterInventoryItem.h"
#include "AMeleeWeapon.h"
#include "AHeadArmor.h"
#include "ATorsoArmor.h"
#include "APantsArmor.h"
#include "AArmorTiers.h" // тест-комплект Т3 для QA-клавиши F3 (ADR-042)
#include "Engine/SkeletalMesh.h" // загрузка мешей одежды Т0 (ApplyStartClothing)
#include "Components/SkeletalMeshComponent.h" // Build 1.2.1 (ТЗ Е): возврат AnimBP после анимации смерти
#include "AConsumableItem.h"
#include "AAmmoItem.h" // патроны как стак-предмет рюкзака (Фаза 5)
#include "ARangedWeapon.h"
#include "ContrarySurvivor/Actors/ShopTypes.h" // FShopEntry, EShopEntryKind (A2)
#include "ContrarySurvivor/Actors/Pickup.h"    // выброс = мировой пикап (BUG3)
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h" // CloseAllUI / экран смерти
#include "ContrarySurvivor/Controllers/EnemyAIController.h" // D6: реестр врагов для боевой камеры (ADR-035)
#include "ContrarySurvivor/Characters/WolfCharacter.h"  // #26: читаемое имя «от кого погиб»
#include "ContrarySurvivor/Characters/EnemyCharacter.h" // #26: читаемое имя «от кого погиб»
#include "ContrarySurvivor/ContrarySurvivor.h"  // LogQA
#include "ContrarySurvivor/Ads/AdGatingLogic.h"  // Build 1.2: суточные счётчики/кулдаун рекламы
#include "ContrarySurvivor/Debug/QADebug.h"      // QA god-mode (неуязвимость)
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Components/AudioComponent.h"
#include "NavigationInvokerComponent.h" // Navigation Invoker: навмеш следует за игроком
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h" // таймеры разовой подсказки хромоты (Build 1)

APlayerCharacter::APlayerCharacter()
{
    

    // Камера в стиле Last Day on Earth (#20): пологий угол сверху, узкий FOV, плавный lag.
    // Конкретные значения берутся из тюнингуемых UPROPERTY (дефолты заданы в заголовке) и
    // применяются здесь + повторно в BeginPlay (ApplyCameraSettings) на случай BP-оверрайда.

    // Create Spring Arm Component
    SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArmComponent->SetupAttachment(RootComponent); // Attaching to RootComponent (CapsuleComponent)
    SpringArmComponent->TargetArmLength = CameraArmLength;          // Дистанция камеры (LDoE ~1000)
    SpringArmComponent->SetRelativeRotation(CameraBoomRotation);    // Угол LDoE (Pitch ~-55, Yaw 90)

    SpringArmComponent->bDoCollisionTest = false;
    //Disable collision chek for springarm. If true -> When spring arm is overlaped by something -> Camera movese closer to player

    // Камера фиксирована относительно мира (top-down/LDoE): не наследует вращение пешки/контроллера.
    SpringArmComponent->bUsePawnControlRotation = false;
    SpringArmComponent->bInheritPitch = false;
    SpringArmComponent->bInheritYaw   = false;
    SpringArmComponent->bInheritRoll  = false;

    // Плавное отставание (lag) — лёгкое «оживление» движения камеры.
    SpringArmComponent->bEnableCameraLag       = bEnableCameraLag;
    SpringArmComponent->CameraLagSpeed         = CameraLagSpeed;
    SpringArmComponent->CameraLagMaxDistance   = CameraLagMaxDistance;

    // Create Camera Component
    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    CameraComponent->SetupAttachment(SpringArmComponent, USpringArmComponent::SocketName); // Attach to   SpringArm
    CameraComponent->bUsePawnControlRotation = false;                                      // Camera not rotates whith character
    CameraComponent->SetProjectionMode(ECameraProjectionMode::Perspective);
    CameraComponent->SetFieldOfView(CameraFieldOfView);                                    // Узкий FOV (LDoE-сжатие)

    
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    //Sets that camera is not rotates whith controller

    // Игра ОДИНОЧНАЯ: репликация не нужна. ACharacter по умолчанию реплицируется и сглаживает
    // движение сетевого прокси (NetworkSmoothingMode=Exponential); это сглаживание/коррекция
    // может давать «резиновый» откат локального персонажа на пару см. Глушим репликацию и
    // сетевое сглаживание, чтобы движение было чисто локальным и без коррекций.
    bReplicates = false;
    SetReplicateMovement(false);
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;
    }

    //Parameters initialasation (устаревшие инлайн-поля, см. заголовок)
    Hunger = 100.0f;
    Thirst = 100.0f;

    // Компонент статов игрока (ADR-015). Источник истины по HP/выживанию.
    Stats = CreateDefaultSubobject<UStatsComponent>(TEXT("StatsComponent"));
    // У игрока (в отличие от врага) деградация голода/жажды включена (GDD §7.3).
    Stats->SetSurvivalDegradationEnabled(true);

    // Журнал квестов (Фаза 5). C++-сабобъект — детерминированно, без BP.
    Quests = CreateDefaultSubobject<UQuestComponent>(TEXT("QuestComponent"));

    // Этап F: удержание — ежедневная награда (F2) и онбординг-подсказки (F1).
    // C++-сабобъекты (как Stats/Quests): работают и без правок BP игрока.
    DailyReward = CreateDefaultSubobject<UDailyRewardComponent>(TEXT("DailyRewardComponent"));
    Onboarding = CreateDefaultSubobject<UOnboardingComponent>(TEXT("OnboardingComponent"));

    // Подсветка сектора ближнего боя (Build 1.1, п.5). Крепится к капсуле — тогда декаль
    // едет и разворачивается вместе с игроком, а её локальная ось «вперёд» совпадает с тем
    // самым GetActorForwardVector(), по которому AMeleeWeapon::Fire отбирает цели в секторе.
    MeleeSectorIndicator = CreateDefaultSubobject<UMeleeSectorIndicatorComponent>(TEXT("MeleeSectorIndicator"));
    MeleeSectorIndicator->SetupAttachment(GetCapsuleComponent());

    // Navigation Invoker (Фаза 5): навмеш генерится локально вокруг игрока и следует за ним.
    // Вместе с bGenerateNavigationOnlyAroundNavigationInvokers=true (DefaultEngine.ini) это
    // гарантирует тайлы навмеша у боевых зон (база бандитов на севере, Логово на западе) в момент, когда
    // игрок туда приходит -> враги получают навигацию (navmesh=yes) и преследуют. Радиусы —
    // через SetGenerationRadii (поля TileGeneration/RemovalRadius protected в компоненте).
    // API сверено по UE 5.5: NavigationInvokerComponent.h:45 SetGenerationRadii(Gen, Removal).
    NavInvoker = CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("NavInvoker"));
    NavInvoker->SetGenerationRadii(NavInvokerGenerationRadius, NavInvokerRemovalRadius);

    // Нож доступен «из коробки» без нового .uasset: дефолт = конкретный AMeleeWeapon.
    // BP игрока может переопределить (например, на BP_Knife) в дефолтах.
    DefaultMeleeWeaponClass = AMeleeWeapon::StaticClass();

    // Тест-комплект для QA-клавиши F3 (ADR-042: автонадевания при старте больше НЕТ).
    // Дефолт — полный сет Т3 (верх прогрессии, 0.48 суммарной защиты).
    TestHeadArmorClass  = AHeadArmorT3::StaticClass();
    TestTorsoArmorClass = ATorsoArmorT3::StaticClass();
    TestPantsArmorClass = APantsArmorT3::StaticClass();

    // Стартовая одежда Т0 (ADR-042): мягкие ссылки на меши слотов тела. Применяются в
    // PostInitializeComponents (до снимка базовых мешей), защиты не дают, не предмет.
    StartClothHeadMesh  = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Characters/Shared/Clothing/SK_Cloth_T0_Head.SK_Cloth_T0_Head")));
    StartClothTorsoMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Characters/Shared/Clothing/SK_Cloth_T0_Torso.SK_Cloth_T0_Torso")));
    StartClothLegsMesh  = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Characters/Shared/Clothing/SK_Cloth_T0_Legs.SK_Cloth_T0_Legs")));

    // --- Аудио (Демо): шаги + эмбиент. Дефолты из импортированных ассетов. ---
    static ConstructorHelpers::FObjectFinder<USoundBase> Step1(TEXT("/Game/Audio/Demo/footstep_soft_1.footstep_soft_1"));
    if (Step1.Succeeded()) { FootstepSounds.Add(Step1.Object); }
    static ConstructorHelpers::FObjectFinder<USoundBase> Step2(TEXT("/Game/Audio/Demo/footstep_soft_2.footstep_soft_2"));
    if (Step2.Succeeded()) { FootstepSounds.Add(Step2.Object); }
    static ConstructorHelpers::FObjectFinder<USoundBase> Step3(TEXT("/Game/Audio/Demo/footstep_soft_3.footstep_soft_3"));
    if (Step3.Succeeded()) { FootstepSounds.Add(Step3.Object); }
    static ConstructorHelpers::FObjectFinder<USoundBase> Step4(TEXT("/Game/Audio/Demo/footstep_soft_4.footstep_soft_4"));
    if (Step4.Succeeded()) { FootstepSounds.Add(Step4.Object); }

    static ConstructorHelpers::FObjectFinder<USoundBase> Ambience(TEXT("/Game/Audio/Demo/forest_ambience_loop.forest_ambience_loop"));
    if (Ambience.Succeeded()) { AmbienceSound = Ambience.Object; }

    // Build 1.2 п.6: класс пикапа для дропа предмета и мешка смерти — BP_Pickup, если он
    // уже есть на диске (создаётся оператором отдельным шагом); нет — мягкий фолбэк на
    // базовый APickup, чтобы дроп работал и до появления ассета.
    static ConstructorHelpers::FClassFinder<APickup> PickupBP(TEXT("/Game/System/BP_Pickup"));
    PickupSpawnClass = PickupBP.Succeeded()
        ? TSubclassOf<APickup>(PickupBP.Class)
        : TSubclassOf<APickup>(APickup::StaticClass());

    SetUpMovement();
}

void APlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Камера НЕ настраивается в BeginPlay: knob-параметры применяются один раз в OnConstruction
    // (см. ApplyCameraSettings). Это убирает анти-паттерн «BeginPlay перетирает правки камеры».

    // Запоминаем стартовый трансформ — фолбэк-точка респауна, если сейва ещё нет.
    // BUG (демка неиграбельна): если игрок появился под картой (Z=-4055 — битый
    // PlayerStart / провал до постройки навмеша), петля "падение -> KillZ -> респавн".
    // Гарантируем, что игрок ВСЕГДА на полу: при битом Z трассируем до пола (не зависит
    // от навмеша) и переставляем. Корректный трансформ запоминаем как фолбэк респауна.
    {
        FVector StartLoc = GetActorLocation();
        const float OldZ = StartLoc.Z;
        const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
        const float GroundClearance = 10.0f; // зазор над полом, чтобы капсула НЕ пенетрировала

        // BugReport12 ФИНАЛ: капсула спавнилась УТОПЛЕННОЙ в пол (PIE-диаг: floorDist=-34,
        // bStartPenetrating=1, overlap с полом). Penetrating-капсула → горизонтальный свип CMC
        // упирается в depenetration → Velocity гасится в 0 каждый кадр → «accel=2048, но не едет».
        // Корень — РАЗМЕЩЕНИЕ (Z игрока в уровне/PlayerStart не учитывает half-height капсулы),
        // не код движения. Прошлый relocate срабатывал только при Z<-1000 (провал под карту) и
        // НЕ ловил «слегка утоплен». Обобщаем: трассируем пол под (X,Y) и ставим ЦЕНТР капсулы на
        // floor + halfHeight + зазор, если игрок (1) под картой ЛИБО (2) утоплен (Z ниже желаемого
        // центра). Если игрок уже на/над полом — не трогаем (валидное размещение сохраняется).
        float FloorCenterZ = 0.0f; // желаемый Z ЦЕНТРА капсулы (= impact + halfHeight + зазор)
        const bool bFoundFloor = SpawnPlacement::TraceFloorZ(
            GetWorld(), StartLoc.X, StartLoc.Y, HalfHeight + GroundClearance, FloorCenterZ, this, TEXT("Player-start"));

        bool bRelocated = false;
        if (bFoundFloor && (OldZ < SpawnPlacement::BadZThreshold || OldZ < FloorCenterZ - 1.0f))
        {
            StartLoc.Z = FloorCenterZ;
            bRelocated = true;
        }
        else if (!bFoundFloor && OldZ < SpawnPlacement::BadZThreshold)
        {
            // Пол не найден И игрок под картой — безопасный фикс-Z (центр капсулы над SafeDefaultZ).
            StartLoc.Z = SpawnPlacement::SafeDefaultZ + HalfHeight;
            bRelocated = true;
        }

        if (bRelocated)
        {
            if (UCharacterMovementComponent* Move = GetCharacterMovement())
            {
                Move->StopMovementImmediately();
            }
            SetActorLocation(StartLoc, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
        }
    }
    InitialSpawnTransform = GetActorTransform();

    // #26: засекаем старт текущей жизни (для статистики «сколько прожил» на экране смерти).
    LifeStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    LastDamagerName = NSLOCTEXT("Death", "KillerUnknown", "Неизвестно");

    // Build 1.2: база суммарного игрового времени — из сейва; остаток сессии копится в Tick
    // и сбрасывается в слот по таймеру (гейт «15 минут без рекламы», ТЗ раздел 0 п.2).
    if (const UContrarySaveGame* PlaySave = LoadOrCreateSaveObject())
    {
        SavedPlayTimeBase = FMath::Max(0.0f, PlaySave->TotalPlayTimeSeconds);
    }
    if (UWorld* PlayWorld = GetWorld())
    {
        PlayWorld->GetTimerManager().SetTimer(PlaytimeFlushTimer, this,
            &APlayerCharacter::FlushPlayTime, FMath::Max(5.0f, PlaytimeFlushInterval), /*bLoop=*/true);
    }

    // Инициализируем HP игрока через UStatsComponent (источник истины).
    if (Stats)
    {
        Stats->InitHealth(PlayerMaxHealth, /*bSetToMax=*/true);
        // НОВЫЙ игрок стартует с GDD §7.6 = 50 денег. Делаем это явно в коде (не полагаясь на
        // дефолт компонента/возможный оверрайд в BP), но ТОЛЬКО как стартовое значение нового
        // персонажа: BeginPlay сейв не загружает, загрузка (RestoreState) идёт позже отдельно.
        Stats->InitMoney(StartingMoney);
        // Смерть игрока -> респаун (GDD §7.8).
        Stats->OnDeath.AddDynamic(this, &APlayerCharacter::HandleDeath);
        // Хромота от низкого HP (Build 1): реагируем на любое изменение HP, чтобы после лечения
        // аптечкой скорость восстанавливалась сама.
        Stats->OnHealthChanged.AddDynamic(this, &APlayerCharacter::UpdateLimpState);

        // НОВАЯ игра (сейва ещё нет) — стартуем «примерно на половине» (ТЗ раздел 2): раненый
        // приход + половина голода/жажды (первый урок выживания/экономики). При наличии сейва
        // статы не трогаем — обычный старт (загрузка идёт позже, при смерти/у костра). SetHealth
        // бродкастит OnHealthChanged → хромота включится сама.
        if (!HasSaveGame())
        {
            Stats->SetHealth(Stats->GetMaxHealth() * NewGameHealthFraction);
            Stats->SetHunger(Stats->GetSurvivalMax() * NewGameSurvivalFraction);
            Stats->SetThirst(Stats->GetSurvivalMax() * NewGameSurvivalFraction);
        }

        // Первичный пересчёт хромоты по итоговому стартовому HP (у новой игры — половина, хромает).
        UpdateLimpState(Stats->GetHealth(), Stats->GetMaxHealth());
    }

    // Стартовое оружие (Фаза 1: автоэкипировка пистолета вместо подбора с земли).
    EquipDefaultWeapon();

    // Нож держим «в кобуре» (скрыт), переключение по SwitchWeapon (Фаза 3).
    SpawnMeleeWeapon();

    // ADR-042: стартовой брони НЕТ — игрок начинает с нулевой защитой (полный урон),
    // первая цель — накопить на первый комплект. Визуально одет в одежду Т0
    // (ApplyStartClothing в PostInitializeComponents), она не предмет и защиты не даёт.

    // Фоновый эмбиент леса (Демо), зациклен и тихо.
    StartAmbience();

    // ЗАЩИТНЫЙ ФИКС движения: NavWalking двигает пешку ТОЛЬКО по навмешу; на участке без
    // навмеш-тайла горизонтальной трансляции нет, при этом orient-to-movement крутит пешку к
    // вводу (симптом «крутится, но не едет»). Walking (физический коллижн-флор) не зависит от
    // навмеша — корректный режим для top-down. Если режим уже Walking — не трогаем.
    if (UCharacterMovementComponent* CM = GetCharacterMovement())
    {
        if (CM->MovementMode == MOVE_NavWalking)
        {
            CM->SetMovementMode(MOVE_Walking);
        }
    }
}

void APlayerCharacter::StartAmbience()
{
    if (!AmbienceSound)
    {
        return;
    }

    // Зацикливание в UE 5.5 — свойство САМОГО ассета (USoundWave::bLooping); у функций
    // SpawnSound2D/PlaySound2D параметра loop нет. Импортированный SoundWave по умолчанию
    // не зациклен, поэтому форсим bLooping на загруженном ассете перед запуском.
    if (USoundWave* Wave = Cast<USoundWave>(AmbienceSound))
    {
        Wave->bLooping = true;
    }

    // bAutoDestroy=false — держим компонент живым (зациклен), храним ссылку.
    AmbienceComponent = UGameplayStatics::SpawnSound2D(
        this, AmbienceSound, AmbienceVolume, 1.0f, 0.0f, nullptr, false, /*bAutoDestroy=*/false);
}

void APlayerCharacter::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // Применяем knob-параметры камеры после сериализации BP-оверрайдов: меняешь CameraArmLength
    // (и др. Camera-поля) в дефолтах BP → значение применяется к SpringArm/Camera здесь, видно
    // в редакторе сразу и в игре, без перетирания в рантайме (BeginPlay/Tick камеру не трогают).
    ApplyCameraSettings();
}

void APlayerCharacter::ApplyCameraSettings()
{
    if (SpringArmComponent)
    {
        SpringArmComponent->TargetArmLength = CameraArmLength;
        SpringArmComponent->SetRelativeRotation(CameraBoomRotation);
        SpringArmComponent->bDoCollisionTest = false;
        SpringArmComponent->bUsePawnControlRotation = false;
        SpringArmComponent->bInheritPitch = false;
        SpringArmComponent->bInheritYaw   = false;
        SpringArmComponent->bInheritRoll  = false;
        SpringArmComponent->bEnableCameraLag     = bEnableCameraLag;
        SpringArmComponent->CameraLagSpeed       = CameraLagSpeed;
        SpringArmComponent->CameraLagMaxDistance = CameraLagMaxDistance;
    }
    if (CameraComponent)
    {
        CameraComponent->SetProjectionMode(ECameraProjectionMode::Perspective);
        CameraComponent->SetFieldOfView(CameraFieldOfView);
    }

    // Постобработка кадра (Build 1) — отдельным шагом, чтобы её же можно было переприменить
    // в рантайме при смене IntroGradeAlpha, не трогая руку/FOV.
    ApplyPostProcessSettings();
}

void APlayerCharacter::ApplyPostProcessSettings()
{
    if (!CameraComponent)
    {
        return;
    }

    FPostProcessSettings& PP = CameraComponent->PostProcessSettings;

    if (!bEnablePostProcess)
    {
        // Снимаем ВСЕ наши оверрайды — камера рисует без нашей постобработки.
        // Список обязан покрывать каждое поле, которое мы включаем ниже (иначе выключатель врёт).
        PP.bOverride_VignetteIntensity   = false;
        PP.bOverride_FilmGrainIntensity  = false;
        PP.bOverride_AutoExposureMethod  = false;
        PP.bOverride_AutoExposureBias    = false;
        PP.bOverride_AutoExposureApplyPhysicalCameraExposure = false;
        PP.bOverride_ColorSaturation     = false;
        PP.bOverride_ColorGainHighlights = false;
        PP.bOverride_ColorGainShadows    = false;

        PP.bOverride_BloomIntensity      = false;
        PP.bOverride_BloomThreshold      = false;
        PP.bOverride_ColorContrast       = false;
        PP.bOverride_ColorGamma          = false;
        PP.bOverride_ColorGainMidtones   = false;
        PP.bOverride_ColorCorrectionShadowsMax    = false;
        PP.bOverride_ColorCorrectionHighlightsMin = false;
        PP.bOverride_FilmSlope           = false;
        PP.bOverride_FilmToe             = false;
        PP.bOverride_FilmShoulder        = false;
        PP.bOverride_SceneFringeIntensity           = false;
        PP.bOverride_ChromaticAberrationStartOffset = false;
        PP.bOverride_Sharpen             = false;
        PP.bOverride_TemperatureType     = false;
        PP.bOverride_WhiteTemp           = false;
        PP.bOverride_WhiteTint           = false;
        PP.bOverride_MotionBlurAmount    = false;
        PP.bOverride_DepthOfFieldScale   = false;
        PP.bOverride_AmbientOcclusionIntensity     = false;
        PP.bOverride_LensFlareIntensity            = false;
        PP.bOverride_ScreenSpaceReflectionIntensity = false;
        return;
    }

    // Арка интро: экспозиция и насыщенность интерполируются от старта интро (Alpha=0) к норме
    // (Alpha=1). В обычной игре Alpha=1 → берутся базовые значения.
    const float Alpha = FMath::Clamp(IntroGradeAlpha, 0.0f, 1.0f);
    const float Exposure   = FMath::Lerp(PPIntroStartExposure,   PPExposureCompensation, Alpha);
    const float Saturation = FMath::Lerp(PPIntroStartSaturation, PPSaturation,           Alpha);

    // Виньетка.
    PP.bOverride_VignetteIntensity = true;
    PP.VignetteIntensity = PPVignetteIntensity;

    // Лёгкое зерно.
    PP.bOverride_FilmGrainIntensity = true;
    PP.FilmGrainIntensity = PPFilmGrainIntensity;

    // Фиксированная экспозиция (отключить авто-адаптацию глаза): ручной режим + компенсация EV.
    if (bPPFixedExposure)
    {
        PP.bOverride_AutoExposureMethod = true;
        PP.AutoExposureMethod = AEM_Manual;
        PP.bOverride_AutoExposureBias = true;
        PP.AutoExposureBias = Exposure;

        // Ручной режим БЕЗ «физической камеры». Иначе движок в Manual считает яркость кадра по
        // ISO/диафрагме/выдержке (дефолт f/4, 1/60, ISO100 = ~EV100 9.9 = яркий день), белая точка
        // взлетает в сотни раз и игровая сцена, освещённая не в реальных люксах, уходит в ЧЁРНОЕ.
        // Отключаем — тогда фиксированную экспозицию ведёт только AutoExposureBias (0 = норма, <0 = темнее).
        PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
        PP.AutoExposureApplyPhysicalCameraExposure = false;
    }
    else
    {
        // Авто-экспозиция включена — свои оверрайды не навязываем.
        PP.bOverride_AutoExposureMethod = false;
        PP.bOverride_AutoExposureBias = false;
        PP.bOverride_AutoExposureApplyPhysicalCameraExposure = false;
    }

    // Насыщенность (лёгкая десатурация); множитель одинаков по RGB, W=1 (мастер).
    PP.bOverride_ColorSaturation = true;
    PP.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.0f);

    // Тёплые света: усиление красного, ослабление синего в светах, помноженные на общую
    // яркость светов (gain по зоне; W=1 — множитель-мастер движка, см. ColorCorrect в
    // PostProcessCombineLUTs.usf: итог = xyz * w).
    PP.bOverride_ColorGainHighlights = true;
    PP.ColorGainHighlights = FVector4(
        PPHighlightGain * (1.0f + PPHighlightWarmth),
        PPHighlightGain,
        PPHighlightGain * (1.0f - PPHighlightWarmth),
        1.0f);

    // Холодные тени: усиление синего, ослабление красного, помноженные на общую яркость теней.
    PP.bOverride_ColorGainShadows = true;
    PP.ColorGainShadows = FVector4(
        PPShadowGain * (1.0f - PPShadowCoolness),
        PPShadowGain,
        PPShadowGain * (1.0f + PPShadowCoolness),
        1.0f);

    // Яркость полутонов — отдельной зоной (движок домножает зону на глобальные значения).
    PP.bOverride_ColorGainMidtones = true;
    PP.ColorGainMidtones = FVector4(PPMidtoneGain, PPMidtoneGain, PPMidtoneGain, 1.0f);

    // Свечение ярких мест. Порог отсекает тусклые пиксели, иначе светится весь кадр и он «плывёт».
    // Мобильный конвейер UE 5.5 включает проход свечения по условию BloomIntensity > 0
    // (PostProcessing.cpp:2435) и читает порог в BloomSetup (PostProcessMobile.cpp:343).
    PP.bOverride_BloomIntensity = true;
    PP.BloomIntensity = bPPEnableBloom ? FMath::Max(0.0f, PPBloomIntensity) : 0.0f;
    PP.bOverride_BloomThreshold = true;
    PP.BloomThreshold = PPBloomThreshold;

    // Контраст и гамма всей картинки (глобальная зона; на неё домножаются зоны выше).
    PP.bOverride_ColorContrast = true;
    PP.ColorContrast = FVector4(PPContrast, PPContrast, PPContrast, 1.0f);
    PP.bOverride_ColorGamma = true;
    PP.ColorGamma = FVector4(PPGamma, PPGamma, PPGamma, 1.0f);

    // Границы зон «тени / полутона / света» по яркости пикселя.
    PP.bOverride_ColorCorrectionShadowsMax = true;
    PP.ColorCorrectionShadowsMax = PPShadowsMax;
    PP.bOverride_ColorCorrectionHighlightsMin = true;
    PP.ColorCorrectionHighlightsMin = PPHighlightsMin;

    // Плёночная кривая: как кадр уходит в чёрное и в белое. Считается в таблицу цвета,
    // на телефоне лишних проходов не добавляет.
    PP.bOverride_FilmSlope = true;
    PP.FilmSlope = PPFilmSlope;
    PP.bOverride_FilmToe = true;
    PP.FilmToe = PPFilmToe;
    PP.bOverride_FilmShoulder = true;
    PP.FilmShoulder = PPFilmShoulder;

    // Хроматическая аберрация у краёв кадра (значение в процентах ширины кадра).
    PP.bOverride_SceneFringeIntensity = true;
    PP.SceneFringeIntensity = PPChromaticAberration;
    PP.bOverride_ChromaticAberrationStartOffset = true;
    PP.ChromaticAberrationStartOffset = PPChromaticAberrationStart;

    // Подрезка резкости в тонмаппере (общий проход с виньеткой, отдельного прохода нет).
    PP.bOverride_Sharpen = true;
    PP.Sharpen = PPSharpen;

    // Баланс белого. Выключен — прописываем нейтральные 6500K/0, чтобы наш профиль не зависел
    // от того, что оставил на камере кто-то другой.
    PP.bOverride_TemperatureType = true;
    PP.TemperatureType = TEMP_WhiteBalance;
    PP.bOverride_WhiteTemp = true;
    PP.WhiteTemp = bPPUseWhiteBalance ? PPWhiteTemp : 6500.0f;
    PP.bOverride_WhiteTint = true;
    PP.WhiteTint = bPPUseWhiteBalance ? PPWhiteTint : 0.0f;

    // Тяжёлые эффекты: гасим явно нулями. На мобильном конвейере UE 5.5 смаз движения и
    // экранные отражения в цепочке постобработки вообще отсутствуют (список проходов —
    // PostProcessing.cpp:2280-2305), размытие по глубине и затенение складок есть, но дороги;
    // прописанный ноль защищает от тома постобработки на уровне.
    if (bPPDisableExpensiveEffects)
    {
        PP.bOverride_MotionBlurAmount = true;
        PP.MotionBlurAmount = 0.0f;
        PP.bOverride_DepthOfFieldScale = true;
        PP.DepthOfFieldScale = 0.0f;
        PP.bOverride_AmbientOcclusionIntensity = true;
        PP.AmbientOcclusionIntensity = 0.0f;
        PP.bOverride_LensFlareIntensity = true;
        PP.LensFlareIntensity = 0.0f;
        PP.bOverride_ScreenSpaceReflectionIntensity = true;
        PP.ScreenSpaceReflectionIntensity = 0.0f;
    }
    else
    {
        // Выключатель снят — не навязываем свои значения, эффектами распоряжается уровень.
        PP.bOverride_MotionBlurAmount = false;
        PP.bOverride_DepthOfFieldScale = false;
        PP.bOverride_AmbientOcclusionIntensity = false;
        PP.bOverride_LensFlareIntensity = false;
        PP.bOverride_ScreenSpaceReflectionIntensity = false;
    }
}

void APlayerCharacter::SetIntroGradeAlpha(float Alpha)
{
    IntroGradeAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
    ApplyPostProcessSettings();
}

void APlayerCharacter::UpdateLimpState(float NewHealth, float InMaxHealth)
{
    // Хромает при HP на пороге и ниже (порог по доле от максимума). «<=» — чтобы старт ровно в
    // половину HP уже читался как «раненый» (издатель: приходит хромая), а любое лечение выше
    // порога — как выздоровление.
    const bool bShouldLimp = (InMaxHealth > 0.0f) && (NewHealth <= InMaxHealth * LimpHealthFraction);
    if (bShouldLimp == bLimping)
    {
        return; // состояние не изменилось — скорость/звук не трогаем
    }
    bLimping = bShouldLimp;

    // Скорость: хромота — сниженный множитель, иначе обычная (учитывается и в спринте).
    SetWalkSpeedMultiplier(bLimping ? LimpSpeedMultiplier : 1.0f);

    // Звук тяжёлого дыхания — пустая точка подключения: играет ТОЛЬКО если Ринат назначил ассет.
    if (bLimping)
    {
        if (LimpBreathingSound && !LimpBreathingComponent)
        {
            if (USoundWave* Wave = Cast<USoundWave>(LimpBreathingSound))
            {
                Wave->bLooping = true; // как эмбиент: зацикливание — свойство самого ассета (UE 5.5)
            }
            LimpBreathingComponent = UGameplayStatics::SpawnSound2D(
                this, LimpBreathingSound, 1.0f, 1.0f, 0.0f, nullptr, false, /*bAutoDestroy=*/false);
        }
    }
    else if (LimpBreathingComponent)
    {
        LimpBreathingComponent->Stop();
        LimpBreathingComponent = nullptr;
    }

    UE_LOG(LogTemp, Log, TEXT("Player limp %s (HP %.0f/%.0f)"),
        bLimping ? TEXT("ON") : TEXT("OFF"), NewHealth, InMaxHealth);
}

void APlayerCharacter::UpdateLimpIndicator()
{
    // Кодовый UMG-виджет без .uasset (паттерн ADR-048: новые экраны — UMG из C++). Владелец —
    // локальный контроллер; без него или без вьюпорта (headless-тесты/коммандлеты) тихо выходим.
    AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(GetController());
    UWorld* World = GetWorld();
    if (!PC || !PC->IsLocalController() || !World || !World->GetGameViewport())
    {
        return;
    }

    if (!LimpIndicatorWidget)
    {
        if (!bLimping)
        {
            return; // виджет не нужен, пока игрок ни разу не захромал
        }
        LimpIndicatorWidget = CreateWidget<ULimpIndicatorWidget>(PC, ULimpIndicatorWidget::StaticClass());
        if (!LimpIndicatorWidget)
        {
            UE_LOG(LogTemp, Warning, TEXT("LimpIndicator: widget creation failed"));
            return;
        }
        LimpIndicatorWidget->ApplyStyle(LimpIndicatorStyle);
        LimpIndicatorWidget->SetIndicatorText(bLimpHintExpandedActive ? LimpFirstHintText : LimpIndicatorText);
    }

    // Постоянные панели живут на ZOrder 5 (как в BeginPlay HUD); интро-экран (50) и модалки (30)
    // выше. Дальше видимость ведёт сам виджет (NativeTick: хромота + отсутствие модалок).
    if (!LimpIndicatorWidget->IsInViewport())
    {
        LimpIndicatorWidget->AddToViewport(/*ZOrder=*/5);
    }

    // Разовая развёрнутая подсказка — при ПЕРВОМ входе в хромоту со свободным управлением:
    // интро уже передало управление игроку (иначе всплыла бы на чёрном экране/на авто-подходе)
    // и не открыт модальный экран (не спорим с диалогом старосты). «Раз за сессию» обеспечивает
    // FLimpFirstHintState; повторные вызовы каждый тик бесплатны.
    const bool bControlFree = !PC->IsIntroMoveInputLocked() && !PC->IsAnyModalUIOpen();
    if (LimpFirstHint.ShouldTrigger(bLimping, bControlFree))
    {
        GetWorldTimerManager().SetTimer(LimpHintTimer, this, &APlayerCharacter::ShowLimpFirstHint,
            FMath::Max(LimpFirstHintDelay, 0.01f), false);
    }
}

void APlayerCharacter::ShowLimpFirstHint()
{
    if (!bLimping)
    {
        // Вылечился, пока подсказка ждала показа — не тратим её: объяснение пригодится,
        // когда игрок снова захромает и удивится потере скорости.
        LimpFirstHint.Rearm();
        return;
    }
    bLimpHintExpandedActive = true;
    if (LimpIndicatorWidget)
    {
        LimpIndicatorWidget->SetIndicatorText(LimpFirstHintText);
    }
    GetWorldTimerManager().SetTimer(LimpHintTimer, this, &APlayerCharacter::EndLimpFirstHint,
        FMath::Max(LimpFirstHintDuration, 1.0f), false);
    UE_LOG(LogQA, Display, TEXT("QA: limp first-hint shown (expanded for %.1f s)"), LimpFirstHintDuration);
}

void APlayerCharacter::EndLimpFirstHint()
{
    bLimpHintExpandedActive = false;
    if (LimpIndicatorWidget)
    {
        LimpIndicatorWidget->SetIndicatorText(LimpIndicatorText);
    }
}

void APlayerCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Экранный индикатор хромоты (Build 1): лениво создаём/держим в viewport и ловим момент
    // разовой развёрнутой подсказки. СТРОГО до раннего выхода по SpringArm ниже.
    UpdateLimpIndicator();

    // Build 1.2: накопитель суммарного игрового времени (гейт рекламы). Тоже до раннего выхода.
    UnflushedPlayTime += DeltaTime;

    // --- Процедурные эффекты камеры (#28): дыхание + look-ahead ---
    // Оба ЕДВА ЗАМЕТНЫ (запрос «чуть-чуть»). Подмешиваем в SpringArm->TargetOffset (world space),
    // не трогая базовую позицию/поворот руки — управление/прицел не затрагиваются.
    if (!SpringArmComponent)
    {
        return;
    }

    FVector DesiredOffset = FVector::ZeroVector;

    // --- Боевой режим камеры (D6, ADR-035): угрозы = враги, ведущие бой, в радиусе ---
    // Пока угрозы есть, камера НЕ смотрит вперёд (look-ahead выключен), а слегка смещается
    // к центру угроз — «всё, что может стрелять по игроку, в кадре». Без зума.
    bool bCombatMode = false;
    FVector CombatTargetOffset = FVector::ZeroVector;
    if (bEnableCombatCamera)
    {
        FVector ThreatSum = FVector::ZeroVector;
        int32 ThreatCount = 0;
        const float ThreatRadiusSq = CombatCameraThreatRadius * CombatCameraThreatRadius;

        for (const TWeakObjectPtr<AEnemyAIController>& Ptr : AEnemyAIController::GetActiveControllers())
        {
            const AEnemyAIController* Enemy = Ptr.Get();
            if (!Enemy || Enemy->GetWorld() != GetWorld() || !Enemy->IsEngagingPlayer())
            {
                continue;
            }
            const APawn* EnemyPawn = Enemy->GetPawn();
            if (!EnemyPawn)
            {
                continue;
            }
            const FVector EnemyLoc = EnemyPawn->GetActorLocation();
            if (FVector::DistSquared(EnemyLoc, GetActorLocation()) > ThreatRadiusSq)
            {
                continue;
            }
            ThreatSum += EnemyLoc;
            ++ThreatCount;
        }

        if (ThreatCount > 0)
        {
            bCombatMode = true;
            FVector ToThreats = (ThreatSum / ThreatCount) - GetActorLocation();
            ToThreats.Z = 0.0f;
            CombatTargetOffset = (ToThreats * CombatCameraOffsetFactor).GetClampedToMaxSize(CombatCameraMaxOffset);
        }
    }

    if (bCombatMode)
    {
        // Бой: тот же сглаженный офсет ведём к центру угроз (переход из look-ahead бесшовный).
        CameraLookAheadOffset = FMath::VInterpTo(CameraLookAheadOffset, CombatTargetOffset, DeltaTime, CombatCameraInterpSpeed);
        DesiredOffset += CameraLookAheadOffset;
        bCombatCameraRecovering = true; // после боя офсет возвращается мягкой скоростью выхода
    }
    else if (bEnableCameraLookAhead)
    {
        // Исследование: look-ahead по ходу движения (как раньше).
        FVector Velocity = GetVelocity();
        Velocity.Z = 0.0f;
        const float Speed = Velocity.Size();
        FVector TargetLookAhead = FVector::ZeroVector;
        if (Speed > LookAheadSpeedThreshold)
        {
            TargetLookAhead = Velocity.GetSafeNormal() * LookAheadAmount;
        }
        // Выход из боя — отдельной, заметно более мягкой скоростью (фидбек Рината 07-05);
        // когда офсет догнал цель исследования — возвращаемся на обычную скорость look-ahead.
        const float InterpSpeed = bCombatCameraRecovering ? CombatCameraExitInterpSpeed : LookAheadInterpSpeed;
        CameraLookAheadOffset = FMath::VInterpTo(CameraLookAheadOffset, TargetLookAhead, DeltaTime, InterpSpeed);
        if (bCombatCameraRecovering && CameraLookAheadOffset.Equals(TargetLookAhead, 25.0f))
        {
            bCombatCameraRecovering = false;
        }
        DesiredOffset += CameraLookAheadOffset;
    }
    else if (bCombatCameraRecovering)
    {
        // Look-ahead выключен: после боя офсет НЕ сбрасываем скачком — мягко ведём к нулю.
        CameraLookAheadOffset = FMath::VInterpTo(CameraLookAheadOffset, FVector::ZeroVector, DeltaTime, CombatCameraExitInterpSpeed);
        if (CameraLookAheadOffset.IsNearlyZero(1.0f))
        {
            CameraLookAheadOffset = FVector::ZeroVector;
            bCombatCameraRecovering = false;
        }
        DesiredOffset += CameraLookAheadOffset;
    }
    else
    {
        CameraLookAheadOffset = FVector::ZeroVector;
    }

    // «Дыхание»: крошечный вертикальный синусный боб — камера кажется живой.
    if (bEnableCameraBreathing)
    {
        CameraBreathingTime += DeltaTime * BreathingSpeed;
        DesiredOffset.Z += FMath::Sin(CameraBreathingTime) * BreathingAmplitude;
    }

    // --- Тряска камеры (D5): trauma-модель на шуме Перлина ---
    // Амплитуда = Max * травма² (квадрат делает слабые тычки мягкими, сильные — заметными).
    if (bEnableCameraShake && CameraShakeTrauma > 0.0f)
    {
        CameraShakeTrauma = FMath::Max(0.0f, CameraShakeTrauma - CameraShakeDecay * DeltaTime);
        CameraShakeTime += DeltaTime * CameraShakeFrequency;
        const float Amp = CameraShakeMaxAmplitude * CameraShakeTrauma * CameraShakeTrauma;
        // Два независимых канала шума (сдвиг аргумента = второй «сид»).
        DesiredOffset.X += Amp * FMath::PerlinNoise1D(CameraShakeTime);
        DesiredOffset.Y += Amp * FMath::PerlinNoise1D(CameraShakeTime + 49.7f);
    }

    SpringArmComponent->TargetOffset = DesiredOffset;

    // Шаги (Демо): по таймеру при ходьбе по земле (анимаций нет, AnimNotify не используем).
    UpdateFootsteps(DeltaTime);
}

void APlayerCharacter::AddCameraShake(float Trauma)
{
    if (!bEnableCameraShake || Trauma <= 0.0f)
    {
        return;
    }
    // Травма копится (несколько попаданий подряд трясут сильнее), но не выше 1.
    CameraShakeTrauma = FMath::Clamp(CameraShakeTrauma + Trauma, 0.0f, 1.0f);
}

void APlayerCharacter::UpdateFootsteps(float DeltaTime)
{
    if (FootstepSounds.Num() == 0)
    {
        return;
    }

    const UCharacterMovementComponent* Move = GetCharacterMovement();
    const bool bOnGround = Move && Move->IsMovingOnGround();
    const float Speed = GetVelocity().Size2D();

    if (!bOnGround || Speed < FootstepSpeedThreshold)
    {
        // Стоит/в воздухе — сбрасываем накопитель, чтобы первый шаг при старте был не сразу.
        FootstepAccumulator = 0.0f;
        return;
    }

    FootstepAccumulator += DeltaTime;
    if (FootstepAccumulator >= FootstepInterval)
    {
        FootstepAccumulator = 0.0f;
        const int32 Index = FMath::RandRange(0, FootstepSounds.Num() - 1);
        if (USoundBase* Step = FootstepSounds[Index])
        {
            UGameplayStatics::PlaySoundAtLocation(this, Step, GetActorLocation(), FootstepVolume);
        }
    }
}

float APlayerCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // QA god-mode (клавиша J): игрок неуязвим — весь входящий урон зануляется.
    if (FQADebug::bGodMode)
    {
        return 0.0f;
    }

    // Намеренно НЕ зовём Super (инлайн-Health базы): единственный источник истины по HP
    // игрока — UStatsComponent. Death/респаун повесим на Stats->OnDeath (Пункт 3).
    if (!Stats || Stats->IsDead() || DamageAmount <= 0.0f)
    {
        return 0.0f;
    }

    // GDD §7.2: броня снижает урон. ПРОЦЕНТНАЯ формула (решение Рината):
    // Final = Incoming * (1 - clamp(SumArmorFraction, 0, Cap)). Без min-1 неуязвимости.
    const float Reduced = ComputeArmoredDamage(DamageAmount);

    // #26: запоминаем «от кого погиб» — читаемое имя источника урона. Само-урон (QA-убийство
    // игрока, DamageCauser == this) и безымянный источник не перетирают последнего врага.
    if (DamageCauser && DamageCauser != this)
    {
        if (Cast<AWolfCharacter>(DamageCauser))
        {
            LastDamagerName = NSLOCTEXT("Death", "KillerWolf", "Волк");
        }
        else if (Cast<AEnemyCharacter>(DamageCauser))
        {
            LastDamagerName = NSLOCTEXT("Death", "KillerBandit", "Бандит");
        }
        else
        {
            // Здесь стояло DamageCauser->GetName(), и игрок читал на экране смерти
            // служебное имя объекта вида BP_BanditBase_C_2 (ADR-049). Показываем
            // нейтральное слово, а сам объект уводим в лог для разбора.
            LastDamagerName = NSLOCTEXT("Death", "KillerUnknown", "Неизвестно");
            UE_LOG(LogTemp, Verbose,
                TEXT("APlayerCharacter: урон от источника без понятного имени '%s' — на экране смерти показано «Неизвестно»"),
                *DamageCauser->GetName());
        }
    }

    const float Applied = Stats->ApplyDamage(Reduced);

    // Звук боли (Демо) — только от боевого урона (эта точка), не от голода/жажды.
    if (Applied > 0.0f)
    {
        Stats->PlayHurtSound();

        // D5: тряска камеры при получении урона (голод/жажда сюда не приходят — они бьют
        // напрямую в Stats, минуя TakeDamage).
        AddCameraShake(DamageShakeTrauma);
    }

    UE_LOG(LogTemp, Log, TEXT("Player took %.1f dmg (incoming %.1f, armor frac %.2f cap %.2f). Health: %.1f/%.1f"),
        Applied, DamageAmount, GetTotalArmorProtection(), ArmorReductionCap, Stats->GetHealth(), Stats->GetMaxHealth());

    return Applied;
}

void APlayerCharacter::EquipDefaultWeapon()
{
    if (!DefaultWeaponClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("EquipDefaultWeapon: DefaultWeaponClass not set, skipping"));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // Спавним оружие. Owner/Instigator — игрок; EquipWeapon довыставит Instigator и прикрепит к сокету.
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AMasterWeapon* SpawnedWeapon = World->SpawnActor<AMasterWeapon>(
        DefaultWeaponClass, GetActorLocation(), GetActorRotation(), SpawnParams);

    if (SpawnedWeapon)
    {
        RangedWeaponInstance = SpawnedWeapon;
        // EquipWeapon крепит оружие к WeaponSocketName на TorsoMesh и выставляет CurrentWeapon.
        EquipWeapon(SpawnedWeapon);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("EquipDefaultWeapon: failed to spawn DefaultWeaponClass"));
    }
}

void APlayerCharacter::SpawnMeleeWeapon()
{
    if (!DefaultMeleeWeaponClass)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AMasterWeapon* Knife = World->SpawnActor<AMasterWeapon>(
        DefaultMeleeWeaponClass, GetActorLocation(), GetActorRotation(), SpawnParams);

    if (Knife)
    {
        MeleeWeaponInstance = Knife;
        // Нож нужен для урона (Instigator), но не активен: гасим видимость/коллизию.
        Knife->SetInstigator(this);
        Knife->SetActorHiddenInGame(true);
        Knife->SetActorEnableCollision(false);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SpawnMeleeWeapon: failed to spawn DefaultMeleeWeaponClass"));
    }
}

void APlayerCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    // Одежда Т0 (ADR-042) ставится ДО BeginPlay базы: CacheBaseSlotMeshes там снимет
    // именно её как «базовые меши слотов» -> снятие брони возвращает Т0, не белое тело.
    ApplyStartClothing();
}

void APlayerCharacter::ApplyStartClothing()
{
    auto ApplyToSlot = [&](const TSoftObjectPtr<USkeletalMesh>& SoftMesh, EArmorSlot Slot)
    {
        if (SoftMesh.IsNull())
        {
            return; // ссылка не задана — слот остаётся с мешем из BP (осознанный фолбэк)
        }

        USkeletalMesh* ClothMesh = SoftMesh.LoadSynchronous();
        if (!ClothMesh)
        {
            UE_LOG(LogTemp, Warning, TEXT("ApplyStartClothing: mesh '%s' failed to load, slot %d keeps BP mesh"),
                *SoftMesh.ToSoftObjectPath().ToString(), (int32)Slot);
            return;
        }

        if (USkeletalMeshComponent* SlotComp = GetMeshComponentForSlot(Slot))
        {
            SlotComp->SetSkeletalMeshAsset(ClothMesh);
            // После подмены меша follower-слоты заново привязываются к Leader Pose (Head — no-op).
            RelinkSlotToLeaderPose(Slot);
        }
    };

    ApplyToSlot(StartClothHeadMesh,  EArmorSlot::Head);
    ApplyToSlot(StartClothTorsoMesh, EArmorSlot::Torso);
    ApplyToSlot(StartClothLegsMesh,  EArmorSlot::Legs);
}

void APlayerCharacter::EquipTestArmor()
{
    // Консольная команда / QA-клавиша F3: (пере)надеть тест-комплект Test*ArmorClass
    // (дефолт — полный сет Т3). Тест-броня НЕ кладётся в рюкзак и не помечается equipped —
    // как и прежний автоэкип; при надевании покупной брони поверх вернётся в рюкзак
    // штатным путём Inv_UseBackpackItem.
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    auto SpawnAndEquip = [&](TSubclassOf<AArmor> ArmorClass)
    {
        if (!ArmorClass)
        {
            return;
        }
        AArmor* Armor = World->SpawnActor<AArmor>(ArmorClass, GetActorLocation(), GetActorRotation(), SpawnParams);
        if (Armor)
        {
            // Предмет брони — данные, не объект сцены: прячем визуал/коллизию.
            Armor->SetActorHiddenInGame(true);
            Armor->SetActorEnableCollision(false);
            EquipArmor(Armor);
        }
    };

    SpawnAndEquip(TestHeadArmorClass);
    SpawnAndEquip(TestTorsoArmorClass);
    SpawnAndEquip(TestPantsArmorClass);

    UE_LOG(LogTemp, Log, TEXT("EquipTestArmor: equipped test set (default T3). Total armor %.2f"),
        GetTotalArmorProtection());
}

void APlayerCharacter::UnequipTestArmor()
{
    // Консольная команда: снять броню всех слотов (возврат базовых мешей тела).
    UnequipArmor(EArmorSlot::Head);
    UnequipArmor(EArmorSlot::Torso);
    UnequipArmor(EArmorSlot::Legs);
    UE_LOG(LogTemp, Log, TEXT("UnequipTestArmor: all slots cleared. Total armor %.2f"),
        GetTotalArmorProtection());
}

// ---------------------------------------------------------------------------
// Действия UI-инвентаря (Фаза 4) — вызываются из AContrarySurvivorHUD по клику
// ---------------------------------------------------------------------------

void APlayerCharacter::Inv_UseBackpackItem(AMasterInventoryItem* Item)
{
    if (!Item || !Inventory)
    {
        return;
    }

    switch (Item->GetItemCategory())
    {
        case EItemCategory::Armor:
        {
            // Броня -> надеть в её слот (подмена меша) и пометить экипированной
            // (чтобы не теряться при смерти и не дублироваться в списке рюкзака).
            if (AArmor* Armor = Cast<AArmor>(Item))
            {
                // Слот уже занят? Любую находящуюся там броню сперва снимаем обратно в рюкзак
                // ТЕМ ЖЕ путём, что и ручное снятие (Inv_UnequipSlot), и только потом надеваем
                // новую. Иначе EquipArmor перезапишет ссылку слота, и старый предмет станет
                // «сиротой»: исчезнет и из paper-doll, и из списка рюкзака (баг: «старая броня
                // пропадает, а не перемещается в инвентарь»).
                // Возвращаем ЛЮБУЮ реальную броню, в т.ч. тест-комплект с QA-клавиши F3
                // (EquipTestArmor): он не помечен экипированным и не лежит в рюкзаке, но
                // Inv_UnequipSlot корректно добавит его (внутри guard от дублирования).
                // Стартовая одежда Т0 (ADR-042) отдельным предметом брони НЕ является — это
                // базовый меш тела, восстанавливаемый UnequipArmor, — поэтому в этот путь как
                // PrevArmor не попадает и спец-исключения не требует.
                const EArmorSlot TargetSlot = Armor->GetArmorSlot();
                if (AArmor* PrevArmor = GetEquippedArmor(TargetSlot))
                {
                    if (PrevArmor != Armor)
                    {
                        Inv_UnequipSlot(TargetSlot);
                    }
                }

                EquipArmor(Armor);
                Inventory->SetItemEquipped(Armor, true);
                UE_LOG(LogTemp, Log, TEXT("Inv: equipped %s"), *Armor->GetName());
            }
            break;
        }
        case EItemCategory::Consumable:
        {
            // Расходник -> применить эффект (еда +Hunger / вода +Thirst) и израсходовать.
            if (AConsumableItem* Cons = Cast<AConsumableItem>(Item))
            {
                if (Cons->ApplyConsumeEffect(Stats))
                {
                    Inventory->RemoveItem(Item);
                    Item->Destroy();
                    UE_LOG(LogTemp, Log, TEXT("Inv: consumed %s"), *Cons->GetName());
                }
            }
            break;
        }
        default:
            UE_LOG(LogTemp, Log, TEXT("Inv: item %s has no use action (category %d)"),
                *Item->GetName(), (int32)Item->GetItemCategory());
            break;
    }
}

void APlayerCharacter::Inv_DropItem(AMasterInventoryItem* Item)
{
    if (!Item || !Inventory)
    {
        return;
    }

    // Если выбрасываем экипированную броню — сперва снять (вернуть меш слота к базовому).
    if (AArmor* Armor = Cast<AArmor>(Item))
    {
        if (Inventory->IsItemEquipped(Armor))
        {
            UnequipArmor(Armor->GetArmorSlot());
        }
    }

    Inventory->RemoveItem(Item);

    // BUG3-фикс (решение Рината/game-lead): выброс = заспавнить предмет МИРОВЫМ пикапом
    // у ног игрока (НЕ Destroy). Подобрать обратно можно клавишей E. Предмет остаётся
    // скрытым/без коллизии и переносится пикапом как данные (его визуал — меш пикапа).
    UWorld* World = GetWorld();
    if (!World)
    {
        Item->Destroy(); // нет мира — фолбэк, не оставляем висящий предмет
        return;
    }

    Item->SetActorHiddenInGame(true);
    Item->SetActorEnableCollision(false);

    // Чуть впереди игрока и ниже (примерно к ногам), чтобы мешок был виден.
    const FVector DropLoc = GetActorLocation()
        + GetActorForwardVector() * DropForwardOffset
        + FVector(0.0f, 0.0f, -DropDownOffset);

    FActorSpawnParameters Sp;
    Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // Build 1.2 п.6: класс пикапа настраиваемый (BP_Pickup с настройками Рината); пустое
    // поле в BP-оверрайде — фолбэк на базовый класс, дроп не ломается.
    UClass* DropClass = PickupSpawnClass ? *PickupSpawnClass : APickup::StaticClass();
    APickup* Dropped = World->SpawnActor<APickup>(DropClass, DropLoc, FRotator::ZeroRotator, Sp);
    if (Dropped)
    {
        Dropped->InitLoot(0.0f, Item);
        UE_LOG(LogTemp, Log, TEXT("Inv: dropped %s as world pickup at %s"), *Item->GetName(), *DropLoc.ToString());
        UE_LOG(LogQA, Display, TEXT("QA: DROP '%s' at feet (world pickup spawned) at %s"), *Item->GetName(), *DropLoc.ToString());
    }
    else
    {
        // Пикап не заспавнился — не оставляем висящий предмет.
        Item->Destroy();
        UE_LOG(LogTemp, Warning, TEXT("Inv: drop failed to spawn pickup for %s (destroyed item)"), *Item->GetName());
    }
}

void APlayerCharacter::Inv_UnequipSlot(EArmorSlot Slot)
{
    AArmor* Armor = GetEquippedArmor(Slot);
    UnequipArmor(Slot);

    if (Armor && Inventory)
    {
        Inventory->SetItemEquipped(Armor, false);
        // Вернуть в рюкзак как неэкипированный (без дублирования).
        if (!Inventory->GetInventoryItems().Contains(Armor))
        {
            Inventory->AddItem(Armor);
        }
        UE_LOG(LogTemp, Log, TEXT("Inv: unequipped slot %d (%s -> backpack)"),
            (int32)Slot, *Armor->GetName());
    }
}

int32 APlayerCharacter::GiveConsumableToBackpack(EConsumableType Type, int32 Count)
{
    UWorld* World = GetWorld();
    if (!World || !Inventory || Count <= 0)
    {
        return 0;
    }

    FActorSpawnParameters Sp;
    Sp.Owner = this;
    Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    int32 Given = 0;
    for (int32 i = 0; i < Count; ++i)
    {
        AConsumableItem* Item = World->SpawnActor<AConsumableItem>(
            AConsumableItem::StaticClass(), GetActorLocation(), GetActorRotation(), Sp);
        if (!Item)
        {
            continue;
        }

        Item->ConsumableType = Type;
        // Служебный ключ (по нему сходится логика квестов) и переводимое название —
        // оба из одного источника, как при покупке и отладочной выдаче (ADR-050, порция 0).
        Item->ItemName = AConsumableItem::GetDefaultDisplayName(Type);
        Item->ItemDisplayText = AConsumableItem::GetDefaultDisplayText(Type);

        // Предмет рюкзака — не объект на сцене: прячем визуал/коллизию, держим как данные.
        Item->SetActorHiddenInGame(true);
        Item->SetActorEnableCollision(false);

        if (Inventory->AddItem(Item))
        {
            ++Given;
        }
        else
        {
            Item->Destroy();
        }
    }

    return Given;
}

// ---------------------------------------------------------------------------
// Магазин торговца (Фаза 4, экономика) — вызываются из HUD по клику
// ---------------------------------------------------------------------------

bool APlayerCharacter::Shop_BuyEntry(const FShopEntry& Entry)
{
    return Shop_BuyEntryQty(Entry, 1);
}

bool APlayerCharacter::Shop_BuyEntryQty(const FShopEntry& Entry, int32 Qty)
{
    if (!Stats)
    {
        return false;
    }

    Qty = FMath::Max(1, Qty);
    const float TotalPrice = Entry.Price * static_cast<float>(Qty);

    // Проверяем платёжеспособность ДО выдачи товара (clamp >=0: нельзя купить без денег).
    if (Stats->GetMoney() < TotalPrice)
    {
        UE_LOG(LogTemp, Log, TEXT("Shop: not enough money for '%s' x%d (%.0f < %.0f)"),
            *Entry.DisplayName, Qty, Stats->GetMoney(), TotalPrice);
        return false;
    }

    if (Entry.Kind == EShopEntryKind::Ammo)
    {
        // Патроны -> в рюкзак СТАКОМ (Фаза 5). Qty единиц * AmmoAmount патронов в каждой.
        const int32 RoundsBought = FMath::Max(0, Entry.AmmoAmount) * Qty;
        AddAmmoToInventory(RoundsBought);
        UE_LOG(LogQA, Display, TEXT("QA: BUY %d ammo (x%d) -> backpack now %d rounds"),
            RoundsBought, Qty, GetReserveAmmoInInventory());
    }
    else
    {
        // Предмет -> в рюкзак (скрытый, как тестовые/лут-предметы). Qty копий.
        if (!Entry.ItemClass)
        {
            return false;
        }
        UWorld* World = GetWorld();
        if (!World || !Inventory)
        {
            return false;
        }

        FActorSpawnParameters Sp;
        Sp.Owner = this;
        Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        for (int32 i = 0; i < Qty; ++i)
        {
            AMasterInventoryItem* Bought = World->SpawnActor<AMasterInventoryItem>(
                Entry.ItemClass, GetActorLocation(), GetActorRotation(), Sp);
            if (!Bought)
            {
                continue;
            }

            // Если это расходник и задан тип — выставляем (еда/вода/аптечка).
            if (Entry.bApplyConsumableType)
            {
                if (AConsumableItem* Cons = Cast<AConsumableItem>(Bought))
                {
                    Cons->ConsumableType = Entry.ConsumableType;
                }
            }
            if (Bought->ItemName.IsEmpty())
            {
                Bought->ItemName = Entry.DisplayName;
            }
            // Переводимое название с позиции каталога: один класс AConsumableItem стоит в
            // каталоге трижды (вода/консервы/бинт), поэтому имя класса их не различает.
            if (Bought->ItemDisplayText.IsEmpty() && !Entry.DisplayText.IsEmpty())
            {
                Bought->ItemDisplayText = Entry.DisplayText;
            }

            Bought->SetActorHiddenInGame(true);
            Bought->SetActorEnableCollision(false);
            Inventory->AddItem(Bought);
        }
    }

    // Списываем итоговую цену (SpendMoney clamp >=0 + бродкаст HUD).
    Stats->SpendMoney(TotalPrice);
    UE_LOG(LogTemp, Log, TEXT("Shop: bought '%s' x%d for %.0f. Money left %.0f"),
        *Entry.DisplayName, Qty, TotalPrice, Stats->GetMoney());
    UE_LOG(LogQA, Display, TEXT("QA: BUY '%s' x%d for %.0f, balance %.0f"),
        *Entry.DisplayName, Qty, TotalPrice, Stats->GetMoney());

    // F3 (ADR-038): событие аналитики «покупка» (предмет + итоговая цена). Без ключей — no-op.
    // В событие идёт латинский AnalyticsId (DisplayName теперь русский и после санитайза
    // слепился бы в подчёркивания); пустой id — фолбэк на DisplayName (старое поведение).
    if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
    {
        Analytics->RecordPurchase(
            Entry.AnalyticsId.IsEmpty() ? Entry.DisplayName : Entry.AnalyticsId, TotalPrice);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Патроны как стак-предмет рюкзака (Фаза 5, STALKER 2-стиль)
// ---------------------------------------------------------------------------

int32 APlayerCharacter::GetReserveAmmoInInventory() const
{
    if (!Inventory)
    {
        return 0;
    }
    int32 Total = 0;
    for (AMasterInventoryItem* It : Inventory->GetInventoryItems())
    {
        if (AAmmoItem* Ammo = Cast<AAmmoItem>(It))
        {
            Total += FMath::Max(0, Ammo->StackCount);
        }
    }
    return Total;
}

void APlayerCharacter::AddAmmoToInventory(int32 Amount)
{
    if (Amount <= 0 || !Inventory)
    {
        return;
    }

    // 1) Дозаполняем существующие пачки (стак), пока есть место.
    for (AMasterInventoryItem* It : Inventory->GetInventoryItems())
    {
        if (Amount <= 0)
        {
            break;
        }
        if (AAmmoItem* Ammo = Cast<AAmmoItem>(It))
        {
            const int32 Space = Ammo->GetStackSpace();
            if (Space > 0)
            {
                const int32 Add = FMath::Min(Space, Amount);
                Ammo->StackCount += Add;
                Amount -= Add;
            }
        }
    }

    // 2) Остаток — в новые пачки (если за раз купили больше MaxStackCount).
    UWorld* World = GetWorld();
    while (Amount > 0 && World)
    {
        FActorSpawnParameters Sp;
        Sp.Owner = this;
        Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        AAmmoItem* NewPack = World->SpawnActor<AAmmoItem>(
            AAmmoItem::StaticClass(), GetActorLocation(), GetActorRotation(), Sp);
        if (!NewPack)
        {
            break;
        }
        const int32 Add = FMath::Min(NewPack->MaxStackCount, Amount);
        NewPack->StackCount = Add;
        Amount -= Add;
        NewPack->SetActorHiddenInGame(true);
        NewPack->SetActorEnableCollision(false);
        Inventory->AddItem(NewPack);
    }
}

int32 APlayerCharacter::TakeAmmoFromInventory(int32 Amount)
{
    if (Amount <= 0 || !Inventory)
    {
        return 0;
    }

    int32 Taken = 0;
    // Копия списка: при опустошении пачки удаляем её из инвентаря (модификация коллекции).
    TArray<AMasterInventoryItem*> Items = Inventory->GetInventoryItems();
    for (AMasterInventoryItem* It : Items)
    {
        if (Taken >= Amount)
        {
            break;
        }
        AAmmoItem* Ammo = Cast<AAmmoItem>(It);
        if (!Ammo || Ammo->StackCount <= 0)
        {
            continue;
        }
        const int32 Pull = FMath::Min(Ammo->StackCount, Amount - Taken);
        Ammo->StackCount -= Pull;
        Taken += Pull;
        if (Ammo->StackCount <= 0)
        {
            Inventory->RemoveItem(Ammo);
            Ammo->Destroy();
        }
    }
    return Taken;
}

void APlayerCharacter::ReloadCurrentWeapon()
{
    // Перед штатной перезарядкой пополняем резерв оружия из пачки патронов рюкзака.
    if (ARangedWeapon* Ranged = Cast<ARangedWeapon>(GetCurrentWeapon()))
    {
        const int32 Space = Ranged->GetReserveSpace();
        if (Space > 0)
        {
            const int32 Pulled = TakeAmmoFromInventory(Space);
            if (Pulled > 0)
            {
                Ranged->AddReserveAmmo(Pulled);
                UE_LOG(LogQA, Display, TEXT("QA: RELOAD pulled %d ammo from backpack -> reserve %d (backpack left %d)"),
                    Pulled, Ranged->GetCurrentAmmoReserve(), GetReserveAmmoInInventory());
            }
        }
    }

    // Штатный перенос резерв -> обойма (база).
    Super::ReloadCurrentWeapon();
}

void APlayerCharacter::Shop_SellItemQty(AMasterInventoryItem* Item, float UnitSellPrice, int32 Qty)
{
    if (!Item || !Inventory || !Stats)
    {
        return;
    }

    // Build 1.2.1 (блок Г, патч cpp): ЛЮБОЙ стак-предмет (патроны, шкуры, тушёнка,
    // аптечки — поля стака в базе AMasterInventoryItem с a4252ba) продаётся по Qty штук
    // за UnitSellPrice/штуку. Поведение патронов не изменилось — тот же путь.
    if (Item->IsStackable())
    {
        const int32 SellCount = FMath::Clamp(Qty, 1, Item->StackCount);
        if (SellCount <= 0)
        {
            return;
        }
        const float Gain = UnitSellPrice * static_cast<float>(SellCount);
        Item->StackCount -= SellCount;
        Stats->AddMoney(Gain);
        if (Item->StackCount <= 0)
        {
            Inventory->RemoveItem(Item);
            Item->Destroy();
        }
        UE_LOG(LogQA, Display, TEXT("QA: SELL %d x '%s' for %.0f, balance %.0f"),
            SellCount, *Item->ItemName, Gain, Stats->GetMoney());
        // Build 1.2 (ТЗ №2 п.6): shop_sell_completed для ВСЕХ продаж (value = сумма).
        if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
        {
            Analytics->RecordShopSellCompleted(Gain);
        }
        return;
    }

    // Нестакающийся предмет — продаём целиком (UnitSellPrice = полная цена выкупа).
    Shop_SellItem(Item, UnitSellPrice);
}

void APlayerCharacter::Shop_SellItem(AMasterInventoryItem* Item, float SellPrice)
{
    if (!Item || !Inventory || !Stats)
    {
        return;
    }

    // Если продаём экипированную броню — сперва снять (вернуть меш слота к базовому).
    if (AArmor* Armor = Cast<AArmor>(Item))
    {
        if (Inventory->IsItemEquipped(Armor))
        {
            UnequipArmor(Armor->GetArmorSlot());
            Inventory->SetItemEquipped(Armor, false);
        }
    }

    const FString SoldName = Item->GetName();
    Inventory->RemoveItem(Item);
    Stats->AddMoney(SellPrice);
    UE_LOG(LogTemp, Log, TEXT("Shop: sold %s for %.0f. Money now %.0f"),
        *SoldName, SellPrice, Stats->GetMoney());
    UE_LOG(LogQA, Display, TEXT("QA: SELL '%s' for %.0f, balance %.0f"),
        *SoldName, SellPrice, Stats->GetMoney());

    // Build 1.2 (ТЗ №2 п.6): shop_sell_completed для ВСЕХ продаж (value = сумма).
    if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
    {
        Analytics->RecordShopSellCompleted(SellPrice);
    }

    Item->Destroy();
}

void APlayerCharacter::GiveTestItems()
{
    UWorld* World = GetWorld();
    if (!World || !Inventory)
    {
        return;
    }

    FActorSpawnParameters Sp;
    Sp.Owner = this;
    Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // Предметы рюкзака — не объекты на сцене: прячем визуал/коллизию, держим как данные.
    auto AddHidden = [&](AMasterInventoryItem* It)
    {
        if (!It)
        {
            return;
        }
        It->SetActorHiddenInGame(true);
        It->SetActorEnableCollision(false);
        Inventory->AddItem(It);
    };

    // Расходники: еда (+Hunger) и вода (+Thirst).
    if (AConsumableItem* Food = World->SpawnActor<AConsumableItem>(
            AConsumableItem::StaticClass(), GetActorLocation(), GetActorRotation(), Sp))
    {
        Food->ConsumableType = EConsumableType::Food;
        Food->ItemName = AConsumableItem::GetDefaultDisplayName(EConsumableType::Food);
        Food->ItemDisplayText = AConsumableItem::GetDefaultDisplayText(EConsumableType::Food);
        AddHidden(Food);
    }
    if (AConsumableItem* Water = World->SpawnActor<AConsumableItem>(
            AConsumableItem::StaticClass(), GetActorLocation(), GetActorRotation(), Sp))
    {
        Water->ConsumableType = EConsumableType::Water;
        Water->ItemName = AConsumableItem::GetDefaultDisplayName(EConsumableType::Water);
        Water->ItemDisplayText = AConsumableItem::GetDefaultDisplayText(EConsumableType::Water);
        AddHidden(Water);
    }

    // Запасная броня (Head_02 / Torso_02) — лежит в рюкзаке неэкипированной,
    // чтобы было что надеть через paper-doll.
    if (AHeadArmor* Head = World->SpawnActor<AHeadArmor>(
            AHeadArmor::StaticClass(), GetActorLocation(), GetActorRotation(), Sp))
    {
        Head->ItemName = TEXT("Spare Head Armor (Head_02)");
        Head->ItemDisplayText = NSLOCTEXT("Items", "SpareHeadArmor", "Броня — голова (запасная)");
        AddHidden(Head);
    }
    if (ATorsoArmor* Torso = World->SpawnActor<ATorsoArmor>(
            ATorsoArmor::StaticClass(), GetActorLocation(), GetActorRotation(), Sp))
    {
        Torso->ItemName = TEXT("Spare Torso Armor (Torso_02)");
        Torso->ItemDisplayText = NSLOCTEXT("Items", "SpareTorsoArmor", "Броня — торс (запасная)");
        AddHidden(Torso);
    }

    UE_LOG(LogTemp, Log, TEXT("GiveTestItems: added 2 consumables + 2 spare armor pieces. Backpack size now %d"),
        Inventory->GetInventoryItems().Num());
}

void APlayerCharacter::SwitchWeapon()
{
    // Тоггл между дальним (пистолет) и ближним (нож) оружием.
    AMasterWeapon* Active = GetCurrentWeapon();
    AMasterWeapon* Target = (Active == MeleeWeaponInstance) ? RangedWeaponInstance : MeleeWeaponInstance;

    if (!Target || Target == Active)
    {
        UE_LOG(LogTemp, Warning, TEXT("SwitchWeapon: no alternate weapon to switch to"));
        return;
    }

    // Прячем снимаемое, показываем экипируемое (EquipWeapon снимет текущее и прикрепит новое).
    if (Active)
    {
        Active->SetActorHiddenInGame(true);
        Active->SetActorEnableCollision(false);
    }

    EquipWeapon(Target);
    Target->SetActorHiddenInGame(false);

    UE_LOG(LogTemp, Log, TEXT("SwitchWeapon: now wielding %s"), *Target->GetName());
}


// ---------------------------------------------------------------------------
// Сейв / смерть / респаун (GDD §7.8)
// ---------------------------------------------------------------------------

bool APlayerCharacter::SaveGame()
{
    if (!Stats)
    {
        return false;
    }

    UContrarySaveGame* Save = Cast<UContrarySaveGame>(
        UGameplayStatics::CreateSaveGameObject(UContrarySaveGame::StaticClass()));
    if (!Save)
    {
        return false;
    }

    Save->bHasData = true;
    Save->Health = Stats->GetHealth();
    Save->MaxHealth = Stats->GetMaxHealth();
    Save->Hunger = Stats->GetHunger();
    Save->Thirst = Stats->GetThirst();
    Save->Money = Stats->GetMoney();
    Save->PlayerLocation = GetActorLocation();
    Save->PlayerRotation = GetActorRotation();

    // ЗАДЕЛ: инвентарь сериализуем как пути классов предметов рюкзака.
    Save->InventoryItemClassPaths.Reset();
    if (Inventory)
    {
        for (const AMasterInventoryItem* Item : Inventory->GetInventoryItems())
        {
            if (Item)
            {
                Save->InventoryItemClassPaths.Add(Item->GetClass()->GetPathName());
            }
        }
    }

    // ЗАДЕЛ (Фаза 4): сериализуем экипированную броню по слотам как пути классов.
    auto ArmorPath = [](AArmor* Armor) -> FString
    {
        return Armor ? Armor->GetClass()->GetPathName() : FString();
    };
    Save->EquippedHeadArmorClassPath  = ArmorPath(GetEquippedArmor(EArmorSlot::Head));
    Save->EquippedTorsoArmorClassPath = ArmorPath(GetEquippedArmor(EArmorSlot::Torso));
    Save->EquippedLegsArmorClassPath  = ArmorPath(GetEquippedArmor(EArmorSlot::Legs));

    // Этап F: поля удержания (серия ежедневной награды + флаги подсказок) живут в ЭТОМ ЖЕ
    // слоте, но заполняются компонентами удержания, а не здесь. Объект Save создан свежим —
    // переносим их из прежнего сейва, иначе каждый автосейв костра обнулял бы серию и подсказки.
    if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex))
    {
        if (const UContrarySaveGame* Prev = Cast<UContrarySaveGame>(
            UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex)))
        {
            UContrarySaveGame::CopyRetentionData(Prev, Save);
        }
    }

    const bool bOk = UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, SaveUserIndex);
    UE_LOG(LogTemp, Log, TEXT("APlayerCharacter::SaveGame -> slot '%s' : %s"),
        *SaveSlotName, bOk ? TEXT("OK") : TEXT("FAIL"));
    return bOk;
}

bool APlayerCharacter::HasSaveGame() const
{
    return UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex);
}

UContrarySaveGame* APlayerCharacter::LoadOrCreateSaveObject() const
{
    // Этап F: компоненты удержания правят СВОИ поля слота и пишут обратно WriteSaveObject.
    // Существующий сейв возвращаем как есть (позиция/статы не теряются при перезаписи);
    // нет сейва — свежий объект с bHasData=false (LoadGame такой игнорирует).
    if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex))
    {
        if (UContrarySaveGame* Loaded = Cast<UContrarySaveGame>(
            UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex)))
        {
            return Loaded;
        }
    }
    return Cast<UContrarySaveGame>(
        UGameplayStatics::CreateSaveGameObject(UContrarySaveGame::StaticClass()));
}

bool APlayerCharacter::WriteSaveObject(UContrarySaveGame* Save) const
{
    return Save && UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, SaveUserIndex);
}

bool APlayerCharacter::LoadGame()
{
    if (!HasSaveGame())
    {
        return false;
    }

    UContrarySaveGame* Save = Cast<UContrarySaveGame>(
        UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex));
    if (!Save || !Save->bHasData)
    {
        return false;
    }

    ApplySaveData(Save);
    return true;
}

void APlayerCharacter::ApplySaveData(const UContrarySaveGame* Save)
{
    if (!Save)
    {
        return;
    }

    if (Stats)
    {
        Stats->RestoreState(Save->Health, Save->Hunger, Save->Thirst, Save->Money);
    }

    // Телепорт в точку респауна (последний костёр/сейв). Гасим скорость.
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->StopMovementImmediately();
    }

    // BUG-кламп: сейв прошлой сломанной сессии мог сохранить Z под картой (-4055).
    // Если так — заменяем высоту трассировкой до пола, чтобы не возрождать петлю падения.
    FVector TargetLoc = Save->PlayerLocation;
    if (TargetLoc.Z < SpawnPlacement::BadZThreshold)
    {
        const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
        TargetLoc.Z = SpawnPlacement::ResolveSpawnZ(GetWorld(), TargetLoc.X, TargetLoc.Y, HalfHeight + 10.0f, TEXT("Player-load"), this);
        UE_LOG(LogTemp, Warning, TEXT("QA: loaded save Z was under map -> clamped to floor Z=%.1f"), TargetLoc.Z);
    }

    SetActorLocationAndRotation(TargetLoc, Save->PlayerRotation,
        /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
}

TArray<AMasterInventoryItem*> APlayerCharacter::GetDeathLossCandidates() const
{
    // Порядок инвентаря — общий для превью экрана смерти и применения плана (DropDeathLoss):
    // теряются ПЕРВЫЕ Plan.LostItems, из них первые Plan.DroppedItems падают мешком.
    return Inventory
        ? Inventory->GetUnequippedItemsOfCategory(EItemCategory::Consumable)
        : TArray<AMasterInventoryItem*>();
}

DeathLoss::FPlan APlayerCharacter::ComputeDeathLossPlan(bool bBackpackRescued) const
{
    const float ItemFrac = bBackpackRescued ? DeathRescuedLossFraction : DeathConsumableLossFraction;
    const float MoneyFrac = bBackpackRescued ? DeathRescuedLossFraction : DeathMoneyLossFraction;

    // Build 1.2.1 (блок Г, стаки): база плана — сумма ШТУК по стакам, а не число акторов,
    // иначе «70% расходников» превратилось бы в «70% стаков» и стак терялся бы целиком.
    int32 TotalPieces = 0;
    for (const AMasterInventoryItem* Item : GetDeathLossCandidates())
    {
        if (IsValid(Item))
        {
            TotalPieces += FMath::Max(1, Item->GetStackCount());
        }
    }
    return DeathLoss::Compute(TotalPieces, MoneyAtDeath,
        ItemFrac, MoneyFrac, DeathDropShareFraction);
}

void APlayerCharacter::DropDeathLoss(const FVector& DeathLoc, const DeathLoss::FPlan& Plan)
{
    // Build 1.2 (переопределение Рината поверх ADR-027): теряется ДОЛЯ расходников, из
    // потерянных половина падает возвращаемым «мешком» (APickup) на месте гибели вместе с
    // долей потерянных денег, остальное исчезает. Квест-предметы, ресурсы, надетая броня
    // и оружие в руках НЕ трогаются.
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    TArray<AMasterInventoryItem*> Candidates = GetDeathLossCandidates();
    const int32 LostCount = FMath::Min(Plan.LostItems, Candidates.Num());

    TArray<AMasterInventoryItem*> BagItems;
    int32 DestroyedCount = 0;
    for (int32 Index = 0; Index < LostCount; ++Index)
    {
        AMasterInventoryItem* Item = Candidates[Index];
        if (!IsValid(Item))
        {
            continue;
        }
        if (Inventory)
        {
            Inventory->RemoveItem(Item);
        }
        if (BagItems.Num() < Plan.DroppedItems)
        {
            // В мешок (данные, как при обычном Inv_DropItem: скрыт, без коллизии).
            Item->SetActorHiddenInGame(true);
            Item->SetActorEnableCollision(false);
            BagItems.Add(Item);
        }
        else
        {
            Item->Destroy(); // потерян безвозвратно
            ++DestroyedCount;
        }
    }

    const float BagMoney = FMath::Max(0.0f, Plan.DroppedMoney);
    if (BagItems.Num() == 0 && BagMoney <= 0.0f)
    {
        UE_LOG(LogTemp, Log, TEXT("Death loss: nothing to drop as bag (lost %d destroyed)."), DestroyedCount);
        return;
    }

    FActorSpawnParameters Sp;
    Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    // Build 1.2 п.6: мешок смерти — тем же настраиваемым классом пикапа, что и дроп
    // предмета (BP_Pickup Рината); пустое поле — фолбэк на базовый класс.
    UClass* BagClass = PickupSpawnClass ? *PickupSpawnClass : APickup::StaticClass();
    APickup* Bag = World->SpawnActor<APickup>(BagClass, DeathLoc, FRotator::ZeroRotator, Sp);
    if (Bag)
    {
        Bag->InitLootBag(BagItems, BagMoney);
        UE_LOG(LogQA, Display, TEXT("QA: DEATH-DROP bag at %s - %d of %d lost consumables + %.0f money (destroyed %d)"),
            *DeathLoc.ToCompactString(), BagItems.Num(), LostCount, BagMoney, DestroyedCount);
    }
    else
    {
        // Мешок не заспавнился — не оставляем висящие предметы в мире.
        for (AMasterInventoryItem* Item : BagItems)
        {
            if (IsValid(Item)) { Item->Destroy(); }
        }
        UE_LOG(LogTemp, Warning, TEXT("Death loss: failed to spawn loot bag; dropped items destroyed."));
    }
}

void APlayerCharacter::ApplyDeathMoneyLoss(const DeathLoss::FPlan& Plan)
{
    // Build 1.2: деньги после смерти = (деньги НА МОМЕНТ СМЕРТИ − потеря по плану) — ровно
    // то число, что игрок видел в превью экрана смерти. Применяется ПОСЛЕ загрузки сейва
    // (LoadGame перетирает баланс значением слота), затем пере-сохранение — анти-эксплойт
    // «quit/reload вернёт деньги» (как прежний ApplyMoneyDeathPenalty).
    if (!Stats)
    {
        return;
    }
    const float Target = FMath::Max(0.0f, MoneyAtDeath - Plan.LostMoney);
    const float Current = Stats->GetMoney();
    if (Current > Target)
    {
        Stats->SpendMoney(Current - Target); // clamp >=0 + бродкаст HUD (OnMoneyChanged)
    }
    else if (Current < Target)
    {
        Stats->AddMoney(Target - Current);
    }
    SaveGame(); // пере-сохраняем итоговый баланс (+ текущую точку костра)
    UE_LOG(LogQA, Display, TEXT("QA: DEATH-MONEY at death %.0f, lost %.0f (dropped in bag %.0f), now %.0f"),
        MoneyAtDeath, Plan.LostMoney, Plan.DroppedMoney, Stats->GetMoney());
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (PlayerInputComponent)
    {
        // Build 1.2.1 (ТЗ В2): F — «реклама доступна сейчас». Легаси-маппинг QAUnlockAds=F
        // в Config/DefaultInput.ini; работает в игровом режиме ввода (жать ДО смерти/магазина).
        PlayerInputComponent->BindAction(TEXT("QAUnlockAds"), IE_Pressed,
            this, &APlayerCharacter::OnQAUnlockAds);
    }
}

void APlayerCharacter::OnQAUnlockAds()
{
    const float Before = GetTotalPlayTimeSeconds();
    const float Deficit = AdMinPlaytimeSeconds - Before;
    if (Deficit > 0.0f)
    {
        UnflushedPlayTime += Deficit; // добить накопитель ровно до порога
    }
    FlushPlayTime(); // немедленно в сейв, не ждать 60-секундный таймер
    UE_LOG(LogQA, Display,
        TEXT("QA: AD UNLOCK (key F) - playtime %.0f -> %.0f s (threshold %.0f), written to save"),
        Before, GetTotalPlayTimeSeconds(), AdMinPlaytimeSeconds);
}

void APlayerCharacter::FlushPlayTime()
{
    if (UnflushedPlayTime <= 0.0f)
    {
        return;
    }
    if (UContrarySaveGame* Save = LoadOrCreateSaveObject())
    {
        Save->TotalPlayTimeSeconds = FMath::Max(0.0f, Save->TotalPlayTimeSeconds) + UnflushedPlayTime;
        if (WriteSaveObject(Save))
        {
            SavedPlayTimeBase = Save->TotalPlayTimeSeconds;
            UnflushedPlayTime = 0.0f;
        }
    }
}

int32 APlayerCharacter::GetBackpackAdUsesToday() const
{
    const UContrarySaveGame* Save = LoadOrCreateSaveObject();
    return Save ? AdGating::UsesToday(FDateTime::Now(),
        Save->BackpackAdCounterDate, Save->BackpackAdUsesOnDate) : 0;
}

void APlayerCharacter::RegisterBackpackAdUse()
{
    if (UContrarySaveGame* Save = LoadOrCreateSaveObject())
    {
        const FDateTime Now = FDateTime::Now();
        Save->BackpackAdUsesOnDate = AdGating::UsesToday(Now,
            Save->BackpackAdCounterDate, Save->BackpackAdUsesOnDate) + 1;
        Save->BackpackAdCounterDate = Now.GetDate();
        WriteSaveObject(Save);
        UE_LOG(LogQA, Display, TEXT("QA: AD backpack use registered (%d today)"), Save->BackpackAdUsesOnDate);
    }
}

int32 APlayerCharacter::GetShopAdUsesToday() const
{
    const UContrarySaveGame* Save = LoadOrCreateSaveObject();
    return Save ? AdGating::UsesToday(FDateTime::Now(),
        Save->ShopAdCounterDate, Save->ShopAdUsesOnDate) : 0;
}

bool APlayerCharacter::IsShopAdCooldownPassed(float CooldownSeconds) const
{
    const UContrarySaveGame* Save = LoadOrCreateSaveObject();
    return !Save || AdGating::IsCooldownPassed(FDateTime::Now(),
        Save->LastShopAdTime, CooldownSeconds);
}

void APlayerCharacter::RegisterShopAdUse()
{
    if (UContrarySaveGame* Save = LoadOrCreateSaveObject())
    {
        const FDateTime Now = FDateTime::Now();
        Save->ShopAdUsesOnDate = AdGating::UsesToday(Now,
            Save->ShopAdCounterDate, Save->ShopAdUsesOnDate) + 1;
        Save->ShopAdCounterDate = Now.GetDate();
        Save->LastShopAdTime = Now;
        WriteSaveObject(Save);
        UE_LOG(LogQA, Display, TEXT("QA: AD shop use registered (%d today)"), Save->ShopAdUsesOnDate);
    }
}

void APlayerCharacter::HandleDeath()
{
    // #26: смерть НЕ возрождает сразу — показываем экран смерти, респаун по кнопке/клавише (Respawn()).

    // Фиксируем длительность жизни для экрана смерти.
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : LifeStartTime;
    LastLifeDuration = FMath::Max(0.0f, Now - LifeStartTime);

    // A4/ADR-027 (правка Рината): штраф БЕЗУСЛОВНЫЙ при ЛЮБОЙ смерти — гейт «вне деревни» убран.
    // Фиксируем место гибели для «мешка» расходников (используется в Respawn до телепорта).
    DeathDropLocation = GetActorLocation();

    // Build 1.2: снимок денег на момент смерти — база плана потерь (превью «Будет потеряно»
    // на экране смерти и фактическое списание в Respawn считаются от этого числа).
    MoneyAtDeath = Stats ? Stats->GetMoney() : 0.0f;
    ++DeathCountThisSession;

    UE_LOG(LogTemp, Warning, TEXT("APlayerCharacter: death -> death screen (lived %.1fs, killer '%s', kills %d)"),
        LastLifeDuration, *LastDamagerName.ToString(), EnemyKillCount);

    // Останавливаем персонажа (гасим движение; тело остаётся на месте).
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->StopMovementImmediately();
    }

    // Build 1.2.1 (ТЗ Е): анимация смерти «ложится на спину» — ДО показа экрана смерти.
    // Поле DeathAnimation уже в мастер-базе гуманоидов (MasterHumanoidCharacter.h:100);
    // ассета нет — false, прежнее поведение (тело просто замирает). true — меш ушёл в
    // Single Node, флаг напоминает Respawn вернуть AnimationBlueprint.
    bDeathAnimPlayed = PlayDeathAnimationIfSet();

    AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(GetController());
    if (PC)
    {
        // Закрываем открытые окна (инвентарь/магазин/диалог) — UI не должен висеть под экраном смерти.
        PC->CloseAllUI();
        // Показываем экран смерти и выключаем геймплей-ввод (InputMode UI + гейт движения/огня).
        PC->ShowDeathScreen();
    }

    const float Money = Stats ? Stats->GetMoney() : 0.0f;
    const int32 QuestsDone = Quests ? Quests->GetTurnedInQuestCount() : 0;
    UE_LOG(LogQA, Display, TEXT("QA: DEATH SCREEN shown - lived %.0fs, killer '%s', money %.0f, quests %d, kills %d"),
        LastLifeDuration, *LastDamagerName.ToString(), Money, QuestsDone, EnemyKillCount);

    // F3 (ADR-038): событие аналитики «смерть игрока». Без ключей — no-op.
    if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
    {
        Analytics->RecordPlayerDeath();
    }
}

void APlayerCharacter::RegisterEnemyKill(const FString& EnemyType)
{
    ++EnemyKillCount;
    UE_LOG(LogQA, Display, TEXT("QA: enemy kill counted -> total %d"), EnemyKillCount);

    // F3 (ADR-038): событие аналитики «убийство врага» с типом (Wolf/Bandit). Без ключей — no-op.
    if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
    {
        Analytics->RecordEnemyKill(EnemyType);
    }
}

void APlayerCharacter::Respawn(bool bBackpackRescued)
{
    UE_LOG(LogTemp, Warning, TEXT("APlayerCharacter: respawn (from death screen, rescued=%s)"),
        bBackpackRescued ? TEXT("yes") : TEXT("no"));

    // Build 1.2: план потерь — от состояния на момент смерти. Без рекламы: 70% расходников
    // и 50% денег; после досмотра «Спасти рюкзак»: по 10%. Из потерянного половина падает
    // мешком, остальное исчезает. Пока экран смерти был открыт — ничего списано не было.
    const DeathLoss::FPlan Plan = ComputeDeathLossPlan(bBackpackRescued);

    // 1) Часть 1: снятие расходников + «мешок» НА МЕСТЕ ГИБЕЛИ — ДО телепорта респауна
    //    (снимок DeathDropLocation из HandleDeath).
    DropDeathLoss(DeathDropLocation, Plan);

    // 2) Респаун: восстановление из последнего сейва (костёр). Если сейва нет —
    //    фолбэк на стартовый трансформ + полные статы.
    if (!LoadGame())
    {
        if (Stats)
        {
            Stats->RestoreState(Stats->GetMaxHealth(), Stats->GetSurvivalMax(),
                Stats->GetSurvivalMax(), Stats->GetMoney());
        }
        if (UCharacterMovementComponent* Move = GetCharacterMovement())
        {
            Move->StopMovementImmediately();
        }
        SetActorTransform(InitialSpawnTransform, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
        UE_LOG(LogTemp, Warning, TEXT("Respawn: no save found, used initial spawn transform."));
    }

    // 2b) Часть 2: деньги = (на момент смерти − потеря по плану) ПОСЛЕ загрузки (LoadGame
    //     перезаписал баланс из сейва) + пере-сохранение (анти-эксплойт quit/reload).
    ApplyDeathMoneyLoss(Plan);

    // 3) Death-респаун = полные HP/Голод/Жажда (решение game-lead). Деньги — из сейва (шаг 2).
    //    Автосейв костра пишет ЖИВЫЕ значения голода/жажды (жажда деградирует быстрее),
    //    поэтому при загрузке они «нестабильны» по таймингу — форсим в максимум здесь.
    //    ВАЖНО: только в death-ветке; обычный quit->reload (ApplySaveData) значения НЕ трогает.
    if (Stats)
    {
        Stats->SetHealth(Stats->GetMaxHealth() * RespawnHealthFraction);
        Stats->SetHunger(Stats->GetSurvivalMax() * RespawnSurvivalFraction);
        Stats->SetThirst(Stats->GetSurvivalMax() * RespawnSurvivalFraction);
        UE_LOG(LogTemp, Warning, TEXT("Respawn stats: HP %.1f/%.1f, Hunger %.1f, Thirst %.1f (frac HP %.2f / Surv %.2f)"),
            Stats->GetHealth(), Stats->GetMaxHealth(), Stats->GetHunger(), Stats->GetThirst(),
            RespawnHealthFraction, RespawnSurvivalFraction);
    }

    // 3б) Build 1.2.1 (ТЗ Е): вернуть меш из Single Node (анимация смерти) в штатный AnimBP.
    //     SetAnimationMode при смене режима переинициализирует AnimInstance
    //     (SkeletalMeshComponent.cpp, UE 5.5) — игрок снова в живой позе.
    if (bDeathAnimPlayed)
    {
        if (USkeletalMeshComponent* MeshComp = GetMesh())
        {
            MeshComp->SetAnimationMode(EAnimationMode::AnimationBlueprint);
        }
        bDeathAnimPlayed = false;
    }

    // 4) Сбрасываем трекинг жизни и врага-убийцу для следующей жизни.
    LifeStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    LastDamagerName = NSLOCTEXT("Death", "KillerUnknown", "Неизвестно");

    // 5) Возвращаем управление и убираем экран смерти.
    if (AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(GetController()))
    {
        PC->HideDeathScreen();
    }

    UE_LOG(LogQA, Display, TEXT("QA: RESPAWN done - HP %.0f, money %.0f"),
        Stats ? Stats->GetHealth() : 0.0f, Stats ? Stats->GetMoney() : 0.0f);
}

void APlayerCharacter::SetUpMovement()
{
    // Configure character movement
    GetCharacterMovement()->bOrientRotationToMovement = false; // Character moves in the direction of input...
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 380.0f, 0.0f); // ...at this rotation rate
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
}