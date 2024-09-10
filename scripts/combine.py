import argparse
from PIL import Image

def extract_channels(image_path, channels):
    img = Image.open(image_path)
    img_channels = {
        'R': img.getchannel('R') if 'R' in img.getbands() else None,
        'G': img.getchannel('G') if 'G' in img.getbands() else None,
        'B': img.getchannel('B') if 'B' in img.getbands() else None,
        'A': img.getchannel('A') if 'A' in img.getbands() else None
    }
    
    return [img_channels[channel] for channel in channels]

def combine_channels(args):
    channels = {'R': None, 'G': None, 'B': None, 'A': None}

    for img_path, img_channels in zip(args.images[::2], args.images[1::2]):
        extracted_channels = extract_channels(img_path, img_channels)
        
        for i, ch in enumerate(img_channels):
            channels[ch] = extracted_channels[i]

    for ch in channels:
        if channels[ch] is None:
            if ch == 'A':
                channels[ch] = Image.new("L", channels['R'].size, 255)  # Full opacity if alpha is missing
            else:
                channels[ch] = Image.new("L", channels['R'].size, 0)  # Black for RGB if missing

    combined_img = Image.merge("RGBA", (channels['R'], channels['G'], channels['B'], channels['A']))
    combined_img.save(args.output)
    print(f"Combined image saved to {args.output}")

parser = argparse.ArgumentParser(description="CLI tool to combine channels from multiple images.")
parser.add_argument('images', nargs='+', help="Images and their respective channels (e.g., img1 RG img2 G img3 A)")
parser.add_argument('-o', '--output', required=True, help="Output image path")

args = parser.parse_args()

if len(args.images) % 2 != 0:
    raise ValueError("Each image must be followed by its respective channel(s).")

combine_channels(args)
