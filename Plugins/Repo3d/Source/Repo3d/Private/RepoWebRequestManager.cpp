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

#include "RepoWebRequestManager.h"
#include "Repo3d.h"
#include "RepoTypes.h"
#include "RepoWebRequestHelpers.h"

DECLARE_MEMORY_STAT(TEXT("Downloaded"), STAT_Downloaded, STATGROUP_Repo3D);

void RepoWebRequestManager::SetApiKey(FString apikey)
{
	ApiKey = apikey;
}

#pragma optimize("", off)

void RepoWebRequestManager::SetHost(FString host) 
{
	State = Status::Configuring;
	Host = host;

	auto PluginVersion = Version::Parse(Owner->Plugin->GetDescriptor().VersionName);
	RepoWebRequestHelpers::GetApplicationVersion(Owner->GetWebRequestManager(), // This is the TSharedRef equivalent of "this"
		RepoWebRequestHelpers::ApplicationVersionDelegate::CreateLambda(
			[this,PluginVersion](TSharedRef<RepoWebRequestHelpers::ApplicationVersion> version)
			{
				if (version->Current == PluginVersion)
				{
					// Nothing to do
					UpdateState(Status::Authenticated);
				}
				else if (version->Supported.Contains(PluginVersion))
				{
					UpdateState(Status::Authenticated);
					Owner->LogWarning("Your version of 3D Repo for Unreal is supported, but there is a new version available.");
				}
				else
				{
					UpdateState(Status::NotSupported);
					Owner->LogError("Your version of 3D Repo for Unreal is unsupported and will not work. Please upgrade your copy of 3D Repo for Unreal.");
				}
			}
	));
}

void RepoWebRequestManager::UpdateState(Status newState)
{
	State = newState;
	if (State >= Status::Authenticated)
	{
		// We have just authenticated; issue any pending requests
		for (auto Request : Requests)
		{
			GetRequest(Request);
		}
		Requests.Reset();
	}
}

void RepoWebRequestManager::GetRequest(RepoWebRequest Request)
{
	switch (State)
	{
	case Status::None:			// Until the server is configured, queue up any requests
	case Status::Configuring:
	case Status::NotSupported:	// The error message should have already been displayed, so we don't do anything
		Requests.Add(Request);  // (Even if this host is unsupported, the user could still reconnect to a supported one...)
		break;
	case Status::Authenticated:
		GetRequestSync(Request);
		break;
	}
}

void RepoWebRequestManager::GetRequestSync(RepoWebRequest Request)
{
	FHttpModule::Get().SetHttpTimeout(3600); // the default timeout of 160 seconds is not enough for many models.
	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetVerb("GET");

	FString postfix;

	if (!ApiKey.IsEmpty())
	{
		postfix = FString::Printf(TEXT("?key=%s"), *ApiKey);
	}

	auto timestamp = FPlatformTime::Seconds();
	auto callback = Request.callback; // local variable for closure capture

	HttpRequest->SetURL(FString::Printf(TEXT("http://%s/api/%s%s"), *Host, *(Request.uri), *postfix));
	HttpRequest->OnProcessRequestComplete().BindLambda(
		[callback, timestamp](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) // note that Unreal should support binding delegates directly, but this function appears to be missing https://docs.unrealengine.com/en-US/Programming/UnrealArchitecture/Delegates/index.html
		{
			auto result = MakeShared<RepoWebResponse>();
			result->bWasSuccessful = bWasSuccessful;
			result->Request = Request;
			result->Response = Response;
			result->Time = FPlatformTime::Seconds() - timestamp;
			INC_MEMORY_STAT_BY(STAT_Downloaded, Response->GetContent().Num());
			callback.ExecuteIfBound(result);
		});
	HttpRequest->ProcessRequest();
}

void RepoWebRequestManager::GetRequest(FString uri, RepoWebRequestDelegate callback)
{
	RepoWebRequest Request;
	Request.callback = callback;
	Request.uri = uri;
	GetRequest(Request);
}

FString RepoWebRequestManager::MakeURI(FString teamspace, FString model, FString revision, FString asset)
{
	if (revision == "")
	{
		revision = TEXT("master/head");
	}
	return (FString::Printf(TEXT("%s/%s/revision/%s/%s"), *teamspace, *model, *revision, *asset));
}

#pragma optimize("", on)