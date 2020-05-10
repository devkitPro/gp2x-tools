#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <limits.h>

#define ARG_MAX 65536

#define O2X_MAGIC 0x3178326F

typedef struct {
  uint32_t length;
  uint32_t loadAddress;
} O2xSection;

typedef struct {
  uint32_t magic;
  char name[32];
  uint16_t icon[16*16];
  uint8_t reserved[8];
  uint32_t paramsLength;
  uint32_t paramsAddr;
  uint8_t numberOfSections;
} O2xHeader;

struct Section {
  char file[ARG_MAX];
  uint32_t loadAddress;
};

void usage();
int setIcon(O2xHeader* header, bool isSet, char* filename);
int writeSections(FILE* outFile, int n, struct Section* sections);

int main(int argc, char* argv[]) {

  static struct option long_options[] = {
    {"name", required_argument, 0, 'n'},
    {"icon", required_argument, 0, 'i'},
    {"section", required_argument, 0, 's'},
    {"out", required_argument, 0, 'o'},
    {"params", required_argument, 0, 'p'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
  };

  int numberOfSections = 0;
  struct Section* sections = malloc(1);
  
  uint32_t loadAddress = 0;
  char filename[ARG_MAX];
  
  bool iconSpecified = false;
  char icon[ARG_MAX];

  bool nameSpecified = false;
  char name[32];

  bool outputSpecified = false;
  char outputPath[ARG_MAX];

  uint32_t paramsLength = 0;
  uint32_t paramsAddr = 0;
  
  int option_index = 0;
  int c = 0;
  while((c = getopt_long(argc, argv, ":n:i:s:o:h", long_options, &option_index)) != -1) {
    switch(c) {
    case 'n':
      if(strlen(optarg) > 32) {
	fprintf(stderr, "Name must be 32 characters or less\n");
	return 1;
      }
      strncpy(name, optarg, 32);
      nameSpecified = true;
      break;

    case 'i':
      strncpy(icon, optarg, ARG_MAX);
      iconSpecified = true;
      break;

    case 's':
      strncpy(filename, optarg, ARG_MAX);
      if (optind < argc && *argv[optind] != '-') {
	loadAddress = strtoul(argv[optind], NULL, 0);
        optind++;
      } else {
        fprintf(stderr, "Option 's' takes a load file and a load address\n");
	usage();
	return 1;
      }

      numberOfSections++;
      sections = realloc(sections, sizeof(struct Section)*numberOfSections);

      strcpy(sections[numberOfSections-1].file, filename);
      sections[numberOfSections-1].loadAddress = loadAddress;
      break;

    case 'p':
      paramsAddr = strtoul(optarg, NULL, 0);
      if (optind < argc && *argv[optind] != '-') {
	paramsLength = strtoul(argv[optind], NULL, 0);
        optind++;
      } else {
        fprintf(stderr, "Option 'p' takes both the length of params address and a length in bytes\n");
	usage();
	return 1;
      }
      break;
      
    case 'o':
      strncpy(outputPath, optarg, ARG_MAX);
      outputSpecified = true;
      break;

    case 'h':
      usage();
      return 0;

    case ':':
      printf("Option '%c' needs an argument\n", optopt);
      return 1;

    case '?':
    default:
      printf("Unknown option '%c'\n", optopt);
      return 1;
    }
  }

  if(!nameSpecified) {
    fprintf(stderr, "Name must be specified\n");
    usage();
    return 1;
  }

  if(numberOfSections < 1) {
    fprintf(stderr, "At least one section be specified\n");
    usage();
    return 1;
  }

  if(!outputSpecified) {
    fprintf(stderr, "Output file must be specified\n");
    usage();
    return 1;
  }  
  
  FILE* outFile = fopen(outputPath, "wb");
  if(outFile == NULL) {
    fprintf(stderr, "Could not open file for writing\n");
    return 1;
  }

  O2xHeader header;
  header.magic = O2X_MAGIC;
  memcpy(header.name, name, 32);
  memset(header.reserved, 0, 8);
  header.paramsLength = paramsLength;
  header.paramsAddr = paramsAddr;
  header.numberOfSections = numberOfSections;

  // write file
  if(setIcon(&header, iconSpecified, icon)) {
    return 1;
  }
  fwrite((void*) &header, sizeof(O2xHeader), 1, outFile);
  if(writeSections(outFile, numberOfSections, sections)) {
    return 1;
  }

  fclose(outFile);
  free(sections);
  return 0;
}

int setIcon(O2xHeader* header, bool isSet, char* filename) {
  FILE* iconFile = fopen(filename, "rb");
  if(iconFile == NULL) {
    fprintf(stderr, "Could not open icon file\n");
    return 1;
  }

  fseek(iconFile, 0L, SEEK_END);
  int size = ftell(iconFile);
  if(size != 512) {
    fprintf(stderr, "Icon file should be 512 bytes, was %d\n", size);
    fclose(iconFile);
  }
  rewind(iconFile);

  if(fread(header->icon, sizeof(uint16_t), 16*16, iconFile) != (16*16)) {
    fprintf(stderr, "Could not read icon file\n");
    fclose(iconFile);
    return 1;
  }

  fclose(iconFile);
  return 0;
}

int writeSections(FILE* outFile, int n, struct Section* sections)  {
  for(int i = 0 ; i < n ; i++) {
    FILE* fp = fopen(sections[i].file, "rb");
    if(fp == NULL) {
      fprintf(stderr, "Could not open '%s'\n", sections[i].file);
      return 1;
    }

    fseek(fp, 0L, SEEK_END);
    int length = ftell(fp);
    rewind(fp);

    void* buf = malloc(length);
    if(buf == NULL) {
      fprintf(stderr, "Could not allocate enough memory to read file\n");
      fclose(fp);
      return 1;
    }

    if(fread(buf, sizeof(uint8_t), length, fp) != length) {
      fprintf(stderr, "Could not read contents of file into memory\n");
      fclose(fp);
      free(buf);
      return 1;
    }

    fclose(fp);

    O2xSection sectionHdr = {
      length,
      sections[i].loadAddress
    };

    fwrite((void*) &sectionHdr, sizeof(O2xSection), 1, outFile);
    fwrite(buf, sizeof(uint8_t), length, outFile);

    free(buf);
  }
  return 0;
}

void usage() {
  printf("mko2x - Produces an o2x executable for the GP2X (%s)\n\n", VERSION);
  // name. icon. section. out. help
  printf("--name,    -n   Set the executable name (max 32 characters, required)\n");
  printf("--icon,    -i   Path to a 16x16 icon in RGB565 binary format (optional)\n");
  printf("--out,     -o   Where to store the produced file (required)\n");
  printf("--section, -s   Binary data file followed by load address in GP2X memory (at least one required)\n");
  printf("--params,  -p   Address in memory params are stored, and maximum length of region (optional)\n");
  printf("--help,    -h   Display this message\n");

  printf("\nExample usage:\n");
  printf("mko2x --name \"Test game\" --section arm920.bin 0x0 --section arm940.bin 0x2000000 --out testgame.o2x\n");
}
