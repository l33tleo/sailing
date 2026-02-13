#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Material-related MCP commands
 * Handles material creation, expression nodes, and connections
 */
class UNREALMCP_API FUnrealMCPMaterialCommands
{
public:
    FUnrealMCPMaterialCommands();

    // Handle material commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Python execution
    TSharedPtr<FJsonObject> HandleExecutePython(const TSharedPtr<FJsonObject>& Params);

    // Material commands
    TSharedPtr<FJsonObject> HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddMaterialExpression(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleConnectMaterialProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleConnectMaterialExpressions(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCompileMaterial(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMaterialOnActor(const TSharedPtr<FJsonObject>& Params);
};
