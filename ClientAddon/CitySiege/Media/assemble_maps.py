"""
Assembles WoW map tiles into SQUARE power-of-2 images (required for BLP)
Place this script in the folder with your converted PNG tiles
Run: python assemble_maps.py
"""

from PIL import Image
import os

# Map configurations: {city_name: (columns, rows, tile_count)}
MAP_CONFIGS = {
    'Stormwind': (4, 3, 12),
    'Ironforge': (4, 3, 12),
    'Darnassis': (4, 3, 12),  # Note: WoW spells it "Darnassis" not "Darnassus"
    'TheExodar': (4, 3, 12),
    'Ogrimmar': (4, 3, 12),
    'Undercity': (4, 3, 12),
    'ThunderBluff': (4, 3, 12),
    'SilvermoonCity': (4, 3, 12),
}

TILE_SIZE = 256
TARGET_SIZE = 1024  # MUST be power-of-2 for BLP (256, 512, 1024, 2048)

def assemble_map(city_name, cols, rows, tile_count):
    """Assemble individual tiles into one SQUARE power-of-2 map"""
    
    # Check if tiles exist
    tiles = []
    for i in range(1, tile_count + 1):
        tile_path = f"{city_name}{i}.png"
        if not os.path.exists(tile_path):
            print(f"  ⚠ Missing {tile_path}, skipping {city_name}")
            return False
        tiles.append(tile_path)
    
    # Calculate original dimensions
    orig_width = cols * TILE_SIZE   # 4 * 256 = 1024
    orig_height = rows * TILE_SIZE  # 3 * 256 = 768
    
    # Create original-sized image first
    temp_img = Image.new('RGB', (orig_width, orig_height), color=(0, 0, 0))
    
    # Paste each tile
    print(f"  Assembling {tile_count} tiles ({orig_width}x{orig_height})...")
    for idx, tile_path in enumerate(tiles):
        img = Image.open(tile_path)
        x = (idx % cols) * TILE_SIZE
        y = (idx // cols) * TILE_SIZE
        temp_img.paste(img, (x, y))
        img.close()
    
    # Create SQUARE image (required for BLP)
    square_img = Image.new('RGB', (TARGET_SIZE, TARGET_SIZE), color=(0, 0, 0))
    
    # Center the original image in the square canvas
    offset_x = (TARGET_SIZE - orig_width) // 2
    offset_y = (TARGET_SIZE - orig_height) // 2
    square_img.paste(temp_img, (offset_x, offset_y))
    temp_img.close()
    
    # Save as PNG (ready for BLP conversion)
    output_path = f"{city_name}.png"
    square_img.save(output_path, 'PNG')
    square_img.close()
    
    print(f"  ✓ Created {output_path} ({TARGET_SIZE}x{TARGET_SIZE} SQUARE)")
    return True

def main():
    print("=" * 60)
    print("WoW Map Tile Assembler (BLP-Ready)")
    print("=" * 60)
    print(f"\nCreating SQUARE {TARGET_SIZE}x{TARGET_SIZE} images for BLP conversion")
    print("BLP format REQUIRES power-of-2 dimensions!\n")
    
    success_count = 0
    
    for city_name, (cols, rows, tile_count) in MAP_CONFIGS.items():
        print(f"\n{city_name}:")
        if assemble_map(city_name, cols, rows, tile_count):
            success_count += 1
    
    print("\n" + "=" * 60)
    print(f"✓ Successfully assembled {success_count}/{len(MAP_CONFIGS)} maps")
    print("=" * 60)
    print("\nNEXT STEPS:")
    print("1. Convert PNG files to BLP using BLPConverter:")
    print("   .\BLPConverter.exe /FBLP_DXT1_A0 /N /M *.png")
    print("2. Copy BLP files to: CitySiege\\Media\\Maps\\")
    print("\n✓ Square 1024x1024 PNG images ready for BLP conversion!")

if __name__ == "__main__":
    main()
