"""
Assembles WoW map tiles into complete map images
Place this script in the folder with your converted PNG tiles
Run: python assemble_maps.py
"""

from PIL import Image
import os
import glob

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

def assemble_map(city_name, cols, rows, tile_count):
    """Assemble individual tiles into one complete map"""
    
    # Check if tiles exist
    tiles = []
    for i in range(1, tile_count + 1):
        tile_path = f"{city_name}{i}.png"
        if not os.path.exists(tile_path):
            print(f"  ⚠ Missing {tile_path}, skipping {city_name}")
            return False
        tiles.append(tile_path)
    
    # Create output image
    width = cols * TILE_SIZE
    height = rows * TILE_SIZE
    result = Image.new('RGB', (width, height))
    
    # Paste each tile
    print(f"  Assembling {tile_count} tiles into {width}x{height}...")
    for idx, tile_path in enumerate(tiles):
        img = Image.open(tile_path)
        x = (idx % cols) * TILE_SIZE
        y = (idx // cols) * TILE_SIZE
        result.paste(img, (x, y))
        img.close()
    
    # Save as TGA
    output_path = f"{city_name}.tga"
    result.save(output_path)
    result.close()
    
    print(f"  ✓ Created {output_path} ({width}x{height})")
    return True

def main():
    print("WoW Map Tile Assembler")
    print("=" * 50)
    
    success_count = 0
    
    for city_name, (cols, rows, tile_count) in MAP_CONFIGS.items():
        print(f"\n{city_name}:")
        if assemble_map(city_name, cols, rows, tile_count):
            success_count += 1
    
    print("\n" + "=" * 50)
    print(f"✓ Successfully assembled {success_count}/{len(MAP_CONFIGS)} maps")
    print("\nCopy the .tga files to: CitySiege\\Media\\Maps\\")

if __name__ == "__main__":
    main()
