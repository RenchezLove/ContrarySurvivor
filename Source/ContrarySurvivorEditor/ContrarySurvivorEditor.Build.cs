// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

// Editor-модуль (ADR-048): программная генерация WBP-ассетов (Widget Blueprint) для Рината —
// коммандлет строит дерево виджетов кодом, Ринат дальше правит стиль мышкой в дизайнере.
// В игру (Runtime/Android-пак) модуль НЕ попадает: Type=Editor в .uproject.
public class ContrarySurvivorEditor : ModuleRules
{
	public ContrarySurvivorEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		// UMG — рантайм-виджеты (UWidgetTree, UCanvasPanel, UBorder, UTextBlock);
		// UMGEditor — UWidgetBlueprint; UnrealEd — FKismetEditorUtilities (создание/компиляция BP)
		// и FBlueprintEditorUtils (правка графов); AssetRegistry — регистрация нового ассета;
		// ContrarySurvivor — родительские C++-классы виджетов.
		// Для правки графа анимационного блюпринта (PatchAnimBpCommandlet, Build 1.1):
		// AnimGraph — редакторные узлы UAnimGraphNode_Slot/_Root и UAnimationGraphSchema;
		// AnimGraphRuntime — структуры самих узлов анимации (FAnimNode_Slot);
		// BlueprintGraph — UEdGraphSchema_K2::GN_AnimGraph, каноническое имя графа анимации.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UMG", "UMGEditor", "UnrealEd", "AssetRegistry", "Slate", "SlateCore", "ContrarySurvivor",
			"AnimGraph", "AnimGraphRuntime", "BlueprintGraph"
		});

		// Заголовки геймплей-модуля лежат подпапками вне его Public/ и включаются по конвенции
		// проекта «ContrarySurvivor/<Subdir>/Header.h» относительно Source/ — добавляем Source/.
		PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, ".."));
	}
}
