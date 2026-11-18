#include <stddef.h>

typedef struct {
	char *s;
	size_t n, nc, cap, ccap;
	size_t *off, *log;
	unsigned char *rtl;
	int base;
} Vis;

int bidivis(Vis *v, const char *s, size_t n);
void bidifree(Vis *v);
