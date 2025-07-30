// Fill out your copyright notice in the Description page of Project Settings.


#include "AITagsEditorSubsystem.h"

#include "AssetCompilingManager.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"
#include "Misc/ScopedSlowTask.h"
#include "Engine/Texture2D.h"
#include "ObjectTools.h"
#include "TextureCompiler.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogAITagsEditor, Log, All);

namespace AITagsEditorUtils
{
	FString GetPythonExecutablePath()
	{
		// UE_LEARNING_CHECKF(PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX, TEXT("Python only supported on Windows, Mac, and Linux."));
		return FPaths::ProjectIntermediateDir() / TEXT("PipInstall") / (PLATFORM_WINDOWS
			                                                                ? TEXT("Scripts/python.exe")
			                                                                : TEXT("bin/python3"));
	}

	FString GetPythonPluginContentPath()
	{
		return FPaths::ProjectPluginsDir() / TEXT("AITagging/Content/Python");
	}

	FString GetTemporaryFolder()
	{
		return FPaths::ProjectIntermediateDir() / TEXT("AITagging");
	}
}

#define LOCTEXT_NAMESPACE "AITagsEditorSubsystem"

void UAITagsEditorSubsystem::CleanCachedAssets()
{
	AssetsForAITagging.Reset();
}

void UAITagsEditorSubsystem::AddAssetToCache(const FAssetData& InAssetData)
{
	AssetsForAITagging.Add(InAssetData);
}

void UAITagsEditorSubsystem::AddAssetsToCache(const TArray<FAssetData>& InAssetDatas)
{
	AssetsForAITagging.Append(InAssetDatas);
}

void UAITagsEditorSubsystem::StartCLIPTagging(bool bUsePerCategory, bool bUseThreshold, float Threshold)
{
	if (AssetsForAITagging.IsEmpty())
	{
		UE_LOG(LogAITagsEditor, Error, TEXT("%hs: No assets added for tagging! Please add assets first."), __FUNCTION__);
		return;
	}

	UE_LOG(LogAITagsEditor, Log, TEXT("%hs: Start bUsePerCategory=%d bUseThreshold=%d Threshold=%.2f"), __FUNCTION__, bUsePerCategory, bUseThreshold, Threshold);
	
	CleanUpTemporaryFolder();
	const FString InputFullPath = PrepareThumbnailsAndInputFile();
	LaunchCLIP(FPaths::ConvertRelativePathToFull(InputFullPath), bUsePerCategory, bUseThreshold, Threshold);
}

void UAITagsEditorSubsystem::StartImageToText()
{
	if (AssetsForAITagging.IsEmpty())
	{
		UE_LOG(LogAITagsEditor, Error, TEXT("%hs: No assets added for tagging! Please add assets first."), __FUNCTION__);
		return;
	}

	UE_LOG(LogAITagsEditor, Log, TEXT("%hs: Start"), __FUNCTION__);

	CleanUpTemporaryFolder();
	const FString InputFullPath = PrepareThumbnailsAndInputFile();
	LaunchImageToText(FPaths::ConvertRelativePathToFull(InputFullPath));
}

FString UAITagsEditorSubsystem::PrepareThumbnailsAndInputFile()
{
	const FString TempDir = AITagsEditorUtils::GetTemporaryFolder();

	TMap<FAssetData, FString> AssetImagePaths;
	for (const FAssetData& AssetData : AssetsForAITagging)
	{
		const FString PngPath = SaveAssetThumbnailToDisk(AssetData, TempDir);
		if (PngPath.IsEmpty())
		{
			UE_LOG(LogAITagsEditor, Error, TEXT("AITagsEditorSubsystem: Failed to export thumbnail for %s"), *AssetData.AssetName.ToString());
			continue; // Skip this asset
		}
		AssetImagePaths.Add(AssetData, FPaths::ConvertRelativePathToFull(PngPath));
	}

	FString InputFullPath;
	WriteAssetImageArrayToJson(AssetImagePaths, TempDir, InputFullPath);
	return InputFullPath;
}

FString UAITagsEditorSubsystem::GetHashedFilename(const FAssetData& InAssetData) const
{
	// -------------------------------
	// 1. Compute a hash-based filename from the AssetData path
	// -------------------------------
	// Use the full object path (e.g., "/Game/Props/SM_Rock.SM_Rock") to generate a CRC32 hash
	const FString AssetPathString = InAssetData.GetObjectPathString();
	const uint32 PathHash = FCrc::StrCrc32(*AssetPathString);
	// Convert to an 8-digit hexadecimal string (lowercase) and append ".png"
	const FString FileName = FString::Printf(TEXT("%08x.png"), PathHash);
	return FileName;
}

FString UAITagsEditorSubsystem::SaveAssetThumbnailToDisk(const FAssetData& AssetData, const FString& FolderPath,
                                                         int32 ThumbnailSize)
{
	if (!AssetData.IsValid())
	{
		return FString();
	}

	const FString FullFilePath = FolderPath / GetHashedFilename(AssetData);

	// Generate a thumbnail!
	FObjectThumbnail GeneratedThumbnail; // = ThumbnailTools::GenerateThumbnailForObjectToSaveToDisk( AssetData.GetAsset() );

	// Does the object support thumbnails?
	UObject* InObject = AssetData.GetAsset();
	if (!InObject)
	{
		return FString();
	}
	
	FThumbnailRenderingInfo* RenderInfo = GUnrealEd
		                                      ? GUnrealEd->GetThumbnailManager()->GetRenderingInfo(InObject)
		                                      : nullptr;
	if (RenderInfo != NULL && RenderInfo->Renderer != NULL)
	{
		FAssetCompilingManager::Get().FinishCompilationForObjects({InObject});

		TArray<UMaterialInterface*> Materials;
		TArray<UTexture*> Textures;

		if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(InObject))
		{
			for (const FStaticMaterial& Material : StaticMesh->GetStaticMaterials())
			{
				if (Material.MaterialInterface)
				{
					Materials.Add(Material.MaterialInterface);
				}
			}
		}
		else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(InObject))
		{
			for (const FSkeletalMaterial& Material : SkeletalMesh->GetMaterials())
			{
				if (Material.MaterialInterface)
				{
					Materials.Add(Material.MaterialInterface);
				}
			}
		}
		else if (UTexture* Texture = Cast<UTexture>(InObject))
		{
			Textures.Add(Texture);
		}
		else if (UMaterialInterface* Material = Cast<UMaterialInterface>(InObject))
		{
			Materials.Add(Material);
		}

		for (auto* Material : Materials)
		{
			// Block until the shader maps that we will save have finished being compiled
			if (FMaterialResource* CurrentResource = Material->GetMaterialResource(GMaxRHIFeatureLevel))
			{
				if (!CurrentResource->IsGameThreadShaderMapComplete())
				{
					CurrentResource->SubmitCompileJobs_GameThread(EShaderCompileJobPriority::High);
				}
				CurrentResource->FinishCompilation();
			}
			
			TArray<UTexture*> MaterialTextures;
			Material->GetUsedTextures(MaterialTextures, EMaterialQualityLevel::Num, false, GMaxRHIFeatureLevel, false);

			Textures.Append(MaterialTextures);
		}

		if (!Textures.IsEmpty())
		{
			FTextureCompilingManager::Get().FinishCompilation(Textures);
		}
		
		for (UTexture* Texture : Textures)
		{
			if (Texture)
			{
				Texture->BlockOnAnyAsyncBuild();
				Texture->WaitForStreaming();
			}
		}

		// Generate the thumbnail
		// FObjectThumbnail NewThumbnail;
		ThumbnailTools::RenderThumbnail(InObject, ThumbnailSize, ThumbnailSize, ThumbnailTools::EThumbnailTextureFlushMode::AlwaysFlush, NULL,
		                                &GeneratedThumbnail);
	}

	const int32 Width = GeneratedThumbnail.GetImageWidth();
	const int32 Height = GeneratedThumbnail.GetImageHeight();
	const TArray<uint8>& ImageData = GeneratedThumbnail.GetUncompressedImageData();
	if (ImageData.IsEmpty())
	{
		UE_LOG(LogAITagsEditor, Error, TEXT("AITagsEditorSubsystem: No image data for thumbnail of %s"), *AssetData.AssetName.ToString());
		return FString();
	}

	// 3) Compress to PNG via IImageWrapper
	IImageWrapperModule& ImgWrpMod = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> PngWrapper = ImgWrpMod.CreateImageWrapper(EImageFormat::PNG);
	if (!PngWrapper.IsValid())
	{
		UE_LOG(LogAITagsEditor, Error, TEXT("AITagsEditorSubsystem: Failed to create PNG Wrapper for %s"), *AssetData.AssetName.ToString());
		return FString();
	}

	PngWrapper->SetRaw(ImageData.GetData(), ImageData.Num(), Width, Height, ERGBFormat::BGRA, 8);
	const TArray64<uint8> PngBytes = PngWrapper->GetCompressed();

	if (PngBytes.Num() == 0)
	{
		// Rendering failed or empty result
		UE_LOG(LogAITagsEditor, Error, TEXT("AITagsEditorSubsystem: Failed to compress thumbnail to PNG for %s"), *AssetData.AssetName.ToString());
		return FString();
	}

	// 4. Save the raw PNG bytes to disk
	FFileHelper::SaveArrayToFile(PngBytes, *FullFilePath);

	return FullFilePath;
}

void UAITagsEditorSubsystem::CleanUpTemporaryFolder()
{
	const FString TempDir = AITagsEditorUtils::GetTemporaryFolder();
	if (IFileManager::Get().DirectoryExists(*TempDir))
	{
		TArray<FString> FileNames;
		// The third parameter = true => include files; fourth = false => don’t include directories
		IFileManager::Get().FindFiles(FileNames, *(TempDir / TEXT("*")), /*Files=*/ true, /*Directories=*/ false);

		// 3) Loop over each file‐name and delete it
		for (const FString& FileName : FileNames)
		{
			// Construct the full path to the file
			const FString FullFilePath = TempDir / FileName;
			IFileManager::Get().Delete(*FullFilePath, /*RequireExists=*/ false, /*EvenReadOnly=*/ false);
		}

		// Optionally clear the directory if it exists
		// IFileManager::Get().DeleteDirectory(*TempDir, /*RequireExists=*/ true, /*Tree=*/ true);
	}
	IFileManager::Get().MakeDirectory(*TempDir, /*Tree=*/ true);
}

void UAITagsEditorSubsystem::WriteAssetImageArrayToJson(const TMap<FAssetData, FString>& AssetImagePaths,
                                                        const FString& FolderPath, FString& OutFullPath)
{
	// 1) Create the root JSON object that holds an array called "Entries"
	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();

	// 2) Build a JSON array
	TArray<TSharedPtr<FJsonValue>> JsonEntries;

	for (const auto& Pair : AssetImagePaths)
	{
		const FAssetData& AssetData = Pair.Key;
		const FString& ThumbnailPath = Pair.Value;

		// Create one JSON object per entry:
		TSharedRef<FJsonObject> EntryObj = MakeShared<FJsonObject>();
		// You can store both the object path and the asset name if you want:
		EntryObj->SetStringField(TEXT("AssetPath"), AssetData.GetObjectPathString());
		EntryObj->SetStringField(TEXT("AssetName"), AssetData.AssetName.ToString());
		EntryObj->SetStringField(TEXT("ImagePath"), ThumbnailPath);

		// Wrap EntryObj as a FJsonValueObject and add to the array:
		JsonEntries.Add(MakeShared<FJsonValueObject>(EntryObj));
	}

	// Set the array into the root under key "Entries":
	RootObject->SetArrayField(TEXT("Entries"), JsonEntries);

	// 3) Serialize to string
	FString OutputString;
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&OutputString);
	if (FJsonSerializer::Serialize(RootObject, JsonWriter))
	{
		const FString FileName = TEXT("input.json");
		OutFullPath = FolderPath / FileName;

		// 5) Write to disk
		if (FFileHelper::SaveStringToFile(OutputString, *OutFullPath))
		{
			UE_LOG(LogAITagsEditor, Log, TEXT("Successfully wrote JSON array to %s"), *OutFullPath);
		}
		else
		{
			UE_LOG(LogAITagsEditor, Error, TEXT("Failed to write JSON file to %s"), *OutFullPath);
		}
	}
	else
	{
		UE_LOG(LogAITagsEditor, Error, TEXT("Failed to serialize AssetImagePaths to JSON"));
	}
}

void UAITagsEditorSubsystem::LaunchCLIP(const FString& InInputFullPath, bool bUsePerCategory, bool bUseThreshold, float Threshold)
{
	const FString PythonExecutablePath = AITagsEditorUtils::GetPythonExecutablePath();
	const FString CLIPScript = AITagsEditorUtils::GetPythonPluginContentPath() / TEXT("tagging") / TEXT("run_clip_category.py");
	if (!FPaths::FileExists(CLIPScript))
	{
		UE_LOG(LogAITagsEditor, Error, TEXT("AITagsEditorSubsystem: Cannot find %s"), *CLIPScript);
		return;
	}
	
	const FString CommandLineArguments = FString::Printf(TEXT("\"%s\" \"%s\" %d %.2f"), *CLIPScript, *InInputFullPath, bUsePerCategory, bUseThreshold ? Threshold : 0);

	// 4) Create MonitoredProcess with stdout pipe
	bool bLaunchHidden = true;
	bool bCreatePipes = true; // we want stdout/stderr pipes

	if (CurrentProcess.IsValid())
	{
		// @todo: check this code
		CurrentProcess->Stop();
		CurrentProcess.Reset();
	}

	CurrentProcess = MakeShared<FMonitoredProcess>(
		PythonExecutablePath,
		CommandLineArguments,
		bLaunchHidden,
		bCreatePipes
	);

	// Optionally force the child working directory to the Scripts folder so any local imports work
	// CLIPProcess->SetWorkingDirectory(PluginScriptsFolder);

	// 5) Bind delegates
	CurrentProcess->OnOutput().BindUObject(this, &UAITagsEditorSubsystem::HandleCLIPOutputReceived);
	CurrentProcess->OnCompleted().BindUObject(this, &UAITagsEditorSubsystem::HandleCLIPProcessCompleted);

	// 6) Launch asynchronously
	if (!CurrentProcess->Launch())
	{
		UE_LOG(LogAITagsEditor, Error, TEXT("AITagsEditorSubsystem: Failed to launch CLIP detect process"));
		CurrentProcess.Reset();
	}
	else
	{
		UE_LOG(LogAITagsEditor, Log, TEXT("AITagsEditorSubsystem: Launched CLIP detect for %s"), *InInputFullPath);
		PushNotification(TEXT("Calculating CLIP tags..."));
	}
}

void UAITagsEditorSubsystem::LaunchImageToText(const FString& InInputFullPath)
{
	const FString PythonExecutablePath = AITagsEditorUtils::GetPythonExecutablePath();
	const FString CLIPScript = AITagsEditorUtils::GetPythonPluginContentPath() / TEXT("tagging") / TEXT("run_clip_img2text.py");
	if (!FPaths::FileExists(CLIPScript))
	{
		UE_LOG(LogAITagsEditor, Error, TEXT("AITagsEditorSubsystem: Cannot find %s"), *CLIPScript);
		return;
	}
	
	const FString CommandLineArguments = FString::Printf(TEXT("\"%s\" \"%s\""), *CLIPScript, *InInputFullPath);
	
	bool bLaunchHidden = true;
	bool bCreatePipes = true; // we want stdout/stderr pipes

	if (CurrentProcess.IsValid())
	{
		// @todo: check this code
		CurrentProcess->Stop();
		CurrentProcess.Reset();
	}

	CurrentProcess = MakeShared<FMonitoredProcess>(
		PythonExecutablePath,
		CommandLineArguments,
		bLaunchHidden,
		bCreatePipes
	);
	
	CurrentProcess->OnOutput().BindUObject(this, &UAITagsEditorSubsystem::HandleCLIPOutputReceived);
	CurrentProcess->OnCompleted().BindUObject(this, &UAITagsEditorSubsystem::HandleImageToTextProcessCompleted);
	
	if (!CurrentProcess->Launch())
	{
		UE_LOG(LogAITagsEditor, Error, TEXT("AITagsEditorSubsystem: Failed to launch Image2Text process"));
		CurrentProcess.Reset();
	}
	else
	{
		UE_LOG(LogAITagsEditor, Log, TEXT("AITagsEditorSubsystem: Launched Image2Text for %s"), *InInputFullPath);
		PushNotification(TEXT("Calculating image2text..."));
	}
}

void UAITagsEditorSubsystem::PushNotification(const FString& InMessage)
{
	FNotificationInfo Info(FText::FromString(InMessage));
	Info.ExpireDuration = 3.f;
	Info.bUseSuccessFailIcons = true;
	Info.bFireAndForget = false;

	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Pending);
		LastNotification = Notification;
	}
}

void UAITagsEditorSubsystem::PopNotification(int32 ReturnCode)
{
	if (LastNotification.IsValid())
	{
		LastNotification.Pin()->SetCompletionState(ReturnCode == 0 ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		LastNotification.Pin()->ExpireAndFadeout();
		LastNotification.Reset();
	}
}

void UAITagsEditorSubsystem::HandleCLIPOutputReceived(FString OutputLine)
{
	{
		UE_LOG(LogAITagsEditor, Display, TEXT("Subprocess: %s"), *OutputLine);
	}
}

void UAITagsEditorSubsystem::HandleCLIPProcessCompleted(int32 ReturnCode)
{
	UE_LOG(LogAITagsEditor, Log, TEXT("%hs: Python exited with code %d"), __FUNCTION__, ReturnCode);

	PopNotification(ReturnCode);
	
	// If the child returned nonzero, log and bail
	if (ReturnCode != 0)
	{
		UE_LOG(LogAITagsEditor, Error, TEXT("%hs: returned nonzero exit code."), __FUNCTION__);
		CurrentProcess.Reset();
		return;
	}

	TSharedPtr<FJsonObject> RootJsonObject;
	{
		const FString FileName = AITagsEditorUtils::GetTemporaryFolder() / TEXT("output.json");

		// 1) Read the file from disk into one big FString
		FString FileContents;
		if (!FFileHelper::LoadFileToString(FileContents, *FileName))
		{
			UE_LOG(LogAITagsEditor, Error, TEXT("AITagsEditorSubsystem: Failed to open file at %s"), *FileName);
			return;
		}

		// 2) Create a JSON reader from that string
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(FileContents);

		// 3) Deserialize into a root FJsonObject
		if (!FJsonSerializer::Deserialize(JsonReader, RootJsonObject) || !RootJsonObject.IsValid())
		{
			UE_LOG(LogAITagsEditor, Error, TEXT("AITagsEditorSubsystem: Failed to parse JSON content from %s"), *FileName);
			return;
		}
	}
	
	AsyncTask(ENamedThreads::GameThread, [this, RootJsonObject]()
	{
		TArray<TSharedPtr<FJsonValue>> EntriesArray = RootJsonObject->GetArrayField(TEXT("Entries"));
		for (const TSharedPtr<FJsonValue>& EntryValue : EntriesArray)
		{
			// Each entry is itself a JSON object
			TSharedPtr<FJsonObject> EntryObj = EntryValue->AsObject();
			if (!EntryObj.IsValid())
				continue;

			const FString AssetPath = EntryObj->GetStringField(TEXT("AssetPath"));
			TArray<TSharedPtr<FJsonValue>> TagsArray = EntryObj->GetArrayField(TEXT("CLIPTags"));
			TArray<FString> Tags;
			for (const auto& TagValue : TagsArray)
			{
				if (TagValue->Type == EJson::String)
				{
					Tags.Add(TagValue->AsString());
				}
			}
			const FString OutValue = FString::Join(Tags, TEXT(", "));
			UE_LOG(LogAITagsEditor, Log, TEXT("Entry: %s → %s"), *AssetPath, *OutValue);
			
			if (UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath))
			{
				UEditorAssetLibrary::SetMetadataTag(Asset, TEXT("AssetTags"), OutValue);
			}
		}
	});
	
	// Finally, drop our handle so the process and its pipes clean up
	CurrentProcess.Reset();
}

void UAITagsEditorSubsystem::HandleImageToTextProcessCompleted(int32 ReturnCode)
{
	UE_LOG(LogAITagsEditor, Log, TEXT("%hs: Python exited with code %d"), __FUNCTION__, ReturnCode);

	PopNotification(ReturnCode);

	// If the child returned nonzero, log and bail
	if (ReturnCode != 0)
	{
		UE_LOG(LogAITagsEditor, Error, TEXT("%hs: returned nonzero exit code."), __FUNCTION__);
		CurrentProcess.Reset();
		return;
	}

	TSharedPtr<FJsonObject> RootJsonObject;
	{
		const FString FileName = AITagsEditorUtils::GetTemporaryFolder() / TEXT("output.json");

		// 1) Read the file from disk into one big FString
		FString FileContents;
		if (!FFileHelper::LoadFileToString(FileContents, *FileName))
		{
			UE_LOG(LogAITagsEditor, Error, TEXT("AITagsEditorSubsystem: Failed to open file at %s"), *FileName);
			return;
		}

		// 2) Create a JSON reader from that string
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(FileContents);

		// 3) Deserialize into a root FJsonObject
		if (!FJsonSerializer::Deserialize(JsonReader, RootJsonObject) || !RootJsonObject.IsValid())
		{
			UE_LOG(LogAITagsEditor, Error, TEXT("AITagsEditorSubsystem: Failed to parse JSON content from %s"), *FileName);
			return;
		}
	}
	
	AsyncTask(ENamedThreads::GameThread, [this, RootJsonObject]()
	{
		TArray<TSharedPtr<FJsonValue>> EntriesArray = RootJsonObject->GetArrayField(TEXT("Entries"));
		for (const TSharedPtr<FJsonValue>& EntryValue : EntriesArray)
		{
			// Each entry is itself a JSON object
			TSharedPtr<FJsonObject> EntryObj = EntryValue->AsObject();
			if (!EntryObj.IsValid())
				continue;

			const FString AssetPath = EntryObj->GetStringField(TEXT("AssetPath"));
			FString Image2TextValue = EntryObj->GetStringField(TEXT("Image2Text"));
			UE_LOG(LogAITagsEditor, Log, TEXT("Entry: %s → %s"), *AssetPath, *Image2TextValue);
			
			if (UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath))
			{
				UEditorAssetLibrary::SetMetadataTag(Asset, TEXT("Image2Text"), Image2TextValue);
			}
		}
	});

	// Finally, drop our handle so the process and its pipes clean up
	CurrentProcess.Reset();
}

#undef LOCTEXT_NAMESPACE
