#pragma once

#include "CoreMinimal.h"
#include "WaterBodyOceanActor.h"
#include "WaterBodyOceanComponent.h"
#include "FjordOceanBodyActor.generated.h"

/**
 * UWaterBodyOceanComponent::CollisionExtents styrer den FAKTISKE kollisjonsboksen som brukes til
 * oppdrift/overlap-deteksjon (alltid i verdensrom, uavhengig av OceanExtents/aktør-skalering — se
 * UWaterBodyOceanComponent::OnUpdateBody). Feltet er `protected` med `friend class AWaterBodyOcean`,
 * så kun den eksakte klassen (ikke underklasser av aktøren) har C++-tilgang, og settefunksjonen
 * (SetCollisionExtents) er kun tilgjengelig i editoren. Protected-arv gir derimot en UNDERKLASSE AV
 * KOMPONENTEN tilgang uten disse begrensningene — det er poenget med denne klassen.
 */
UCLASS()
class SAILING_API UFjordOceanBodyComponent : public UWaterBodyOceanComponent
{
	GENERATED_BODY()

public:
	/** bAffectsLandscape settes til false HER (konstruktør), ikke i en post-spawn-setter: Water-
	 *  editor-modulens GEngine->OnLevelActorAdded()-lytter kjører SYNKRONT under selve SpawnActor()-
	 *  kallet, altså FØR noen kode etter SpawnActor() rekker å kjøre. Uten dette trigger enhver
	 *  registrering av vann-aktøren en automatisk "water brush manager"-spawn (+ en modal "Insert
	 *  New Landscape Edit Layer"-dialog) siden nivået har et (irrelevant, dekorativt) Landscape —
	 *  noe som viste seg å henge en automatisert PIE-økt uten en bruker til å klikke bort dialogen.
	 */
	UFjordOceanBodyComponent(const FObjectInitializer& ObjectInitializer);

	/** Setter CollisionExtents (verdensrom-halvutstrekning) i spillkode, uten editor-avhengighet. */
	void SetRuntimeCollisionExtents(const FVector& NewExtents)
	{
		CollisionExtents = NewExtents;
	}

	/** Setter OceanExtents (havets fulle VISUELLE utstrekning i verdensrom) i spillkode. Samme
	 *  protected/editor-only-begrensning som CollisionExtents. Aktør-skalering kan IKKE brukes i
	 *  stedet: motoren deler OceanExtents på komponentskalaen (OceanExtentScaled) nettopp for at
	 *  havet skal beholde samme verdensstørrelse uansett skala. Må settes FØR UpdateAll(). */
	void SetRuntimeOceanExtents(const FVector2D& NewExtents)
	{
		OceanExtents = NewExtents;
	}
};

/**
 * AWaterBodyOcean-underklasse som tvinger motoren til å bruke UFjordOceanBodyComponent i stedet for
 * standard UWaterBodyOceanComponent. WaterBodyOceanComponentClass leses av AWaterBody::InitializeBody()
 * (kalt fra PostInitProperties(), dvs. ETTER hele konstruktør-kjeden) — å sette den her i konstruktøren
 * er dermed tidsnok til å påvirke hvilken komponentklasse som faktisk opprettes.
 */
UCLASS()
class SAILING_API AFjordOceanBodyActor : public AWaterBodyOcean
{
	GENERATED_BODY()

public:
	AFjordOceanBodyActor(const FObjectInitializer& ObjectInitializer);
};
