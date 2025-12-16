#include <stdint.h>
#include <iostream>
#include <assert.h>
#include "include/debug.h"

extern uint8_t pmem[];

// load a binary file into your cpu
uint64_t load_img(char *img_file){
    if (img_file == NULL) {
        Log("No img is given. Use the default build-in img.");
        return 4096;
    }
    FILE *fp = fopen(img_file, "rb");
    Assert(fp, "Can not open file %s", img_file);

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);

    Log("The image is %s, size = %ld", img_file, size);

    fseek(fp, 0, SEEK_SET);
    int ret = fread(pmem, size, 1, fp);
    assert(ret == 1);

    fclose(fp);
    return size;
}