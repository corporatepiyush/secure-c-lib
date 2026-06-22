#ifndef SCL_STDLIB_H
#define SCL_STDLIB_H

#include <stddef.h>
#include <stdint.h>
#include "../common/scl_common.h"

/* Integer conversion */
int scl_atoi(const char *str);
long scl_atol(const char *str);
long long scl_atoll(const char *str);

long scl_strtol(const char *str, char **endptr, int base);
long long scl_strtoll(const char *str, char **endptr, int base);
unsigned long scl_strtoul(const char *str, char **endptr, int base);
unsigned long long scl_strtoull(const char *str, char **endptr, int base);

double scl_atof(const char *str);
double scl_strtod(const char *str, char **endptr);

/* Absolute value */
int scl_abs(int x);
long scl_labs(long x);
long long scl_llabs(long long x);

/* Random numbers */
int scl_rand(void);
void scl_srand(unsigned int seed);

/* Environment */
char *scl_getenv(const char *name);

/* Execution */
int scl_system(const char *command);

#endif // SCL_STDLIB_H
