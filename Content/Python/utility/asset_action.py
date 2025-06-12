import unreal
def run_image_2_text():
    """
    Function to run image to text tagging.
    """
    subsystem = unreal.get_editor_subsystem(unreal.AITagsEditorSubsystem)
    subsystem.clean_cached_assets()

    selected_assets = unreal.EditorUtilityLibrary.get_selected_asset_data()
    subsystem.add_assets_to_cache(selected_assets)
    
    unreal.log("Python:: Running image to text tagging...")
    subsystem.start_image_to_text()

def run_clip_tagging(use_per_category=False, use_threshold=False, threshold=0.2):
    """
    Function to run clip tagging.
    """
    subsystem = unreal.get_editor_subsystem(unreal.AITagsEditorSubsystem)
    subsystem.clean_cached_assets()

    selected_assets = unreal.EditorUtilityLibrary.get_selected_asset_data()
    subsystem.add_assets_to_cache(selected_assets)

    unreal.log("Python:: Running clip tagging...")
    subsystem.start_clip_tagging(use_per_category, use_threshold, threshold)
    
@unreal.uclass()
class PythonAITagsEditorLibrary(unreal.BlueprintFunctionLibrary):
    """
    Blueprint function library for AITagging.
    Provides static methods to run image to text and clip tagging.
    """
    
    @unreal.ufunction(static=True, meta=dict(Category="AITagging"))
    def RunImage2TextFunc():
        run_image_2_text()

    @unreal.ufunction(static=True, meta=dict(Category="AITagging"), params=[bool, bool, float])
    def RunCLIPTaggingFunc(use_per_category:bool=True, use_threshold:bool=False, threshold:float=0.2):
        run_clip_tagging(use_per_category, use_threshold, threshold)

# NOTICE:
# This class is not working with Blutility because Epic searches for FAssetData instead runtime UObject
#
# @unreal.uclass()
# class AITaggingAssetActionTool(unreal.AssetActionUtility):
#     
#     @unreal.ufunction(static=True,meta={"Category":"AITagging","CallInEditor":True})
#     def RunImage2Text():
#         unreal.log(f"run_image_2_text")
# 
#     @unreal.ufunction(meta=dict(Category="AITagging",CallInEditor=True))
#     def RunImage2TextV2(self):
#         unreal.log(f"run_image_2_text")
# 
#     @unreal.ufunction(meta={"Category":"AITagging", "CallInEditor":True})
#     def RunImage2TextV3(self):
#         unreal.log(f"run_image_2_text")
# 
#     @unreal.ufunction(static=True,meta=dict(CallInEditor=True))
#     def RunImage2TextV4():
#         unreal.log(f"run_image_2_text")
#         
#     @unreal.ufunction(meta=dict(Category="AITagging"))
#     def run_clip_tagging(self):
#         unreal.log(f"run_clip_tagging")