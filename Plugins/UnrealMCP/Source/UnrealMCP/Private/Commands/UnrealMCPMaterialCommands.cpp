#include "Commands/UnrealMCPMaterialCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "MaterialEditingLibrary.h"
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "GameFramework/Actor.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "IPythonScriptPlugin.h"
#include "Modules/ModuleManager.h"

FUnrealMCPMaterialCommands::FUnrealMCPMaterialCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("execute_python"))
    {
        return HandleExecutePython(Params);
    }
    else if (CommandType == TEXT("create_material"))
    {
        return HandleCreateMaterial(Params);
    }
    else if (CommandType == TEXT("add_material_expression"))
    {
        return HandleAddMaterialExpression(Params);
    }
    else if (CommandType == TEXT("connect_material_property"))
    {
        return HandleConnectMaterialProperty(Params);
    }
    else if (CommandType == TEXT("connect_material_expressions"))
    {
        return HandleConnectMaterialExpressions(Params);
    }
    else if (CommandType == TEXT("compile_material"))
    {
        return HandleCompileMaterial(Params);
    }
    else if (CommandType == TEXT("set_material_on_actor"))
    {
        return HandleSetMaterialOnActor(Params);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown material command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleExecutePython(const TSharedPtr<FJsonObject>& Params)
{
    FString Code;
    if (!Params->TryGetStringField(TEXT("code"), Code))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'code' parameter"));
    }

    // Check if Python plugin is loaded
    IPythonScriptPlugin* PythonPlugin = FModuleManager::GetModulePtr<IPythonScriptPlugin>("PythonScriptPlugin");
    if (!PythonPlugin)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Python plugin is not loaded. Enable the Python Editor Script Plugin in your project."));
    }

    // Execute the Python code
    TArray<FString> PythonOutput;
    bool bSuccess = PythonPlugin->ExecPythonCommand(*Code);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bSuccess);
    if (!bSuccess)
    {
        ResultObj->SetStringField(TEXT("error"), TEXT("Python execution failed. Check Unreal's Output Log for details."));
    }
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_name' parameter"));
    }

    FString Path = TEXT("/Game/Materials");
    Params->TryGetStringField(TEXT("path"), Path);

    bool bTwoSided = false;
    Params->TryGetBoolField(TEXT("two_sided"), bTwoSided);

    // Full asset path
    FString FullPath = Path / MaterialName;

    // Delete existing if present
    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        UEditorAssetLibrary::DeleteAsset(FullPath);
    }

    // Create material using asset tools
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();

    UObject* NewAsset = AssetTools.CreateAsset(MaterialName, Path, UMaterial::StaticClass(), Factory);
    UMaterial* Material = Cast<UMaterial>(NewAsset);

    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material"));
    }

    // Set two-sided if requested
    if (bTwoSided)
    {
        Material->TwoSided = true;
    }

    // Mark as modified
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("material_path"), FullPath);
    ResultObj->SetStringField(TEXT("material_name"), MaterialName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleAddMaterialExpression(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_name' parameter"));
    }

    FString ExpressionType;
    if (!Params->TryGetStringField(TEXT("expression_type"), ExpressionType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'expression_type' parameter"));
    }

    int32 PositionX = 0;
    int32 PositionY = 0;
    Params->TryGetNumberField(TEXT("position_x"), PositionX);
    Params->TryGetNumberField(TEXT("position_y"), PositionY);

    // Find the material
    FString MaterialPath = TEXT("/Game/Materials/") + MaterialName;
    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
    }

    // Create the expression based on type
    UMaterialExpression* NewExpression = nullptr;

    if (ExpressionType == TEXT("VertexColor"))
    {
        NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionVertexColor::StaticClass(), PositionX, PositionY);
    }
    else if (ExpressionType == TEXT("Constant"))
    {
        UMaterialExpressionConstant* ConstExpr = Cast<UMaterialExpressionConstant>(
            UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionConstant::StaticClass(), PositionX, PositionY));
        if (ConstExpr)
        {
            // Check for properties
            const TSharedPtr<FJsonObject>* Properties;
            if (Params->TryGetObjectField(TEXT("properties"), Properties))
            {
                double RValue = 0.0;
                if ((*Properties)->TryGetNumberField(TEXT("R"), RValue))
                {
                    ConstExpr->R = RValue;
                }
            }
            NewExpression = ConstExpr;
        }
    }
    else if (ExpressionType == TEXT("Constant3Vector") || ExpressionType == TEXT("VectorConstant"))
    {
        UMaterialExpressionConstant3Vector* VecExpr = Cast<UMaterialExpressionConstant3Vector>(
            UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionConstant3Vector::StaticClass(), PositionX, PositionY));
        if (VecExpr)
        {
            const TSharedPtr<FJsonObject>* Properties;
            if (Params->TryGetObjectField(TEXT("properties"), Properties))
            {
                double R = 0, G = 0, B = 0;
                (*Properties)->TryGetNumberField(TEXT("R"), R);
                (*Properties)->TryGetNumberField(TEXT("G"), G);
                (*Properties)->TryGetNumberField(TEXT("B"), B);
                VecExpr->Constant = FLinearColor(R, G, B);
            }
            NewExpression = VecExpr;
        }
    }
    else if (ExpressionType == TEXT("Constant4Vector"))
    {
        UMaterialExpressionConstant4Vector* VecExpr = Cast<UMaterialExpressionConstant4Vector>(
            UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionConstant4Vector::StaticClass(), PositionX, PositionY));
        if (VecExpr)
        {
            const TSharedPtr<FJsonObject>* Properties;
            if (Params->TryGetObjectField(TEXT("properties"), Properties))
            {
                double R = 0, G = 0, B = 0, A = 1;
                (*Properties)->TryGetNumberField(TEXT("R"), R);
                (*Properties)->TryGetNumberField(TEXT("G"), G);
                (*Properties)->TryGetNumberField(TEXT("B"), B);
                (*Properties)->TryGetNumberField(TEXT("A"), A);
                VecExpr->Constant = FLinearColor(R, G, B, A);
            }
            NewExpression = VecExpr;
        }
    }
    else if (ExpressionType == TEXT("TextureSample"))
    {
        NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionTextureSample::StaticClass(), PositionX, PositionY);
    }
    else if (ExpressionType == TEXT("VectorParameter"))
    {
        UMaterialExpressionVectorParameter* ParamExpr = Cast<UMaterialExpressionVectorParameter>(
            UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionVectorParameter::StaticClass(), PositionX, PositionY));
        if (ParamExpr)
        {
            const TSharedPtr<FJsonObject>* Properties;
            if (Params->TryGetObjectField(TEXT("properties"), Properties))
            {
                FString ParamName;
                if ((*Properties)->TryGetStringField(TEXT("ParameterName"), ParamName))
                {
                    ParamExpr->ParameterName = FName(*ParamName);
                }
                double R = 0, G = 0, B = 0, A = 1;
                (*Properties)->TryGetNumberField(TEXT("R"), R);
                (*Properties)->TryGetNumberField(TEXT("G"), G);
                (*Properties)->TryGetNumberField(TEXT("B"), B);
                (*Properties)->TryGetNumberField(TEXT("A"), A);
                ParamExpr->DefaultValue = FLinearColor(R, G, B, A);
            }
            NewExpression = ParamExpr;
        }
    }
    else if (ExpressionType == TEXT("ScalarParameter"))
    {
        UMaterialExpressionScalarParameter* ParamExpr = Cast<UMaterialExpressionScalarParameter>(
            UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionScalarParameter::StaticClass(), PositionX, PositionY));
        if (ParamExpr)
        {
            const TSharedPtr<FJsonObject>* Properties;
            if (Params->TryGetObjectField(TEXT("properties"), Properties))
            {
                FString ParamName;
                if ((*Properties)->TryGetStringField(TEXT("ParameterName"), ParamName))
                {
                    ParamExpr->ParameterName = FName(*ParamName);
                }
                double DefaultValue = 0.0;
                if ((*Properties)->TryGetNumberField(TEXT("DefaultValue"), DefaultValue))
                {
                    ParamExpr->DefaultValue = DefaultValue;
                }
            }
            NewExpression = ParamExpr;
        }
    }
    else if (ExpressionType == TEXT("Multiply"))
    {
        NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionMultiply::StaticClass(), PositionX, PositionY);
    }
    else if (ExpressionType == TEXT("Add"))
    {
        NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionAdd::StaticClass(), PositionX, PositionY);
    }
    else if (ExpressionType == TEXT("TextureCoordinate"))
    {
        NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionTextureCoordinate::StaticClass(), PositionX, PositionY);
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown expression type: %s"), *ExpressionType));
    }

    if (!NewExpression)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material expression"));
    }

    // Find the node ID (index in the expressions array)
    int32 NodeId = Material->GetExpressions().Find(NewExpression);

    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetNumberField(TEXT("node_id"), NodeId);
    ResultObj->SetStringField(TEXT("expression_type"), ExpressionType);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleConnectMaterialProperty(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_name' parameter"));
    }

    int32 NodeId;
    if (!Params->TryGetNumberField(TEXT("node_id"), NodeId))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
    }

    FString OutputName;
    Params->TryGetStringField(TEXT("output_name"), OutputName);

    FString MaterialProperty;
    if (!Params->TryGetStringField(TEXT("material_property"), MaterialProperty))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_property' parameter"));
    }

    // Find the material
    FString MaterialPath = TEXT("/Game/Materials/") + MaterialName;
    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
    }

    // Get the expression by index
    const auto Expressions = Material->GetExpressions();
    if (NodeId < 0 || NodeId >= Expressions.Num())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid node_id: %d"), NodeId));
    }

    UMaterialExpression* Expression = Expressions[NodeId];

    // Map string to EMaterialProperty
    EMaterialProperty Property;
    if (MaterialProperty == TEXT("BaseColor"))
    {
        Property = MP_BaseColor;
    }
    else if (MaterialProperty == TEXT("Metallic"))
    {
        Property = MP_Metallic;
    }
    else if (MaterialProperty == TEXT("Roughness"))
    {
        Property = MP_Roughness;
    }
    else if (MaterialProperty == TEXT("Normal"))
    {
        Property = MP_Normal;
    }
    else if (MaterialProperty == TEXT("EmissiveColor"))
    {
        Property = MP_EmissiveColor;
    }
    else if (MaterialProperty == TEXT("Opacity"))
    {
        Property = MP_Opacity;
    }
    else if (MaterialProperty == TEXT("OpacityMask"))
    {
        Property = MP_OpacityMask;
    }
    else if (MaterialProperty == TEXT("AmbientOcclusion"))
    {
        Property = MP_AmbientOcclusion;
    }
    else if (MaterialProperty == TEXT("Specular"))
    {
        Property = MP_Specular;
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown material property: %s"), *MaterialProperty));
    }

    // Connect using MaterialEditingLibrary
    bool bSuccess = UMaterialEditingLibrary::ConnectMaterialProperty(Expression, OutputName, Property);

    if (!bSuccess)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to connect material property"));
    }

    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleConnectMaterialExpressions(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_name' parameter"));
    }

    int32 SourceNodeId, TargetNodeId;
    if (!Params->TryGetNumberField(TEXT("source_node_id"), SourceNodeId))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_node_id' parameter"));
    }
    if (!Params->TryGetNumberField(TEXT("target_node_id"), TargetNodeId))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_node_id' parameter"));
    }

    FString SourceOutput, TargetInput;
    Params->TryGetStringField(TEXT("source_output"), SourceOutput);
    Params->TryGetStringField(TEXT("target_input"), TargetInput);

    // Find the material
    FString MaterialPath = TEXT("/Game/Materials/") + MaterialName;
    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
    }

    // Get the expressions by index
    const auto Expressions = Material->GetExpressions();
    if (SourceNodeId < 0 || SourceNodeId >= Expressions.Num())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid source_node_id: %d"), SourceNodeId));
    }
    if (TargetNodeId < 0 || TargetNodeId >= Expressions.Num())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Invalid target_node_id: %d"), TargetNodeId));
    }

    UMaterialExpression* SourceExpression = Expressions[SourceNodeId];
    UMaterialExpression* TargetExpression = Expressions[TargetNodeId];

    // Connect using MaterialEditingLibrary
    bool bSuccess = UMaterialEditingLibrary::ConnectMaterialExpressions(SourceExpression, SourceOutput, TargetExpression, TargetInput);

    if (!bSuccess)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to connect material expressions"));
    }

    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCompileMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_name' parameter"));
    }

    // Find the material
    FString MaterialPath = TEXT("/Game/Materials/") + MaterialName;
    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
    }

    // Recompile the material
    UMaterialEditingLibrary::RecompileMaterial(Material);

    // Save the material
    UEditorAssetLibrary::SaveAsset(MaterialPath);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleSetMaterialOnActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
    }

    int32 SlotIndex = 0;
    Params->TryGetNumberField(TEXT("slot_index"), SlotIndex);

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);

    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Load the material
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
    }

    // Find mesh component
    UMeshComponent* MeshComponent = TargetActor->FindComponentByClass<UMeshComponent>();
    if (!MeshComponent)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Actor has no mesh component"));
    }

    // Set the material
    MeshComponent->SetMaterial(SlotIndex, Material);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("actor_name"), ActorName);
    ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    return ResultObj;
}
