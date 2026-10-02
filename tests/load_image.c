#include "s3e_loader.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    if (argc != 3 && argc != 4) return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file) return 1;
    fseek(file, 0, SEEK_END); long length = ftell(file); rewind(file);
    if (length <= 0) { fclose(file); return 1; }
    unsigned char *data = malloc((size_t)length);
    if (!data || fread(data, 1, (size_t)length, file) != (size_t)length) return 1;
    fclose(file);
    S3eImage image;
    if (s3e_load(data, (size_t)length, 0x00800000, &image)) { fprintf(stderr, "%s\n", s3e_error()); return 1; }
    free(data);
    file = fopen(argv[2], "wb");
    if (!file) return 1;
    int ok = fwrite(image.image, 1, image.image_alloc, file) == image.image_alloc;
    if (fclose(file)) ok = 0;
    if (ok && argc == 4) {
        file = fopen(argv[3], "w");
        if (!file) ok = 0;
        else {
            for (uint32_t i = 0; i < image.got_count; ++i)
                if (image.got_import[i] >= 0 && fprintf(file, "%08x %s\n", image.got_rva[i],
                    image.import_names[image.got_import[i]]) < 0) ok = 0;
            if (fclose(file)) ok = 0;
        }
    }
    s3e_unload(&image);
    return ok ? 0 : 1;
}
