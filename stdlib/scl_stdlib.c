#include "scl_stdlib.h"
#include <stdlib.h>

int scl_atoi(const char *str) {
    if (scl_unlikely(!str)) return 0;
    return atoi(str);
}

long scl_atol(const char *str) {
    if (scl_unlikely(!str)) return 0;
    return atol(str);
}

long long scl_atoll(const char *str) {
    if (scl_unlikely(!str)) return 0;
    return atoll(str);
}

long scl_strtol(const char *str, char **endptr, int base) {
    if (scl_unlikely(!str)) return 0;
    return strtol(str, endptr, base);
}

long long scl_strtoll(const char *str, char **endptr, int base) {
    if (scl_unlikely(!str)) return 0;
    return strtoll(str, endptr, base);
}

unsigned long scl_strtoul(const char *str, char **endptr, int base) {
    if (scl_unlikely(!str)) return 0;
    return strtoul(str, endptr, base);
}

unsigned long long scl_strtoull(const char *str, char **endptr, int base) {
    if (scl_unlikely(!str)) return 0;
    return strtoull(str, endptr, base);
}

double scl_atof(const char *str) {
    if (scl_unlikely(!str)) return 0.0;
    return atof(str);
}

double scl_strtod(const char *str, char **endptr) {
    if (scl_unlikely(!str)) return 0.0;
    return strtod(str, endptr);
}

int scl_abs(int x) {
    return abs(x);
}

long scl_labs(long x) {
    return labs(x);
}

long long scl_llabs(long long x) {
    return llabs(x);
}

int scl_rand(void) {
    return rand();
}

void scl_srand(unsigned int seed) {
    srand(seed);
}

char *scl_getenv(const char *name) {
    if (scl_unlikely(!name)) return NULL;
    return getenv(name);
}

int scl_system(const char *command) {
    if (scl_unlikely(!command)) return -1;
    return system(command);
}
