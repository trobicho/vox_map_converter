#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

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

uint8_t* allocate_vox_map(const VoxSize vsize, int stride_byte, int *rsize) {
  uint64_t size = vsize.x * vsize.y * vsize.z;
  uint64_t final_size;

  if (size > 1000000000 && stride_byte != 0)
    return (NULL);

  if (stride_byte < 1) {
    final_size = size / 8;
    if (size % 8)
      final_size += 1;
  }
  else
    final_size = size * stride_byte;

  if (final_size > 1000000000) //Max final VoxMap 1Go
    return (NULL);
  *rsize = final_size;
  return (calloc(final_size, 1));
}

void  printHelp() {
  printf("Usage: vox_converter [OPTION] [FILE]\n");
  printf("Convert voxel format from 'https://drububu.com/miscellaneous/voxelizer' txt to compact form.\n");
  printf("  format is in binary header:[VXMA Xsize[32b] Ysize[32b] Zsize[32b]]\n  followed by voxelMap:[Xsize*Ysize*Zsize * (stride byte or 1bit if stride=0)]\n\n");

  printf("  -h\tshow this help\n");
  printf("  -o\toutput filename\n");
  printf("  -s\tsize x,y,z (all negative position in the file will be ignored)\n\t(if not supplied will be deduced and negative handled)\n");
  printf("  -b\tstride (1=uint8 2=uint16 4=uint32 0=1bit all other are invalid) (1 by default)\n");
}

int   getStride(char* arg) {
  int stride;

  if (arg[1] == '\0' && arg[0] >= '0' && arg[0] <= '4') {
    stride = (int)(arg[0] - '0');
    if (stride == 3)
      return (-1);
  }

  else
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
          if (n > 1000) {
            printf("test %d\n", n);
          }
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

int   unknownSizeParsing(FILE* file, uint8_t** map, VoxSize *vsize, int stride) {
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
        if (pos.x > 1000)
          printf("teste\n");
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
  int msize = 0;
  *map = allocate_vox_map(*vsize, stride, &msize);
  int i = 0;
  for (i = 0; i < offsetPos; i++) {
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

int write_map(FILE* file, uint8_t *voxMap, VoxSize vsize, int mapSize) {
  uint32_t header[4];
  header[0] = 'V' << 24 | 'O' << 16 | 'M' << 8 | 'A';
  header[1] = vsize.x;
  header[2] = vsize.y;
  header[3] = vsize.z;
  if (fwrite(header, sizeof(uint32_t), 4, file) != 4) {
    printf("error: (%s)\n", ferror(file));
    return (-1);
  }
  size_t sizeWrited = fwrite(voxMap, 1, mapSize, file);
  if (sizeWrited != mapSize) {
    printf("size writed %d, expected: %d\n", sizeWrited, mapSize);
    printf("error: (%s)\n", ferror(file));
    return (-1);
  }
  else {
    printf("size writed %d bytes\n", sizeWrited, mapSize);
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
            if ((stride = getStride(av[++i])) < 0) {
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

  file = fopen(filename, "r");
  if (file == NULL) {
    printf("error opening file %s", filename);
    printHelp();
  }

  int mapSize = 0;
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
    printf("error opening out_file for writing %s", outFilename);
    free(voxMap);
    return (error);
  }
  if (write_map(out_file, voxMap, voxSize, mapSize) != 0) {
    printf("error writing to file %s", outFilename);
  }
  free(voxMap);
  fclose(out_file);
  return (error);
}
