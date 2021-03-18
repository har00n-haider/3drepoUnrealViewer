/*
 *	Copyright (C) 2020 3D Repo Ltd
 *
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU Affero General Public License as
 *	published by the Free Software Foundation, either version 3 of the
 *	License, or (at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU Affero General Public License for more details.
 *
 *	You should have received a copy of the GNU Affero General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/UnitConversion.h"
#include "RepoStaticSupermeshActor.h"
#include "RepoInterfaces.h"
#include "RepoSupermeshMapComponent.h"
#include "RepoSupermeshActor.generated.h"

class IAssetTools; // Forward declaration for the static conversion methods. This is not used at runtime.

UCLASS()
class REPO3D_API ARepoSupermeshActor : public AActor, public IRepoTraceable
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Transform Component")
	USceneComponent* Transform;

	// Contains the Id's of the submeshes/objects within this actor. The UV1 attribute of the supermesh geometries contains indices into this array.
	UPROPERTY(AdvancedDisplay)
	TArray<FString> IdMap;

public:
	UPROPERTY(VisibleAnywhere, Category = "3DRepo Model Data")
	FString Teamspace;

	UPROPERTY(VisibleAnywhere, Category = "3DRepo Model Data")
	FString Model;

	UPROPERTY(VisibleAnywhere, Category = "3DRepo Model Data")
	FString Revision;

	UPROPERTY(VisibleAnywhere, Category = "3DRepo Model Data")
	EUnit Units;

	// A SupermeshMap Component that stores the diffuse colours of each subobject. To add additional parameters, subclass ARepoSupermeshActor.
	UPROPERTY(VisibleAnywhere, Category = "3DRepo Supermeshing Data")
	URepoSupermeshMapComponent* DiffuseMap;

public:	
	// Sets default values for this actor's properties
	ARepoSupermeshActor();

	UProceduralMeshComponent* AddProceduralMesh();

#if WITH_EDITOR
	ARepoStaticSupermeshActor* AddStaticSupermeshActor();

	// This base actor includes functionality to convert its transient Unreal types into their static equivalents.
	void ConvertToStaticHierarchy();

	// Converts the transient texture Texture into an asset and saves it into the Package provided. Returns the static instance
	// that can be directly applied to a material.
	UTexture2D* ConvertToStaticTexture(UTexture2D* Texture, IAssetTools& AssetTools, UPackage* Package);
#endif

	TArray<FString>& GetSubmeshMap();

	virtual FString GetMeshIdFromFaceIndex(TWeakObjectPtr<class UPrimitiveComponent> Component, int32 FaceIndex);
	virtual TWeakObjectPtr<ARepoSupermeshActor> GetActor();

	// Sets the Parameter on all Dynamic Materials under this ARepoSupermeshActor. This will operate on both static
	// and transient types (before and after the conversion to a static hierarchy).
	void SetMaterialsTextureParameter(FName ParameterName, UTexture* Value);

	// When the hierarchy is created at runtime, this map is used to dereference the Face Indices returned by
	// an FHitResult (which will return ARepoSupermeshActor as the Actor). 
	// This is not a UProperty, because it only has to survive as long as the Procedural Meshes; the maps will be
	// distributed to the ARepoStaticSupermeshActors when the scene hierarchy is baked.
	TMap<UPrimitiveComponent*, TArray<int>> MeshComponentTriangleMaps;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
