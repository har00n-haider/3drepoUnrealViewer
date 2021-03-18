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
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "RepoInterfaces.h"
#include "RepoStaticSupermeshActor.generated.h"

class ARepoSupermeshActor;

UCLASS()
class REPO3D_API ARepoStaticSupermeshActor : public AActor, public IRepoTraceable
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Transform Component")
	USceneComponent* Transform;

	UPROPERTY(VisibleAnywhere, Category = "Mesh Component")
	UStaticMeshComponent* Mesh;

	UPROPERTY(VisibleAnywhere, Category = "3D Repo Supermeshing Data")
	ARepoSupermeshActor* SupermeshActor;

	UPROPERTY(VisibleAnywhere, Category = "3D Repo Supermeshing Data")
	TArray<int> FaceIndexToId;
	
public:	
	// Sets default values for this actor's properties
	ARepoStaticSupermeshActor();

	UStaticMeshComponent* GetStaticMeshComponent();
	void SetPrimarySupermeshActor(ARepoSupermeshActor* actor);
	void SetFaceMap(TArray<int>& map);

	virtual FString GetMeshIdFromFaceIndex(TWeakObjectPtr<class UPrimitiveComponent> Component, int32 FaceIndex);
	virtual TWeakObjectPtr<ARepoSupermeshActor> GetActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
