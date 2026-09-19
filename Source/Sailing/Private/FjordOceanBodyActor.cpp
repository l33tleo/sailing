#include "FjordOceanBodyActor.h"

UFjordOceanBodyComponent::UFjordOceanBodyComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAffectsLandscape = false;
	// Havets render-bounds (CalcBounds) sentreres ellers på SavedZoneLocation, som bare skrives av
	// editor-kode ETTER første meshbygging — for et hav spawnet i spillet er den (0,0) når CPU-
	// vannmeshen bygges, så flisene dekket bare ±halv sone rundt verdens origo (X ≥ −12 km,
	// Y ≥ −20 km): ved Nesøya og i søndre fjord ble vannflaten aldri tegnet. Havaktøren spawnes
	// på sonens senter, så bounds rundt aktøren selv er riktige. Må settes i konstruktøren: den
	// første meshbyggingen kan skje allerede under SpawnActor().
	bCenterOnWaterZone = false;
}

AFjordOceanBodyActor::AFjordOceanBodyActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WaterBodyOceanComponentClass = UFjordOceanBodyComponent::StaticClass();
}
