// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Misc/MonitoredProcess.h"
#include "AssetRegistry/AssetData.h"

#include "AITagsEditorSubsystem.generated.h"

class SNotificationItem;

UCLASS()
class AITAGGING_API UAITagsEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
    UFUNCTION(CallInEditor, BlueprintCallable, Category = "AITagging")
    void CleanCachedAssets();

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "AITagging")
    void AddAssetToCache(const FAssetData& InAssetData);

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "AITagging")
    void AddAssetsToCache(const TArray<FAssetData>& InAssetDatas);

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "AITagging")
    void StartCLIPTagging(bool bUsePerCategory, bool bUseThreshold, float Threshold);

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "AITagging")
    void StartImageToText();

protected:
    // ~~~ Internal Helpers ~~~
    
    FString GetHashedFilename(const FAssetData& InAssetData) const;

    void CleanUpTemporaryFolder();
    FString PrepareThumbnailsAndInputFile();

    FString SaveAssetThumbnailToDisk(const FAssetData& AssetData, const FString& FolderPath, int32 ThumbnailSize = 224);

    void WriteAssetImageArrayToJson(const TMap<FAssetData, FString>& AssetImagePaths, const FString& FolderPath, FString& OutFullPath);

    void LaunchCLIP(const FString& InInputFullPath, bool bUsePerCategory, bool bUseThreshold, float Threshold);
    void LaunchImageToText(const FString& InInputFullPath);

    /** Delegate: Called each time the subprocess prints a line. We accumulate stdout here. */
    void HandleCLIPOutputReceived(FString OutputLine);

    /** Delegate: Called once the subprocess has exited. We parse JSON and write metadata to PendingAssetData. */
    void HandleCLIPProcessCompleted(int32 ReturnCode);
    void HandleImageToTextProcessCompleted(int32 ReturnCode);

    void PushNotification(const FString& InMessage);
    void PopNotification(int32 ReturnCode);

private:
    TArray<FAssetData> AssetsForAITagging;
    
    /** Holds our FMonitoredProcess instance until the Python job is done. */
    TSharedPtr<FMonitoredProcess> CurrentProcess;
    
    TWeakPtr<SNotificationItem> LastNotification;
};
