# vox_map_converter
Convert voxel format from 'https://drububu.com/miscellaneous/voxelizer' txt to a more compact form.

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
