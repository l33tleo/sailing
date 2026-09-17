#include "FjordOceanBodyActor.h"

UFjordOceanBodyComponent::UFjordOceanBodyComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAffectsLandscape = false;
}

AFjordOceanBodyActor::AFjordOceanBodyActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WaterBodyOceanComponentClass = UFjordOceanBodyComponent::StaticClass();
}
