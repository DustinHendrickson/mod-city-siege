"""
Convert TGA map files to BLP format for WoW 3.3.5

MANUAL CONVERSION REQUIRED:
TGA files cannot be automatically converted to BLP without special tools.

SOLUTION OPTIONS:
=================

OPTION 1: BLPConverter (RECOMMENDED - Easy)
-------------------------------------------
1. Download BLPConverter: https://www.wowinterface.com/downloads/info8127
2. Open BLPConverter.exe
3. Drag your TGA files into it
4. Click "Convert to BLP"
5. Save the BLP files in this Maps folder

OPTION 2: Warcraft Art Tools (For Photoshop users)
--------------------------------------------------
1. Download from Blizzard's site
2. Open TGA in Photoshop
3. Save As -> BLP format

OPTION 3: Use PNG instead (FALLBACK - may not work)
---------------------------------------------------
This script will convert TGA to PNG as a fallback.
Note: WoW 3.3.5 addons typically don't support PNG either.
"""

import os
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Error: Pillow not installed. Run: pip install pillow")
    exit(1)

MAPS_DIR = Path(__file__).parent / "Maps"

def convert_tga_to_png(tga_path):
    """Convert TGA to PNG as fallback (may not work in WoW)"""
    try:
        img = Image.open(tga_path)
        
        # Convert to RGB if needed
        if img.mode != 'RGB':
            img = img.convert('RGB')
        
        # Save as PNG
        png_path = tga_path.with_suffix('.png')
        img.save(png_path, 'PNG')
        print(f"✓ Converted {tga_path.name} -> {png_path.name} (PNG fallback)")
        return True
    except Exception as e:
        print(f"✗ Failed to convert {tga_path.name}: {e}")
        return False

def main():
    print("=" * 60)
    print("IMPORTANT: Automatic BLP conversion is not available!")
    print("=" * 60)
    print("\nYou must manually convert TGA files to BLP format.")
    print("\nRECOMMENDED TOOL: BLPConverter")
    print("Download: https://www.wowinterface.com/downloads/info8127")
    print("\nSteps:")
    print("1. Download and run BLPConverter.exe")
    print("2. Drag your TGA files into the window")
    print("3. Click 'Convert to BLP'")
    print("4. Save BLP files in this Maps folder")
    print("\n" + "=" * 60)
    
    response = input("\nCreate PNG fallback files anyway? (y/n): ").lower()
    
    if response != 'y':
        print("\nConversion cancelled.")
        print("Please use BLPConverter manually.")
        return
    
    print(f"\nConverting TGA to PNG in: {MAPS_DIR}")
    
    if not MAPS_DIR.exists():
        print(f"Error: Maps directory not found: {MAPS_DIR}")
        return
    
    tga_files = list(MAPS_DIR.glob("*.tga"))
    
    if not tga_files:
        print("No TGA files found in Maps directory")
        return
    
    print(f"\nFound {len(tga_files)} TGA files:\n")
    
    success = 0
    for tga_file in tga_files:
        if convert_tga_to_png(tga_file):
            success += 1
    
    print(f"\n{success}/{len(tga_files)} PNG files created")
    print("\nWARNING: PNG files may not load in WoW 3.3.5!")
    print("Use BLPConverter to create proper BLP files.")

if __name__ == "__main__":
    main()
