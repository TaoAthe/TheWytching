#include "CognitiveMapJsonLibrary.h"

#include "Dom/JsonObject.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	static FString GetCognitiveJsonPath()
	{
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Cognitive"),
			TEXT("CognitiveMap.json"));
	}

	static bool ParseEntryObject(
		const FString& ActorKey,
		const FString& JsonEntry,
		TSharedPtr<FJsonObject>& OutObject)
	{
		TSharedPtr<FJsonObject> Parsed;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonEntry);
		if (FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid())
		{
			if (!Parsed->HasTypedField<EJson::String>(TEXT("ActorName")))
			{
				Parsed->SetStringField(TEXT("ActorName"), ActorKey);
			}
			OutObject = Parsed;
			return true;
		}

		TSharedPtr<FJsonObject> Fallback = MakeShared<FJsonObject>();
		Fallback->SetStringField(TEXT("ActorName"), ActorKey);
		Fallback->SetStringField(TEXT("RawValue"), JsonEntry);
		OutObject = Fallback;
		return false;
	}

	static FString JoinTags(const TSharedPtr<FJsonObject>& Entry)
	{
		const TArray<TSharedPtr<FJsonValue>>* TagValues = nullptr;
		if (!Entry.IsValid() || !Entry->TryGetArrayField(TEXT("Tags"), TagValues) || TagValues == nullptr)
		{
			return TEXT("-");
		}

		TArray<FString> Tags;
		for (const TSharedPtr<FJsonValue>& Value : *TagValues)
		{
			if (Value.IsValid() && Value->Type == EJson::String)
			{
				Tags.Add(Value->AsString());
			}
		}
		return Tags.Num() > 0 ? FString::Join(Tags, TEXT(", ")) : TEXT("-");
	}
}

bool UCognitiveMapJsonLibrary::SaveCognitiveMapToJsonFile(
	const TMap<FString, FString>& EntriesMap,
	FString& OutFilePath)
{
	OutFilePath = GetCognitiveJsonPath();
	const FString Directory = FPaths::GetPath(OutFilePath);
	IFileManager::Get().MakeDirectory(*Directory, true);

	TArray<FString> Keys;
	EntriesMap.GetKeys(Keys);
	Keys.Sort();

	TArray<TSharedPtr<FJsonValue>> SeenEntries;
	for (const FString& Key : Keys)
	{
		const FString* Value = EntriesMap.Find(Key);
		if (!Value)
		{
			continue;
		}

		TSharedPtr<FJsonObject> EntryObject;
		ParseEntryObject(Key, *Value, EntryObject);
		SeenEntries.Add(MakeShared<FJsonValueObject>(EntryObject));
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetArrayField(TEXT("SeenEntries"), SeenEntries);
	Root->SetNumberField(TEXT("Count"), SeenEntries.Num());
	Root->SetStringField(TEXT("UpdatedAtUtc"), FDateTime::UtcNow().ToIso8601());

	FString OutputJson;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputJson);
	if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
	{
		return false;
	}

	return FFileHelper::SaveStringToFile(
		OutputJson,
		*OutFilePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool UCognitiveMapJsonLibrary::BuildSensorReadoutFromJsonFile(
	FString& OutReadout,
	FString& OutFilePath,
	int32& OutCount)
{
	OutFilePath = GetCognitiveJsonPath();
	OutReadout = TEXT("NO SENSOR DATA");
	OutCount = 0;

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *OutFilePath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* SeenEntries = nullptr;
	if (!Root->TryGetArrayField(TEXT("SeenEntries"), SeenEntries) || SeenEntries == nullptr)
	{
		return false;
	}

	TArray<FString> Lines;
	int32 Index = 1;
	for (const TSharedPtr<FJsonValue>& EntryValue : *SeenEntries)
	{
		if (!EntryValue.IsValid() || EntryValue->Type != EJson::Object)
		{
			continue;
		}

		const TSharedPtr<FJsonObject> Entry = EntryValue->AsObject();
		if (!Entry.IsValid())
		{
			continue;
		}

		const FString ActorName = Entry->GetStringField(TEXT("ActorName"));
		const FString Tags = JoinTags(Entry);

		double Distance = 0.0;
		Entry->TryGetNumberField(TEXT("Distance"), Distance);

		FString Location = TEXT("-");
		Entry->TryGetStringField(TEXT("Location"), Location);

		FString Material = TEXT("-");
		Entry->TryGetStringField(TEXT("Material"), Material);

		Lines.Add(FString::Printf(TEXT("[%02d] %s"), Index, *ActorName));
		Lines.Add(FString::Printf(TEXT("  TAGS: %s"), *Tags));
		Lines.Add(FString::Printf(TEXT("  DIST: %.1f"), Distance));
		Lines.Add(FString::Printf(TEXT("  LOC : %s"), *Location));
		Lines.Add(FString::Printf(TEXT("  MAT : %s"), *Material));
		Lines.Add(TEXT(""));
		++Index;
	}

	OutCount = Index - 1;
	OutReadout = OutCount > 0
		? FString::Join(Lines, TEXT("\n"))
		: TEXT("NO SENSOR TARGETS");
	return true;
}

