/*
 * I assert the work on this project was all my own.
 * <put your name here>
   Trevor Davenport
   Ryan Erickson
 */

#include <stdio.h>
#include <stdlib.h>
#include "png.h"
#include "jpg.h"

int try_analyze_png_file(char *filename) {
    FILE *f = fopen(filename, "r");
    if (f == NULL)
        return -1;
    int rv = analyze_png(f);
    fclose(f);
    return rv;
}

int try_analyze_jpg_file(char *filename) {
    FILE *f = fopen(filename, "r");
    if (f == NULL)
        return -1;
    int rv = analyze_jpg(f);
    fclose(f);
    return rv;
}

int analyze(char *filename) {
    int rv;
    printf("File: %s\n", filename);
    rv = try_analyze_png_file(filename);
    if (rv < 0) {
        rv = try_analyze_jpg_file(filename);
    }
    return rv;
}

/*
 * Forensics analysis of PNG and JPG files.
 *
 * Usage: <program> bar.png baf.png fred.jpg sally.jpg
 */
int main(int argc, char** argv) {
    int i;
    for (i=1; i<argc; i++) {
        if (analyze(argv[i]) < 0) {
            printf("Error reading file %s\n", argv[i]);
        }
    }
    return 0;
}
