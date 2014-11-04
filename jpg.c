#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "jpg.h"

#define ZEROTH 0
#define EXIF 1
/*
 * Analyze a JPG file that contains Exif data.
 * If it is a JPG file, print out all relevant metadata and return 0.
 * If it isn't a JPG file, return -1 and print nothing.
 */
int fread_safe(void*, size_t, size_t, FILE*);
int fseek_safe(FILE*, long int, int);

int analyze_jpg(FILE *f) {
  unsigned char marker[2];
  //JPEG MUST start with this. Else Invalid.
  unsigned char START_OF_IMAGE[2] = { 0xff, 0xd8 };
  //JPGE MUST end with this.
  unsigned char END_OF_IMAGE[2] = { 0xff, 0xd9 };
  //Marks TIFF file, only data we need to parse.
  unsigned char APP1_chunk[2] = { 0xff, 0xe1 };
  unsigned int num_bytes;
  int app1 = 0;
  if(f == NULL) return -1; 

  num_bytes = fread_safe(marker, 1, 2, f); // marker[0] is ALWAYS 0xff
  if(num_bytes != 2) return -1;

  if(memcmp(marker, START_OF_IMAGE, 2) != 0) return -1;
  
  while(fgetc(f) != EOF) {
    fseek_safe(f, -1, SEEK_CUR);
    num_bytes = fread_safe(marker, 1, 2, f);
    if(num_bytes != 2) return 0;
    if(marker[0] != 0xff) {
      printf("Invalid marker. 0x%x 0x%x \n", marker[0], marker[1]);
      return -1;
    }

    if(marker[1] == 0xd0 ||
       marker[1] == 0xd1 ||
       marker[1] == 0xd2 ||
       marker[1] == 0xd3 ||
       marker[1] == 0xd4 ||
       marker[1] == 0xd5 ||
       marker[1] == 0xd6 ||
       marker[1] == 0xd7 ||
       marker[1] == 0xd8 ||
       marker[1] == 0xd9 ||
       marker[1] == 0xda) { 
      parseSuperChunk(f);
    }

    else if(memcmp(marker, END_OF_IMAGE, 2) == 0) 
      break;

    else if(memcmp(marker, APP1_chunk, 2) == 0) {
      if(app1 == 0) {
        analyzeTIFF(f);
        app1 = 1;
      } else {
        parseChunk(f);
      }
      break;
    }
    else { //standard chunk
      parseChunk(f);
    }
  }
  //Print one line per EXIF Tag containing:
  //          DocumentName, ImageDescription, Make, Model, Software, DateTime, Artist
  //          HostComputer, Copyright, RelatedSoundFile, DateTimeOriginal, DateTimeDigitized
  //          MakerNote, UserComment, ImageUniqueID
  return 0;
}

void analyzeTIFF(FILE* f) {
  unsigned char expected[6] = {0x45, 0x78, 0x69, 0x66, 0x00, 0x00};
  unsigned char header[6];
  unsigned char tiff_header[8];
  long int IFD = 0; //signed to detect negative offsets
  fpos_t tiff_front, pos;
  int num_bytes, parseIFDflag;
  unsigned int EXIF_offset = 0;
  unsigned int length;
  unsigned char len_field[2];
  
  fseek_safe(f, 2, SEEK_CUR);

  num_bytes = fread_safe(header, 1, 6, f);
  if(num_bytes != 6) exit(EXIT_FAILURE); 

  if(memcmp(header, expected, 6) != 0) {
    printf("Error in Tiff Format.\n");
    exit(EXIT_FAILURE);
  }
  //Read in Tiff Header.
  fgetpos(f, &tiff_front); //save for future offset calculations

  num_bytes = fread_safe(tiff_header, 1, 8, f);
  if(num_bytes < 8) exit(EXIT_FAILURE); 

  //NOTE: Tiff Headers always Little Endian for our purposes.
  //Check anyway for safety.
  if(tiff_header[0] != 0x49 && tiff_header[1] != 0x49) {
    printf("Shizzle, its not little endian. We dont have to worry.\n");
    exit(EXIT_FAILURE);
  }
  if(tiff_header[2] != 0x2a && tiff_header[3] != 0x00) {
    printf("Magic String is incorrect.\n");
    exit(EXIT_FAILURE);
  }
  
  int i;
  for(i = 4; i < 8; i++) {
    //Convert offset (IFD) little endian. IFD's last 4 bytes of header.
    IFD += (tiff_header[i] << 8*(i-4));
  }

  IFD -= 8; //relative to beginning of TIFF header
  if(IFD < 0) {
    printf("Negative/invalid offset into TIFF file.\n");
    exit(EXIT_FAILURE);
  }

  //Move file pointer ahead by IFD bytes.
  //Now we're pointing at beginning of 0th Tag Structure.
  fseek_safe(f, IFD, SEEK_CUR);
  
  //Next two bytes are number of Tag Structs in JPEG.
  parseIFDflag = parseIFD(f, &tiff_front, ZEROTH);
  if(parseIFDflag == -1) {
    printf("EOF during IFD parsing.\n");
    return; 
  } else { EXIF_offset = (unsigned int) parseIFDflag; }

  fgetpos(f, &pos);
  fsetpos(f, &tiff_front);
  fseek_safe(f, EXIF_offset, SEEK_CUR);
  parseIFD(f, &tiff_front, EXIF);
    
  //Restore file pointer (epilogue)
  fsetpos(f, &pos);

  num_bytes = fread_safe(len_field, 1, 2, f);

  if(num_bytes < 2) exit(EXIT_FAILURE);

  length = (len_field[0] << 8) + len_field[1];
  if(length-2 > 0)
    fseek_safe(f, length-2, SEEK_CUR);
}

int parseIFD(FILE* f, fpos_t* tiff_front, int option) {
  int num_bytes;
  unsigned char tag_struct_count_data[2];
  unsigned int tag_struct_count;
  unsigned char tag_id_data[2];
  unsigned char datatype_data[2];
  unsigned char count_data[4];
  unsigned char offset_or_value_data[4];
  unsigned int tag_id, datatype, count = 0, offset_or_value = 0;
  unsigned int i, j;
  unsigned int EXIF_tag = 0x8769;
  unsigned int EXIF_count = 0;
  unsigned int EXIF_offset = 0;
  
  num_bytes = fread_safe(tag_struct_count_data, 1, 2, f);
  if(num_bytes < 2) { return -1; }
  tag_struct_count = tag_struct_count_data[0] + (tag_struct_count_data[1] << 8);

  for(i = 0; i < tag_struct_count; i++) {
    num_bytes = fread_safe(tag_id_data, 1, 2, f);
    if(num_bytes < 2) { return -1; }
    num_bytes = fread_safe(datatype_data, 1, 2, f);
    if(num_bytes < 2) { return -1; }
    num_bytes = fread_safe(count_data, 1, 4, f);
    if(num_bytes < 4) { return -1; }    
    num_bytes = fread_safe(offset_or_value_data, 1, 4, f);
    if(num_bytes < 4) { return -1; }

    tag_id = tag_id_data[0] + (tag_id_data[1] << 8);
    datatype = datatype_data[0] + (datatype_data[1] << 8);
    count = 0;
    offset_or_value = 0;

    for(j = 0; j <= 3; j++) {
      count += count_data[j] << 8*j;
      offset_or_value += offset_or_value_data[j] << 8*j;
    }

    if(option == ZEROTH) {
      if(tag_id == EXIF_tag) {
        //Found the EXIF flag / EXIF ID ptr 
        //Start position of the EXIF ID
        EXIF_count++;
        if(EXIF_count > 1) {
          printf("More than one EXIF tag found.\n");
          exit(EXIT_FAILURE);
        }
        EXIF_offset = offset_or_value;
      }
    } 
    printTag(f, tiff_front, tag_id, datatype, count, offset_or_value);  
  }
  if(option == EXIF) {
    return -2;
  } else {
    return EXIF_offset;
  }
}

void parseSuperChunk(FILE* f) {
  unsigned int count = 0;
  unsigned char datum[1];
  unsigned int check = 0;
  int num_bytes;
  fpos_t pos;
  fgetpos(f, &pos);
  num_bytes = fread_safe(datum, 1, 1, f);
  if(num_bytes < 1) { printf("supermega fail\n"); return; }
  while(check == 0 || datum[0] == 0x00) {
    if(datum[0] == 0xff) {
      check = 1;
    } 
    else if(check == 1) {
      check = 0;
      count += 2;
    }
    else {
      count++;
    }
    fread_safe(datum, 1, 1, f);
    if(num_bytes < 1) { printf("super fail\n"); return; }
  }
  fsetpos(f, &pos);
  fseek_safe(f, count, SEEK_CUR);
}

void parseChunk(FILE* f) {
  unsigned int length, num_bytes;
  unsigned char len_field[2];

  num_bytes = fread_safe(len_field, 1, 2, f);
  if(num_bytes < 2) { return; }

  length = (len_field[0] << 8) + len_field[1];
  if(length-2 > 0) {
    fseek_safe(f, length-2, SEEK_CUR);
  }
}

//Memory Safe Implementation of fread()
int fread_safe(void* ptr, size_t size, size_t count, FILE* stream) {
  int retval = fread(ptr, size, count, stream);
  if(ferror(stream) != 0) { // or retVal < size ?
    printf("Error reading from input file.\n");
    exit(EXIT_FAILURE);
  }
  return retval;
}
//Memory Safe Implementation of fseek()
int fseek_safe(FILE* stream, long int offset, int origin) {
  int retval = fseek(stream, offset, origin);
  if(ferror(stream) || retval != 0) {
    printf("Error during fseek. Retval: %d, Offset: %ld, Origin: %d\n", retval, offset, origin);
    exit(EXIT_FAILURE);
  }
  return retval;
}

void printTag(FILE* f, fpos_t* tiff_front, unsigned int tag, 
              unsigned int datatype, unsigned int count, unsigned int offset_or_value) {
  //prologue - saves old file pointer for restoration at end
  char* format;
  int user_comment = 0;
  switch(tag) {
    case 0x010D:
      //DocumentName:
      format = "DocumentName: %s\n";
      break;
    case 0x010E:
      //ImageDescription
      format = "ImageDescription: %s\n";
      break;
    case 0x010F:
      //Make
      format = "Make: %s\n";
      break;
    case 0x0110:
      //Model
      format = "Model: %s\n";
      break;
    case 0x0131:
      //Software
      format = "Software: %s\n";
      break;
    case 0x0132:
      //DateTime:
      format = "DateTime: %s\n";
      break;
    case 0x013B:
      //Artist:
      format = "Artist: %s\n";
      break;
    case 0x013C:
      //HostComputer
      format = "HostComputer: %s\n";
      break;
    case 0x8298:
      //Copyright
      format = "Copyright: %s\n";
      break;
    case 0xA004:
      //RelatedSoundFile
      format = "RelatedSoundFile: %s\n";
      break;
    case 0x9003:
      //DateTimeOriginal
      format = "DateTimeOriginal: %s\n";
      break;
    case 0x9004:
      //DateTimeDigitized
      format = "DateTimeDigitized: %s\n";
      break;
    case 0x927C:
      //MakerNote [Special Case, use count field.]
      format = "MakerNote: %s\n";
      break;
    case 0x9286:
      //UserComment [Specical Case, use count field.]
      format = "UserComment: %s\n";
      user_comment = 1;
      break;
    case 0xA420:
      //ImageUniqueID
      format = "ImageUniqueID: %s\n";
      break;
    
    default: //"If tag not in tag list, just skip it"
      return;
  }

  fpos_t pos;
  fgetpos(f, &pos);
  int num_bytes = 0;
  unsigned char* data = (unsigned char*) malloc(count*sizeof(unsigned char)+1);

  if(count > 1048576) //2^20
    return;
  
  if(data == NULL) {
    printf("Bad malloc in print tag! %u\n", count);
    return;
  }
  data[count] = 0x00;

  if(count > 4) { //offset
    fsetpos(f, tiff_front);
    fseek_safe(f, offset_or_value, SEEK_CUR);
    num_bytes = fread_safe(data, 1, count, f);
    if(num_bytes < count) { return; }
  }
  else {
    int i;
    for(i = 0; i < count; i++) {
      data[i] = (offset_or_value >> 8*i) & 255;
    }
  }
  if(user_comment == 0) {
    printf(format, data);
  } else {
    if(count <= 8) { exit(EXIT_FAILURE); }
    printf(format, data+8);
  }
  free(data);
  fsetpos(f, &pos);
}
