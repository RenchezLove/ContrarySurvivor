// Fill out your copyright notice in the Description page of Project Settings.

#include "PatchAnimBpCommandlet.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimData/BoneMaskFilter.h" // FBranchFilter — фильтр по кости C_Spine01
#include "Animation/AnimSequenceBase.h"  // поза прицеливания как ассет анимации
#include "Animation/Skeleton.h"          // FAnimSlotGroup::DefaultSlotName — стандартное имя слота
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SequenceEvaluator.h"
#include "AnimGraphNode_Slot.h"
#include "ContrarySurvivor/Animation/HumanoidAnimInstance.h" // переменная силы наложения позы
#include "K2Node_VariableGet.h"
#include "AnimationGraphSchema.h"        // UAnimationGraphSchema::IsLocalSpacePosePin
#include "EdGraph/EdGraph.h"             // FGraphNodeCreator
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"            // UEdGraphSchema_K2::GN_AnimGraph — имя графа анимации
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogPatchAnimBp, Log, All);

namespace
{
	// Правится один конкретный ассет — общий анимационный блюпринт гуманоидов.
	const TCHAR* GAnimBpPackage = TEXT("/Game/Characters/Shared/Humanoid/ABP_HumanoidCharacter");
	const TCHAR* GAnimBpObjectPath = TEXT("/Game/Characters/Shared/Humanoid/ABP_HumanoidCharacter.ABP_HumanoidCharacter");

	UAnimBlueprint* LoadHumanoidAnimBp()
	{
		UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, GAnimBpObjectPath);
		if (!AnimBP)
		{
			UE_LOG(LogPatchAnimBp, Error, TEXT("Не удалось загрузить анимационный блюпринт %s."), GAnimBpObjectPath);
		}
		return AnimBP;
	}

	// Главный граф анимации блюпринта. Ищем по каноническому имени графа
	// (UEdGraphSchema_K2::GN_AnimGraph = «AnimGraph»): GetAllGraphs отдаёт ВСЕ графы, включая
	// подграфы машины состояний и переходов, а нам нужен только верхний.
	UEdGraph* FindAnimGraph(const UAnimBlueprint* AnimBP)
	{
		TArray<UEdGraph*> AllGraphs;
		AnimBP->GetAllGraphs(AllGraphs);
		for (UEdGraph* Graph : AllGraphs)
		{
			if (Graph && Graph->GetFName() == UEdGraphSchema_K2::GN_AnimGraph)
			{
				return Graph;
			}
		}
		UE_LOG(LogPatchAnimBp, Error, TEXT("В блюпринте нет графа с именем '%s' (всего графов: %d)."),
			*UEdGraphSchema_K2::GN_AnimGraph.ToString(), AllGraphs.Num());
		return nullptr;
	}

	// Пин позы (локальное пространство) заданного направления. Ищем ПО ТИПУ, а не по имени:
	// имена пинов узлов анимации берутся из имён полей структуры узла и меняются от узла к узлу,
	// а тип позы един (UAnimationGraphSchema::IsLocalSpacePosePin, AnimationGraphSchema.h:101).
	UEdGraphPin* FindPosePin(const UEdGraphNode* Node, EEdGraphPinDirection Direction)
	{
		if (!Node)
		{
			return nullptr;
		}
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction && UAnimationGraphSchema::IsLocalSpacePosePin(Pin->PinType))
			{
				return Pin;
			}
		}
		return nullptr;
	}

	// Читаемое имя узла для логов: класс + заголовок, как он подписан в графе.
	FString DescribeNode(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return TEXT("(нет узла)");
		}
		return FString::Printf(TEXT("%s «%s»"),
			*Node->GetClass()->GetName(),
			*Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
	}

	// Узел, чей выход позы воткнут в Output Pose, плюс сам пин. Возвращает false, если граф
	// не в ожидаемом виде (нет корня, нет входного пина, вход пуст или ветвится).
	bool FindPoseFeedingRoot(UEdGraph* AnimGraph, UAnimGraphNode_Root*& OutRoot,
		UEdGraphPin*& OutRootPosePin, UEdGraphPin*& OutSourcePin)
	{
		// Корень ищем сами, а НЕ через FBlueprintEditorUtils::GetAnimGraphRoot: там стоит
		// check(Roots.Num() == 1) (BlueprintEditorUtils.cpp:4269), то есть при неожиданном
		// содержимом графа коммандлет упал бы вместо внятного сообщения.
		TArray<UAnimGraphNode_Root*> Roots;
		AnimGraph->GetNodesOfClass<UAnimGraphNode_Root>(Roots);
		if (Roots.Num() != 1 || !Roots[0])
		{
			UE_LOG(LogPatchAnimBp, Error,
				TEXT("В графе анимации узлов Output Pose: %d (ожидался ровно один) — НЕ трогаю ассет."),
				Roots.Num());
			return false;
		}
		OutRoot = Roots[0];

		OutRootPosePin = FindPosePin(OutRoot, EGPD_Input);
		if (!OutRootPosePin)
		{
			UE_LOG(LogPatchAnimBp, Error, TEXT("У узла Output Pose нет входного пина позы."));
			return false;
		}

		if (OutRootPosePin->LinkedTo.Num() != 1)
		{
			UE_LOG(LogPatchAnimBp, Error,
				TEXT("Во вход Output Pose воткнуто связей: %d (ожидалась ровно одна). Граф не в том виде, ")
				TEXT("который я умею править, — НЕ трогаю ассет."),
				OutRootPosePin->LinkedTo.Num());
			return false;
		}

		OutSourcePin = OutRootPosePin->LinkedTo[0];
		return OutSourcePin != nullptr;
	}
}

int32 UPatchAnimBpCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	if (Switches.Contains(TEXT("verify")))
	{
		return VerifyMontageSlot();
	}
	if (Switches.Contains(TEXT("dump")))
	{
		return DumpAnimGraph();
	}
	if (Switches.Contains(TEXT("aim")))
	{
		return InsertAimPoseLayer();
	}
	return InsertMontageSlot();
}

int32 UPatchAnimBpCommandlet::InsertMontageSlot()
{
	UAnimBlueprint* AnimBP = LoadHumanoidAnimBp();
	if (!AnimBP)
	{
		return 1;
	}

	UEdGraph* AnimGraph = FindAnimGraph(AnimBP);
	if (!AnimGraph)
	{
		return 1;
	}

	UAnimGraphNode_Root* Root = nullptr;
	UEdGraphPin* RootPosePin = nullptr;
	UEdGraphPin* SourcePin = nullptr;
	if (!FindPoseFeedingRoot(AnimGraph, Root, RootPosePin, SourcePin))
	{
		return 1;
	}

	UEdGraphNode* SourceNode = SourcePin->GetOwningNode();

	// Идемпотентность: слот уже стоит перед Output Pose — второй не плодим.
	if (const UAnimGraphNode_Slot* ExistingSlot = Cast<UAnimGraphNode_Slot>(SourceNode))
	{
		if (ExistingSlot->Node.SlotName == FAnimSlotGroup::DefaultSlotName)
		{
			UE_LOG(LogPatchAnimBp, Display,
				TEXT("Слот монтажа '%s' уже стоит перед Output Pose — делать нечего, ассет не трогаю."),
				*ExistingSlot->Node.SlotName.ToString());
		}
		else
		{
			UE_LOG(LogPatchAnimBp, Warning,
				TEXT("Перед Output Pose уже стоит слот, но с другим именем: '%s' (ожидалось '%s'). ")
				TEXT("Ассет не трогаю — решай вручную, какое имя слота правильное."),
				*ExistingSlot->Node.SlotName.ToString(), *FAnimSlotGroup::DefaultSlotName.ToString());
		}
		return 0;
	}

	UE_LOG(LogPatchAnimBp, Display, TEXT("Сейчас в Output Pose воткнут: %s. Вставляю слот монтажа между ними."),
		*DescribeNode(SourceNode));

	// Создаём узел слота штатным помощником (EdGraph.h:273): Finalize() выдаёт узлу GUID и
	// создаёт пины по структуре FAnimNode_Slot. Он же следит, что узел не забыли доделать.
	UAnimGraphNode_Slot* SlotNode = nullptr;
	{
		FGraphNodeCreator<UAnimGraphNode_Slot> NodeCreator(*AnimGraph);
		SlotNode = NodeCreator.CreateNode(/*bSelectNewNode=*/false);
		SlotNode->Node.SlotName = FAnimSlotGroup::DefaultSlotName;
		NodeCreator.Finalize();
	}

	// Раскладка: слот встаёт между источником и Output Pose. Если между ними тесно, разводим
	// узлы — это косметика, чтобы граф читался, когда его откроют мышкой.
	const int32 MinGap = 300;
	if (Root->NodePosX - SourceNode->NodePosX < 2 * MinGap)
	{
		Root->NodePosX = SourceNode->NodePosX + 2 * MinGap;
	}
	SlotNode->NodePosX = SourceNode->NodePosX + MinGap;
	SlotNode->NodePosY = Root->NodePosY;

	UEdGraphPin* SlotInputPin = FindPosePin(SlotNode, EGPD_Input);
	UEdGraphPin* SlotOutputPin = FindPosePin(SlotNode, EGPD_Output);
	if (!SlotInputPin || !SlotOutputPin)
	{
		UE_LOG(LogPatchAnimBp, Error, TEXT("У созданного узла слота нет пинов позы (вход %s, выход %s)."),
			SlotInputPin ? TEXT("есть") : TEXT("НЕТ"), SlotOutputPin ? TEXT("есть") : TEXT("НЕТ"));
		return 1;
	}

	const UEdGraphSchema* Schema = AnimGraph->GetSchema();
	if (!Schema)
	{
		UE_LOG(LogPatchAnimBp, Error, TEXT("У графа анимации нет схемы — связи создать нечем."));
		return 1;
	}

	// Порядок важен: сначала рвём старую связь (источник висел прямо в Output Pose), только
	// потом собираем цепочку через слот. Иначе вход Output Pose, который держит одну связь,
	// разорвал бы её сам и порядок стал бы неявным.
	RootPosePin->BreakLinkTo(SourcePin);

	// Сорваться на полпути не страшно: ниже, при любом выходе с кодом 1, ассет НЕ сохраняется,
	// а правки живут только в памяти коммандлета — файл на диске остаётся прежним.
	if (!Schema->TryCreateConnection(SourcePin, SlotInputPin))
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("Не удалось соединить %s с входом слота — НЕ сохраняю, ассет на диске цел."),
			*DescribeNode(SourceNode));
		return 1;
	}
	if (!Schema->TryCreateConnection(SlotOutputPin, RootPosePin))
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("Не удалось соединить выход слота с Output Pose — НЕ сохраняю, ассет на диске цел."));
		return 1;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);

	FKismetEditorUtilities::CompileBlueprint(AnimBP);
	if (AnimBP->Status == BS_Error)
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("Блюпринт скомпилировался с ошибками — НЕ сохраняю, ассет на диске остаётся целым."));
		return 1;
	}

	const FString Filename = FPackageName::LongPackageNameToFilename(
		GAnimBpPackage, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(AnimBP->GetOutermost(), AnimBP, *Filename, SaveArgs))
	{
		UE_LOG(LogPatchAnimBp, Error, TEXT("SavePackage не сохранил %s."), *Filename);
		return 1;
	}

	UE_LOG(LogPatchAnimBp, Display,
		TEXT("ГОТОВО: слот монтажа '%s' вставлен между %s и Output Pose, блюпринт скомпилирован и сохранён (%s)."),
		*FAnimSlotGroup::DefaultSlotName.ToString(), *DescribeNode(SourceNode), *Filename);
	return 0;
}

int32 UPatchAnimBpCommandlet::VerifyMontageSlot()
{
	UAnimBlueprint* AnimBP = LoadHumanoidAnimBp();
	if (!AnimBP)
	{
		return 1;
	}

	UEdGraph* AnimGraph = FindAnimGraph(AnimBP);
	if (!AnimGraph)
	{
		return 1;
	}

	UAnimGraphNode_Root* Root = nullptr;
	UEdGraphPin* RootPosePin = nullptr;
	UEdGraphPin* SourcePin = nullptr;
	if (!FindPoseFeedingRoot(AnimGraph, Root, RootPosePin, SourcePin))
	{
		return 1;
	}

	UEdGraphNode* SourceNode = SourcePin->GetOwningNode();
	const UAnimGraphNode_Slot* SlotNode = Cast<UAnimGraphNode_Slot>(SourceNode);
	if (!SlotNode)
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("ПРОВЕРКА НЕ ПРОЙДЕНА: в Output Pose воткнут %s, а не слот монтажа — монтажи играть не будут."),
			*DescribeNode(SourceNode));
		return 1;
	}

	int32 Problems = 0;

	if (SlotNode->Node.SlotName != FAnimSlotGroup::DefaultSlotName)
	{
		UE_LOG(LogPatchAnimBp, Error, TEXT("ПРОВЕРКА: имя слота '%s', ожидалось '%s'."),
			*SlotNode->Node.SlotName.ToString(), *FAnimSlotGroup::DefaultSlotName.ToString());
		++Problems;
	}

	const UEdGraphPin* SlotInputPin = FindPosePin(SlotNode, EGPD_Input);
	if (!SlotInputPin || SlotInputPin->LinkedTo.Num() != 1 || !SlotInputPin->LinkedTo[0])
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("ПРОВЕРКА: во вход слота воткнуто связей: %d (ожидалась ровно одна — поза ходьбы). ")
			TEXT("Пустой вход слота = персонаж замрёт, когда монтаж не играет."),
			SlotInputPin ? SlotInputPin->LinkedTo.Num() : -1);
		++Problems;
	}
	else
	{
		UE_LOG(LogPatchAnimBp, Display, TEXT("ПРОВЕРКА: цепочка целая — %s → слот '%s' → Output Pose."),
			*DescribeNode(SlotInputPin->LinkedTo[0]->GetOwningNode()),
			*SlotNode->Node.SlotName.ToString());
	}

	if (Problems > 0)
	{
		UE_LOG(LogPatchAnimBp, Error, TEXT("ПРОВЕРКА НЕ ПРОЙДЕНА: замечаний %d."), Problems);
		return 1;
	}

	UE_LOG(LogPatchAnimBp, Display, TEXT("ПРОВЕРКА ПРОЙДЕНА: слот монтажа на месте, связи целы."));
	return 0;
}

int32 UPatchAnimBpCommandlet::InsertAimPoseLayer()
{
	// Поза прицеливания сделана моделлером только на 12 костях верха тела: корень и ноги в ней
	// неподвижны. Поэтому играть её полным телом нельзя — накладываем ПОСЛОЙНО от кости
	// C_Spine01 вверх, а ноги остаются от ходьбы.
	static const TCHAR* AimPoseObjectPath =
		TEXT("/Game/Characters/Shared/Humanoid/Anim_AimPistol_Humanoid.Anim_AimPistol_Humanoid");
	static const FName SpineBoneName(TEXT("C_Spine01"));
	static const FName AimWeightPropertyName(TEXT("AimBlendWeight"));

	UAnimBlueprint* AnimBP = LoadHumanoidAnimBp();
	if (!AnimBP)
	{
		return 1;
	}
	UEdGraph* AnimGraph = FindAnimGraph(AnimBP);
	if (!AnimGraph)
	{
		return 1;
	}

	// Слот монтажа должен уже стоять: поза встаёт ПЕРЕД ним, чтобы анимации выстрела и удара
	// (они играют через слот) перекрывали позу целиком, а не боролись с ней за верх тела.
	TArray<UAnimGraphNode_Slot*> SlotNodes;
	AnimGraph->GetNodesOfClass<UAnimGraphNode_Slot>(SlotNodes);
	if (SlotNodes.Num() != 1 || !SlotNodes[0])
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("Узлов слота монтажа в графе: %d (нужен ровно один). Сначала прогони основной ")
			TEXT("проход без ключей — он ставит слот. Ассет не трогаю."),
			SlotNodes.Num());
		return 1;
	}
	UAnimGraphNode_Slot* SlotNode = SlotNodes[0];

	UEdGraphPin* SlotInputPin = FindPosePin(SlotNode, EGPD_Input);
	if (!SlotInputPin || SlotInputPin->LinkedTo.Num() != 1 || !SlotInputPin->LinkedTo[0])
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("Во вход слота монтажа воткнуто связей: %d (ожидалась ровно одна — поза ходьбы). ")
			TEXT("Ассет не трогаю."),
			SlotInputPin ? SlotInputPin->LinkedTo.Num() : -1);
		return 1;
	}
	UEdGraphPin* LocomotionPin = SlotInputPin->LinkedTo[0];
	UEdGraphNode* LocomotionNode = LocomotionPin->GetOwningNode();

	// Идемпотентность: слой позы уже стоит перед слотом — второй не плодим.
	if (Cast<UAnimGraphNode_LayeredBoneBlend>(LocomotionNode))
	{
		UE_LOG(LogPatchAnimBp, Display,
			TEXT("Слой позы прицеливания уже стоит перед слотом монтажа — делать нечего, ассет не трогаю."));
		return 0;
	}

	UAnimSequenceBase* AimPose = LoadObject<UAnimSequenceBase>(nullptr, AimPoseObjectPath);
	if (!AimPose)
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("Поза прицеливания '%s' не найдена — ассет не трогаю. Импортирует её оператор."),
			AimPoseObjectPath);
		return 1;
	}

	// Силу наложения граф читает из переменной класса-родителя. Значит родителя надо сменить
	// ДО создания узла чтения переменной, иначе переменной ещё не существует и узел повиснет.
	if (AnimBP->ParentClass != UHumanoidAnimInstance::StaticClass())
	{
		UE_LOG(LogPatchAnimBp, Display, TEXT("Меняю родителя блюпринта на UHumanoidAnimInstance (был %s)."),
			AnimBP->ParentClass ? *AnimBP->ParentClass->GetName() : TEXT("(пусто)"));
		AnimBP->ParentClass = UHumanoidAnimInstance::StaticClass();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
		FKismetEditorUtilities::CompileBlueprint(AnimBP); // пересобрать класс, чтобы переменная стала видна
		if (AnimBP->Status == BS_Error)
		{
			UE_LOG(LogPatchAnimBp, Error,
				TEXT("После смены родителя блюпринт не компилируется — НЕ сохраняю, ассет на диске цел."));
			return 1;
		}
	}

	// Узел позы: SequenceEvaluator выдаёт ОДИН кадр анимации (поза, а не проигрывание).
	UAnimGraphNode_SequenceEvaluator* PoseNode = nullptr;
	{
		FGraphNodeCreator<UAnimGraphNode_SequenceEvaluator> NodeCreator(*AnimGraph);
		PoseNode = NodeCreator.CreateNode(/*bSelectNewNode=*/false);
		NodeCreator.Finalize();
	}
	PoseNode->SetAnimationAsset(AimPose);

	// Узел послойного смешивания: база — ходьба, слой — поза прицеливания, фильтр по кости.
	UAnimGraphNode_LayeredBoneBlend* BlendNode = nullptr;
	{
		FGraphNodeCreator<UAnimGraphNode_LayeredBoneBlend> NodeCreator(*AnimGraph);
		BlendNode = NodeCreator.CreateNode(/*bSelectNewNode=*/false);
		// Режим ставим ДО добавления слоя: AddPose кладёт запись либо в фильтры костей, либо в
		// маски смешивания — смотря какой режим стоит в этот момент.
		BlendNode->Node.BlendMode = ELayeredBoneBlendMode::BranchFilter;
		NodeCreator.Finalize();
	}
	// Добавляет слой целиком: позу, вес и запись фильтра, плюс перестраивает пины узла.
	BlendNode->AddPinToBlendByFilter();
	if (BlendNode->Node.LayerSetup.Num() != 1 || BlendNode->Node.BlendPoses.Num() != 1)
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("Узел послойного смешивания получил слоёв: фильтров %d, поз %d (ожидалось по одному). ")
			TEXT("НЕ сохраняю, ассет на диске цел."),
			BlendNode->Node.LayerSetup.Num(), BlendNode->Node.BlendPoses.Num());
		return 1;
	}
	{
		FBranchFilter Filter;
		Filter.BoneName = SpineBoneName;
		Filter.BlendDepth = 0; // 0 = кость и всё ниже по иерархии берутся из накладываемой позы
		BlendNode->Node.LayerSetup[0].BranchFilters.Empty();
		BlendNode->Node.LayerSetup[0].BranchFilters.Add(Filter);
	}

	// Узел чтения переменной силы наложения (обычная переменная — её видно, открыв граф).
	UK2Node_VariableGet* WeightNode = nullptr;
	{
		FGraphNodeCreator<UK2Node_VariableGet> NodeCreator(*AnimGraph);
		WeightNode = NodeCreator.CreateNode(/*bSelectNewNode=*/false);
		WeightNode->VariableReference.SetExternalMember(AimWeightPropertyName, UHumanoidAnimInstance::StaticClass());
		NodeCreator.Finalize();
	}

	// --- Поиск пинов. Позы ищем по типу, базу от слоя отличаем по имени свойства «BasePose»
	// (у немассивных свойств имя пина совпадает с именем поля). Вес — единственный
	// вещественный вход узла. ---
	UEdGraphPin* BlendBasePin = nullptr;
	UEdGraphPin* BlendLayerPin = nullptr;
	UEdGraphPin* BlendWeightPin = nullptr;
	for (UEdGraphPin* Pin : BlendNode->Pins)
	{
		if (!Pin || Pin->Direction != EGPD_Input)
		{
			continue;
		}
		if (UAnimationGraphSchema::IsLocalSpacePosePin(Pin->PinType))
		{
			if (Pin->PinName == TEXT("BasePose"))
			{
				BlendBasePin = Pin;
			}
			else
			{
				BlendLayerPin = Pin;
			}
		}
		else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Real
			|| Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Float
			|| Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Double)
		{
			BlendWeightPin = Pin;
		}
	}
	UEdGraphPin* BlendOutPin = FindPosePin(BlendNode, EGPD_Output);
	UEdGraphPin* PoseOutPin = FindPosePin(PoseNode, EGPD_Output);
	UEdGraphPin* WeightOutPin = WeightNode->FindPin(AimWeightPropertyName, EGPD_Output);

	if (!BlendBasePin || !BlendLayerPin || !BlendWeightPin || !BlendOutPin || !PoseOutPin || !WeightOutPin)
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("Не нашёл нужные пины (база %d, слой %d, вес %d, выход смешивания %d, выход позы %d, ")
			TEXT("выход переменной %d) — НЕ сохраняю, ассет на диске цел."),
			BlendBasePin ? 1 : 0, BlendLayerPin ? 1 : 0, BlendWeightPin ? 1 : 0,
			BlendOutPin ? 1 : 0, PoseOutPin ? 1 : 0, WeightOutPin ? 1 : 0);
		return 1;
	}

	// Раскладка: поза и вес слева-сверху от смешивания, само смешивание — между ходьбой и слотом.
	const int32 BlendX = (LocomotionNode->NodePosX + SlotNode->NodePosX) / 2;
	BlendNode->NodePosX = BlendX;
	BlendNode->NodePosY = SlotNode->NodePosY;
	PoseNode->NodePosX = BlendX - 320;
	PoseNode->NodePosY = SlotNode->NodePosY - 220;
	WeightNode->NodePosX = BlendX - 320;
	WeightNode->NodePosY = SlotNode->NodePosY - 60;

	const UEdGraphSchema* Schema = AnimGraph->GetSchema();
	if (!Schema)
	{
		UE_LOG(LogPatchAnimBp, Error, TEXT("У графа анимации нет схемы — связи создать нечем."));
		return 1;
	}

	// Сначала рвём «ходьба → слот», потом собираем цепочку через смешивание.
	SlotInputPin->BreakLinkTo(LocomotionPin);

	struct FLink { UEdGraphPin* A; UEdGraphPin* B; const TCHAR* What; };
	const FLink Links[] = {
		{ LocomotionPin, BlendBasePin,  TEXT("ходьба -> база смешивания") },
		{ PoseOutPin,    BlendLayerPin, TEXT("поза прицеливания -> слой смешивания") },
		{ WeightOutPin,  BlendWeightPin,TEXT("сила наложения -> вес слоя") },
		{ BlendOutPin,   SlotInputPin,  TEXT("смешивание -> слот монтажа") },
	};
	for (const FLink& Link : Links)
	{
		if (!Schema->TryCreateConnection(Link.A, Link.B))
		{
			UE_LOG(LogPatchAnimBp, Error, TEXT("Не удалось соединить: %s — НЕ сохраняю, ассет на диске цел."),
				Link.What);
			return 1;
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
	FKismetEditorUtilities::CompileBlueprint(AnimBP);
	if (AnimBP->Status == BS_Error)
	{
		UE_LOG(LogPatchAnimBp, Error,
			TEXT("Блюпринт скомпилировался с ошибками — НЕ сохраняю, ассет на диске остаётся целым."));
		return 1;
	}

	const FString Filename = FPackageName::LongPackageNameToFilename(
		GAnimBpPackage, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(AnimBP->GetOutermost(), AnimBP, *Filename, SaveArgs))
	{
		UE_LOG(LogPatchAnimBp, Error, TEXT("SavePackage не сохранил %s."), *Filename);
		return 1;
	}

	UE_LOG(LogPatchAnimBp, Display,
		TEXT("ГОТОВО: поза прицеливания накладывается от кости '%s' поверх ходьбы, сила наложения — ")
		TEXT("из переменной '%s'. Цепочка: %s -> послойное смешивание -> слот монтажа -> Output Pose. Сохранено (%s)."),
		*SpineBoneName.ToString(), *AimWeightPropertyName.ToString(), *DescribeNode(LocomotionNode), *Filename);
	return 0;
}

int32 UPatchAnimBpCommandlet::DumpAnimGraph()
{
	UAnimBlueprint* AnimBP = LoadHumanoidAnimBp();
	if (!AnimBP)
	{
		return 1;
	}

	TArray<UEdGraph*> AllGraphs;
	AnimBP->GetAllGraphs(AllGraphs);
	UE_LOG(LogPatchAnimBp, Display, TEXT("СРЕЗ %s: графов всего %d."), GAnimBpObjectPath, AllGraphs.Num());

	UEdGraph* AnimGraph = FindAnimGraph(AnimBP);
	if (!AnimGraph)
	{
		return 1;
	}

	// Стабильный порядок: два прогона сравниваются диффом, поэтому сортируем по координатам
	// узла и классу, а не полагаемся на порядок в массиве.
	TArray<UEdGraphNode*> Nodes = AnimGraph->Nodes;
	Nodes.Sort([](const UEdGraphNode& A, const UEdGraphNode& B)
	{
		if (A.NodePosX != B.NodePosX)
		{
			return A.NodePosX < B.NodePosX;
		}
		if (A.NodePosY != B.NodePosY)
		{
			return A.NodePosY < B.NodePosY;
		}
		return A.GetClass()->GetName() < B.GetClass()->GetName();
	});

	for (const UEdGraphNode* Node : Nodes)
	{
		if (!Node)
		{
			continue;
		}
		UE_LOG(LogPatchAnimBp, Display, TEXT("УЗЕЛ x=%d y=%d %s"),
			Node->NodePosX, Node->NodePosY, *DescribeNode(Node));

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin)
			{
				continue;
			}
			TArray<FString> Links;
			for (const UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked)
				{
					Links.Add(FString::Printf(TEXT("%s.%s"),
						*DescribeNode(Linked->GetOwningNode()), *Linked->PinName.ToString()));
				}
			}
			Links.Sort();
			UE_LOG(LogPatchAnimBp, Display, TEXT("    ПИН %s %s поза=%d -> %s"),
				Pin->Direction == EGPD_Input ? TEXT("вход ") : TEXT("выход"),
				*Pin->PinName.ToString(),
				UAnimationGraphSchema::IsLocalSpacePosePin(Pin->PinType) ? 1 : 0,
				Links.Num() > 0 ? *FString::Join(Links, TEXT(", ")) : TEXT("(не подключён)"));
		}
	}

	return 0;
}
