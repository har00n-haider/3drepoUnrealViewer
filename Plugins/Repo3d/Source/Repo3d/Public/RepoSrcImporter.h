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
#include "Repo3d.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/AssetManager.h"
#include "Engine/ObjectLibrary.h"
#include "Misc/DefaultValueHelper.h"
#include "ProceduralMeshComponent.h"
#include "RepoSupermeshActor.h"
#include "Json.h"
#include "RepoWebRequestManager.h"
#include "RepoWebRequestHelpers.h"
#include "Http.h"
#include "HttpModule.h"
#include "RepoSrcImporter.generated.h"

DECLARE_DELEGATE(RepoSrcImportersCompleted)

/*
 * The URepoSrcImporter handles an srcAssets.json file that instructs Unreal
 * how to load a model as a set of SRC files.
 * This class mainly drives a set of RepoSrcAssetImporter instances, that 
 * actually process the files.
 * This class is a UObject as it will handle the lifetime of the actor
 * and materials.
 */
UCLASS()
class REPO3D_API URepoSrcImporter : public UObject
{
	GENERATED_BODY()

	TSharedPtr<RepoWebRequestManager> manager;
	TArray<TSharedRef<class RepoSrcAssetImporter>> importers;

	UPROPERTY()
	ARepoSupermeshActor* actor;
	UPROPERTY()
	UMaterialInterface* materialOpaque;
	UPROPERTY()
	UMaterialInterface* materialTranslucent;

public:
	URepoSrcImporter()
	{
	}

	void SetWebManager(TSharedRef<RepoWebRequestManager> Manager)
	{
		this->manager = Manager;
	}

	void SetActor(ARepoSupermeshActor* Actor)
	{
		this->actor = Actor;
	}

	void SetTranslucentMaterial(UMaterialInterface* translucent)
	{
		this->materialTranslucent = translucent;
	}

	void SetOpaqueMaterial(UMaterialInterface* opaque)
	{
		this->materialOpaque = opaque;
	}

	void RequestRevision(FString teamspace, FString model, FString revision);

	RepoSrcImportersCompleted OnComplete;

	void BeginDestroy() override;

private:
	void AssetsRequestCompleted(TSharedPtr<RepoWebResponse> Result);
	void HandleAssets(const FString& string);
	void HandleCompleted(TSharedRef<RepoSrcAssetImporter> importer);
	void HandleModelSettings(TSharedRef<RepoWebRequestHelpers::ModelSettings> Settings);
};

/**
 * RepoSrcImporter imports a single SRC file into a ProceduralMeshComponent, 
 * dynamically created and attached to the pre-specified Actor.
 */
class REPO3D_API RepoSrcAssetImporter
{
private:
	TSharedPtr<RepoWebRequestManager> manager;

	TWeakObjectPtr<ARepoSupermeshActor> actor;
	UMaterialInterface* materialOpaque;
	UMaterialInterface* materialTranslucent;

	TSharedPtr<FJsonObject> header;
	TSharedPtr<FJsonObject> indexViews;
	TSharedPtr<FJsonObject> attributeViews;
	TSharedPtr<FJsonObject> bufferViews;
	TSharedPtr<FJsonObject> bufferChunks;
	TSharedPtr<FJsonObject> mappings;

	const uint8* buffer;
	int numSubmeshes;
	TArray<uint32> LocalToActorSubmeshMap;

	struct Material
	{
		FLinearColor diffuse;
		FLinearColor specular;
		float transparency;
	};

	TMap<FString, Material> mappingMaterials;
	bool hasTransparency;

	uint32 mappingsRequestTime;
	uint32 srcRequestTime;

public:
	RepoSrcAssetImporter(TSharedPtr<RepoWebRequestManager> manager) :
		manager(manager),
		materialOpaque(nullptr),
		materialTranslucent(nullptr),
		Bounds(ForceInit)
	{
	}

	void SetActor(TWeakObjectPtr<ARepoSupermeshActor> Actor)
	{
		this->actor = Actor;
	}

	void SetMaterialPrototype(UMaterialInterface* opaque, UMaterialInterface* translucent)
	{
		this->materialOpaque = opaque;
		this->materialTranslucent = translucent;
	}

	void SetOffset(FVector v);

	TBaseDelegate<void> OnComplete;

	void SetUri(FString uri)
	{
		Uri = uri;
	}

	void RequestSrc();

	static void TransformCoordinateSystem(TArray<FVector>& array); // from Unity to Unreal
	static FVector TransformCoordinateSystem(FVector v);

	FString Uri;
	FVector Offset;
	FBox Bounds;

private:
	void SrcRequestCompleted(TSharedPtr<RepoWebResponse> Result);
	bool MappingRequestCompleted(TSharedPtr<RepoWebResponse> Result);
	void HandleMapping(const FString& string);
	void HandleSrc(const TArray<uint8>& src);
	void ResolveIndices(const FString& viewName, TArray<int32>& array);
	template <typename T>
	void ResolveAttribute(const FString& viewName, TArray<T>& array);
	void GenerateSupermeshMapIndices(TArray<float>& ids, TArray<FVector2D>& uvs);
	void GenerateTriangleIdMap(TArray<int>& triangles, TArray<float>& ids, TArray<int>& triangleIdMap);

	FLinearColor ParseJsonColour(const FString& field)
	{
		FJsonSerializableArray array;
		field.ParseIntoArrayWS(array);
		FLinearColor colour;
		FDefaultValueHelper::ParseFloat(array[0], colour.R);
		FDefaultValueHelper::ParseFloat(array[1], colour.G);
		FDefaultValueHelper::ParseFloat(array[2], colour.B);
		return colour;
	}
};
