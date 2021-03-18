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

#include "RepoSrcImporter.h"
#include "RepoWebRequestHelpers.h"
#include "Misc/Compression.h"
#include "HAL/UnrealMemory.h"
#include "Materials/MaterialInstanceDynamic.h"
#include <string>

DECLARE_CYCLE_STAT(TEXT("Handle SRC"), STAT_HandleSRC, STATGROUP_Repo3D);
DECLARE_CYCLE_STAT(TEXT("Generate Mesh"), STAT_GenerateMesh, STATGROUP_Repo3D);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Last Mappings Response Time (ms)"), STAT_DownloadMappings, STATGROUP_Repo3D);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Last SRC Response Time (ms)"), STAT_DownloadSRC, STATGROUP_Repo3D);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Active Requests"), STAT_ActiveRequests, STATGROUP_Repo3D);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Triangles"), STAT_TotalTriangles, STATGROUP_Repo3D);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Vertices"), STAT_TotalVertices, STATGROUP_Repo3D);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Objects"), STAT_TotalObjects, STATGROUP_Repo3D);
DECLARE_MEMORY_STAT(TEXT("Uncompressed"), STAT_Uncompressed, STATGROUP_Repo3D);

void URepoSrcImporter::BeginDestroy()
{
	Super::BeginDestroy();
	// URepoSrcImporter cleanup 
}

void URepoSrcImporter::RequestRevision(FString teamspace, FString model, FString revision)
{
	check(manager.IsValid());

	// in parallel, set the model units
	RepoWebRequestHelpers::GetModelSettings(
		manager.ToSharedRef(), teamspace, model,
		RepoWebRequestHelpers::ModelSettingsDelegate::CreateUObject(this, &URepoSrcImporter::HandleModelSettings)
	);

	if (!revision.IsEmpty())
	{
		manager->GetRequest(
			FString::Printf(TEXT("%s/%s/revision/%s/srcAssets.json"), *teamspace, *model, *revision),
			RepoWebRequestDelegate::CreateUObject(this, &URepoSrcImporter::AssetsRequestCompleted)
		);
	}
	else
	{
		manager->GetRequest(
			FString::Printf(TEXT("%s/%s/revision/master/head/srcAssets.json"), *teamspace, *model),
			RepoWebRequestDelegate::CreateUObject(this, &URepoSrcImporter::AssetsRequestCompleted)
		);
	}
}

void URepoSrcImporter::HandleModelSettings(TSharedRef<RepoWebRequestHelpers::ModelSettings> Settings)
{
	// Unreal's world units are cm

	actor->Units = Settings->Units;

	double factor = 1.0;
	switch (Settings->Units)
	{
	case EUnit::Millimeters:
		factor = 0.1;
		break;
	case EUnit::Centimeters:
		factor = 1.0;
		break;	
	case EUnit::Meters:
		factor = 10;
		break;
	case EUnit::Kilometers:
		factor = 10000;
		break;
	}

	actor->SetActorScale3D(FVector(factor, factor, factor));
}

void URepoSrcImporter::HandleAssets(const FString& string)
{
	TSharedPtr<FJsonObject> assetsList = MakeShareable(new FJsonObject());
	TSharedRef<TJsonReader<TCHAR>> reader = TJsonReaderFactory<TCHAR>::Create(string);
	FJsonSerializer::Deserialize(reader, assetsList);

	FVector worldOffset;
	bool worldOffsetInitialised = false;

	for (auto model : assetsList->GetArrayField("models"))
	{
		for (auto asset : model->AsObject()->GetArrayField("assets"))
		{
			TSharedRef<RepoSrcAssetImporter> importer = MakeShared<RepoSrcAssetImporter>(manager);
			importer->SetActor(actor);
			importer->SetMaterialPrototype(materialOpaque, materialTranslucent);
			importers.Add(importer);

			importer->OnComplete.BindUObject(this, &URepoSrcImporter::HandleCompleted, importer);

			auto srcAssetUri = FString::Printf(TEXT("%s/%s/%s"),
				*(model->AsObject()->GetStringField("database")),
				*(model->AsObject()->GetStringField("model")),
				*(asset->AsString()));

			FVector offsetVector;
			auto offsetElements = model->AsObject()->GetArrayField("offset");
			for (int32 i = 0; i < offsetElements.Num(); i++)
			{
				offsetVector[i] = offsetElements[i]->AsNumber();
			}

			if (!worldOffsetInitialised)
			{
				worldOffset = offsetVector;
				worldOffsetInitialised = true;
			}

			offsetVector -= worldOffset;

			importer->SetOffset(offsetVector); // this method automatically performs the coordinate transform from server coordinate system to Unreal
			importer->SetUri(srcAssetUri);

			importer->RequestSrc();
		}
	}
}

void URepoSrcImporter::HandleCompleted(TSharedRef<RepoSrcAssetImporter> importer)
{
	UE_LOG(LogTemp, Log, TEXT("Completed SRC %s"), *(importer->Uri));

	importers.Remove(importer);
	
	if(importers.Num() <= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Completed All Importers"));
		OnComplete.ExecuteIfBound();
	}
}

void URepoSrcImporter::AssetsRequestCompleted(TSharedPtr<RepoWebResponse> Result)
{
	if (Result->bWasSuccessful && Result->Response->GetResponseCode() == 200)
	{
		UE_LOG(LogTemp, Log, TEXT("Received SRC Assets Json"));
		HandleAssets(Result->Response->GetContentAsString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failure reading SRC %d %s"), Result->Response->GetResponseCode(), *(Result->Request->GetURL()));
	}
}

void RepoSrcAssetImporter::SetOffset(FVector v)
{
	Offset = TransformCoordinateSystem(v);
}

void RepoSrcAssetImporter::RequestSrc()
{
	UE_LOG(LogTemp, Log, TEXT("Requesting SRC %s"), *Uri);

	// For this version, just read the mapping first, and only when its received request the main SRC file
	manager->GetRequest(
		FString::Printf(TEXT("%s.json.mpc"), *Uri),
		RepoWebRequestDelegate::CreateLambda(
			[this](TSharedPtr<RepoWebResponse> result)
			{	
				DEC_DWORD_STAT_BY(STAT_ActiveRequests, 1);
				if (MappingRequestCompleted(result)) {
					manager->GetRequest(
						FString::Printf(TEXT("%s.src.mpc"), *Uri),
						RepoWebRequestDelegate::CreateRaw(this, &RepoSrcAssetImporter::SrcRequestCompleted)
					);
					INC_DWORD_STAT_BY(STAT_ActiveRequests, 1);
				}
				else
				{
					OnComplete.ExecuteIfBound();
				}
			})
	);

	INC_DWORD_STAT_BY(STAT_ActiveRequests, 1);
}

void RepoSrcAssetImporter::SrcRequestCompleted(TSharedPtr<RepoWebResponse> Result)
{
	DEC_DWORD_STAT_BY(STAT_ActiveRequests, 1);
	SET_FLOAT_STAT(STAT_DownloadSRC, Result->Time * 1000.0);
	

	if (Result->bWasSuccessful && Result->Response->GetResponseCode() == 200)
	{
		UE_LOG(LogTemp, Log, TEXT("Received SRC %s.src.mpc"), *Uri);
		HandleSrc(Result->Response->GetContent());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failure reading SRC %d %s"), Result->Response->GetResponseCode(), *(Result->Request->GetURL()));
	}

	OnComplete.ExecuteIfBound();
}

bool RepoSrcAssetImporter::MappingRequestCompleted(TSharedPtr<RepoWebResponse> Result)
{
	SET_FLOAT_STAT(STAT_DownloadMappings, Result->Time * 1000.0);

	if (Result->bWasSuccessful && Result->Response->GetResponseCode() == 200)
	{
		UE_LOG(LogTemp, Log, TEXT("Received Json Supermesh Mapping (%s.json.mpc)"), *Uri);
		HandleMapping(Result->Response->GetContentAsString());
		return true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failure reading Json Supermesh Mapping for SRC %d %s"), Result->Response->GetResponseCode(), *(Result->Request->GetURL()));
		return false;
	}
}

#pragma optimize("", off)

void RepoSrcAssetImporter::HandleMapping(const FString& string)
{
	if (LocalToActorSubmeshMap.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("Non-empty Local Map"));
	}

	auto &ActorIdMap = actor->GetSubmeshMap();

	mappings = MakeShareable(new FJsonObject());
	TSharedRef<TJsonReader<TCHAR>> reader = TJsonReaderFactory<TCHAR>::Create(string);
	FJsonSerializer::Deserialize(reader, mappings);

	numSubmeshes = mappings->GetIntegerField(TEXT("numberOfIDs"));

	auto maps = mappings->GetArrayField(TEXT("mapping"));
	for (int32 i = 0; i < maps.Num(); i++)
	{
		auto mapping = maps[i]->AsObject();
		auto name = mapping->GetStringField(TEXT("Name"));

		auto globalIndex = ActorIdMap.AddUnique(name);
		LocalToActorSubmeshMap.Add(globalIndex);
	}

	auto materials = mappings->GetArrayField(TEXT("appearance"));
	for (auto material : materials)
	{
		auto name = material->AsObject()->GetStringField(TEXT("name"));
		Material m;

		auto materialJson = material->AsObject()->GetObjectField(TEXT("material"));
		
		m.diffuse = ParseJsonColour(materialJson->GetStringField(TEXT("diffuseColor")));

		if (materialJson->HasField(TEXT("specularColor")))
		{
			m.specular = ParseJsonColour(materialJson->GetStringField(TEXT("specularColor")));
		}
		if(materialJson->HasField(TEXT("transparency")))
		{
			FDefaultValueHelper::ParseFloat(materialJson->GetStringField(TEXT("transparency")), m.transparency);
		}

		mappingMaterials.Add(name, m);
	}

	hasTransparency = false;
	for (auto material : mappingMaterials)
	{
		if (material.Value.transparency != 0)
		{
			hasTransparency = true;
		}
	}

	for (int32 i = 0; i < maps.Num(); i++)
	{
		auto mapping = maps[i]->AsObject();
		auto name = mapping->GetStringField(TEXT("Name")); // in this format (.json.mpc) name is the id
		auto appearance = mapping->GetStringField(TEXT("appearance"));
		auto material = mappingMaterials[appearance];

		material.diffuse.A = 1.0 - material.transparency;

		actor->DiffuseMap->SetParameter(name, material.diffuse);
	}

	// Every time the parameters change we update all map components, not only the ones we explicitly know of,
	// to ensure the materials are initialised to sensible values.
	for (auto component : actor->GetComponents())
	{
		auto map = Cast<URepoSupermeshMapComponent>(component);
		if (map) {
			map->UpdateTexture();
		}
	}

	INC_DWORD_STAT_BY(STAT_TotalObjects, maps.Num())
}

void RepoSrcAssetImporter::HandleSrc(const TArray<uint8>& src)
{
	SCOPE_CYCLE_COUNTER(STAT_HandleSRC);

	auto data = (const uint8*)src.GetData();

	auto preamble = (const uint32*)(data);
	auto srcMagicBit = preamble[0];
	auto srcVersion = preamble[1];
	auto jsonByteSize = preamble[2];

	if (srcMagicBit != 23 && srcMagicBit != 24)
	{
		UE_LOG(LogTemp, Error, TEXT("SRC Magic Bit Mismatch. Expected 23 or 24 but received %d. Import will be aborted."), srcMagicBit);
		return;
	}
	if (srcVersion != 42)
	{
		UE_LOG(LogTemp, Error, TEXT("SRC Magic Bit Mismatch. Expected 42 but received %d. Import will be aborted."), srcVersion);
		return;
	}

	bool isCompressed = false;
	if (srcMagicBit == 24)
	{
		isCompressed = true;
	}

	buffer = data + 12 + jsonByteSize;

	if (isCompressed) //zlib buffer compression is enabled
	{
		auto uncompressedSize = ((uint32_t*)buffer)[0];
		buffer += 4;
		auto compressedSize = src.Num() - 12 - jsonByteSize - 4;

		//check() compressedSize

		auto uncompressed = FMemory::Malloc(uncompressedSize);

		FCompression::UncompressMemory(
			NAME_Zlib,
			uncompressed,
			uncompressedSize,
			buffer,
			compressedSize);

		buffer = (uint8_t*)uncompressed;

		INC_MEMORY_STAT_BY(STAT_Uncompressed, uncompressedSize)
	}

	const std::string cstr(reinterpret_cast<const char*>(data + 12), jsonByteSize); // exporter explicitly uses 12
	auto jsonStr = FString(cstr.c_str());
	header = MakeShareable(new FJsonObject());
	TSharedRef<TJsonReader<TCHAR>> reader = TJsonReaderFactory<TCHAR>::Create(jsonStr);
	FJsonSerializer::Deserialize(reader, header);

	indexViews = header->GetObjectField(TEXT("accessors"))->GetObjectField(TEXT("indexViews"));
	attributeViews = header->GetObjectField(TEXT("accessors"))->GetObjectField(TEXT("attributeViews"));
	bufferViews = header->GetObjectField(TEXT("bufferViews"));
	bufferChunks = header->GetObjectField(TEXT("bufferChunks"));

	// A 3D Repo scene will have multiple meshes, each delivered as a separate SRC. Within the SRC there are multiple meshes
	// with correspond to the split parts of the SRC's scene mesh.

	auto meshes = header->GetObjectField(TEXT("meshes"))->Values;

	UE_LOG(LogTemp, Log, TEXT("Creating %d Procedural Meshes for %s."), meshes.Num(), *Uri);

	for (auto mesh_field : meshes)
	{
		SCOPE_CYCLE_COUNTER(STAT_GenerateMesh);

		auto object = mesh_field.Value->AsObject();
		auto attributes = object->GetObjectField(TEXT("attributes"));

		TArray<int32> triangles;
		ResolveIndices(object->GetStringField(TEXT("indices")), triangles);


		TArray<FVector> vertices;
		if (attributes->HasField(TEXT("position")))
		{
			ResolveAttribute(attributes->GetStringField(TEXT("position")), vertices);
		}

		TArray<FVector> normals;
		if (attributes->HasField(TEXT("normal")))
		{
			ResolveAttribute(attributes->GetStringField(TEXT("normal")), normals);
		}

		TArray<FVector2D> uv0;
		if (attributes->HasField(TEXT("texcoord")))
		{
			ResolveAttribute(attributes->GetStringField(TEXT("texcoord")), uv0);
		}

		//Ids are indices into the 'mapping' array provided by the counterpart .json.mpc file.

		TArray<float> ids;
		if (attributes->HasField(TEXT("id")))
		{
			ResolveAttribute(attributes->GetStringField(TEXT("id")), ids);
		}

		TransformCoordinateSystem(vertices);
		TransformCoordinateSystem(normals);

		TArray<FLinearColor> vertexColors; // Empty arrays
		TArray<FProcMeshTangent> tangents;
		TArray<FVector2D> uv1;
		TArray<FVector2D> uv2;
		TArray<FVector2D> uv3;

		GenerateSupermeshMapIndices(ids, uv1); // SupermeshMapIndices relative to the Supermesh itself, and the Actor

		auto mesh = actor->AddProceduralMesh();
		mesh->SetRelativeLocation(Offset);
		mesh->CreateMeshSection_LinearColor(0, vertices, triangles, normals, uv0, uv1, uv2, uv3, vertexColors, tangents, true);

		mesh->SetCollisionProfileName(FName("IgnoreOnlyPawn"));

		actor->MeshComponentTriangleMaps.Add(mesh, TArray<int>());
		auto triangleIdMap = actor->MeshComponentTriangleMaps.Find(mesh); // This slightly odd way of getting and passing the array is to avoid a copy when adding to the map
		GenerateTriangleIdMap(triangles, ids, *triangleIdMap);

		UMaterialInterface* materialPrototype = nullptr;

		if (hasTransparency)
		{
			materialPrototype = materialTranslucent;
		}
		else
		{
			materialPrototype = materialOpaque;
		}

		if (materialPrototype) 
		{
			auto material = UMaterialInstanceDynamic::Create(materialPrototype, mesh);
			
			for (auto component : actor->GetComponents())
			{
				auto map = Cast<URepoSupermeshMapComponent>(component);
				if (map) {
					map->ApplyTextureToMaterials(material);
				}
			}

			mesh->SetMaterial(0, material);
		}

		Bounds += mesh->CalcLocalBounds().TransformBy(mesh->GetComponentTransform()).GetBox();

		INC_DWORD_STAT_BY(STAT_TotalTriangles, triangles.Num() / 3)
		INC_DWORD_STAT_BY(STAT_TotalVertices, vertices.Num())
	}

	if (isCompressed)
	{
		FMemory::Free((void*)buffer);
	}

	buffer = nullptr;

	UE_LOG(LogTemp, Log, TEXT("Finished SRC %s"), *Uri);
}

FVector RepoSrcAssetImporter::TransformCoordinateSystem(FVector v)
{
	auto tmp = v.Z;
	v.Z = v.Y;
	v.Y = -tmp;
	v.X = -v.X;
	return v;
}

void RepoSrcAssetImporter::TransformCoordinateSystem(TArray<FVector>& array) // from Unity to Unreal
{
	for (int32 i = 0; i < array.Num(); i++)
	{
		auto tmp = array[i].Z;
		array[i].Z = array[i].Y;
		array[i].Y = -tmp;
		array[i].X = -array[i].X;
	}
}

void RepoSrcAssetImporter::GenerateSupermeshMapIndices(TArray<float>& ids, TArray<FVector2D>& uvs)
{
	uvs.SetNumUninitialized(ids.Num());
	for (int32 i = 0; i < ids.Num(); i++)
	{
		uvs[i].X = (float)ids[i];
		uvs[i].Y = (float)LocalToActorSubmeshMap[ids[i]];
	}
}

void RepoSrcAssetImporter::GenerateTriangleIdMap(TArray<int>& triangles, TArray<float>& ids, TArray<int>& triangleIdMap)
{
	auto numTriangles = triangles.Num() / 3;
	triangleIdMap.SetNumUninitialized(numTriangles);
	for (int32 i = 0; i < numTriangles; i++)
	{
		auto index0 = triangles[i * 3];
		auto localId = ids[index0];
		auto globalId = LocalToActorSubmeshMap[localId];
		triangleIdMap[i] = globalId;
	}
}

#pragma optimize("", on)

void RepoSrcAssetImporter::ResolveIndices(const FString& viewName, TArray<int32>& array)
{
	auto indexView = indexViews->GetObjectField(viewName);

	auto viewOffset = indexView->GetIntegerField(TEXT("byteOffset"));
	auto viewCount = indexView->GetIntegerField(TEXT("count"));
	auto viewComponentType = indexView->GetIntegerField(TEXT("componentType"));

	auto bufferView = bufferViews->GetObjectField(indexView->GetStringField(TEXT("bufferView")));
	auto chunks = bufferView->GetArrayField(TEXT("chunks"));

	if (chunks.Num() != 1)
	{
		UE_LOG(LogTemp, Error, TEXT("Recevied SRC with bufferView having %d Chunks. This version only supports one chunk per bufferView."), chunks.Num());
		return;
	}

	auto bufferChunk = bufferChunks->GetObjectField(chunks[0]->AsString());

	auto chunkOffset = bufferChunk->GetIntegerField(TEXT("byteOffset"));
	auto chunkLength = bufferChunk->GetIntegerField(TEXT("byteLength"));

	if (viewOffset + (sizeof(uint16) * viewCount) != chunkLength)
	{
		UE_LOG(LogTemp, Error, TEXT("Buffer chunk length mismatch. Possible corruption."));
		return;
	}

	array.SetNumUninitialized(viewCount);

	auto data = (const uint16*)(buffer + chunkOffset + viewOffset);
	for (int32 i = 0; i < viewCount; i++)
	{
		array[i] = data[i]; // use for loop here because we are casting uint16 to uint32
	}
}

template <typename T>
void RepoSrcAssetImporter::ResolveAttribute(const FString& viewName, TArray<T>& array)
{
	auto attributeView = attributeViews->GetObjectField(viewName);

	auto viewOffset = attributeView->GetIntegerField(TEXT("byteOffset"));
	auto viewStride = attributeView->GetIntegerField(TEXT("byteStride"));
	auto viewComponentType = attributeView->GetIntegerField(TEXT("componentType"));
	auto viewType = attributeView->GetStringField(TEXT("type"));
	auto viewCount = attributeView->GetIntegerField(TEXT("count"));

	// Assume the offset and scale are identity.

	if (viewStride != sizeof(T))
	{
		UE_LOG(LogTemp, Error, TEXT("Attributes SRC byte stride does not match Array."));
	}

	auto bufferView = bufferViews->GetObjectField(attributeView->GetStringField(TEXT("bufferView")));
	auto chunks = bufferView->GetArrayField(TEXT("chunks"));

	if (chunks.Num() != 1)
	{
		UE_LOG(LogTemp, Error, TEXT("Recevied SRC with bufferView having %d Chunks. This version only supports one chunk per bufferView."), chunks.Num());
		return;
	}

	auto bufferChunk = bufferChunks->GetObjectField(chunks[0]->AsString());

	auto chunkOffset = bufferChunk->GetIntegerField(TEXT("byteOffset"));
	auto chunkLength = bufferChunk->GetIntegerField(TEXT("byteLength"));

	if (viewOffset + (viewStride * viewCount) != chunkLength)
	{
		UE_LOG(LogTemp, Error, TEXT("Buffer chunk length mismatch. Possible corruption."));
		return;
	}

	auto data = (buffer + chunkOffset + viewOffset);
	array.SetNumUninitialized(viewCount);
	memcpy(array.GetData(), data, chunkLength);
}