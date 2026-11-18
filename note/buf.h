#include <stddef.h>

enum { ModeInsert, ModeNormal };
enum {
	KeyEsc = 0x100, KeyReturn, KeyTab, KeyBackspace, KeyDelete,
	KeyLeft, KeyRight, KeyUp, KeyDown, KeyHome, KeyEnd,
	KeyCtrlW, KeyCtrlU, KeyCtrlR
};

typedef struct Buf Buf;

Buf *bufnew(void);
void buffree(Buf *b);
void bufload(Buf *b, const char *s, size_t n);
const char *buftext(Buf *b, size_t *n);
size_t bufcursor(Buf *b);
void bufsetcursor(Buf *b, size_t off);
int bufmode(Buf *b);
void bufsetmode(Buf *b, int mode);
void bufinsert(Buf *b, const char *s, size_t n);
void bufkey(Buf *b, int key);
int bufdirty(Buf *b);
void bufclean(Buf *b);
