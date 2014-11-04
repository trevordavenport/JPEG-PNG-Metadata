#ifndef EXIF_H_GUARD
#define EXIF_H_GUARD

int analyze_jpg(FILE *f);
void printTag(FILE*, fpos_t*, unsigned int, unsigned int, unsigned int, unsigned int);
void parseChunk(FILE*);
void parseSuperChunk(FILE*);
void analyzeTIFF(FILE*);
int parseIFD(FILE*, fpos_t*, int);

#endif
