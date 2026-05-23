/* Stub: no OS entropy source. DOS/9 injects entropy via rdtsc in gemini.c. */
#include <bearssl.h>

br_prng_seeder br_prng_seeder_system(const char **name) {
    if (name) *name = "none";
    return NULL;
}
