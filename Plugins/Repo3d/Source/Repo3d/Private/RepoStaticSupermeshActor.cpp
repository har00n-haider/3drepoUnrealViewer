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

#include "RepoStaticSupermeshActor.h"
#include "RepoSupermeshActor.h"

// Sets default values
ARepoStaticSupermeshActor::ARepoStaticSupermeshActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Transform = CreateDefaultSubobject<USceneComponent>(FName("Transform"));
	SetRootComponent(Transform);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(FName("Mesh"));
	Mesh->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
void ARepoStaticSupermeshActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ARepoStaticSupermeshActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

UStaticMeshComponent* ARepoStaticSupermeshActor::GetStaticMeshComponent() 
{
	return Mesh;
}

void ARepoStaticSupermeshActor::SetPrimarySupermeshActor(ARepoSupermeshActor* actor)
{
	SupermeshActor = actor;
}

void ARepoStaticSupermeshActor::SetFaceMap(TArray<int>& map)
{
	FaceIndexToId = map;
}

FString ARepoStaticSupermeshActor::GetMeshIdFromFaceIndex(TWeakObjectPtr<class UPrimitiveComponent> Component, int32 FaceIndex)
{
	// todo: bounds checking.
	return SupermeshActor->GetSubmeshMap()[FaceIndexToId[FaceIndex]];
}

TWeakObjectPtr<ARepoSupermeshActor> ARepoStaticSupermeshActor::GetActor()
{
	return TWeakObjectPtr<ARepoSupermeshActor>(SupermeshActor);
}