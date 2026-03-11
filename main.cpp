#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Build nkf as a single compilation unit (same as many upstream build recipes). */
#define PERL_XS 1
#include "utf8tbl.cpp"
#include "libnkf.hpp"

static void die_usage(const char* exe)
{
    fprintf(stderr, "Usage: %s <nkf-options>\n", exe);
    fprintf(stderr, "Example (Shift_JIS -> UTF-8): %s -w input.txt\n", exe);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        die_usage(argc > 0 ? argv[0] : "nkf");
        return 2;
    }
    nkf_cnv* nkfcnv = new nkf_cnv();
    nkfcnv->reinit();
	// -s
	// -w
	// -e
    nkfcnv->options((unsigned char*)argv[1]);
    FILE* rs = fopen(argv[2], "rb");
    if (!rs) {
        fprintf(stderr, "Error: Cannot open file %s\n", argv[2]);
        return 1;
    }
    nkfcnv->kanji_convert(rs);
    delete nkfcnv;
	nkfcnv = nullptr;
    fclose(rs);
    return 0;
}
