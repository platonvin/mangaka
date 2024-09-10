import argparse
from PIL import Image

def compress_texture(input_path, output_path, quality):
    img = Image.open(input_path)
    img.save(output_path, quality=quality, optimize=True)

parser = argparse.ArgumentParser(description="Compress a texture to reduce file size.")
parser.add_argument("input", help="Path to the input texture")
parser.add_argument("output", help="Path to the output texture")
parser.add_argument("quality", type=int, help="Compression quality (1-100)")
args = parser.parse_args()

compress_texture(args.input, args.output, args.quality)