// Copyright (C) 2020 3D Repo Ltd

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/Texture2D.h"
#include "RepoSupermeshMapComponent.generated.h"

class IAssetTools;
class ARepoSupermeshActor;
class UMaterialInstanceDynamic;

/*
* The URepoSupermeshMapComponent maintains a map of parameters for each each object in a Supermesh Actor, allowing user code to update them by Id.
* This component is responsible for keeping track of the values, maintaining the underlying representation, and updating the settings in the 
* ARepoSupermeshActor's hierarchy. 
* The implementation uses texture maps, which are sampled by instances of Dynamic Materials.
*/
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class REPO3D_API URepoSupermeshMapComponent : public UActorComponent
{
	GENERATED_BODY()

	// A Texture applied to each instanced material on an ARepoSupermeshActor's rendering components.
	UPROPERTY(VisibleAnywhere, Category = "3DRepo Supermeshing Data")
	UTexture2D* Texture;

	// The parameter values belonging to the object Ids contained in this component. The indexing is the same the map returned by Actor::GetSubmeshMap()
	UPROPERTY(AdvancedDisplay)
	TArray<FVector4> Parameters;

public:
	// The Dynamic Instanced Material parameter that the Texture is assigned to.
	UPROPERTY(VisibleAnywhere, Category = "3DRepo Supermeshing Data")
	FName ParameterName;

	ARepoSupermeshActor* GetActor();

	// Sets default values for this component's properties
	URepoSupermeshMapComponent();

	void SetParameter(int Id, FVector4 Value);
	void SetParameter(FString& Id, FVector4 Value);
	FVector4 GetParameter(FString& Id);
	FVector4 GetParameter(int Id);

	// Updates the Texture with the current parameters. At run-time, this will be done automatically on demand.
	void UpdateTexture();
	void ApplyTextureToMaterials();
	void ApplyTextureToMaterials(UMaterialInstanceDynamic* material);

private:
	void CreateTexture();
	int GetSupermeshMapSize();

	bool IsMapDirty; // At runtime, if a parameter changes within a frame the texture should be updated. At design time it is assumed the update will be manual.

#if WITH_EDITOR
public:
	// Called when the ARepoSupermeshActor is being transformed to use static types. This method will save the map to an Asset
	// and update the Actor's materials.
	void ConvertToStaticTexture(IAssetTools& AssetTools, UPackage* Package);
#endif

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
		
};
