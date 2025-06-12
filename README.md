
# AI Tagging

This plugin provides AI-powered tagging for Unreal Engine assets using pretrained CLIP models.
⚠️ Note: This is a proof-of-concept and educational tool, not intended for production use.

## About

The goal of this plugin is to explore whether it's possible to automatically tag Unreal Engine assets using **only open, locally run AI models**, without relying on paid services like ChatGPT API or other proprietary LLMs.  
Many commercial plugins require cloud access to perform tagging, while this solution is entirely local and open-source.

Currently, the plugin supports two modes of image-to-text generation:

1. **Natural language description** using fork of [CLIP Interrogator](https://github.com/pharmapsychotic/clip-interrogator), which creates full sentence captions.
2. **Tag-based classification** using a CLIP model to compute similarity between asset images and a pre-defined list of tags.

It processes thumbnail images of assets, extracts semantic embeddings via CLIP, and assigns relevant tags—such as `type`, `environment`, or `color`—to the asset’s metadata. These tags can then be searched directly from the Content Browser.

> The plugin includes a sample `game_asset_tags.json` used for CLIP-based tagging.  
> This file was initially generated with ChatGPT for testing purposes. You are encouraged to update or replace it with a tag set tailored to your project's needs.

### CLIP
CLIP (Contrastive Language-Image Pretraining) is a multimodal model from OpenAI that learns to connect images and text in a shared embedding space. It consists of an image encoder and a text encoder trained together on millions of image-caption pairs. Given an image and a set of text descriptions, CLIP can identify which text best matches the image – enabling zero-shot classification by providing category names or prompts as text inputs.
[OpenAI CLIP](https://openai.com/index/clip/)

### BLIP
BLIP (Bootstrapping Language-Image Pre-training) is a vision-language model by Salesforce designed for both understanding and generating text about images. Unlike CLIP’s focus on embeddings for similarity, BLIP can generate descriptions (captions) or answer questions about an image. BLIP was trained on large web image-text data with a clever bootstrap: a pre-trained captioner generates candidate descriptions for web images, and a filter removes noisy ones to curate training data.
[HuggingFace BLIP](https://huggingface.co/Salesforce/blip-image-captioning-base)

## Install

### Python Dependencies

Make sure to enable both the `PythonScriptPlugin` and `PythonFoundationPackages` in your Unreal project.
The second one (`PythonFoundationPackages`) includes dependencies required for using PyTorch with CUDA.

To install correctly, you need to run pip commands from the following folder:  
`{YourProject}/Intermediate/PipInstall/Scripts`

Install clip-interrogator (from my fork):
```bash
./pip install git+https://github.com/RazielSun/clip-interrogator-ue.git@v0.6.1
```

Install OpenAI CLIP:
```bash
./pip install git+https://github.com/openai/CLIP.git
```

>❗ I couldn't get the plugin to work with Unreal's built-in PythonRequirements mechanism.
> It seems to require hashes for all transitive dependencies, which is difficult to resolve manually.
Example (this won't work without all hashes):
```
"PythonRequirements": [
	{
		"Platform": "All",
		"Requirements": [
			"clip-interrogator==0.6.0 --hash=sha256:cd7c6bf9db170f005b4179e943fc1658aa0f8eebcc75ab3428b0a992aaeabd1c"
		]
	}
]
```
If anyone finds a clean way to auto-generate or extract all the required hashes, feel free to contribute a solution!

## Usage

### Run for Tagging
You can run from `Scripted Asset Actions` or via python methods. CLIPTags save to AssetTags metadata, 

### Unreal Content Browser Search
How to setup: Add AssetTags and Image2Text to
`Project Settings -> Asset Manager -> Metadata Tags for Asset Registry`

Write in Context Browser Search field: `AssetTags == house`

## What's next?
- Try to implement variant from TagCLIP (GitHub: linyq2117/TagCLIP)
- Caching & batching support for performance.
- Optional prompt tuning using asset names.

## License
MIT