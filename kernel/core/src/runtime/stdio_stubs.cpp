#include <ntddk.h>

extern "C" {

struct _iobuf;
typedef _iobuf FILE;

FILE* fopen(const char*, const char*) {
    return nullptr;
}

int fclose(FILE*) {
    return 0;
}

int fprintf(FILE*, const char*, ...) {
    return 0;
}

}  // extern "C"
