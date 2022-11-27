# vox_map_converter
Convert voxel format from 'https://drububu.com/miscellaneous/voxelizer' txt to a more compact form.

Single source file only use the standard c library.

## Format

### Header 128 bits

|  Item  | Size |
|--------|------|
| 'VXMA' | 32 bits|
| Xsize  | 32 bits|
| Ysize  | 32 bits|
| Zsize  | 32 bits|

### Map

#### Stride > 0
Xsize * Ysize * Zsize * stride bytes

#### Stride == 0
Xsize * Ysize * Zsize bits

## Usage

vox_map_converter [OPTIONS] [FILE]

|Option|Result|
|------|------|
| -h   | Show Help |
| -o   | Output file (FILE.vox by default) |
| -s   | Size X,Y,Z (no offset no negative) (if not supplied will be deduced with offset and negative positions) |
| -b   | Stride (1=uint8 2=uint16 4=uint32 0=1bit) others values are invalid (1 by default) |
| -r   | RLE compressed (stride will be ignored (uint32:uint8) will be used |
| -l   | same as -r but the lenght do not carry through Y increment |

## Build
exemple (gcc ./vox_map_converter.c -o ./vox_map_converter) optimisations flags can be use.
