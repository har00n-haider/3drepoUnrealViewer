// Copyright Epic Games, Inc. All Rights Reserved.

#include "Repo3DPluginTest.h"
#include "Modules/ModuleManager.h"

#include "Repo3d.h"


class SampleGameModule : public IModuleInterface
{
public:
	SampleGameModule()
	{
		UE_LOG(LogTemp, Log, TEXT("Loaded 3D Repo Sample Game Module."));
	}

	/*
	 * ImportModel shows how to use the plugin's API. First, the plugin must be
	 * loaded, and logged in. The Plugin also needs some parameters about how
	 * it should interact with Unreal, including in which World to create the
	 * models, and what materials to use.
	 *
	 * Once these are set, the command to import the model can be given.
	 */

	void ImportModel(const TArray<FString>& args, UWorld* world)
	{
		if (apiKey == TEXT("") || url == TEXT(""))
		{
			UE_LOG(LogTemp, Log, TEXT("API key is not configured. Either set it in the code or use the 3drepowebconfig command"));
			return;
		}

		if (args.Num() < 2)
		{
			UE_LOG(LogTemp, Log, TEXT("Incorrect number of arguments. Command requires 2 or 3 arguments: teamspace, model id [and revision]"));
			return;
		}

		auto teamspace = args[0];
		auto model = args[1];
		auto revision = FString("");

		if (args.Num() > 2)
		{
			revision = args[2];
		}

		// Get an interface to the 3D Repo Plugin, and login (or set your API key).

		// The Get() method is a convenience function that returns a singleton 
		// (so one instance remains logged in application wide). You can also create
		// a new instance of Repo3d.
		auto repo3d = FRepo3dModule::Get();

		repo3d->SetHost(url);

		repo3d->SetApiKey(apiKey);

		// Prepare the import by indicating where to find the supermesh-aware 
		// materials. 
		// This sample project comes with two materials to build on. 
		// If no material is specified, the Unreal default will be applied.
		repo3d->SetAssetPackage("Game");
		repo3d->SetOpaqueMaterial("RepoMaterial");
		repo3d->SetTranslucentMaterial("RepoMaterialTranslucent");

		// Provide an Actor in the world to import models under. The top level 
		// Actor is where supermeshing data such as the submesh ID arrays are 
		// stored.
		// This sample gets the World to create the Actor in from the Console 
		// Command Callback.
		if (currActor != nullptr)
		{
			world->DestroyActor(currActor);
		}
		currActor = (ARepoSupermeshActor*)world->SpawnActor(ARepoSupermeshActor::StaticClass());
		// Unreal's world units are cm
		currActor->SetActorScale3D(FVector(0.1, 0.1, 0.1));
		repo3d->SetActor(currActor);


		// Finally, specify the teamspace, model & revision to load.
		repo3d->LoadModel(teamspace, model, revision);
	}

	/*
	 * The imported Supermeshes contain many individual objects in one. The Ids
	 * of these objects are stored in the ARepoSupermeshActor's SubmeshMap.
	 * The meshes underneath a ARepoSupermeshActor store the index to this array
	 * in their Id vertex attributes (in the X component of UV2).
	 *
	 * This function shows how the original Id can be recovered by referencing
	 * into this array.
	 */

	void GetSubmeshId()
	{
		// In the sample below, the current ARepoSupermeshActor for the FRepo3dModule 
		// singleton is returned.
		// It is possible to import models underneath different ARepoSupermeshActors 
		// by instantiating and calling FRepo3dModule::SetActor(). Ensure you
		// perform the lookup on the right one.
		// 
		// (Note you can inspect the array in the Unreal Editor using the Details 
		// Pane)

		auto repo3d = FRepo3dModule::Get();
		auto submeshIndex = 0;
		auto submeshId = repo3d->GetActor()->GetSubmeshMap()[submeshIndex];
	}

	/*
	 * By default, models are imported as transient assets, both at Design and
	 * Run Time. This reduces loading time and overhead.
	 * When running in the Editor, it is possible to convert an imported Model
	 * into a hierarchy of StaticMeshes and StaticMeshComponents, that can be
	 * saved to a PAK.
	 * The function below converts the current ARepoSupermeshActor of the
	 * FRepo3dModule singleton to a form that can be saved and reloaded by the
	 * Editor.
	 */

	void Convert()
	{
		auto repo3d = FRepo3dModule::Get();
		repo3d->GetActor()->ConvertToStaticHierarchy();
	}

	void Configure3dRepoInstance(const TArray<FString>& args)
	{
		if (args.Num() != 2)
		{
			UE_LOG(LogTemp, Log, TEXT("Incorrect number of arguments. Command requires 2 in this order: url api_key"));
			return;
		}

		//if (!IsValidApiString(args[1]))
		//{
		//	UE_LOG(LogTemp, Log, TEXT("API key is not valid. Might be the order. Command requires 2 in this order: url api_key"));
		//	return;
		//}

		apiKey = args[1];
		url = args[0];

		UE_LOG(LogTemp, Log, TEXT("Web settings succesfully configured"));
	}

	virtual void StartupModule() override
	{
		// Registers the ImportModel() callback under the command 3drepoimport
		IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("3drepoimport"),
			TEXT("Import a file from 3dRepo.io (arguments: teamspace modelid [revisiontag])"),
			FConsoleCommandWithWorldAndArgsDelegate::CreateRaw(this, &SampleGameModule::ImportModel),
			0);
		IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("3drepoconvert"),
			TEXT("Convert scene to use Unreal static classes (for further inspection in the Editor, or for saving to a PAK)"),
			FConsoleCommandDelegate::CreateRaw(this, &SampleGameModule::Convert),
			0);
		IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("3drepowebconfig"),
			TEXT("Configure the instance of 3drepo to use (arguments: url api_key"),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &SampleGameModule::Configure3dRepoInstance),
			0);

		// Import a default world 
	}

private:

	bool IsValidApiString(const FString& apiString)
	{
		FString lowerCaseApiString = apiString.ToLower();
		static FString validChars = "abcdef0123456789";
		bool result = true;
		for (auto k : lowerCaseApiString)
		{
			const TCHAR* c = &k;
			if (!validChars.Contains(c))
			{
				result = false;
				return result;
			};
		}
		return result;
	}
	// The URL of the 3drepo instance to get the models from
	FString url = TEXT("staging.dev.3drepo.io");
	// The Unreal plugin uses API keys for authentication. Generate a key from your user profile
	// in the 3drepo instance you are using
	FString apiKey = TEXT("670f65dd5a45cc01dc97d771ffad2b35"); // HH - 670f65dd5a45cc01dc97d771ffad2b35
	// To make sure we only have one instance around
	ARepoSupermeshActor* currActor = nullptr;
};



IMPLEMENT_PRIMARY_GAME_MODULE(SampleGameModule, Repo3DPluginTest, "Repo3DPluginTest" );
