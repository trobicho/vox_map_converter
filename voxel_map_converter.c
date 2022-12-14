#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#define BUFFER_READ_SIZE    1000000 //1 Mo Buffer
#define ALLOC_STEP_VOX_POS  100000

typedef struct  VoxSize {
  uint32_t  x;
  uint32_t  y;
  uint32_t  z;
}VoxSize;

typedef struct  ivec3 {
  int32_t  x;
  int32_t  y;
  int32_t  z;
}ivec3;

uint8_t* allocate_vox_map(const VoxSize vsize, int stride_byte, uint64_t *rsize) {
  uint64_t size = vsize.x * vsize.y * vsize.z;
  uint64_t final_size;

  if (size > 4000000000 && stride_byte != 0)
    return (NULL);

  if (stride_byte < 1) {
    final_size = size / 8;
    if (size % 8)
      final_size += 1;
  }
  else
    final_size = size * stride_byte;

  if (final_size > 4000000000) //Max final VoxMap 1Go
    return (NULL);
  *rsize = final_size;
  return (calloc(final_size, 1));
}

void  printHelp() {
  printf("Usage: vox_converter [OPTION] [FILE]\n");
  printf("Convert voxel format from 'https://drububu.com/miscellaneous/voxelizer' txt to compact form.\n");
  printf("  format is in binary header: [VXMA Xsize[32b] Ysize[32b] Zsize[32b]]\n  followed by voxelMap:[Xsize*Ysize*Zsize * (stride byte or 1bit if stride=0)]\n");
  printf("  or if compressed is use: [VXMC Xsize[32b] Ysize[32b] Zsize[32b]]\n  followed by voxelMap RLE compressed (uint32:uint8)\n\n");

  printf("  -h\tshow this help\n");
  printf("  -o\toutput filename\n");
  printf("  -s\tsize x,y,z (all negative position in the file will be ignored)\n\t(if not supplied will be deduced and negative handled)\n");
  printf("  -b\tstride (1=uint8 2=uint16 4=uint32 0=1bit all other are invalid) (1 by default)\n");
  printf("  -r\tRLE compressed (stride will be ignored (uint32:uint8) will be used (could be larger))\n");
  printf("  -l\tsame as -r but the lenght do not carry through Y increment\n");
}

int   getStride(char* arg) {
  int stride;

  sscanf(arg, "%d", &stride);
  if (stride >= 0 && stride <= 4) {
    return (stride);
  }
  return (-1);
}

void  cmpPos(ivec3* min, ivec3 *max, ivec3 pos) {
  if (min->x > pos.x)
    min->x = pos.x;
  if (min->y > pos.y)
    min->y = pos.y;
  if (min->z > pos.z)
    min->z = pos.z;

  if (max->x < pos.x)
    max->x = pos.x;
  if (max->y < pos.y)
    max->y = pos.y;
  if (max->z < pos.z)
    max->z = pos.z;
}

void  writeInMap(ivec3 pos, uint8_t* map, VoxSize vsize, int stride) {
  uint32_t index = pos.x + pos.y * vsize.x + pos.z * (vsize.x + vsize.y);
  if (stride != 0) {
    index *= stride;
    map[index] = 1;
  }
  else {
    index / 8;
    int bit = 7 - (index % 8);
    map[index] |= (1 << bit);
  }
}

int   read_pos(char buffer[BUFFER_READ_SIZE], int size_read, int *offset, ivec3 *pos, int res) {
  static char   lastCharBuf[12] = {'\0'};
  static int    lastCharIndex = 0;
  static int    countNfind = 0;
  static int    shrNextLine = 0;

  int   newOffset = *offset;
  int   n = 0;
  int   findN = 0;

  int shr = *offset;
  for (int i = 0; i + *offset < size_read; i++) {
    shr = *offset + i;
    if (shrNextLine) {
      if (buffer[shr] == '\n') {
        countNfind = 0;
        shrNextLine = 0;
        lastCharIndex = 0;
      }
    }
    else {
      if (findN || lastCharIndex > 0) {
        if (buffer[shr] >= '0' && buffer[shr] <= '9') {
          lastCharBuf[lastCharIndex++] = buffer[shr];
        }
        findN++;
        if (buffer[shr] == ',') {
          lastCharBuf[lastCharIndex] = '\0';
          findN = 0;
          sscanf(lastCharBuf, "%d", &n);
          lastCharIndex = 0;
          if (countNfind == 0)
            pos->x = n;
          else if (countNfind == 1)
            pos->y = n;
          else if (countNfind == 2) {
            pos->z = n;
            shrNextLine = 1;
            *offset = shr;
            countNfind = 0;
            break;
          }
          countNfind++;
          newOffset = shr;
        }
      }
      else {
        if (buffer[shr]) {
          if (buffer[shr] >= '0' && buffer[shr] <= '9') {
            lastCharBuf[lastCharIndex++] = buffer[shr];
            findN = 1;
          }
        }
      }
    }
  }
  *offset = newOffset;
  if (shr == size_read - 1)
    return (-1);
  if (countNfind < 3 && !shrNextLine) {
    return (1);
  }
  return (0);
}

uint64_t  unknownSizeParsing(FILE* file, uint8_t** map, VoxSize *vsize, int stride) {
  char    buffer[BUFFER_READ_SIZE];
  ivec3*  bufferPos = malloc(ALLOC_STEP_VOX_POS * sizeof(ivec3));
  ivec3   pos;
  int     offset = 0;
  int     offsetPos = 0;
  int     res = 0;
  int     sizeBuffer = ALLOC_STEP_VOX_POS;
  ivec3   min, max;
  int     minmaxSet = 0;
  int     size_read;

  if (bufferPos == NULL)
    return (-1);

  size_read = (int)fread(buffer, 1, BUFFER_READ_SIZE, file);
  while (res != -1 || !feof(file)) {
    res = read_pos(buffer, size_read, &offset, &pos, res);
    if (res != 0) {
      if (!feof(file)) {
        size_read = fread(buffer, 1, BUFFER_READ_SIZE, file);
        offset = 0;
      }
      else {
        if (res < 0)
          break;
        else
          return (-1);
      }
    }
    if (res == 0) {
      if (!minmaxSet) {
        min = pos;
        max = pos;
        minmaxSet = 1;
      }
      else {
        cmpPos(&min, &max, pos);
      }
      if (sizeBuffer <= offsetPos) {
        sizeBuffer += ALLOC_STEP_VOX_POS;
        bufferPos = realloc(bufferPos, sizeBuffer * sizeof(ivec3));
      }
      bufferPos[offsetPos++] = pos;
    }
  }
  vsize->x = (max.x - min.x) + 1;
  vsize->y = (max.y - min.y) + 1;
  vsize->z = (max.z - min.z) + 1;
  uint64_t msize = 0;
  *map = allocate_vox_map(*vsize, stride, &msize);
  for (int i = 0; i < offsetPos; i++) {
    ivec3 pos = bufferPos[i];
    pos.x += min.x;
    pos.y += min.y;
    pos.z += min.z;
    writeInMap(pos, *map, *vsize, stride);
  }
  free(bufferPos);
  return (msize);
}

int   knownSizeParsing(FILE* file, uint8_t* map, VoxSize vsize, int stride) {
  char  buffer[BUFFER_READ_SIZE];
  ivec3 pos;
  int   offset = 0;
  int   res = 0;
  int   size_read;
        
  size_read = fread(buffer, 1, BUFFER_READ_SIZE, file);
  while (res != -1 || !feof(file)) {
    res = read_pos(buffer, size_read, &offset, &pos, res);
    if (res != 0) {
      if (!feof(file)) {
        size_read = fread(buffer, 1, BUFFER_READ_SIZE, file);
        offset = 0;
      }
      else if (res != -1)
        return (-1);
    }
    if (res == 0) {
      writeInMap(pos, map, vsize, stride);
    }
  }
  return (0);
}

int   parseArgSize(char* arg, VoxSize* size) {
  int x = 0, y = 0, z = 0;
  sscanf(arg, "%d,%d,%d", &x, &y, &z);
  if (x > 0 && y > 0 && z > 0) {
    size->x = (uint32_t)x;
    size->y = (uint32_t)y;
    size->z = (uint32_t)z;
    return (1);
  }
  return (0);
}

int write_map(FILE* file, uint8_t *voxMap, VoxSize vsize, uint64_t mapSize) {
  uint32_t header[4];
  header[0] = 'V' << 24 | 'O' << 16 | 'M' << 8 | 'A';
  header[1] = vsize.x;
  header[2] = vsize.y;
  header[3] = vsize.z;
  if (fwrite(header, 1, 16, file) != 16) {
    printf("error: (%s)\n", ferror(file));
    return (-1);
  }
  size_t sizeWritten = fwrite(voxMap, 1, mapSize, file);
  if (sizeWritten != mapSize) {
    printf("size written %d, expected: %d\n", sizeWritten, mapSize);
    printf("error: (%s)\n", ferror(file));
    return (-1);
  }
  else {
    printf("size written %d bytes\n", sizeWritten);
  }
  return (0);
}

int write_map_RLE(FILE* file, uint8_t *voxMap, VoxSize vsize, uint64_t mapSize, int rleLineBreak) {
  uint32_t header[4];
  header[0] = 'V' << 24 | 'O' << 16 | 'M' << 8 | 'C';
  header[1] = vsize.x;
  header[2] = vsize.y;
  header[3] = vsize.z;

  if (fwrite(header, 1, 16, file) != 16) {
    printf("error: (%s)\n", ferror(file));
    return (-1);
  }

  uint32_t  nbValue = 0;
  uint8_t   bufferRLE[BUFFER_READ_SIZE];
  int       offset = 0;
  size_t    sizeExpected = 0;
  size_t    sizeWritten = 0;
  int       lastValue = voxMap[0];
  uint32_t  RL = 0;
  printf("mapSize = %d\n", mapSize);

  for (int i = 1; i < mapSize; i++) {
    RL++;
    if (voxMap[i] != lastValue || (rleLineBreak && i % vsize.x == 0)) {
      *((uint32_t*)(bufferRLE + offset)) = RL;
      bufferRLE[offset + 4] = lastValue;
      offset += 5;
      sizeExpected += (size_t)5;
      lastValue = voxMap[i];
      RL = 0;
      nbValue++;
    }
    if (offset + 5 > BUFFER_READ_SIZE || RL + 1 == UINT_MAX) {
      sizeWritten += fwrite(bufferRLE, 1, offset, file);
      offset = 0;
    }
  }
  if (offset > 0) {
    sizeWritten += fwrite(bufferRLE, 1, offset, file);
    offset = 0;
  }
  if (RL > 0) {
    *((uint32_t*)bufferRLE) = RL;
    bufferRLE[4] = lastValue;
    sizeExpected += (size_t)5;
    sizeWritten += fwrite(bufferRLE, 1, 5, file);
    nbValue++;
  }
  printf("number of value written: %u\n", nbValue);
  if (sizeWritten != sizeExpected) {
    printf("size written %d, expected: %d\n", sizeWritten, sizeExpected);
    printf("error: (%s)\n", ferror(file));
    return (-1);
  }
  else {
    printf("size written %d bytes\n", sizeWritten);
  }
  return (0);
}

int main(int ac, char** av) {
  char*     filename;
  char      outFilename[1000] = {'\0'};
  FILE*     file;
  FILE*     out_file;
  int       stride = 1;
  VoxSize   voxSize;
  int       voxSizeIsKnow = 0;
  uint8_t*  voxMap;
  int       error = 0;
  int	    compressed = 0;

  if (ac > 1) {
    for (int i = 1; i < ac; i++) {
      if (av[i][0] == '-') {
        if (i >= (ac + 1)) {
          printHelp();
          return (1);
        }
        switch(av[i][1]) {
          case 'h':
            printHelp();
            return (0);
            break;
          case 'b':
            stride = getStride(av[++i]);
            if (stride < 0) {
              printf("Invalid stride: %s\n", av[i]);
              printHelp();
              return (1);
            }
            break;
          case 's':
            if (!parseArgSize(av[++i], &voxSize)) {
              printf("Invalid size: %s\n", av[i]);
              printHelp();
              return (1);
            }
            voxSizeIsKnow = 1;
            break;
          case 'r':
            compressed = 1;
            break;
          case 'l':
            compressed = 2;
            break;
          case 'o':
            strcpy(outFilename, av[++i]);
            break;
        }
      }
      else {
        filename = av[i];
        if (outFilename[0] == '\0') {
          strcpy(outFilename, filename);
          strcat(outFilename , ".vox");
        }
        printf("converting file %s saving to %s\n...\n", filename, outFilename);
        break;
      }
    }
  }
  else {
    printHelp();
    return (0);
  }
  if (compressed) {
  	stride = 1;
  }

  file = fopen(filename, "r");
  if (file == NULL) {
    printf("error opening file %s\n", filename);
    printHelp();
    error = 1;
    return (error);
  }

  uint64_t mapSize = 0;
  if (voxSizeIsKnow) {
    voxMap = allocate_vox_map(voxSize, stride, &mapSize);
    if (voxMap == NULL) {
      printf("Invalid size: X:%d, Y:%d, Z:%d\n", voxSize.x, voxSize.y, voxSize.z);
      printHelp();
      error = 1;
    }
    else if (knownSizeParsing(file, voxMap, voxSize, stride) != 0) {
      printf("error while parsing file\n");
      error = 1;
    }
    else
      printf("success mapDimension = %d, %d, %d\n", voxSize.x, voxSize.y, voxSize.z);
  }
  else {
    if ((mapSize = unknownSizeParsing(file, &voxMap, &voxSize, stride)) < 0) {
      printf("error while parsing file\n");
      error = 1;
    }
    else
      printf("success mapDimension = %d, %d, %d\n", voxSize.x, voxSize.y, voxSize.z);
  }
  fclose(file);
  if (error) {
    free(voxMap);
    return (error);
  }
  out_file = fopen(outFilename, "w");
  if (out_file == NULL) {
    printf("error opening out_file for writing %s\n", outFilename);
    free(voxMap);
    return (error);
  }
  if (compressed == 0) {
    if (write_map(out_file, voxMap, voxSize, mapSize) != 0) {
      printf("error writing to file %s", outFilename);
      error = 1;
    }
  }
  else if (compressed) {
    if (write_map_RLE(out_file, voxMap, voxSize, mapSize, compressed == 2) != 0) {
      printf("error writing to file %s", outFilename);
      error = 1;
    }
  }
  free(voxMap);
  fclose(out_file);
  return (error);
}
