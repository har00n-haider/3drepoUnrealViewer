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
#include "Math/UnitConversion.h"
#include "RepoTypes.h"

/*
 * This class implements wrappers around the 3D Repo Web API and returns parsed results as native types.
 * The web API documentation can be found at https://3drepo.github.io/3drepo.io/
 * This class is for the internal use only, and the reflection is not guaranteed to be comprehensive or
 * complete.
 * Parsing web API responses is the responsibility of client code. See the SampleWebRequestHelper for
 * how this may be done.
 */
class REPO3D_API RepoWebRequestHelpers
{
public:
	class ModelSettings 
	{
	public:
		EUnit Units;
	};
	DECLARE_DELEGATE_OneParam(ModelSettingsDelegate, TSharedRef<ModelSettings>);
	static void GetModelSettings(TSharedRef<class RepoWebRequestManager> manager, const FString& teamspace, const FString& model, ModelSettingsDelegate callback);


	class ApplicationVersion
	{
	public:
		Version Current;
		TArray<Version> Supported;
	};
	DECLARE_DELEGATE_OneParam(ApplicationVersionDelegate, TSharedRef<ApplicationVersion>)
	static void GetApplicationVersion(TSharedRef<class RepoWebRequestManager> manager, ApplicationVersionDelegate callback);
};