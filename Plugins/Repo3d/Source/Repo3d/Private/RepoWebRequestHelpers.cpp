#include "RepoWebRequestHelpers.h"
#include "RepoWebRequestManager.h"
#include "Json.h"
#include "Misc/DefaultValueHelper.h"

void RepoWebRequestHelpers::GetModelSettings(TSharedRef<RepoWebRequestManager> manager, const FString& teamspace, const FString& model, ModelSettingsDelegate callback)
{
	manager->GetRequest(
		FString::Printf(TEXT("%s/%s.json"), *teamspace, *model),
		RepoWebRequestDelegate::CreateLambda(
			[callback](TSharedPtr<RepoWebResponse> Result) {
				if (Result->bWasSuccessful) {
					auto string = Result->Response->GetContentAsString();
					auto reader = TJsonReaderFactory<TCHAR>::Create(string);
					TSharedPtr<FJsonObject> jsonResponse = MakeShareable(new FJsonObject());
					FJsonSerializer::Deserialize(reader, jsonResponse);

					auto settings = MakeShared<RepoWebRequestHelpers::ModelSettings>();

					auto Units = jsonResponse->GetObjectField(TEXT("properties"))->GetStringField(TEXT("unit"));
					if (Units == TEXT("cm"))
					{
						settings->Units = EUnit::Centimeters;
					}
					else if (Units == TEXT("m"))
					{
						settings->Units = EUnit::Meters;
					}
					else if (Units == TEXT("km"))
					{
						settings->Units = EUnit::Kilometers;
					}
					else
					{
						settings->Units = EUnit::Millimeters; // the default
					}

					callback.ExecuteIfBound(settings);
				}
			}));
}

void RepoWebRequestHelpers::GetApplicationVersion(TSharedRef<class RepoWebRequestManager> manager, ApplicationVersionDelegate callback)
{
	RepoWebRequest Request;
	Request.uri = TEXT("version");
	Request.callback = RepoWebRequestDelegate::CreateLambda(
			[callback](TSharedPtr<RepoWebResponse> Result) {
				if (Result->bWasSuccessful)
				{
					auto string = Result->Response->GetContentAsString();
					auto reader = TJsonReaderFactory<TCHAR>::Create(string);
					TSharedPtr<FJsonObject> jsonResponse = MakeShareable(new FJsonObject());
					FJsonSerializer::Deserialize(reader, jsonResponse);

					auto version = MakeShared<RepoWebRequestHelpers::ApplicationVersion>();

					auto jsonVersion = jsonResponse->GetObjectField(TEXT("unreal"));
					version->Current = Version::Parse(jsonVersion->GetStringField(TEXT("current")));
					for (auto& element : jsonVersion->GetArrayField(TEXT("supported")))
					{
						version->Supported.Add(Version::Parse(element->AsString()));
					}

					callback.ExecuteIfBound(version);
				}
			});
	manager->GetRequestSync(Request); // Version checking shouldn't wait on authentication, because it is part of the process
}