/* On-device preparation of the player's APK and ETC data. No game assets are
 * embedded. ZIP extraction is restricted to an explicit list, and DTRZ records
 * are validated before their byte ranges are written to the BOZI index. */
#include "game_setup.h"
#include "LzmaDec.h"
#include <minizip/unzip.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <ctype.h>

#define IMAGE_SIZE 4550559u
#define IMAGE_HASH 0x7bb8168cu
#define MAX_ENTRIES 32768u
typedef struct {
    const char *root;
    BozSetupProgress progress;
    void *user;
    char *error;
    size_t capacity;
} Setup;
typedef struct { char *name; uint32_t archive, offset, size; } Entry;
typedef struct { Entry *entry; unsigned count; } Index;

static int fail(Setup *s, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(s->error, s->capacity, format, args);
    va_end(args);
    return -1;
}
static int report(Setup *s, const char *message, int permille) {
    return !s->progress || s->progress(message, permille, s->user) ? 0 : fail(s, "Setup cancelled.");
}
static int path(Setup *s, char *out, size_t capacity, const char *relative) {
    int n = snprintf(out, capacity, "%s/%s", s->root, relative);
    return n >= 0 && (size_t)n < capacity ? 0 : fail(s, "Game folder path is too long.");
}
static uint16_t le16(const unsigned char *p) { return p[0] | (uint16_t)p[1] << 8; }
static uint32_t le32(const unsigned char *p) { return le16(p) | (uint32_t)le16(p + 2) << 16; }
static uint32_t hash(const unsigned char *p, size_t length) {
    uint32_t value = 2166136261u;
    for (size_t i = 0; i < length; ++i) value = (value ^ p[i]) * 16777619u;
    return value;
}
static int regular_size(const char *name, uint64_t size) {
    struct stat st;
    return !stat(name, &st) && S_ISREG(st.st_mode) && (uint64_t)st.st_size == size;
}
static int finish_file(FILE *file, const char *temporary, const char *destination) {
    int ok = !ferror(file);
    if (fclose(file)) ok = 0;
    if (ok) {
        remove(destination);
        ok = !rename(temporary, destination);
    }
    if (!ok) remove(temporary);
    return ok ? 0 : -1;
}
static int write_image(Setup *s, unzFile zip) {
    char destination[1024], temporary[1030];
    if (path(s, destination, sizeof destination, "boz.s3e.unpacked")) return -1;
    unsigned char *image = malloc(IMAGE_SIZE);
    if (!image) return fail(s, "Not enough memory to prepare game code.");
    /* Prepared installs are cheap to check and interrupted setup can resume. */
    FILE *existing = fopen(destination, "rb");
    int valid = existing && regular_size(destination, IMAGE_SIZE) &&
                fread(image, 1, IMAGE_SIZE, existing) == IMAGE_SIZE && hash(image, IMAGE_SIZE) == IMAGE_HASH;
    if (existing) fclose(existing);
    if (unzLocateFile(zip, "assets/boz.s3e", 1) != UNZ_OK) {
        free(image); return fail(s, "This APK does not contain BOZ's game code.");
    }
    unz_file_info64 info;
    if (unzGetCurrentFileInfo64(zip, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK ||
        info.uncompressed_size < 13 || info.uncompressed_size > (8u << 20)) {
        free(image); return fail(s, "Invalid compressed game-code size.");
    }
    /* Validate the supplied APK even if a previous version's image exists. */
    unsigned char *packed = malloc((size_t)info.uncompressed_size);
    if (!packed || unzOpenCurrentFile(zip) != UNZ_OK) {
        free(packed); free(image); return fail(s, "Cannot read BOZ's APK.");
    }
    size_t got = 0;
    while (got < info.uncompressed_size) {
        int n = unzReadCurrentFile(zip, packed + got, (unsigned)(info.uncompressed_size - got));
        if (n <= 0) break;
        got += (unsigned)n;
    }
    int crc_ok = unzCloseCurrentFile(zip) == UNZ_OK;
    if (got != info.uncompressed_size || !crc_ok || le32(packed + 1) > (64u << 20)) {
        free(packed); free(image); return fail(s, "Damaged or unsupported compressed game code.");
    }
    if (report(s, "Preparing game code", 50)) { free(packed); free(image); return -1; }
    SizeT output_size = IMAGE_SIZE, input_size = got - 13;
    ELzmaStatus status;
    extern const ISzAlloc boz_lzma_allocator;
    SRes result = LzmaDecode(image, &output_size, packed + 13, &input_size, packed, 5,
                             LZMA_FINISH_END, &status, &boz_lzma_allocator);
    free(packed);
    if (result != SZ_OK || output_size != IMAGE_SIZE || hash(image, IMAGE_SIZE) != IMAGE_HASH) {
        free(image); return fail(s, "Unsupported APK: use BOZ Android v1.0.11 (verified game image).");
    }
    if (valid) { free(image); return 0; }
    snprintf(temporary, sizeof temporary, "%s.part", destination);
    FILE *file = fopen(temporary, "wb");
    if (!file) { free(image); return fail(s, "Cannot write game code. Check free SD space."); }
    int ok = fwrite(image, 1, IMAGE_SIZE, file) == IMAGE_SIZE;
    free(image);
    if (!ok) { fclose(file); remove(temporary); return fail(s, "Could not write game code."); }
    if (finish_file(file, temporary, destination)) return fail(s, "Could not finish writing game code.");
    return 0;
}
static void *lzma_alloc(ISzAllocPtr self, size_t size) { (void)self; return malloc(size); }
static void lzma_free(ISzAllocPtr self, void *memory) { (void)self; free(memory); }
const ISzAlloc boz_lzma_allocator = {lzma_alloc, lzma_free};

static int music_name(const char *name) {
    const char *file = NULL;
    static const char blackops[] = "assets/blackops-music/";
    static const char deadops[] = "assets/deadops-music/";
    if (!strncmp(name, blackops, sizeof blackops - 1)) file = name + sizeof blackops - 1;
    if (!strncmp(name, deadops, sizeof deadops - 1)) file = name + sizeof deadops - 1;
    size_t length = file ? strlen(file) : 0;
    return length > 4 && !strchr(file, '/') && !strchr(file, '\\') && !strstr(file, "..") &&
           !strcmp(file + length - 4, ".mp3");
}
static int extract(Setup *s, unzFile zip, const char *relative, uint64_t size, int progress, int force) {
    char destination[1024], temporary[1030];
    if (path(s, destination, sizeof destination, relative)) return -1;
    if (!force && regular_size(destination, size)) return 0;
    if (unzOpenCurrentFile(zip) != UNZ_OK) return fail(s, "Cannot extract %s.", relative);
    snprintf(temporary, sizeof temporary, "%s.part", destination);
    FILE *file = fopen(temporary, "wb");
    if (!file) { unzCloseCurrentFile(zip); return fail(s, "Cannot write %s. Check SD space.", relative); }
    unsigned char buffer[64 * 1024];
    uint64_t got = 0;
    int ok = 1;
    for (;;) {
        int n = unzReadCurrentFile(zip, buffer, sizeof buffer);
        if (n < 0) { ok = 0; break; }
        if (!n) break;
        got += (unsigned)n;
        if (got > size || fwrite(buffer, 1, (size_t)n, file) != (size_t)n) { ok = 0; break; }
        if (report(s, relative, progress)) { ok = 0; break; }
    }
    if (unzCloseCurrentFile(zip) != UNZ_OK) ok = 0;
    if (!ok || got != size) {
        fclose(file); remove(temporary); return fail(s, "Extraction failed for %s. Check APK and SD space.", relative);
    }
    return finish_file(file, temporary, destination) ? fail(s, "Cannot finish %s.", relative) : 0;
}
static int add_entry(Index *index, const char *name, unsigned archive, uint32_t offset, uint32_t size) {
    char normalized[512];
    size_t length = strlen(name);
    if (!length || length >= sizeof normalized) return -1;
    for (size_t i = 0; i <= length; ++i)
        normalized[i] = name[i] == '\\' ? '/' : (char)tolower((unsigned char)name[i]);
    for (unsigned i = 0; i < index->count; ++i)
        if (!strcmp(index->entry[i].name, normalized)) return 0; /* first archive wins */
    if (index->count == MAX_ENTRIES) return -1;
    char *copy = malloc(length + 1);
    if (!copy) return -1;
    memcpy(copy, normalized, length + 1);
    index->entry[index->count++] = (Entry){copy, archive, offset, size};
    return 0;
}
static int parse_archive(Setup *s, Index *index, const char *name, unsigned archive) {
    char filename[1024], **names = NULL;
    unsigned char header[16];
    FILE *file = NULL;
    unsigned count = 0;
    int ok = 0;
    if (path(s, filename, sizeof filename, name)) return -1;
    struct stat st;
    if (stat(filename, &st) || st.st_size <= 8 || (uint64_t)st.st_size > UINT32_MAX) goto done;
    file = fopen(filename, "rb");
    if (!file || fread(header, 1, 8, file) != 8 || memcmp(header, "DTRZ", 4)) goto done;
    count = le16(header + 4);
    unsigned groups = le16(header + 6);
    if (!count || count > 8192 || !groups || count + groups > MAX_ENTRIES) goto done;
    names = calloc(count, sizeof *names);
    if (!names) goto done;
    for (unsigned j = 0; j < count + groups; ++j) {
        char text[512]; size_t n = 0; int ch;
        do {
            ch = fgetc(file);
            if (ch == EOF || n == sizeof text - 1 || ftell(file) > (16 << 20)) goto done;
            text[n++] = (char)ch;
        } while (ch);
        if (!j && text[0]) goto done;
        if (j >= 1 && j <= count) {
            names[j - 1] = malloc(n);
            if (!names[j - 1]) goto done;
            memcpy(names[j - 1], text, n);
        }
    }
    if (fseek(file, (long)count * 6, SEEK_CUR) || fread(header, 1, 4, file) != 4 ||
        le16(header) != 1 || le16(header + 2) != count) goto done;
    uint64_t expected = (uint64_t)ftell(file) + count * 16u;
    for (unsigned i = 0; i < count; ++i) {
        if (fread(header, 1, 16, file) != 16) goto done;
        uint32_t offset = le32(header), size = le32(header + 4), unpacked = le32(header + 8);
        if (offset != expected || size != unpacked || (uint64_t)offset + size > (uint64_t)st.st_size) goto done;
        expected += size;
        if (add_entry(index, names[i], archive, offset, size)) goto done;
        const char *base = names[i];
        for (const char *p = names[i]; *p; ++p) if (*p == '/' || *p == '\\') base = p + 1;
        if (add_entry(index, base, archive, offset, size)) goto done;
    }
    ok = 1;
done:
    if (file) fclose(file);
    if (names) { for (unsigned i = 0; i < count; ++i) free(names[i]); free(names); }
    return ok ? 0 : fail(s, "Invalid or compressed DTRZ archive: %s. Use the game's ETC data.", name);
}
static int compare_entry(const void *a, const void *b) {
    return strcmp(((const Entry *)a)->name, ((const Entry *)b)->name);
}
static void put16(FILE *file, unsigned value) { unsigned char p[2] = {value, value >> 8}; fwrite(p, 1, 2, file); }
static void put32(FILE *file, uint32_t value) { put16(file, value); put16(file, value >> 16); }
static int build_index(Setup *s) {
    Index index = {calloc(MAX_ENTRIES, sizeof(Entry)), 0};
    static const char *archives[] = {"blackops_etc.dz", "blackops_loader.dz"};
    if (!index.entry) return fail(s, "Not enough memory to build archive index.");
    int result = -1;
    for (unsigned i = 0; i < 2; ++i) if (parse_archive(s, &index, archives[i], i)) goto done;
    qsort(index.entry, index.count, sizeof(Entry), compare_entry);
    char destination[1024], temporary[1030];
    if (path(s, destination, sizeof destination, "boz_files.idx")) goto done;
    snprintf(temporary, sizeof temporary, "%s.part", destination);
    FILE *file = fopen(temporary, "wb");
    if (!file) { fail(s, "Cannot write archive index."); goto done; }
    fwrite("BOZI", 1, 4, file); put32(file, 1); put32(file, 2); put32(file, index.count);
    for (unsigned i = 0; i < 2; ++i) {
        size_t length = strlen(archives[i]); put16(file, (unsigned)length); fwrite(archives[i], 1, length, file);
    }
    for (unsigned i = 0; i < index.count; ++i) {
        Entry *e = &index.entry[i]; size_t length = strlen(e->name);
        put32(file, e->archive); put32(file, e->offset); put32(file, e->size);
        put16(file, (unsigned)length); fwrite(e->name, 1, length, file);
    }
    result = finish_file(file, temporary, destination);
    if (result) fail(s, "Could not finish archive index.");
done:
    for (unsigned i = 0; i < index.count; ++i) free(index.entry[i].name);
    free(index.entry);
    return result;
}
int boz_setup(const char *root, const char *apk, BozSetupProgress progress,
              void *user, char *error, size_t capacity) {
    Setup s = {root, progress, user, error, capacity};
    if (capacity) error[0] = 0;
    unzFile zip = unzOpen64(apk);
    if (!zip) return fail(&s, "Cannot open the APK. Copy your BOZ Android APK beside codboz.nro.");
    int result = write_image(&s, zip);
    if (result) { unzClose(zip); return result; }
    char folder[1024], state_path[1024], signature[160], previous[160] = {0};
    struct stat apk_stat;
    if (stat(apk, &apk_stat)) { unzClose(zip); return fail(&s, "Cannot inspect the APK."); }
    snprintf(signature, sizeof signature, "BOZ-SETUP-1 %llu %lld\n",
             (unsigned long long)apk_stat.st_size, (long long)apk_stat.st_mtime);
    if (path(&s, state_path, sizeof state_path, "setup.state")) { unzClose(zip); return -1; }
    FILE *stamp = fopen(state_path, "rb");
    if (stamp) { fgets(previous, sizeof previous, stamp); fclose(stamp); }
    int force = strcmp(signature, previous) != 0;
    if (path(&s, folder, sizeof folder, "blackops-music")) { unzClose(zip); return -1; }
    mkdir(folder, 0777);
    if (path(&s, folder, sizeof folder, "deadops-music")) { unzClose(zip); return -1; }
    mkdir(folder, 0777);
    unz_global_info64 global;
    if (unzGetGlobalInfo64(zip, &global) != UNZ_OK) { unzClose(zip); return fail(&s, "Damaged APK directory."); }
    int code = unzGoToFirstFile(zip), loader = 0;
    unsigned seen = 0;
    while (code == UNZ_OK) {
        char name[512]; unz_file_info64 info;
        if (unzGetCurrentFileInfo64(zip, &info, name, sizeof name, NULL, 0, NULL, 0) != UNZ_OK ||
            info.size_filename >= sizeof name) { result = fail(&s, "Invalid APK entry name."); break; }
        if (!strcmp(name, "assets/blackops_loader.dz")) loader = 1;
        if (!strcmp(name, "assets/blackops_loader.dz") || !strcmp(name, "assets/blackops_etc.dz") || music_name(name)) {
            if (info.uncompressed_size > (2ull << 30)) {
                result = fail(&s, "APK asset is too large: %s.", name); break;
            }
            if (extract(&s, zip, name + 7, info.uncompressed_size,
                        100 + (int)(seen * 600ull / (global.number_entry ? global.number_entry : 1)), force)) {
                result = -1; break;
            }
        }
        ++seen; code = unzGoToNextFile(zip);
    }
    if (!result && code != UNZ_END_OF_LIST_OF_FILE) result = fail(&s, "Could not finish reading APK directory.");
    unzClose(zip);
    if (result) return result;
    if (!loader) return fail(&s, "APK is missing blackops_loader.dz.");
    if (path(&s, folder, sizeof folder, "blackops_etc.dz")) return -1;
    struct stat data;
    if (stat(folder, &data) || data.st_size <= 0)
        return fail(&s, "Graphics data is separate from this APK. Copy your blackops_etc.dz beside codboz.nro, then launch again.");
    if (report(&s, "Indexing game data", 800) || build_index(&s)) return -1;
    char temporary[1030]; snprintf(temporary, sizeof temporary, "%s.part", state_path);
    stamp = fopen(temporary, "wb");
    if (!stamp) return fail(&s, "Cannot record completed setup.");
    fputs(signature, stamp);
    if (finish_file(stamp, temporary, state_path)) return fail(&s, "Could not finish setup state.");
    return report(&s, "Game files ready", 1000);
}
