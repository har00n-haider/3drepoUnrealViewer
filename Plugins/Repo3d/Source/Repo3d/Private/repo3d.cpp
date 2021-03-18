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

#include "Repo3d.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "Engine/AssetManager.h"
#include "Engine/ObjectLibrary.h"
#include "Templates/SharedPointer.h"
#include "ProceduralMeshComponent.h"
#include "RepoSupermeshActor.h"
#include "Json.h"
#include "RepoWebRequestManager.h"
#include "RepoSrcImporter.h"
#include "RepoTypes.h"
#include "Http.h"
#include "HttpModule.h"
#include "RHI.h"

#define LOCTEXT_NAMESPACE "FRepo3dModule"


Repo3d::Repo3d(TSharedRef<IPlugin> plugin):manager(MakeShared<RepoWebRequestManager>(this)),Plugin(plugin)
{
}

TSharedRef<RepoWebRequestManager> Repo3d::GetWebRequestManager()
{
	return manager;
}

void Repo3d::SetApiKey(FString key)
{
	manager->SetApiKey(key);
}

void Repo3d::SetHost(FString host)
{
	manager->SetHost(host);
}

void Repo3d::LogWarning(FString warning) 
{
	UE_LOG(LogTemp, Log, TEXT("%s"), *warning);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, warning);
	}
}

void Repo3d::LogError(FString error)
{
	UE_LOG(LogTemp, Log, TEXT("%s"), *error);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3600.0f, FColor::Red, error);
	}
}

void Repo3d::SetAssetPackage(FString packageName)
{
	package = packageName;
}

void Repo3d::SetOpaqueMaterial(FString materialName)
{
	opaqueMaterial = LoadMaterial(materialName);
}

void Repo3d::SetTranslucentMaterial(FString materialName)
{
	translucentMaterial = LoadMaterial(materialName);
}

void Repo3d::FindMaterials()
{
	auto Manager = UAssetManager::GetIfValid(); // use GetIfValid rather than Get because Get is marked EDITOR only and so cannot be linked from plugins

	if (!Manager)
	{
		UE_LOG(LogTemp, Fatal, TEXT("Do not have an AssetManager Instance."));
	}

	TArray<FAssetData> materials;
	if (!Manager->GetAssetRegistry().GetAssetsByClass(TEXT("Material"), materials, false))
	{
		UE_LOG(LogTemp, Fatal, TEXT("Material Search Failed."));
	}

	for (auto asset : materials) // iterates the asset registry for helping find our material
	{
		UE_LOG(LogTemp, Log, TEXT("Found asset %s"), *(asset.GetFullName()));
	}
}

UMaterialInterface* Repo3d::LoadMaterial(FString materialName)
{
	auto Manager = UAssetManager::GetIfValid();

	if (!Manager)
	{
		UE_LOG(LogTemp, Fatal, TEXT("Do not have an AssetManager Instance."));
	}

	FSoftObjectPath materialPath(FString::Printf(TEXT("/%s/%s.%s"), *package, *materialName, *materialName));
	FAssetData materialData;
	if (!Manager->GetAssetDataForPath(materialPath, materialData))
	{
		UE_LOG(LogTemp, Fatal, TEXT("Unable to load Material /%s/%s."), *package, *materialName);
	}

	auto materialAsset = (UMaterialInterface*)materialData.GetAsset();

	return materialAsset;
}

void Repo3d::LoadModel(FString teamspace, FString model, FString revision, TWeakObjectPtr<ARepoSupermeshActor> actor)
{
	auto emptyDelegate = Repo3dLoadModelCompleteDelegate();
	LoadModel(teamspace, model, revision, actor, emptyDelegate);
}

void Repo3d::LoadModel(FString teamspace, FString model, FString revision, TWeakObjectPtr<ARepoSupermeshActor> actor, Repo3dLoadModelCompleteDelegate& oncomplete)
{
	UE_LOG(LogTemp, Log, TEXT("Importing 3D Repo Model..."));

	auto importer = NewObject<URepoSrcImporter>(); // Create a new importer manager under the Transient package

	if (actor.IsStale()) // We don't know where the Actor came from, so check it's still OK before proceeeding. Once assigned to the URepoSrcImporter actor UProperty, it will be protected from garbage collection.
	{
		return;
	}
	else
	{
		importer->SetActor(actor.Get());
	}

	importer->AddToRoot(); // Protect the importer from garbage collection

	actor->Teamspace = teamspace;
	actor->Model = model;
	actor->Revision = revision;

	importer->SetWebManager(GetWebRequestManager());
	importer->SetOpaqueMaterial(opaqueMaterial);
	importer->SetTranslucentMaterial(translucentMaterial);
	
	importer->OnComplete.BindLambda(
		[this, importer, actor, oncomplete]()
		{
			UE_LOG(LogTemp, Log, TEXT("Import complete."));
			oncomplete.ExecuteIfBound(actor);
			importer->RemoveFromRoot();
		}
	);

	importer->RequestRevision(teamspace, model, revision);
}

TSharedRef<Repo3d> FRepo3dModule::Get()
{
	return FRepo3dModule::singleton.ToSharedRef();
}

TSharedPtr<Repo3d> FRepo3dModule::singleton; // instance of static member

void FRepo3dModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	// Turn off half-float support to force meshes to use full precision for our indexing UVs. It is not ideal to do this for the whole
	// application, but there are no APIs to control the UV precision in the procedural pathways otherwise.
	GVertexElementTypeSupport.SetSupported(VET_Half2, false);

	// Find the plugin that hosts this module
	IPluginManager& PluginManager = IPluginManager::Get();
	for (auto plugin : PluginManager.GetEnabledPlugins())
	{
		if (plugin->GetName() == TEXT("Repo3d")) // Find the plugin that hosts this module based on the name (there is no direct reference)
		{
			FRepo3dModule::singleton = MakeShared<Repo3d>(plugin); // Create the singleton class that actually implements our behaviours
		}
	}

	if (!singleton.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("Cannot find the 3d Repo Plugin that is hosting the running module. This should never happen."));
	}

	auto Singleton = FRepo3dModule::singleton; // create a local variable for the lambda closure
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("3drepoversion"),
		TEXT("Return the version number of the loaded 3D Repo Plugin"),
		FConsoleCommandDelegate::CreateLambda(
			[Singleton]() {
				auto Message = FString::Printf(TEXT("Using 3DRepo Plugin Version %s"), *(Singleton->Plugin->GetDescriptor().VersionName));
				UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, Message);
				}
			}),
		0);
}

void FRepo3dModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRepo3dModule, Repo3d)