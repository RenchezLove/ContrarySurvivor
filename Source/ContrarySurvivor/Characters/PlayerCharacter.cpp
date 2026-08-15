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
#include "ContrarySurvivor/HUD/ContrarySurvivorHUD.h"         // слот LimpIndicatorWidgetClass (ADR-048)
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
#include "ContrarySurvivor/Actors/Campfire.h"  // смертельное возрождение — всегда у костра (вариант 1+3)
#include "EngineUtils.h"                        // TActorIterator: поиск костра по классу
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h" // CloseAllUI / экран смерти
#include "ContrarySurvivor/Controllers/EnemyAIController.h" // D6: реестр врагов для боевой камеры (ADR-035)
#include "ContrarySurvivor/Characters/WolfCharacter.h"  // #26: читаемое имя «от кого погиб»
#include "ContrarySurvivor/Characters/EnemyCharacter.h" // #26: читаемое имя «от кого погиб»
#include "ContrarySurvivor/ContrarySurvivor.h"  // LogQA
#include "ContrarySurvivor/Ads/AdGatingLogic.h"  // Build 1.2: суточные счётчики/кулдаун рекламы
#include "ContrarySurvivor/Debug/QADebug.h"      // QA god-mode (неуязвимость)
#include "ContrarySurvivor/Settings/ContrarySurvivorGameUserSettings.h" // громкость музыки/эффектов игрока
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

    // Стартовый огнестрел. Build 1.2.2 (решение Рината 05-08): по умолчанию НЕ выдаётся —
    // игрок начинает с одним ножом, а пистолет копит и покупает у торговца за 150 монет.
    // Патроны отдельной выдачи не имели никогда: они приходили внутри самого пистолета
    // (обойма 12 + резерв 48), поэтому вместе с ним исчезают и они.
    if (bStartWithRangedWeapon)
    {
        EquipDefaultWeapon();
    }

    // Нож держим «в кобуре» (скрыт), переключение по SwitchWeapon (Фаза 3).
    SpawnMeleeWeapon();

    // Снять «огнестрел мимо слота», если его успел экипировать легаси-граф BP на
    // ReceiveBeginPlay (тот отрабатывает в Super::BeginPlay выше). Идёт ПОСЛЕ SpawnMeleeWeapon,
    // чтобы страховке было чем заменить снятый пистолет (нож уже заспавнен).
    ReconcileOutOfSlotRangedWeapon();

    // Без огнестрела в руках не оказалось бы вообще ничего (нож спавнится скрытым и
    // неэкипированным), а безоружный игрок не может ни ударить, ни защититься. Поэтому
    // при пустых руках сразу берём нож — он и есть стартовое оружие новой игры.
    if (!GetCurrentWeapon() && MeleeWeaponInstance)
    {
        EquipWeapon(MeleeWeaponInstance);
        MeleeWeaponInstance->SetActorHiddenInGame(false);
        UE_LOG(LogQA, Display, TEXT("QA: старт без огнестрела — в руках нож"));
    }

    // Итоговое состояние оружия игрока — ОДНОЙ строкой, БЕЗУСЛОВНО (не только в ветке выше),
    // чтобы расследование расхождений между кодом и живой игрой не требовало собирать картину
    // по разрозненным строкам EquipWeapon (те теперь помечены владельцем — MasterHumanoidCharacter.cpp).
    UE_LOG(LogQA, Display,
        TEXT("QA: PLAYER WEAPON STATE at BeginPlay end — ranged slot %s, in hands %s"),
        RangedWeaponInstance ? *RangedWeaponInstance->GetName() : TEXT("empty"),
        GetCurrentWeapon() ? *GetCurrentWeapon()->GetName() : TEXT("nothing"));

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
    // Громкость музыки с экрана настроек (ADR-062): фоновый эмбиент — единственный
    // непрерывный фон игры, он и живёт на канале музыки, пока настоящей музыки нет.
    AmbienceComponent = UGameplayStatics::SpawnSound2D(
        this, AmbienceSound,
        AmbienceVolume * UContrarySurvivorGameUserSettings::GetMusicVolumeSafe(),
        1.0f, 0.0f, nullptr, false, /*bAutoDestroy=*/false);
}

bool APlayerCharacter::SetAmbienceSilenced(bool bSilenced)
{
    if (bSilenced == bAmbienceSilenced)
    {
        return false; // уже в нужном состоянии — молчим и ничего не трогаем
    }
    bAmbienceSilenced = bSilenced;

    if (!AmbienceComponent)
    {
        return false; // фон ещё не заведён (или уже уничтожен) — вернёмся к этому позже
    }

    // Ставим на паузу, а не останавливаем: при возврате лес продолжится с того же места,
    // а не начнётся заново с первой птицы.
    AmbienceComponent->SetPaused(bSilenced);
    return true;
}

void APlayerCharacter::ApplyAudioSettings()
{
    // Зацикленные звуки уже играют — им громкость меняем прямо на компоненте
    // (SetVolumeMultiplier), иначе настройка подействовала бы только после перезапуска звука.
    if (AmbienceComponent)
    {
        AmbienceComponent->SetVolumeMultiplier(
            AmbienceVolume * UContrarySurvivorGameUserSettings::GetMusicVolumeSafe());
    }
    if (LimpBreathingComponent)
    {
        LimpBreathingComponent->SetVolumeMultiplier(
            UContrarySurvivorGameUserSettings::GetEffectsVolumeSafe());
    }
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
        // Снимаем наши оверрайды — камера рисует без нашей постобработки.
        PP.bOverride_VignetteIntensity   = false;
        PP.bOverride_FilmGrainIntensity  = false;
        PP.bOverride_AutoExposureMethod  = false;
        PP.bOverride_AutoExposureBias    = false;
        PP.bOverride_AutoExposureApplyPhysicalCameraExposure = false;
        PP.bOverride_ColorSaturation     = false;
        PP.bOverride_ColorGainHighlights = false;
        PP.bOverride_ColorGainShadows    = false;
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

    // Тёплые света: усиление красного, ослабление синего в светах (gain, W=1).
    PP.bOverride_ColorGainHighlights = true;
    PP.ColorGainHighlights = FVector4(1.0f + PPHighlightWarmth, 1.0f, 1.0f - PPHighlightWarmth, 1.0f);

    // Холодные тени: усиление синего, ослабление красного в тенях.
    PP.bOverride_ColorGainShadows = true;
    PP.ColorGainShadows = FVector4(1.0f - PPShadowCoolness, 1.0f, 1.0f + PPShadowCoolness, 1.0f);
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
                this, LimpBreathingSound,
                UContrarySurvivorGameUserSettings::GetEffectsVolumeSafe(),
                1.0f, 0.0f, nullptr, false, /*bAutoDestroy=*/false);
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
        // Слот на HUD назначен (ADR-048) — индикатор из WBP, правится в дизайнере; пусто —
        // кодовое дерево (замечание qa к волне night-0807: слот был объявлен, но не читался).
        TSubclassOf<ULimpIndicatorWidget> LimpClass = ULimpIndicatorWidget::StaticClass();
        if (const AContrarySurvivorHUD* Hud = Cast<AContrarySurvivorHUD>(PC->GetHUD()))
        {
            if (Hud->LimpIndicatorWidgetClass)
            {
                LimpClass = Hud->LimpIndicatorWidgetClass;
            }
        }
        LimpIndicatorWidget = CreateWidget<ULimpIndicatorWidget>(PC, LimpClass);
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
            // Громкость эффектов с экрана настроек (ADR-062): разовые звуки берут её в момент
            // проигрывания, поэтому правка ползунка слышна со следующего же шага.
            UGameplayStatics::PlaySoundAtLocation(this, Step, GetActorLocation(),
                FootstepVolume * UContrarySurvivorGameUserSettings::GetEffectsVolumeSafe());
        }
    }
}

float APlayerCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // QA god-mode (клавиша T): игрок неуязвим — весь входящий урон зануляется.
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

        // Короткий импульс вибрации (решение game-lead 08-09). Гейт вынесен в чистое правило,
        // чтобы автотест мог проверить его без телефона.
        if (ShouldPlayDamageVibration(UContrarySurvivorGameUserSettings::IsVibrationEnabledSafe(), Applied))
        {
            PlayVibration(DamageVibrationIntensity, DamageVibrationDuration);
        }
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

void APlayerCharacter::ReconcileOutOfSlotRangedWeapon()
{
    // Дальнобой в руках обязан быть ИМЕННО стволом слота: все штатные пути (EquipDefaultWeapon,
    // TryAdoptRangedWeapon -> SwitchWeapon) ставят RangedWeaponInstance ДО экипировки.
    // Несовпадение — артефакт обхода слота (см. комментарий в заголовке).
    ARangedWeapon* Ranged = Cast<ARangedWeapon>(GetCurrentWeapon());
    if (!Ranged || Ranged == RangedWeaponInstance)
    {
        return;
    }

    UE_LOG(LogQA, Warning,
        TEXT("QA: огнестрел '%s' экипирован МИМО слота (слот: '%s') — снят и уничтожен, это артефакт обхода RangedWeaponInstance"),
        *Ranged->GetName(),
        RangedWeaponInstance ? *RangedWeaponInstance->GetName() : TEXT("пусто"));

    UnequipWeapon(); // CurrentWeapon -> null, мировая коллизия оружию возвращена
    Ranged->Destroy();

    // Руки не оставляем пустыми: нож — стартовое оружие (тот же ход, что фолбэк BeginPlay).
    if (!GetCurrentWeapon() && MeleeWeaponInstance)
    {
        EquipWeapon(MeleeWeaponInstance);
        MeleeWeaponInstance->SetActorHiddenInGame(false);
    }
}

bool APlayerCharacter::HasTurnedInFirstQuest() const
{
    // ADR-063: единое квестовое условие порога рекламы (РИ-29) и приглушения истощения.
    return Quests && Quests->GetTurnedInQuestCount() > 0;
}

bool APlayerCharacter::TryAdoptRangedWeapon(AMasterInventoryItem* Item)
{
    ARangedWeapon* Ranged = Cast<ARangedWeapon>(Item);
    if (!Ranged || RangedWeaponInstance)
    {
        return false; // не огнестрел либо слот уже занят — вещь остаётся в рюкзаке
    }

    // Оружие в слоте — это не содержимое рюкзака: нож и прежний стартовый пистолет тоже
    // живут отдельно от него. Иначе один и тот же ствол показывался бы и слотом оружия,
    // и плиткой рюкзака, и его можно было бы продать прямо из рук.
    if (Inventory)
    {
        Inventory->RemoveItem(Ranged);
    }

    RangedWeaponInstance = Ranged;
    Ranged->SetOwner(this);
    Ranged->SetInstigator(this);
    // «В кобуре», как нож: в руки берёт сам игрок кнопкой «Оружие» (SwitchWeapon).
    Ranged->SetActorHiddenInGame(true);
    Ranged->SetActorEnableCollision(false);

    UE_LOG(LogQA, Display, TEXT("QA: огнестрел '%s' занял пустой слот оружия (в кобуре)"),
        *Ranged->GetItemDisplayText().ToString());
    return true;
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

            // ТЗ Рината 08-08 (STALKER-поток): купленный огнестрел попадает В РЮКЗАК и остаётся
            // там. Автоэкип в слот оружия убран — в слот его переносит сам игрок тапом по плитке
            // в окне инвентаря (UInventoryScreenWidget::HandleTileUse -> TryAdoptRangedWeapon).
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

    // Б3: рюкзак + экипированная броня — единый список (FSavedInventoryEntry), см. комментарий
    // в ContrarySaveGame.h. GetInventoryItems() уже включает экипированные предметы (Фаза 4:
    // «предмет может лежать в InventoryItems и быть экипированным одновременно») — отдельно
    // спрашивать GetEquippedArmor по слотам не нужно, IsItemEquipped на том же предмете хватает.
    Save->InventoryEntries.Reset();
    if (Inventory)
    {
        for (const AMasterInventoryItem* Item : Inventory->GetInventoryItems())
        {
            if (!Item)
            {
                continue;
            }
            FSavedInventoryEntry Entry;
            Entry.ClassPath = Item->GetClass()->GetPathName();
            Entry.ItemName = Item->ItemName;
            Entry.ItemDisplayText = Item->ItemDisplayText;
            Entry.StackCount = Item->GetStackCount();
            Entry.bEquipped = Inventory->IsItemEquipped(Item);
            // AConsumableItem: один класс обслуживает еду/воду/аптечку, тип — на экземпляре,
            // классом (Entry.ClassPath) не определяется — без него восстановленный расходник
            // всегда откатывался бы на дефолт Food (вода лечила бы голод вместо жажды).
            if (const AConsumableItem* Cons = Cast<const AConsumableItem>(Item))
            {
                Entry.ConsumableType = Cons->ConsumableType;
            }
            Save->InventoryEntries.Add(Entry);
        }
    }

    // Б3: журнал квестов — полный снимок (см. комментарий у поля Quests в ContrarySaveGame.h).
    Save->Quests.Reset();
    if (Quests)
    {
        Save->Quests = Quests->GetQuests();
    }

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

    // Записалось — значит заработанное на диске: с этой секунды несохранённого прогресса нет.
    if (bOk)
    {
        CaptureProgressSnapshot();
    }
    return bOk;
}

bool APlayerCharacter::SaveGameAtCampfire()
{
    const bool bOk = SaveGame();
    if (!bOk)
    {
        // Соврать «сохранено», когда запись не прошла, нельзя — игрок понадеется и потеряет игру.
        UE_LOG(LogQA, Warning, TEXT("QA: campfire save FAILED — надпись игроку не показываем"));
        return false;
    }

    // Тот же тост, что у подсказок обучения (второй механизм всплывашек не заводим).
    if (Onboarding)
    {
        Onboarding->ShowTransientHint(ProgressSavedMessage);
    }
    UE_LOG(LogQA, Display, TEXT("QA: campfire save OK — показана надпись «%s»"),
        *ProgressSavedMessage.ToString());
    return true;
}

APlayerCharacter::FSavedProgressSnapshot APlayerCharacter::MakeProgressSnapshot() const
{
    FSavedProgressSnapshot Snapshot;
    Snapshot.bValid = true;
    Snapshot.Money = Stats ? Stats->GetMoney() : 0;

    if (Inventory)
    {
        for (const AMasterInventoryItem* Item : Inventory->GetInventoryItems())
        {
            if (Item)
            {
                // Считаем ШТУКИ, а не строки списка: съеденная консерва из стака «3 шт»
                // меняет количество, но не длину списка — иначе такую трату мы бы не заметили.
                Snapshot.ItemUnits += FMath::Max(1, Item->GetStackCount());
            }
        }
    }

    if (Quests)
    {
        Snapshot.QuestCount = Quests->GetQuests().Num();
        Snapshot.TurnedInQuests = Quests->GetTurnedInQuestCount();
    }
    return Snapshot;
}

void APlayerCharacter::CaptureProgressSnapshot()
{
    SavedProgress = MakeProgressSnapshot();
}

bool APlayerCharacter::IsProgressUnsaved() const
{
    // Снимка нет — в этой сессии игрок ещё ни разу не сохранялся и не загружался: считаем,
    // что терять есть что. Ошибиться безопаснее в эту сторону: лишний вопрос стоит одного
    // касания, а молчание стоит игроку прохождения.
    if (!SavedProgress.bValid)
    {
        return true;
    }

    const FSavedProgressSnapshot Now = MakeProgressSnapshot();
    return Now.Money != SavedProgress.Money
        || Now.ItemUnits != SavedProgress.ItemUnits
        || Now.QuestCount != SavedProgress.QuestCount
        || Now.TurnedInQuests != SavedProgress.TurnedInQuests;
}

bool APlayerCharacter::ShouldPlayDamageVibration(bool bVibrationEnabled, float AppliedDamage)
{
    return bVibrationEnabled && AppliedDamage > 0.0f;
}

void APlayerCharacter::PlayVibration(float Intensity, float Duration) const
{
    if (!UContrarySurvivorGameUserSettings::IsVibrationEnabledSafe())
    {
        return;
    }
    if (Intensity <= 0.0f || Duration <= 0.0f)
    {
        return;
    }
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC)
    {
        return;
    }
    // Все четыре канала разом: у телефона мотор один, движок берёт наибольшее значение
    // каналов (AndroidInputInterface.cpp: UpdateVibeMotors) — так импульс не зависит от того,
    // какой канал платформа считает «своим».
    PC->PlayDynamicForceFeedback(Intensity, Duration,
        /*bAffectsLeftLarge=*/true, /*bAffectsLeftSmall=*/true,
        /*bAffectsRightLarge=*/true, /*bAffectsRightSmall=*/true,
        EDynamicForceFeedbackAction::Start);
}

bool APlayerCharacter::HasSaveGame() const
{
    // Одно место правды на всю игру: статическая проверка ниже. Живому персонажу остаётся
    // только подставить СВОЙ слот (слот — параметр персонажа, а не константа).
    return HasSaveGameInSlot(SaveSlotName, SaveUserIndex);
}

bool APlayerCharacter::HasSaveGameInSlot(const FString& SlotName, int32 UserIndex)
{
    // Б3: файл слота — это ещё не «есть что продолжать». FlushPlayTime создаёт файл уже через
    // минуту игры (пишет ТОЛЬКО накопитель времени через LoadOrCreateSaveObject/WriteSaveObject,
    // bHasData у такого объекта остаётся false), поэтому смотрим на признак РЕАЛЬНЫХ данных
    // (bHasData=true ставит только настоящий SaveGame(), т.е. автосейв костра/пере-сейв смерти),
    // а не на голый факт существования файла.
    if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
    {
        return false;
    }
    const UContrarySaveGame* Save = Cast<UContrarySaveGame>(
        UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
    return Save && Save->bHasData;
}

FString APlayerCharacter::GetDefaultSaveSlotName()
{
    // Значение из умолчаний класса — второй раз строкой имя слота не пишем.
    const APlayerCharacter* Defaults = GetDefault<APlayerCharacter>();
    return Defaults ? Defaults->SaveSlotName : FString(TEXT("ContrarySave"));
}

int32 APlayerCharacter::GetDefaultSaveUserIndex()
{
    const APlayerCharacter* Defaults = GetDefault<APlayerCharacter>();
    return Defaults ? Defaults->SaveUserIndex : 0;
}

bool APlayerCharacter::HasDefaultSaveGame()
{
    return HasSaveGameInSlot(GetDefaultSaveSlotName(), GetDefaultSaveUserIndex());
}

void APlayerCharacter::DeleteSaveInSlot(const FString& SlotName, int32 UserIndex)
{
    UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
    UE_LOG(LogQA, Display, TEXT("QA: слот сохранения '%s' стёрт"), *SlotName);
}

void APlayerCharacter::DeleteDefaultSaveGame()
{
    DeleteSaveInSlot(GetDefaultSaveSlotName(), GetDefaultSaveUserIndex());
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

bool APlayerCharacter::LoadGameForContinue()
{
    // Б3 («Продолжить» на стартовом экране): полное восстановление прогресса — статы+позиция
    // (как LoadGame), плюс содержимое рюкзака, экипированная броня, журнал квестов и накопитель
    // игрового времени. Respawn() эту функцию НЕ зовёт (см. комментарий у LoadGame в заголовке) —
    // смерть по-прежнему откатывает только статы/позицию, инвентарь между чекпоинтами не трогает.
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

    // 08-07 (жалоба Рината «стартую хуй пойми где»): позицию в слот пишут только автосейв
    // костра и пере-сейв смерти (после перестановки к костру), поэтому точка вдали от костров —
    // всегда испорченный слот старой сборки (петля смерти ADR-061 закрепила точку поля).
    // Смертельный путь лечится в Respawn(); «Продолжить» обязан лечиться так же, иначе
    // игрок продолжает игру посреди поля рядом с врагами.
    RelocateToCampfireIfSavedPointFar(TEXT("CONTINUE"));

    RestoreInventoryAndArmor(Save);

    if (Quests)
    {
        Quests->RestoreQuests(Save->Quests);
    }

    // BeginPlay уже прочитал накопитель времени в SavedPlayTimeBase; переприменяем явно —
    // идемпотентно, но не оставляет скрытую зависимость от порядка BeginPlay/выбора игрока.
    SavedPlayTimeBase = FMath::Max(0.0f, Save->TotalPlayTimeSeconds);
    UnflushedPlayTime = 0.0f;

    // Живое состояние теперь совпадает с содержимым слота: сразу после «Продолжить»
    // несохранённого прогресса нет, и меню паузы не должно спрашивать лишнего.
    CaptureProgressSnapshot();

    UE_LOG(LogQA, Display, TEXT("QA: CONTINUE - loaded save (money %.0f, backpack %d item(s), quests %d)"),
        Save->Money, Save->InventoryEntries.Num(), Save->Quests.Num());
    return true;
}

void APlayerCharacter::ResetToNewGame()
{
    // Б3 («Новая игра» поверх существующего сейва, ТЗ издателя п.4 «честно стирает прогресс»):
    // удаляем слот целиком (иначе получится каша из старых и новых данных) и приводим ЖИВЫЕ
    // статы к стартовым значениям новой игры. Отдельная функция нужна потому, что BeginPlay уже
    // отработал раньше решения игрока: HasSaveGame() тогда была true (сейв ещё существовал), и
    // ветка «новая игра» (половина HP/голода/жажды) в BeginPlay не сработала.
    DeleteSaveInSlot(SaveSlotName, SaveUserIndex);

    if (Stats)
    {
        Stats->InitHealth(PlayerMaxHealth, /*bSetToMax=*/true);
        Stats->InitMoney(StartingMoney);
        Stats->SetHealth(Stats->GetMaxHealth() * NewGameHealthFraction);
        Stats->SetHunger(Stats->GetSurvivalMax() * NewGameSurvivalFraction);
        Stats->SetThirst(Stats->GetSurvivalMax() * NewGameSurvivalFraction);
        UpdateLimpState(Stats->GetHealth(), Stats->GetMaxHealth());
    }

    // Накопитель игрового времени — заново (гейт рекламы 15 минут не наследует чужой прогресс).
    SavedPlayTimeBase = 0.0f;
    UnflushedPlayTime = 0.0f;

    // Позиция — стартовый спавн (сейв в живого персонажа ещё не грузился, откатывать нечего).
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->StopMovementImmediately();
    }
    SetActorTransform(InitialSpawnTransform, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);

    UE_LOG(LogQA, Display, TEXT("QA: NEW GAME chosen over existing save - save wiped, stats reset to fresh start"));
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

void APlayerCharacter::RestoreInventoryAndArmor(const UContrarySaveGame* Save)
{
    if (!Save || !Inventory)
    {
        return;
    }
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FActorSpawnParameters Sp;
    Sp.Owner = this;
    Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    int32 Restored = 0;
    for (const FSavedInventoryEntry& Entry : Save->InventoryEntries)
    {
        UClass* ItemClass = StaticLoadClass(AMasterInventoryItem::StaticClass(), nullptr, *Entry.ClassPath);
        if (!ItemClass)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("LoadGameForContinue: inventory class '%s' not found - entry skipped"), *Entry.ClassPath);
            continue;
        }

        AMasterInventoryItem* Item = World->SpawnActor<AMasterInventoryItem>(
            ItemClass, GetActorLocation(), GetActorRotation(), Sp);
        if (!Item)
        {
            continue;
        }

        // Предмет рюкзака — данные, не объект сцены (как везде: GiveTestItems и т.п.).
        Item->SetActorHiddenInGame(true);
        Item->SetActorEnableCollision(false);
        if (!Entry.ItemName.IsEmpty())
        {
            Item->ItemName = Entry.ItemName;
        }
        if (!Entry.ItemDisplayText.IsEmpty())
        {
            Item->ItemDisplayText = Entry.ItemDisplayText;
        }
        Item->StackCount = FMath::Clamp(Entry.StackCount, 0, FMath::Max(1, Item->MaxStackCount));

        // AConsumableItem: тип (еда/вода/аптечка) — на экземпляре, класс его не определяет.
        if (AConsumableItem* Cons = Cast<AConsumableItem>(Item))
        {
            Cons->ConsumableType = Entry.ConsumableType;

            // Лечение сейвов, записанных ДО фикса 08-07 (лут лагеря спавнился без имён и в
            // таком виде уезжал в сейв): запись без ключа и названия получает штатные имена
            // своего типа — иначе «Предмет» пережил бы фикс через старый сейв. Тип к этому
            // моменту уже восстановлен строкой выше, имена выводятся по нему.
            if (Cons->ItemName.IsEmpty())
            {
                Cons->ItemName = AConsumableItem::GetDefaultDisplayName(Cons->ConsumableType);
            }
            if (Cons->ItemDisplayText.IsEmpty())
            {
                Cons->ItemDisplayText = AConsumableItem::GetDefaultDisplayText(Cons->ConsumableType);
            }
        }

        Inventory->AddItem(Item);
        ++Restored;

        // Броня, надетая на момент сохранения, — надеваем заново (подмена меша слота + защита).
        if (Entry.bEquipped)
        {
            if (AArmor* Armor = Cast<AArmor>(Item))
            {
                EquipArmor(Armor);
                Inventory->SetItemEquipped(Armor, true);
            }
        }
    }

    UE_LOG(LogQA, Display, TEXT("QA: CONTINUE - restored %d/%d backpack item(s)"),
        Restored, Save->InventoryEntries.Num());
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

void APlayerCharacter::RelocateToCampfireIfSavedPointFar(const TCHAR* ContextTag)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // Ближайший костёр — по классу: имя экземпляра в уровне может уехать при пересохранении.
    ACampfire* NearestCampfire = nullptr;
    float BestDistSq = TNumericLimits<float>::Max();
    const FVector CurrentLoc = GetActorLocation();
    for (TActorIterator<ACampfire> It(World); It; ++It)
    {
        const float DistSq = FVector::DistSquared2D(It->GetActorLocation(), CurrentLoc);
        if (DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            NearestCampfire = *It;
        }
    }

    if (!NearestCampfire)
    {
        // Мир без костра (служебный/тестовый) — переставлять некуда, точка остаётся прежней.
        UE_LOG(LogTemp, Warning, TEXT("%s: no ACampfire in world - loaded spawn point left as is."), ContextTag);
        return;
    }

    if (BestDistSq <= FMath::Square(RespawnNearCampfireRadius))
    {
        return; // точка настоящего сейва у костра — валидна, не трогаем
    }

    // Точка «у костра, но не в огне»: полрадиуса зоны вперёд от костра, лицом к огню, Z по полу.
    const FVector FireLoc = NearestCampfire->GetActorLocation();
    FVector Target = FireLoc + NearestCampfire->GetActorForwardVector() * (NearestCampfire->GetSafeZoneRadius() * 0.5f);
    const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
    Target.Z = SpawnPlacement::ResolveSpawnZ(World, Target.X, Target.Y, HalfHeight + 10.0f,
        TEXT("Player-load-relocate"), this);

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->StopMovementImmediately();
    }
    const FRotator FaceFire = (FireLoc - Target).GetSafeNormal2D().Rotation();
    SetActorLocationAndRotation(Target, FaceFire, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);

    UE_LOG(LogQA, Display, TEXT("QA: %s moved to campfire '%s' (saved point was %.0f cm away, limit %.0f)"),
        ContextTag, *NearestCampfire->GetName(), FMath::Sqrt(BestDistSq), RespawnNearCampfireRadius);
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

    // Вибрация смерти — подлиннее и сильнее, чем при уроне (решение game-lead 08-09).
    PlayVibration(DeathVibrationIntensity, DeathVibrationDuration);

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

    // 2а) Смертельное возрождение всегда заканчивается у костра (решение лида 08-06, вариант
    //     1+3, петля смерти дистрибуционной сборки): и когда сейва не было (фолбэк выше
    //     поставил на стартовое поле, где мог дежурить волк-убийца), и когда в слоте
    //     закреплена точка вне костра (пере-сейв смерти прошлых версий). Стоит ДО шага 2b:
    //     пере-сохранение там пишет уже точку у костра — петля не закрепляется в сейве.
    RelocateToCampfireIfSavedPointFar(TEXT("DEATH RESPAWN"));

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