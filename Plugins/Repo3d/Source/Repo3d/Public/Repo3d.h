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
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Templates/SharedPointer.h"
#include "RepoWebRequestManager.h"
#include "RepoSupermeshActor.h"
#include "RepoSupermeshMapComponent.h"

DECLARE_STATS_GROUP(TEXT("3D Repo Plugin"), STATGROUP_Repo3D, STATCAT_Advanced)

DECLARE_DELEGATE_OneParam(Repo3dLoadModelCompleteDelegate, TWeakObjectPtr<ARepoSupermeshActor>);

class REPO3D_API Repo3d
{
private:
	FString package;
	UMaterialInterface* opaqueMaterial;
	UMaterialInterface* translucentMaterial;
	TSharedRef<RepoWebRequestManager> manager;

	UMaterialInterface* LoadMaterial(FString materialName);
	void FindMaterials();

public:
	Repo3d(TSharedRef<IPlugin> plugin);

	// The Plugin that is hosting the module
	TSharedRef<IPlugin> Plugin;
	TSharedRef<RepoWebRequestManager> GetWebRequestManager();

	void SetHost(FString host);
	void SetApiKey(FString key);

	void SetAssetPackage(FString package);
	void SetOpaqueMaterial(FString materialName);
	void SetTranslucentMaterial(FString materialName);

	void LoadModel(FString teamspace, FString model, FString revision, TWeakObjectPtr<ARepoSupermeshActor> actor);
	void LoadModel(FString teamspace, FString model, FString revision, TWeakObjectPtr<ARepoSupermeshActor> actor, Repo3dLoadModelCompleteDelegate& oncomplete);

	// These log warnings to the user, as well as the UE_LOG
	void LogWarning(FString warning);
	void LogError(FString error);
};


class REPO3D_API FRepo3dModule : public IModuleInterface
{
	static TSharedPtr<Repo3d> singleton;

public:
	static TSharedRef<Repo3d> Get();

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
