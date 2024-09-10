import os
from PIL import Image

def convert_to_grayscale(image_path):
    img = Image.open(image_path)
    img_bw = img.convert('L')
    
    base_name = os.path.basename(image_path)
    new_name = base_name.replace("_base_color.jpg", "_base_color_bw.jpg")
    
    img_bw.save(new_name)
    print(f"Saved: {new_name}")

for file_name in os.listdir('.'):
    if file_name.endswith("_base_color.jpg"):
        convert_to_grayscale(file_name)