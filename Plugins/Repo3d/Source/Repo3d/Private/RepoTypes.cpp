#include "RepoTypes.h"
#include "Misc/DefaultValueHelper.h"

Version::Version()
{
	Major = 0;
	Minor = 0;
	Revision = 0;
}

Version Version::Parse(const FString& versionString)
{
	TArray<FString> elements;
	versionString.ParseIntoArray(elements, TEXT("."));
	auto Version = Version::Version();
	if (elements.Num() > 0)
	{
		FDefaultValueHelper::ParseInt(elements[0], Version.Major);
	}
	if (elements.Num() > 1)
	{
		FDefaultValueHelper::ParseInt(elements[1], Version.Minor);
	}
	if (elements.Num() > 2)
	{
		FDefaultValueHelper::ParseInt(elements[2], Version.Revision);
	}
	return Version;
}