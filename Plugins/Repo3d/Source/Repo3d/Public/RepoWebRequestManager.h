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
#include "Http.h"
#include "HttpModule.h"

class RepoWebResponse
{
public:
	FHttpRequestPtr Request;
	FHttpResponsePtr Response;
	bool bWasSuccessful;
	uint32 Time;
};

DECLARE_DELEGATE_OneParam(RepoWebRequestDelegate, TSharedPtr<RepoWebResponse>);

class RepoWebRequest
{
public:
	FString uri;
	RepoWebRequestDelegate callback;
};

class Repo3d;
class RepoWebRequestHelpers;

class REPO3D_API RepoWebRequestManager
{
	friend class RepoWebRequestHelpers;
public:
	RepoWebRequestManager(Repo3d* owner)
		:Owner(owner) // the Repo3D instance owns the manager, so the manager will go away before the repo instance
	{
		State = Status::None;
	}

	enum Status {
		None = 0,
		Configuring = 1,
		Authenticated = 2,
		NotSupported = -1
	};

	void GetRequest(FString uri, RepoWebRequestDelegate callback);
	void SetHost(FString host);
	void SetApiKey(FString apikey);

	Status GetState()
	{
		return State;
	}

	/*
	 * Convenience function to return URIs of the form /:teamspace/:model/revision/:revision/:asset.
	 * Will replace empty revisions with 'master/head'.
	 */
	static FString MakeURI(FString teamspace, FString model, FString revision, FString asset);

private:
	// The plugin that hosts this manager
	Repo3d* Owner;

	// Pending requests. New requests will be held here until the manager is authenticated.
	TArray<RepoWebRequest> Requests; 

	Status State;

	FString Host;
	FString ApiKey;

	void GetRequest(RepoWebRequest Request);
	void GetRequestSync(RepoWebRequest Request);
	void UpdateState(Status newState);
};