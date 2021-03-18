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

#include "RepoSupermeshActor.h"
#include "RepoSupermeshMapComponent.h"
#include <AssetRegistryModule.h>
#if WITH_EDITOR 
#include <AssetToolsModule.h>
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshConversion.h"
#include "RepoStaticSupermeshActor.h"
#endif

// Sets default values
ARepoSupermeshActor::ARepoSupermeshActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	Transform = CreateDefaultSubobject<USceneComponent>(FName("Transform"));
	SetRootComponent(Transform);

	DiffuseMap = CreateDefaultSubobject<URepoSupermeshMapComponent>(FName("DiffuseMap"));
	DiffuseMap->ParameterName = FName("DiffuseMap");
}

// Called when the game starts or when spawned
void ARepoSupermeshActor::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ARepoSupermeshActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

#pragma optimize("", off)

UProceduralMeshComponent* ARepoSupermeshActor::AddProceduralMesh()
{
	auto component = NewObject<UProceduralMeshComponent>(this, UProceduralMeshComponent::StaticClass());

	UE_LOG(LogTemp, Log, TEXT("Created Procedural Mesh %s"), *component->GetName());

	component->OnComponentCreated();
	component->RegisterComponent();
	component->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

	RootComponent->Modify();
	component->Modify();

	return component;
}

#if WITH_EDITOR
ARepoStaticSupermeshActor* ARepoSupermeshActor::AddStaticSupermeshActor()
{
	auto actor = (ARepoStaticSupermeshActor*)this->GetWorld()->SpawnActor(ARepoStaticSupermeshActor::StaticClass());
	actor->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform);
	actor->SetPrimarySupermeshActor(this);
	return actor;
}

void ARepoSupermeshActor::ConvertToStaticHierarchy()
{
	// This function is based on the code in ProceduralMeshComponentDetails.cpp (c) Epic Games.

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	TArray<UProceduralMeshComponent*> MeshComponents;
	TArray<URepoSupermeshMapComponent*> MapComponents;
	TArray<FName> ManagedMaps;

	for (auto component : GetComponents())
	{
		if (component->IsA<URepoSupermeshMapComponent>())
		{
			MapComponents.Add(Cast<URepoSupermeshMapComponent>(component));
		}
	}

	for (size_t i = 0; i < RootComponent->GetNumChildrenComponents(); i++)
	{
		auto component = RootComponent->GetChildComponent(i);
		if (component->IsA<UProceduralMeshComponent>())
		{
			MeshComponents.Add(Cast<UProceduralMeshComponent>(component));
		}
	}

	for (auto component : MapComponents) // Convert the maps first so we don't re-bake the textures later
	{
		FString PackageName = FString(TEXT("/Game/Meshes/")) + FString(TEXT("ProcMap")) + component->ParameterName.ToString();
		FString Name;
		AssetToolsModule.Get().CreateUniqueAssetName(PackageName, TEXT(""), PackageName, Name);
		UPackage* Package = CreatePackage(NULL, *PackageName);
		check(Package);

		component->ConvertToStaticTexture(AssetToolsModule.Get(), Package);

		ManagedMaps.Add(component->ParameterName);
	}

	for (auto ProcMeshComp : MeshComponents)
	{
		UE_LOG(LogTemp, Log, TEXT("Converting ProceduralMeshComponent %s"), *(ProcMeshComp->GetName()));

		FString NewNameSuggestion = FString(TEXT("ProcMesh"));
		FString PackageName = FString(TEXT("/Game/Meshes/")) + NewNameSuggestion;
		FString Name;
		AssetToolsModule.Get().CreateUniqueAssetName(PackageName, TEXT(""), PackageName, Name);

		FMeshDescription MeshDescription = BuildMeshDescription(ProcMeshComp);

		// If we got some valid data.
		if (MeshDescription.Polygons().Num() > 0)
		{
			// Then find/create it.
			UPackage* Package = CreatePackage(NULL, *PackageName);
			check(Package);

			// Create StaticMesh object
			UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, *Name, RF_Public | RF_Standalone);

			StaticMesh->LODForCollision = 0;
			StaticMesh->InitResources();
			StaticMesh->LightingGuid = FGuid::NewGuid();

			// Add source to new StaticMesh
			FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
			SrcModel.BuildSettings.bRecomputeNormals = false;
			SrcModel.BuildSettings.bRecomputeTangents = true;
			SrcModel.BuildSettings.bRemoveDegenerates = false;
			SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
			SrcModel.BuildSettings.bUseFullPrecisionUVs = true;
			SrcModel.BuildSettings.bGenerateLightmapUVs = true;
			SrcModel.BuildSettings.SrcLightmapIndex = 0;
			SrcModel.BuildSettings.DstLightmapIndex = 3; // Take care not to override any of our own UVs
			StaticMesh->CreateMeshDescription(0, MoveTemp(MeshDescription));
			StaticMesh->CommitMeshDescription(0);

			//// SIMPLE COLLISION
			//if (!ProcMeshComp->bUseComplexAsSimpleCollision)
			{
				StaticMesh->CreateBodySetup();
				UBodySetup* NewBodySetup = StaticMesh->BodySetup;
				NewBodySetup->BodySetupGuid = FGuid::NewGuid();
				NewBodySetup->bGenerateMirroredCollision = false;
				NewBodySetup->bDoubleSidedGeometry = true;
				NewBodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
				NewBodySetup->CreatePhysicsMeshes();
			}

			//// MATERIALS
			TArray<UMaterialInterface*> Materials;
			const int32 NumSections = ProcMeshComp->GetNumSections();
			for (int32 SectionIdx = 0; SectionIdx < NumSections; SectionIdx++)
			{
				FProcMeshSection* ProcSection =
					ProcMeshComp->GetProcMeshSection(SectionIdx);
				UMaterialInterface* Material = ProcMeshComp->GetMaterial(SectionIdx);
				Materials.Add(Material);
			}

			TSet<UMaterialInterface*> UniqueMaterials;
			for (auto* Material : Materials)
			{
				UniqueMaterials.Add(Material);
			}

			TSet<UTexture*> UniqueTextures;
			for (auto* Material : UniqueMaterials)
			{
				auto DynamicMaterial = Cast<UMaterialInstanceDynamic>(Material);
				if (DynamicMaterial) 
				{
					TArray<FMaterialParameterInfo> TextureParameters;
					TArray<FGuid> ParameterGuids;
					DynamicMaterial->GetAllTextureParameterInfo(TextureParameters, ParameterGuids);

					for (auto Parameter : TextureParameters)
					{
						if (ManagedMaps.Contains(Parameter.Name))
						{
							continue; // We are already aware of this texture
						}

						UTexture* Texture;
						if (DynamicMaterial->GetTextureParameterValue(Parameter, Texture))
						{
							if (!Texture->IsA<UTexture2D>())
							{
								continue;
							}

							DynamicMaterial->SetTextureParameterValue(
								Parameter.Name,
								ConvertToStaticTexture((UTexture2D*)Texture, AssetToolsModule.Get(), Package));
						}
					}
				}
			}

			for (auto Material : UniqueMaterials)
			{
				AssetToolsModule.Get().CreateUniqueAssetName(PackageName, TEXT("Material"), PackageName, Name);
				Material->Rename(*Name, Package); // Change the Outer
				FAssetRegistryModule::AssetCreated(Material);
				Material->MarkPackageDirty();
			}

			for (auto* Material : Materials)
			{
				StaticMesh->StaticMaterials.Add(FStaticMaterial(Material));// todo: add back material support
			}

			//Set the Imported version before calling the build
			StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

			// Build mesh from source
			StaticMesh->Build(true);
			StaticMesh->PostEditChange();

			// Now that the mesh has been built, update the FaceMaps using the Collision Data.
			// Use the CollisionData from the built UStaticMesh, in case the layout changed during cooking.

			// The UStaticMesh::GetPhysicsTriMeshData() method relies on bSupportUVFromHitResults being true
			// in order to return the UVs (which we need). To change this flag properly, the Editor needs to be
			// restarted, so as a workaround we set it here, get the mesh, then set it back.

			bool bSupportUVFromHitResultsSetting = UPhysicsSettings::Get()->bSupportUVFromHitResults;
			UPhysicsSettings::Get()->bSupportUVFromHitResults = true;

			TSharedPtr<FTriMeshCollisionData> CollisionData = MakeShareable(new FTriMeshCollisionData());
			StaticMesh->GetPhysicsTriMeshData(CollisionData.Get(), true);

			UPhysicsSettings::Get()->bSupportUVFromHitResults = bSupportUVFromHitResultsSetting;

			TArray<int> FaceMap;
			FaceMap.SetNumUninitialized(CollisionData->Indices.Num());
			for (int32 i = 0; i < FaceMap.Num(); i++)
			{
				FaceMap[i] = (int)(CollisionData->UVs[1][CollisionData->Indices[i].v0].Y); //UV1 is the ARepoSupermeshActor-level Id
			}

			// Notify asset registry of new asset
			FAssetRegistryModule::AssetCreated(StaticMesh);
			StaticMesh->MarkPackageDirty();

			auto actor = AddStaticSupermeshActor();
			actor->SetActorRelativeLocation(ProcMeshComp->GetRelativeLocation());

			auto component = actor->GetStaticMeshComponent();
			component->SetStaticMesh(StaticMesh);
			component->BodyInstance.SetCollisionProfileName(ProcMeshComp->GetCollisionProfileName());
			actor->SetFaceMap(FaceMap);

			actor->MarkPackageDirty();
		}
	}

	for (auto component : MeshComponents)
	{
		component->UnregisterComponent();
		component->DestroyComponent();
	}
}

UTexture2D* ARepoSupermeshActor::ConvertToStaticTexture(UTexture2D* Texture, IAssetTools& AssetTools, UPackage* Package) 
{
	FString Name;
	FString PackageName;
	Package->GetName(PackageName);
	AssetTools.CreateUniqueAssetName(PackageName, TEXT(""), PackageName, Name);

	if (Texture->GetFlags() & RF_Transient) // Transient textures must be rebuilt because they cannot be serialised
	{
		// Based on https://isaratech.com/save-a-procedurally-generated-texture-as-a-new-asset/

		auto NewTexture = NewObject<UTexture2D>(Package, *Name, RF_Public | RF_Standalone | RF_MarkAsRootSet);

		NewTexture->AddToRoot();	// This line prevents garbage collection of the texture
		NewTexture->PlatformData = new FTexturePlatformData();	// Then we initialize the PlatformData
		NewTexture->PlatformData->SizeX = Texture->PlatformData->SizeX;
		NewTexture->PlatformData->SizeY = Texture->PlatformData->SizeY;
		NewTexture->PlatformData->PixelFormat = Texture->PlatformData->PixelFormat;

		NewTexture->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps; // this is an Editor only property, though the Material's Sampler should always explicitly use Mip Level 0 anyway
		NewTexture->CompressionSettings = TC_VectorDisplacementmap;
		NewTexture->Filter = TextureFilter::TF_Nearest;

		uint8* Pixels = (uint8*)(Texture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));

		// Allocate first mipmap.
		FTexture2DMipMap* Mip = new FTexture2DMipMap();
		NewTexture->PlatformData->Mips.Add(Mip);
		Mip->SizeX = NewTexture->PlatformData->SizeX;
		Mip->SizeY = NewTexture->PlatformData->SizeY;

		// Lock the texture so it can be modified
		Mip->BulkData.Lock(LOCK_READ_WRITE);
		uint8* TextureData = (uint8*)Mip->BulkData.Realloc(Mip->SizeX * Mip->SizeY * 4);
		FMemory::Memcpy(TextureData, Pixels, sizeof(uint8) * Mip->SizeX * Mip->SizeY * 4);
		Mip->BulkData.Unlock();

		NewTexture->Source.Init(Mip->SizeX, Mip->SizeY, 1, 1, ETextureSourceFormat::TSF_BGRA8, Pixels);
		NewTexture->UpdateResource();

		Texture->PlatformData->Mips[0].BulkData.Unlock(); // Unlock Pixels

		Texture = NewTexture;
	}
	else
	{
		Texture->Rename(*Name, Package); // If the texture is already static, move it into the Package
	}

	FAssetRegistryModule::AssetCreated(Texture);
	Texture->MarkPackageDirty();

	return Texture;
}
#endif

void ARepoSupermeshActor::SetMaterialsTextureParameter(FName ParameterName, UTexture* Value)
{
	// Enumerate transient types

	for (int32 i = 0; i < RootComponent->GetNumChildrenComponents(); i++)
	{
		auto Mesh = Cast<UProceduralMeshComponent>(RootComponent->GetChildComponent(i));
		if (Mesh)
		{
			const int32 NumSections = Mesh->GetNumSections();
			for (int32 SectionIdx = 0; SectionIdx < NumSections; SectionIdx++)
			{
				FProcMeshSection* ProcSection = Mesh->GetProcMeshSection(SectionIdx);
				UMaterialInterface* Material = Mesh->GetMaterial(SectionIdx);
				auto DynamicInstance = Cast<UMaterialInstanceDynamic>(Material);
				if (DynamicInstance)
				{
					DynamicInstance->SetTextureParameterValue(ParameterName, Value);
				}
			}
		}
	}

	// Enumerate static types

	for (int32 i = 0; i < Children.Num(); i++)
	{
		auto Child = Cast<ARepoStaticSupermeshActor>(Children[i]);
		if (Child)
		{
			auto Mesh = Child->GetStaticMeshComponent();
			for (auto Material : Mesh->GetMaterials())
			{
				auto DynamicMaterial = Cast<UMaterialInstanceDynamic>(Material);
				if (DynamicMaterial)
				{
					DynamicMaterial->SetTextureParameterValue(ParameterName, Value);
				}
			}
		}
	}
}

TArray<FString>& ARepoSupermeshActor::GetSubmeshMap()
{
	return IdMap;
}

FString ARepoSupermeshActor::GetMeshIdFromFaceIndex(TWeakObjectPtr<class UPrimitiveComponent> Component, int32 FaceIndex)
{
	// Though the ARepoSupermeshActor exists above the static hierarchy too, we will only end up here from a ProceduralMeshComponent.
	TArray<int>& faceToIdMap = *MeshComponentTriangleMaps.Find(Component.Get());
	auto id = faceToIdMap[FaceIndex];
	return IdMap[id];
}

TWeakObjectPtr<ARepoSupermeshActor> ARepoSupermeshActor::GetActor()
{
	return TWeakObjectPtr<ARepoSupermeshActor>(this);
}

#pragma optimize("", on)