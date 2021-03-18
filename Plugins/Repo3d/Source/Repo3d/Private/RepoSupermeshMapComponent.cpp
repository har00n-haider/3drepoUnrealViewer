// Copyright (C) 2020 3D Repo Ltd


#include "RepoSupermeshMapComponent.h"
#include "RepoSupermeshActor.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

#if WITH_EDITOR 
#include <AssetToolsModule.h>
#endif

// Sets default values for this component's properties
URepoSupermeshMapComponent::URepoSupermeshMapComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	Texture = nullptr;
}


// Called when the game starts
void URepoSupermeshMapComponent::BeginPlay()
{
	Super::BeginPlay();
	IsMapDirty = false;
}


// Called every frame
void URepoSupermeshMapComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// At runtime, check if the map has changed, and if so update the texture. At design time it is assumed the editor
	// will manually rebuild the texture as necessary.

	if (IsMapDirty)
	{
		UpdateTexture();
		IsMapDirty = false;
	}
}

ARepoSupermeshActor* URepoSupermeshMapComponent::GetActor()
{
	return Cast<ARepoSupermeshActor>(GetOwner()); // This component should only ever be applied to an ARepoSupermeshActor
}

FVector2D IdToPixel(float id, int size)
{
	auto v = FMath::FloorToInt(id / size);
	auto u = id - (v * size);
	FVector2D uv;
	uv.X = (float)u;
	uv.Y = (float)v;
	return uv;
}

FVector2D IdToUv(float id, int size)
{
	return (IdToPixel(id, size) / size) + (0.5 / size);
}

int URepoSupermeshMapComponent::GetSupermeshMapSize()
{
	return FMath::CeilToFloat(FMath::Sqrt(Parameters.Num()));
}

#pragma optimize("", off)

void URepoSupermeshMapComponent::SetParameter(int Id, FVector4 Value)
{
	Parameters.SetNum(GetActor()->GetSubmeshMap().Num());
	Parameters[Id] = Value;
	IsMapDirty = true;
}

void URepoSupermeshMapComponent::SetParameter(FString& Id, FVector4 Value)
{
	SetParameter(GetActor()->GetSubmeshMap().Find(Id), Value);
}

FVector4 URepoSupermeshMapComponent::GetParameter(FString& Id)
{
	return GetParameter(GetActor()->GetSubmeshMap().Find(Id));
}

FVector4 URepoSupermeshMapComponent::GetParameter(int Id)
{
	if (Id >= Parameters.Num())
	{
		Parameters.SetNum(GetActor()->GetSubmeshMap().Num());
	}
	return Parameters[Id];
}

#pragma optimize("", on)

void URepoSupermeshMapComponent::CreateTexture()
{
	// In case this is being called because the paramters are being initialised to their default values
	auto Actor = GetActor();
	Parameters.SetNum(Actor->GetSubmeshMap().Num());
	
	auto Map = UTexture2D::CreateTransient(GetSupermeshMapSize(), GetSupermeshMapSize(), EPixelFormat::PF_B8G8R8A8); // (Note the element order is swapped with the ToPackedARGB call below)
#if WITH_EDITORONLY_DATA
	Map->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps; // this is an Editor only property, though the Material's Sampler should always explicitly use Mip Level 0 anyway
#endif
	Map->CompressionSettings = TC_VectorDisplacementmap;
	Map->Filter = TextureFilter::TF_Nearest;

	Texture = Map;
}

void URepoSupermeshMapComponent::UpdateTexture()
{
	// Check the texture matches the number of parameters, otherwise it may be uninitialised
	Parameters.SetNum(GetActor()->GetSubmeshMap().Num());

	bool recreatedTexture = false;
	if (!Texture || Texture->GetSizeX() < GetSupermeshMapSize() || Texture->GetSizeY() < GetSupermeshMapSize())
	{
		CreateTexture();
		recreatedTexture = true;
	}

	auto MapData = (int32*)Texture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);

	for (int32 i = 0; i < Parameters.Num(); i++)
	{
		MapData[i] = FLinearColor(Parameters[i]).ToFColor(true).ToPackedARGB(); // This line is heavily dependent on the format and layout. Its inverse must match the IdToPixel method.
	}

	Texture->PlatformData->Mips[0].BulkData.Unlock();
	Texture->UpdateResource();

	if (recreatedTexture)
	{
		ApplyTextureToMaterials();
	}

	IsMapDirty = false;
}

#if WITH_EDITOR 
void URepoSupermeshMapComponent::ConvertToStaticTexture(IAssetTools& AssetTools, UPackage* Package)
{
	Texture = GetActor()->ConvertToStaticTexture(Texture, AssetTools, Package);
	ApplyTextureToMaterials();
}
#endif

void URepoSupermeshMapComponent::ApplyTextureToMaterials()
{
	GetActor()->SetMaterialsTextureParameter(ParameterName, Texture);
}

void URepoSupermeshMapComponent::ApplyTextureToMaterials(UMaterialInstanceDynamic* material)
{
	material->SetTextureParameterValue(ParameterName, Texture);
}