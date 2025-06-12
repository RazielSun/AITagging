import sys
import os
import json
import warnings
from PIL import Image

# Suppress specific warning message
warnings.filterwarnings("ignore", message=".*Torch was not compiled with flash attention.*")

try:
    import torch
    import clip
    device: str = ("mps" if torch.backends.mps.is_available() else "cuda" if torch.cuda.is_available() else "cpu")
    model, preprocess = clip.load("ViT-L/14", device=device)
    print(f"Device: {device}")
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

# --- ENCODE TAGS BY CATEGORY ---
def get_tag_embeddings(per_category=True):
    script_dir = os.path.dirname(os.path.abspath(__file__))
    json_path = os.path.join(script_dir, 'game_asset_tags.json')
    with open(json_path, 'r', encoding='utf-8') as f:
        tags_json = json.load(f)
        
        tag_embeddings = {}
        if per_category:
            for category, tags in tags_json.items():
                with torch.no_grad():
                    # print(f"EMBEDDINGS: {category} {tags}\n")
                    text_tokens = clip.tokenize(tags).to(device)
                    tag_embeds = model.encode_text(text_tokens).float()
                    tag_embeds /= tag_embeds.norm(dim=-1, keepdim=True)
                tag_embeddings[category] = dict(tags=tags, embeddings=tag_embeds)
        else:
            all_tags = []
            for tag_list in tags_json.values():
                all_tags.extend(tag_list)
            unique_tags = list(set(all_tags))
            with torch.no_grad():
                text_tokens = clip.tokenize(unique_tags).to(device)
                tag_embeds = model.encode_text(text_tokens).float()
                tag_embeds /= tag_embeds.norm(dim=-1, keepdim=True)
            tag_embeddings['all'] = dict(tags=all_tags, embeddings=tag_embeds)
            
        return tag_embeddings

# --- PROCESS IMAGES ---
def get_image_embedding(image_path):
    image = preprocess(Image.open(image_path).convert("RGB")).unsqueeze(0).to(device)
    with torch.no_grad():
        image_embedding = model.encode_image(image).float()
        image_embedding /= image_embedding.norm(dim=-1, keepdim=True)
    return image_embedding[0]

log_enabled = True
USE_PER_CATEGORY = True
USE_THRESHOLD = False
THRESHOLD = 0.2
TOPK = 1 # 5
USE_SOFTMAX = True
USE_MAX_TAGS = True
MAX_TAGS = 3

def process_data(data):
    if not data:
        print(f"Error: No data provided.")
        sys.stdout.flush()
        return None

    if log_enabled:
        print(f"Processing json: {data}")
        sys.stdout.flush()

    tag_embeddings = get_tag_embeddings(USE_PER_CATEGORY)

    # get entries array from data
    entries = data.get('Entries', [])

    for entry in entries:
        if log_enabled:
            print(f"Processing entry: {entry}")
            sys.stdout.flush()
        image_path = entry.get('ImagePath', None)
        if image_path:
            if log_enabled:
                print(f"Processing image: {image_path}")
                sys.stdout.flush()

            image_embedding = get_image_embedding(image_path)

            matched = {}
            combined_tags = []
            for category, tag_vectors in tag_embeddings.items():
                selected = []
                tags = tag_vectors["tags"]
                tag_embeds = tag_vectors["embeddings"]
                if USE_THRESHOLD:
                    values = []
                    for index in range(len(tag_embeds)):
                        tag = tags[index]
                        similarity = torch.dot(image_embedding, tag_embeds[index]).item()
                        # print(f"{category} {tag}: tag_embeds: \n{similarity}\n")
                        if similarity >= THRESHOLD:
                            values.append((tag, round(similarity, 3) * 100))
                    values.sort(key=lambda x: -x[1])
                    # print(f"{category}: values = {values}\n")
                    result_tags = [item[0] for item in values]
                    if USE_MAX_TAGS:
                        selected.extend(result_tags[:MAX_TAGS])
                    else:
                        selected.extend(result_tags)
                else:
                    # print(f"{category} {tags}: tag_embeds: \n{tag_embeds}\n")
                    similarity = (image_embedding @ tag_embeds.T).squeeze(0)
                    # tag_score_pairs = [(tag, round(float(score), 3)) for tag, score in zip(tags, similarity)]
                    # print(f"Similarity for {category}: \n"
                    #       f"\t\t{tag_score_pairs}")
                    if USE_SOFTMAX:
                        top_indices = [torch.argmax(torch.softmax(similarity, dim=0))]
                    else:
                        top_indices = similarity.topk(TOPK).indices
                    selected = [tags[i] for i in top_indices]
                matched[category] = selected
                combined_tags.extend(selected)
            
            print(matched)
            entry["CLIPTags"] = combined_tags

    return data

if __name__ == '__main__':
    if len(sys.argv) <= 1:
        raise Exception('Input file is not provided')
    else:
        input_filepath = sys.argv[1]
        
    if len(sys.argv) >= 3:
        USE_PER_CATEGORY = sys.argv[2] == '1'
    
    if len(sys.argv) >= 4:
        THRESHOLD = float(sys.argv[3])
        USE_THRESHOLD = THRESHOLD > 0.0
        
    if log_enabled:
        print(f"Using per category: {USE_PER_CATEGORY}, threshold: {THRESHOLD}, use threshold: {USE_THRESHOLD}")
        sys.stdout.flush()

    json_data = load_input_file(input_filepath)
    parsed_data = process_data(json_data)
    save_output_file(parsed_data, input_filepath)
