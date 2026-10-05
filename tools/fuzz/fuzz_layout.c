/* The spec parser on arbitrary bytes (design 16.6): it never reads outside
   the spec, never writes outside the field array, reports an error position
   inside the spec, and every layout it accepts is self-consistent. Each
   input is parsed four ways (both default byte orders, packed and aligned),
   and once more into a field array one too small. */
#include "fuzz.h"

#define MAX_INPUT 4096
static zb_field fields[MAX_INPUT + 1];

static void check_layout(const char *spec, size_t n, int align, const zb_layout *l)
{
    uint32_t i, end = 0;
    FUZZ_CHECK(l->fields == fields);
    FUZZ_CHECK(l->nfields >= 1 && l->nfields <= zb_layout_count_fields(spec, n));
    FUZZ_CHECK(l->size <= ZB_LAYOUT_MAX);
    FUZZ_CHECK(l->align == 1 || (align && (l->align == 2 || l->align == 4 || l->align == 8)));
    for (i = 0; i < l->nfields; i++) {
        const zb_field *f = &l->fields[i];
        uint32_t w = zb_type_width(f->type);
        FUZZ_CHECK(f->type >= ZB_U8 && f->type <= ZB_PAD && w >= 1);
        FUZZ_CHECK(f->count >= 1);
        if (f->type == ZB_BYTES || f->type == ZB_STR || f->type == ZB_PAD) {
            FUZZ_CHECK(f->size == f->count);
        } else {
            FUZZ_CHECK((uint64_t)f->size == (uint64_t)f->count * w);
        }
        FUZZ_CHECK(f->offset >= end);
        if (!align) FUZZ_CHECK(f->offset == end);
        else FUZZ_CHECK(f->offset % (f->type == ZB_BOOL ? 1 : w) == 0 && f->offset - end < w + 0u);
        FUZZ_CHECK((uint64_t)f->offset + f->size <= l->size);
        end = f->offset + f->size;
        if (w == 1) FUZZ_CHECK(f->big_endian == 0);
        FUZZ_CHECK(f->big_endian <= 1);
        if (f->name) {
            FUZZ_CHECK(f->name >= spec && f->name + f->name_len <= spec + n && f->name_len >= 1);
            FUZZ_CHECK(f->type != ZB_PAD);
        }
    }
    FUZZ_CHECK(l->size >= end && l->size % l->align == 0);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const char *spec;
    char *copy;
    int big, align;
    FUZZ_CANARY(data, size);
    if (size > MAX_INPUT) return 0;
    /* an exact-size heap copy, so ASan sees any read past the end */
    copy = (char *)malloc(size ? size : 1);
    if (!copy) return 0;
    if (size) memcpy(copy, data, size);
    spec = copy;
    for (big = 0; big < 2; big++) {
        for (align = 0; align < 2; align++) {
            zb_layout l;
            size_t pos = (size_t)-1;
            uint32_t bound = zb_layout_count_fields(spec, size);
            zb_status st = zb_layout_parse(spec, size, big, align, fields, bound, &l, &pos);
            if (st == ZB_OK) {
                uint32_t nf = l.nfields;
                check_layout(spec, size, align, &l);
                if (nf > 1) {
                    zb_layout l2 = l;
                    st = zb_layout_parse(spec, size, big, align, fields, nf - 1, &l2, &pos);
                    FUZZ_CHECK(st == ZB_ERR_LIMIT && pos < size);
                    FUZZ_CHECK(l2.nfields == l.nfields && l2.size == l.size);
                }
            } else {
                FUZZ_CHECK(st == ZB_ERR_SPEC && pos <= size);
            }
        }
    }
    free(copy);
    return 0;
}
