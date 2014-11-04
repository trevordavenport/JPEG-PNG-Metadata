#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zlib.h>
#include "png.h"

int fread_safe(void*, size_t, size_t, FILE*);
int fseek_safe(FILE*, long int, int);

/*
 * Analyze a PNG file.
 * If it is a PNG file, print out all relevant metadata and return 0.
 * If it isn't a PNG file, return -1 and print nothing.
 */
int analyze_png(FILE *f) {
    char buff[8];
    int i = 0;
    int num_bytes = 0;
    char expected[] = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
    unsigned char length[4] = {0,0,0,0};
    unsigned char chunk_type[4] = {0,0,0,0};
    unsigned int size = 0;
    unsigned char text[] = { 0x74, 0x45, 0x58, 0x74 };
    unsigned char zTxt[] = { 0x7A, 0x54, 0x58, 0x74 };
    unsigned char tIME[] = { 0x74, 0x49, 0x4D, 0x45 };
    unsigned char IEND[] = { 0x49, 0x45, 0x4e, 0x44 };
    int tIMEcount = 0; //Must never be greater than 1 per png file    

    if (f == NULL) 
        return -1;

    num_bytes = fread_safe(buff, 1, 8, f); //Make sure png is 8 bytes, else we overflow
    if(num_bytes < 8) 
        return -1;

    //Check to make sure first 8 bytes are valid.
    if(memcmp(buff,expected, 8) != 0)
	   return -1;

    while(fgetc(f) != EOF) {
	   fseek_safe(f, -1, SEEK_CUR);
	   //read length
	   size = 0;
	   num_bytes = fread_safe(length, 1, 4, f);
	   if(num_bytes < 4) 
         return -1; 

	for(i = 0; i <= 3; i++) {
	    size += (length[i] << (24-8*i));
	}
	if(size > 1048576) 
      return -1; 

	//Read in one chunk_type
	num_bytes = fread_safe(chunk_type, 1, 4, f);
	if(num_bytes < 4) 
	    return -1;

	if (memcmp(chunk_type, text, 4) == 0) 
	    analyzeText(f, size);
	else if (memcmp(chunk_type, zTxt, 4) == 0) 
	    analyzeZtxt(f, size);
	else if (memcmp(chunk_type, tIME, 4) == 0) { 
	    if(tIMEcount > 1) {
		  printf("Invalid PNG File. TimeCount > 1\n"); 
		  return -1;
	    } 
	    else if(size != 7) {
		  printf("Invalid tIME chunk size.\n");
		  return -1;
	    }
	    else {
		  tIMEcount = tIMEcount + 1;
		  analyzeTime(f, size);
	    }
	}
	else if(memcmp(chunk_type, IEND, 4) == 0) {
	    break;
	}
	else {
	    fseek_safe(f, size, SEEK_CUR);
	}

	fseek_safe(f, 4, SEEK_CUR);
    }
    return 0;
}

void analyzeText(FILE* f, unsigned int size) {
    unsigned char data[size];
    unsigned int result = 0;
    unsigned int key_length = 0;
    result = fread_safe(data, 1, size, f);

    if ( result < size )
        return; 
    //Key
    key_length = keyLength(data, size);
    if(key_length == size) { //no null separator found
	    return; //-1; 

    unsigned char key[key_length+1];
    memcpy(key, data, key_length);
    key[key_length] = 0x00;

    unsigned int val_length = (size - key_length) - 1; //Make sure not negative, will underflow
    unsigned char value[val_length+1];
    memcpy(value, data+key_length+1, val_length); //TODO: (should be val_length or val - 1???) Check off by one's 
    value[val_length] = 0x00;
    printKV(key, value);
}

//Helper Print Function for Key: Value
void printKV(unsigned char key[], unsigned char value[]) {
    printf("%s: %s\n", key, value);
}

//Helper Function to grab Key Lengths
unsigned int keyLength(unsigned char data[], unsigned int size) {
    unsigned int key_size = 0;
    unsigned int i = 0;
    for(i = 0; i < size && data[i] != 0x00; i++) {
	   key_size++;
    }
    return key_size;
}

void analyzeZtxt(FILE* f, unsigned int size) {
    if(size - 2 > size) { exit(EXIT_FAILURE); } //underflow check
    unsigned char data[size];
    unsigned int key_length = 0;
    unsigned int compressionType = 0;
    unsigned int compressionSize = 0;
    int uncompress_code = Z_BUF_ERROR;
    uLongf udata_len = 1000;
    int num_bytes = 0;
    unsigned char* udata = (unsigned char*) malloc(sizeof(unsigned char) * udata_len + 1);

    if(udata == NULL) {
	   printf("Bad malloc.\n");
	   exit(EXIT_FAILURE);
    }

    num_bytes = fread_safe(data, 1, size, f);
    if(num_bytes < size)
        exit(EXIT_FAILURE);

    //read key length
    key_length = keyLength(data, size);
    if(key_length > size-2) { //double-check lower bound?
	   printf("Key_length > size - 2 clause.\n");
	   return;
    }
    unsigned char key[key_length+2];
    memcpy(key, data, key_length+2);
    if(key[key_length] != 0x00) 
        exit(EXIT_FAILURE); //Value after key is not Null.
   
    compressionType = key[key_length + 1];
    if (compressionType != 0x00) { 
	   printf("Error in compressionType not null\n");
	   return; 
    } 

    compressionSize = (size - key_length) - 2; 
    if(compressionSize == 0) 
        exit(EXIT_FAILURE);

    unsigned char compressionValue[compressionSize];
    memcpy(compressionValue, data+key_length+2, compressionSize); //Copy compression values

    //1. skip over null byte
    //2. Read in compressiontype (1byte) make sure its 0x00
    //3. Read in compressionValue (also zlib.uncompress() ??? )
    while(uncompress_code != Z_OK && udata_len * 2 > udata_len && udata_len*2 < 1048576) {
    	udata_len *= 2;
    	uncompress_code = uncompress((Bytef *) udata, 
    		&udata_len, 
    		(Bytef *) compressionValue, 
    		compressionSize);
    	if(uncompress_code == Z_MEM_ERROR || 
    		uncompress_code == Z_DATA_ERROR) { //Simpler: if(uncompress_code != Z_OK) { exit; }
    	    printf("Decompression error.\n");
    	    exit(EXIT_FAILURE);
    	}
    	udata = (unsigned char*) realloc(udata, sizeof(unsigned char) * udata_len + 1);
    	if(udata == NULL) {
    	    printf("Realloc failed bro :[\n");
    	    exit(EXIT_FAILURE);
    	}
    }
    udata[udata_len] = 0x00; 
    printKV(key, udata);
    free(udata);
}

void analyzeTime(FILE* f, unsigned int size) {
    unsigned int year, month, day, hour, minute, second;
    unsigned char data[size];
    int num_bytes;
    num_bytes = fread_safe(data, 1, size, f); //Reading in data

    if(num_bytes < size) 
        exit(EXIT_FAILURE); 

    year = (data[0] << 8) + data[1];
    month = data[2];
    day = data[3];
    hour = data[4];
    minute = data[5];
    second = data[6];
    printf("Timestamp: %d/%d/%d %d:%d:%d\n", month, day, year, hour, minute, second);
}

int fread_safe(void* ptr, size_t size, size_t count, FILE* stream) {
    int retval = fread(ptr, size, count, stream);
    if(ferror(stream) != 0) { // or retVal < size ?
    	printf("Error reading from input file.");
    	exit(EXIT_FAILURE);
    }
    return retval;
}

int fseek_safe(FILE* stream, long int offset, int origin) {
    int retval = fseek(stream, offset, origin);
    if(ferror(stream) || retval != 0) {
    	printf("Error during fseek.");
    	exit(EXIT_FAILURE);
    }
    return retval;
}
