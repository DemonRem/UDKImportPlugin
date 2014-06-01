#include "UDKImportPluginPrivatePCH.h"
#include "T3DParser.h"

float T3DParser::UnrRotToDeg = 0.00549316540360483;
float T3DParser::IntensityMultiplier = 5000;

T3DParser::T3DParser(const FString &T3DText)
{
	CurrentRequirement = NULL;
	World = NULL;
	LineIndex = 0;
	ParserLevel = 0;
	T3DText.ParseIntoArray(&Lines, TEXT("\n"), true);
}

bool T3DParser::NextLine()
{
	if (LineIndex < Lines.Num())
	{
		Line = Lines[LineIndex].Trim().TrimTrailing();
		++LineIndex;
		return true;
	}
	return false;
}

bool T3DParser::IgnoreSubObjects()
{
	while (Line.StartsWith(TEXT("Begin Object "), ESearchCase::CaseSensitive))
	{
		JumpToEnd();
		if (!NextLine())
			return false;
	}

	return true;
}

bool T3DParser::IgnoreSubs()
{
	while (Line.StartsWith(TEXT("Begin "), ESearchCase::CaseSensitive))
	{
		JumpToEnd();
		if (!NextLine())
			return false;
	}

	return true;
}

void T3DParser::JumpToEnd()
{
	int32 Level = 1;
	while (NextLine())
	{
		if (Line.StartsWith(TEXT("Begin "), ESearchCase::CaseSensitive))
		{
			++Level;
		}
		else if (Line.StartsWith(TEXT("End "), ESearchCase::CaseSensitive))
		{
			--Level;
			if (Level == 0)
				break;
		}
	}
}

bool T3DParser::IsBeginObject(FString &Class, FString &Name)
{
	if (Line.StartsWith(TEXT("Begin Object "), ESearchCase::CaseSensitive))
	{
		GetOneValueAfter(TEXT(" Class="), Class);
		return true;
	}
	return false;
}

bool T3DParser::IsEndObject()
{
	return Line.Equals(TEXT("End Object"));
}

bool T3DParser::GetOneValueAfter(const FString &Key, FString &Value)
{
	int32 start = Line.Find(Key, ESearchCase::CaseSensitive, ESearchDir::FromStart, 0);
	if (start != -1)
	{
		start += Key.Len();
		int32 end = Line.Find(TEXT(" "), ESearchCase::CaseSensitive, ESearchDir::FromStart, start);
		if (end == -1)
			end = Line.Len();
		Value = Line.Mid(start, end - start);

		return true;
	}
	return false;
}

bool T3DParser::GetProperty(const FString &Key, FString &Value)
{
	if (Line.StartsWith(Key, ESearchCase::CaseSensitive))
	{
		Value = Line.Mid(Key.Len());
		return true;
	}
	return false;
}

void T3DParser::RegisterObject(const FString &UDKObjectName, UObject * Object)
{

}

void T3DParser::AddRequirement(const FString &UDKRequiredObjectName, FExecuteAction Action)
{

}

bool T3DParser::ParseUDKRotation(const FString &InSourceString, FRotator &Rotator)
{
	int32 Pitch = 0;
	int32 Yaw = 0;
	int32 Roll = 0;

	const bool bSuccessful = FParse::Value(*InSourceString, TEXT("Pitch="), Pitch) && FParse::Value(*InSourceString, TEXT("Yaw="), Yaw) && FParse::Value(*InSourceString, TEXT("Roll="), Roll);

	Rotator.Pitch = Pitch * UnrRotToDeg;
	Rotator.Yaw = Yaw * UnrRotToDeg;
	Rotator.Roll = Roll * UnrRotToDeg;

	return bSuccessful;

}

bool T3DParser::IsActorLocation(AActor * Actor)
{
	FString Value;
	if (GetProperty(TEXT("Location="), Value))
	{
		FVector Location;
		ensure(Location.InitFromString(Value));
		Actor->SetActorLocation(Location);
		return true;
	}

	return false;
}

bool T3DParser::IsActorRotation(AActor * Actor)
{
	FString Value;
	if (GetProperty(TEXT("Rotation="), Value))
	{
		FRotator Rotator;
		ensure(ParseUDKRotation(Value, Rotator));
		Actor->SetActorRotation(Rotator);
		return true;
	}

	return false;
}

bool T3DParser::IsActorScale(AActor * Actor)
{
	FString Value;
	if (GetProperty(TEXT("DrawScale="), Value))
	{
		float DrawScale = FCString::Atof(*Value);
		Actor->SetActorScale3D(Actor->GetActorScale() * DrawScale);
		return true;
	}
	else if (GetProperty(TEXT("DrawScale3D="), Value))
	{
		FVector DrawScale3D;
		ensure(DrawScale3D.InitFromString(Value));
		Actor->SetActorScale3D(Actor->GetActorScale() * DrawScale3D);
		return true;
	}

	return false;
}

template<class T>
T * T3DParser::SpawnActor()
{
	if (World == NULL)
	{
		FLevelEditorModule & LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		World = LevelEditorModule.GetFirstLevelEditor().Get()->GetWorld();
	}
	ensure(World != NULL);

	return World->SpawnActor<T>();
}

void T3DParser::ImportLevel()
{
	FString Class, Name;

	ensure(NextLine());
	ensure(Line.Equals(TEXT("Begin Object Class=Level Name=PersistentLevel")));

	while (NextLine() && !IsEndObject())
	{
		if (IsBeginObject(Class, Name))
		{
			UObject * Object = 0;
			if (Class.Equals(TEXT("StaticMeshActor")))
				ImportStaticMeshActor();
			else if (Class.Equals(TEXT("Brush")))
				ImportBrush();
			else if (Class.Equals(TEXT("PointLight")))
				ImportPointLight();
			else if (Class.Equals(TEXT("SpotLight")))
				ImportSpotLight();
			else
				JumpToEnd();
		}
	}
}

void T3DParser::ImportBrush()
{
	FString Value, Class, Name;
	ABrush * Brush = SpawnActor<ABrush>();

	while (NextLine() && !IsEndObject())
	{
		if (Line.StartsWith(TEXT("Begin Brush ")))
		{
			while (NextLine() && !Line.StartsWith(TEXT("End Brush")))
			{
				if (Line.StartsWith(TEXT("Begin PolyList")))
				{
					ImportPolyList(Brush->Brush->Polys);
				}
			}
		}
		else if (IsActorLocation(Brush))
		{
			continue;
		}
	}
}

void T3DParser::ImportPolyList(UPolys * Polys)
{
	while (NextLine() && !Line.StartsWith(TEXT("End PolyList")))
	{
		if (Line.StartsWith(TEXT("Begin Polygon ")))
		{
			bool GotBase = false;
			FPoly Poly;
			while (NextLine() && !Line.StartsWith(TEXT("End Polygon")))
			{
				const TCHAR* Str = *Line;
				if (FParse::Command(&Str, TEXT("ORIGIN")))
				{
					GotBase = true;
					GetFVECTOR(Str, Poly.Base);
				}
				else if (FParse::Command(&Str, TEXT("VERTEX")))
				{
					FVector TempVertex;
					GetFVECTOR(Str, TempVertex);
					new(Poly.Vertices) FVector(TempVertex);
				}
				else if (FParse::Command(&Str, TEXT("TEXTUREU")))
				{
					GetFVECTOR(Str, Poly.TextureU);
				}
				else if (FParse::Command(&Str, TEXT("TEXTUREV")))
				{
					GetFVECTOR(Str, Poly.TextureV);
				}
				else if (FParse::Command(&Str, TEXT("NORMAL")))
				{
					GetFVECTOR(Str, Poly.Normal);
				}
				else if (GetEND(&Str, TEXT("POLYGON")))
				{
					if (!GotBase)
						Poly.Base = Poly.Vertices[0];
					if (Poly.Finalize(NULL, 1) == 0)
						new(Polys->Element)FPoly(Poly);
					GotBase = 0;
				}
			}
			if (!GotBase)
				Poly.Base = Poly.Vertices[0];
			new(Polys->Element)FPoly(Poly);
		}
	}
}

void T3DParser::ImportPointLight()
{
	FString Value, Class, Name;
	APointLight* PointLight = SpawnActor<APointLight>();

	while (NextLine() && !IsEndObject())
	{
		if (IsBeginObject(Class, Name))
		{
			if (Class.Equals(TEXT("SpotLightComponent")))
			{
				while (NextLine() && IgnoreSubs() && !IsEndObject())
				{
					if (GetProperty(TEXT("Radius="), Value))
					{
						PointLight->PointLightComponent->AttenuationRadius = FCString::Atof(*Value);
					}
					else if (GetProperty(TEXT("Brightness="), Value))
					{
						PointLight->PointLightComponent->Intensity = FCString::Atof(*Value) * IntensityMultiplier;
					}
					else if (GetProperty(TEXT("LightColor="), Value))
					{
						FColor Color;
						Color.InitFromString(Value);
						PointLight->PointLightComponent->LightColor = Color;
					}
				}
			}
			else
			{
				JumpToEnd();
			}
		}
		else if (IsActorLocation(PointLight) || IsActorRotation(PointLight))
		{
			continue;
		}
	}
}

void T3DParser::ImportSpotLight()
{
	FString Value, Class, Name;
	ASpotLight* SpotLight = SpawnActor<ASpotLight>();

	while (NextLine() && !IsEndObject())
	{
		if (IsBeginObject(Class, Name))
		{
			if (Class.Equals(TEXT("SpotLightComponent")))
			{
				while (NextLine() && IgnoreSubs() && !IsEndObject())
				{
					if (GetProperty(TEXT("Radius="), Value))
					{
						SpotLight->SpotLightComponent->AttenuationRadius = FCString::Atof(*Value);
					}
					else if (GetProperty(TEXT("InnerConeAngle="), Value))
					{
						SpotLight->SpotLightComponent->InnerConeAngle = FCString::Atof(*Value);
					}
					else if (GetProperty(TEXT("OuterConeAngle="), Value))
					{
						SpotLight->SpotLightComponent->OuterConeAngle = FCString::Atof(*Value);
					}
					else if (GetProperty(TEXT("Brightness="), Value))
					{
						SpotLight->SpotLightComponent->Intensity = FCString::Atof(*Value) * IntensityMultiplier;
					}
					else if (GetProperty(TEXT("LightColor="), Value))
					{
						FColor Color;
						Color.InitFromString(Value);
						SpotLight->SpotLightComponent->LightColor = Color;
					}
				}
			}
			else
			{
				JumpToEnd();
			}
		}
		else if (IsActorLocation(SpotLight) || IsActorRotation(SpotLight))
		{
			continue;
		}
	}
}

void T3DParser::ImportStaticMeshActor()
{
	FString Value, Class, Name;
	AStaticMeshActor * StaticMeshActor = SpawnActor<AStaticMeshActor>();
	
	while (NextLine() && !IsEndObject())
	{
		if (IsBeginObject(Class, Name))
		{
			if (Class.Equals(TEXT("StaticMeshComponent")))
			{
				while (NextLine() && !IsEndObject())
				{
					if (GetProperty(TEXT("StaticMesh="), Value))
					{
						AddRequirement(Value, FExecuteAction::CreateRaw(this, &T3DParser::SetStaticMesh, StaticMeshActor->StaticMeshComponent.Get()));
					}
				}
			}
			else
			{
				JumpToEnd();
			}
		}
		else if (IsActorLocation(StaticMeshActor) || IsActorRotation(StaticMeshActor) || IsActorScale(StaticMeshActor))
		{
			continue;
		}
	}
}

USoundCue * T3DParser::ImportSoundCue()
{
	USoundCue * SoundCue = 0;
	FString Value;

	while (NextLine())
	{
		if (GetProperty(TEXT("SoundClass="), Value))
		{
			// TODO
		}
		else if (GetProperty(TEXT("FirstNode="), Value))
		{
			AddRequirement(Value, FExecuteAction::CreateRaw(this, &T3DParser::SetSoundCueFirstNode, SoundCue));
		}
	}

	return SoundCue;
}

void T3DParser::SetStaticMesh(UStaticMeshComponent * StaticMeshComponent)
{
	StaticMeshComponent->SetStaticMesh(Cast<UStaticMesh>(CurrentRequirement));
}

void T3DParser::SetSoundCueFirstNode(USoundCue * SoundCue)
{
	SoundCue->FirstNode = Cast<USoundNode>(CurrentRequirement);
}