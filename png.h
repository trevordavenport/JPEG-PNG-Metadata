#ifndef PNG_H_GUARD
#define PNG_H_GUARD

int analyze_png(FILE *f);
void analyzeText(FILE *, unsigned int);
void analyzeZtxt(FILE *, unsigned int);
void analyzeTime(FILE *, unsigned int);
void printKV(unsigned char[], unsigned char[]);
unsigned int keyLength(unsigned char[], unsigned int);

#endif
