import argparse
from PIL import Image
import numpy as np

def grayscale_image(input_path, output_path):
    img = Image.open(input_path).convert('RGBA')
    img_np = np.array(img)
    
    r = img_np[:, :, 0]
    g = img_np[:, :, 1]
    b = img_np[:, :, 2]

    grayscale = (0.2989 * r + 0.5870 * g + 0.1140 * b).astype(np.uint8)
    grayscale_img = Image.fromarray(grayscale, mode='L')
    grayscale_img.save(output_path)

parser = argparse.ArgumentParser(description="Grayscale a 4-channel image to a 1-channel image.")
parser.add_argument("input", help="Path to the input image")
parser.add_argument("-o", "--output", required=True, help="Path to the output grayscale image")
args = parser.parse_args()

grayscale_image(args.input, args.output)
