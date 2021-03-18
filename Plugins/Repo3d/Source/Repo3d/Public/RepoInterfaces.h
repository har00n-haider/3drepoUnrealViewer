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

#include "RepoInterfaces.generated.h"

// Interfaces in Unreal: https://docs.unrealengine.com/en-US/ProgrammingAndScripting/GameplayArchitecture/Interfaces/index.html

/// <summary>
/// ARepoTraceable is an abstract (interface) class for Repo AActors that can be used to lookup a mesh id following a LineTrace call,
/// if the actor has components with complex collision (geometry) enabled.
/// </summary>
UINTERFACE()
class REPO3D_API URepoTraceable : public UInterface
{
	GENERATED_BODY()
};

class IRepoTraceable
{
	GENERATED_BODY()

public:
	virtual FString GetMeshIdFromFaceIndex(TWeakObjectPtr<class UPrimitiveComponent> Component, int32 FaceIndex) = 0;
	virtual TWeakObjectPtr<class ARepoSupermeshActor> GetActor() = 0;
};