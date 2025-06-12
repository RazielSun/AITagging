import sys
import os
import json
import warnings
from PIL import Image

# Suppress specific warning message
warnings.filterwarnings("ignore", message=".*Torch was not compiled with flash attention.*")
warnings.filterwarnings("ignore", message=".*Using a slow image processor as `use_fast` is unset.*")

def torch_cuda_available():
    try:
        import torch
        print(f"Cuda:{torch.cuda.is_available()}")
        print(f"Device count: {torch.cuda.device_count()}")
    except ImportError as e:
        print(f"Torch is not available: {e}")
def load_input_file(input_path):
    print(f"Loading input file: {input_path}")
    with open(input_path, 'r', encoding='utf-8') as f:
        return json.load(f)

def save_output_file(data, input_path):
    input_dir = os.path.dirname(input_path)
    output_path = os.path.join(input_dir, "output.json")
    print(f"Saving output file: {output_path}")
    with open(output_path, "w", encoding="utf-8") as outfile:
        json.dump(data, outfile, indent=4)

use_clip_interrogator = True
log_enabled = True

try:
    from clip_interrogator import Config, Interrogator
    config = Config(clip_model_name="ViT-L-14/openai", caption_model_name="blip-large")
    ci = Interrogator(config)
except ImportError as e:
    use_clip_interrogator = False
    
if not use_clip_interrogator:
    print("Error: CLIP Interrogator not available. Please install the required package.")
    sys.exit(1)  # "Error: CLIP Interrogator not available. Please install the required package."
    
def get_ai_tags(image_path):
    if log_enabled:
        print(f"Running CLIP Interrogator on image: {image_path}")
        sys.stdout.flush()
    image = Image.open(image_path).convert('RGB')
    # return ci.interrogate(image)
    return ci.interrogate_fast(image)

def process_data(data):
    if not data:
        print(f"Error: No data provided.")
        sys.stdout.flush()
        return None

    # get entries array from data
    entries = data.get('Entries', [])
    out_entries = []

    if log_enabled:
        print(f"Processing json (entries: {len(entries)}): {data}")
        sys.stdout.flush()
        
    for entry in entries:
        if log_enabled:
            print(f"Processing entry: {entry}")
            sys.stdout.flush()
        image_path = entry.get('ImagePath', None)
        if image_path:
            if log_enabled:
                print(f"Processing image: {image_path}")
                sys.stdout.flush()
            # get ai tags for image
            output = get_ai_tags(image_path)
            if log_enabled:
                print(f"Result: {output}")
                sys.stdout.flush()
            # add ai tags to entry
            entry['Image2Text'] = output
            out_entries.append(entry)
    return dict(Entries = out_entries)

if __name__ == '__main__':
    if len(sys.argv) <= 1:
        raise Exception('Input file is not provided')
    else:
        input_filepath = sys.argv[1]

    torch_cuda_available()
        
    json_data = load_input_file(input_filepath)
    parsed_data = process_data(json_data)
    save_output_file(parsed_data, input_filepath)
    