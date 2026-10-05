/* Translation unit 5: rdz's generic codec in miniature (issue #26). An object
   is serialized through a sink into 4 KiB blocks, each written behind a
   40-byte block header into one buffer; then the headers are walked with a
   cursor, each block's hash checked, the payload reassembled, and the object
   unserialized from it. Every piece is zubin's or zufast's. */
#include <zubin.h>
#include <zubin-r.h>
#include <zufast/hash.h>
#include "zubintest.h"

#define BLOCK 4096
#define MAGIC 0x6B4C4252u   /* "RBLk" */

/* The block header, described once. */
static const char header_spec[] =
    "<magic:u32 index:u32 payload:u32 flags:u32 offset:u64 hash:u64 total:u64";

typedef struct {
    zb_buf *out;
    const zb_field *f;      /* the header's seven fields */
    uint8_t block[BLOCK];
    size_t used;
    uint32_t index;
    uint64_t offset;        /* payload bytes before this block */
    zb_status st;
} writer;

static void flush(writer *w)
{
    uint8_t *h;
    if (!w->used || w->st) return;
    h = zb_put_raw(w->out, 40);
    if (!h) { w->st = ZB_ERR_LIMIT; return; }
    zb_wr_u32le(h + w->f[0].offset, MAGIC);
    zb_wr_u32le(h + w->f[1].offset, w->index++);
    zb_wr_u32le(h + w->f[2].offset, (uint32_t)w->used);
    zb_wr_u32le(h + w->f[3].offset, 0);
    zb_wr_u64le(h + w->f[4].offset, w->offset);
    zb_wr_u64le(h + w->f[5].offset, zuf_hash64(w->block, w->used));
    zb_wr_u64le(h + w->f[6].offset, 0);   /* the stream's total, unknown yet */
    w->st = zb_put_bytes(w->out, w->block, w->used);
    w->offset += w->used;
    w->used = 0;
}

static void sink(void *state, const void *p, size_t n)
{
    writer *w = (writer *)state;
    const uint8_t *q = (const uint8_t *)p;
    while (n) {
        size_t take = BLOCK - w->used < n ? BLOCK - w->used : n;
        memcpy(w->block + w->used, q, take);
        w->used += take;
        q += take;
        n -= take;
        if (w->used == BLOCK) flush(w);
    }
}

/* Writes x as blocks, reads it back, and returns list(object, blocks,
   container bytes); a corrupted byte at `corrupt` (0-based, -1 for none)
   must be caught by a block's hash, which returns "hash mismatch". */
SEXP zt_rdz(SEXP x, SEXP corrupt)
{
    zb_field fields[8];
    zb_layout l;
    size_t pos = 0, total = 0, blocks = 0;
    zb_status st;
    writer *w;
    zb_cur c;
    SEXP out, back, cptr, pptr;
    zb_buf *container, *payload;
    int bad = Rf_asInteger(corrupt);

    if (zb_layout_parse(header_spec, sizeof header_spec - 1, 0, 0, fields, 8, &l, &pos) ||
        l.size != 40 || l.nfields != 7) Rf_error("zt_rdz: the header layout");

    cptr = PROTECT(zb_r_buf_new(0, 0, &st));
    if (st) Rf_error("zt_rdz: allocation");
    container = zb_r_buf_get(cptr);
    w = (writer *)R_alloc(1, sizeof *w);
    memset(w, 0, sizeof *w);
    w->out = container;
    w->f = l.fields;
    zb_serialize_to_sink(x, sink, w, 3, 1, 0);
    flush(w);
    if (w->st) Rf_error("zt_rdz: write failed");
    if (bad >= 0 && (size_t)bad < container->len) container->data[bad] ^= 0x5a;

    /* read: walk the headers, check each block, gather the payload */
    pptr = PROTECT(zb_r_buf_new(0, 0, &st));
    if (st) Rf_error("zt_rdz: allocation");
    payload = zb_r_buf_get(pptr);
    zb_cur_init(&c, container->data, container->len);
    while (zb_cur_remaining(&c)) {
        uint32_t magic, index, n, flags;
        uint64_t offset, hash, unused;
        const uint8_t *p;
        if (zb_cur_u32le(&c, &magic) || zb_cur_u32le(&c, &index) || zb_cur_u32le(&c, &n) ||
            zb_cur_u32le(&c, &flags) || zb_cur_u64le(&c, &offset) || zb_cur_u64le(&c, &hash) ||
            zb_cur_u64le(&c, &unused) || zb_cur_bytes(&c, &p, n)) {
            UNPROTECT(2);
            return Rf_mkString("truncated");
        }
        if (magic != MAGIC || index != blocks || offset != total || zuf_hash64(p, n) != hash) {
            UNPROTECT(2);
            return Rf_mkString("hash mismatch");
        }
        if (zb_put_bytes(payload, p, n)) Rf_error("zt_rdz: gather failed");
        total += n;
        blocks++;
    }
    zb_cur_init(&c, payload->data, payload->len);
    back = PROTECT(zb_unserialize(&c, R_NilValue, &st));
    if (st || zb_cur_remaining(&c)) {
        UNPROTECT(3);
        return Rf_mkString(st ? zb_status_string(st) : "trailing bytes");
    }
    out = PROTECT(Rf_allocVector(VECSXP, 3));
    SET_VECTOR_ELT(out, 0, back);
    SET_VECTOR_ELT(out, 1, Rf_ScalarReal((double)blocks));
    SET_VECTOR_ELT(out, 2, Rf_ScalarReal((double)container->len));
    zb_r_buf_free(cptr);
    zb_r_buf_free(pptr);
    UNPROTECT(4);
    return out;
}
