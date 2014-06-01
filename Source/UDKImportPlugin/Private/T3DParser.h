#pragma once

struct T3DParser
{
public:
	T3DParser(const FString &T3DText);
	void ImportLevel();

private:
	static float UnrRotToDeg;
	static float IntensityMultiplier;

	TArray<FString> Lines;
	FString Line;

	int32 LineIndex, ParserLevel;
	UObject * CurrentRequirement;
	UWorld * World;

	bool NextLine();
	bool IgnoreSubs();
	bool IgnoreSubObjects();
	void JumpToEnd();
	bool IsBeginObject(FString &Class, FString &Name);
	bool IsEndObject();
	bool GetOneValueAfter(const FString &Key, FString &Value);
	bool GetProperty(const FString &Key, FString &Value);
	void RegisterObject(const FString &UDKObjectName, UObject * Object);
	void AddRequirement(const FString &UDKRequiredObjectName, FExecuteAction Action);
	bool ParseUDKRotation(const FString &InSourceString, FRotator &Rotator);
	bool IsActorLocation(AActor * Actor);
	bool IsActorRotation(AActor * Actor);
	bool IsActorScale(AActor * Actor);

	template<class T>
	T * SpawnActor();

	void ImportBrush();
	void ImportPolyList(UPolys * Polys);
	void ImportStaticMeshActor();
	void ImportPointLight();
	void ImportSpotLight();
	USoundCue * ImportSoundCue();
	void SetStaticMesh(UStaticMeshComponent * StaticMeshComponent);
	void SetSoundCueFirstNode(USoundCue * SoundCue);
};