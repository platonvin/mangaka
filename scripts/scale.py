import argparse
from PIL import Image

def scale_image(input_path, output_path, size):
    img = Image.open(input_path)
    img_resized = img.resize(size, Image.LANCZOS)
    img_resized.save(output_path)

parser = argparse.ArgumentParser(description="Scale an image down to the specified size.")
parser.add_argument("input", help="Path to the input image")
parser.add_argument("output", help="Path to the output image")
parser.add_argument("--width", type=int, required=True, help="Desired width")
parser.add_argument("--height", type=int, required=True, help="Desired height")
args = parser.parse_args()

scale_image(args.input, args.output, (args.width, args.height))