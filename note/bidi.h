#include <stddef.h>

typedef struct {
	char *s;
	size_t n, nc, cap, ccap;
	size_t *off, *log;
	unsigned char *rtl;
	int base;
} Vis;

int bidiparadir(const char *s, size_t n);
int bidivis(Vis *v, const char *s, size_t n, int dir);
void bidifree(Vis *v);
