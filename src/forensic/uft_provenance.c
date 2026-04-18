/**
 * @file uft_provenance.c
 * @brief Forensic provenance chain — real impl with SHA-256 hashing.
 *
 * Replaces the 4 ABI-broken stubs in uft_core_stubs.c (local
 * uft_prov_chain_t typedef + 3-arg add() didn't match
 * include/uft/forensic/uft_provenance.h which declared 6-arg
 * add() on uft_provenance_chain_t).
 *
 * Scope:
 *   - create / free             : POD alloc/free
 *   - add                        : append entry with input/output/chain hashes
 *   - verify                     : recompute chain hashes, compare
 *   - export_json                : line-delimited JSON out
 *   - action_name                : enum→string
 *   - import_json                : deferred (parsing infra not here yet)
 *
 * SHA-256 is a self-contained static copy of the FIPS-180-4 reference
 * implementation used elsewhere in the tree (src/formats/uff/uft_uff.c).
 * Kept local to avoid exposing a new public hash API right now.
 */

#include "uft/forensic/uft_provenance.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * Local SHA-256 (FIPS-180-4). Same implementation as src/formats/uff.
 * ======================================================================== */

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t  buffer[64];
} sha256_ctx_t;

static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,
    0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,
    0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,
    0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,
    0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,
    0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define ROTR(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define CH(x,y,z) (((x)&(y))^(~(x)&(z)))
#define MAJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
#define EP0(x) (ROTR(x,2)^ROTR(x,13)^ROTR(x,22))
#define EP1(x) (ROTR(x,6)^ROTR(x,11)^ROTR(x,25))
#define S0(x)  (ROTR(x,7)^ROTR(x,18)^((x)>>3))
#define S1(x)  (ROTR(x,17)^ROTR(x,19)^((x)>>10))

static void sha256_transform(sha256_ctx_t *c, const uint8_t *d) {
    uint32_t w[64],a,b,cc,dd,e,f,g,h,t1,t2;
    for (int i=0;i<16;i++)
        w[i]=((uint32_t)d[i*4]<<24)|((uint32_t)d[i*4+1]<<16)|
             ((uint32_t)d[i*4+2]<<8)|d[i*4+3];
    for (int i=16;i<64;i++) w[i]=S1(w[i-2])+w[i-7]+S0(w[i-15])+w[i-16];
    a=c->state[0]; b=c->state[1]; cc=c->state[2]; dd=c->state[3];
    e=c->state[4]; f=c->state[5]; g=c->state[6]; h=c->state[7];
    for (int i=0;i<64;i++) {
        t1=h+EP1(e)+CH(e,f,g)+sha256_k[i]+w[i];
        t2=EP0(a)+MAJ(a,b,cc);
        h=g;g=f;f=e;e=dd+t1; dd=cc;cc=b;b=a;a=t1+t2;
    }
    c->state[0]+=a; c->state[1]+=b; c->state[2]+=cc; c->state[3]+=dd;
    c->state[4]+=e; c->state[5]+=f; c->state[6]+=g; c->state[7]+=h;
}

static void sha256_init(sha256_ctx_t *c) {
    c->state[0]=0x6a09e667; c->state[1]=0xbb67ae85;
    c->state[2]=0x3c6ef372; c->state[3]=0xa54ff53a;
    c->state[4]=0x510e527f; c->state[5]=0x9b05688c;
    c->state[6]=0x1f83d9ab; c->state[7]=0x5be0cd19;
    c->count=0;
}

static void sha256_update(sha256_ctx_t *c, const void *data, size_t n) {
    const uint8_t *p=(const uint8_t*)data;
    size_t fill=c->count & 63;
    c->count+=n;
    if (fill) {
        size_t left=64-fill;
        if (n<left) { memcpy(c->buffer+fill,p,n); return; }
        memcpy(c->buffer+fill,p,left);
        sha256_transform(c,c->buffer);
        p+=left; n-=left;
    }
    while (n>=64) { sha256_transform(c,p); p+=64; n-=64; }
    if (n) memcpy(c->buffer,p,n);
}

static void sha256_final(sha256_ctx_t *c, uint8_t *out) {
    uint8_t pad[64]={0x80};
    uint64_t bits=c->count*8;
    size_t fill=c->count & 63;
    size_t pad_len=(fill<56)?(56-fill):(120-fill);
    sha256_update(c,pad,pad_len);
    uint8_t len_be[8];
    for (int i=0;i<8;i++) len_be[i]=(bits>>(56-i*8))&0xFF;
    sha256_update(c,len_be,8);
    for (int i=0;i<8;i++) {
        out[i*4]  =(c->state[i]>>24)&0xFF;
        out[i*4+1]=(c->state[i]>>16)&0xFF;
        out[i*4+2]=(c->state[i]>>8)&0xFF;
        out[i*4+3]= c->state[i]&0xFF;
    }
}

static void hash_bytes(const void *data, size_t len, uint8_t out[32]) {
    sha256_ctx_t c; sha256_init(&c);
    sha256_update(&c,data,len);
    sha256_final(&c,out);
}

/* ========================================================================
 * Public API
 * ======================================================================== */

uft_provenance_chain_t *uft_prov_create(void) {
    uft_provenance_chain_t *ch = (uft_provenance_chain_t *)calloc(1, sizeof(*ch));
    if (!ch) return NULL;
    ch->valid = true;
    return ch;
}

void uft_prov_free(uft_provenance_chain_t *chain) {
    free(chain);
}

int uft_prov_add(uft_provenance_chain_t *chain,
                  uft_prov_action_t action,
                  const uint8_t *data, size_t data_size,
                  const char *description,
                  const char *operator_id)
{
    if (!chain) return -1;
    if (chain->count >= UFT_PROV_MAX_ENTRIES) return -1;

    uft_prov_entry_t *e = &chain->entries[chain->count];
    memset(e, 0, sizeof(*e));
    e->action = action;
    e->timestamp = time(NULL);
    e->data_size = (uint32_t)data_size;

    if (data && data_size > 0) hash_bytes(data, data_size, e->input_hash);
    memcpy(e->output_hash, e->input_hash, UFT_PROV_HASH_SIZE);

    if (description) strncpy(e->description, description, sizeof(e->description)-1);
    if (operator_id) strncpy(e->operator_id, operator_id, sizeof(e->operator_id)-1);
    strncpy(e->tool_version, "UFT 4.1", sizeof(e->tool_version)-1);

    /* Chain hash = SHA256(prev_chain_hash || entry_fields) */
    sha256_ctx_t c; sha256_init(&c);
    if (chain->count > 0)
        sha256_update(&c, chain->entries[chain->count-1].chain_hash,
                       UFT_PROV_HASH_SIZE);
    else
        sha256_update(&c, chain->root_hash, UFT_PROV_HASH_SIZE);
    sha256_update(&c, &e->action,     sizeof(e->action));
    sha256_update(&c, &e->timestamp,  sizeof(e->timestamp));
    sha256_update(&c, e->input_hash,  UFT_PROV_HASH_SIZE);
    sha256_update(&c, e->output_hash, UFT_PROV_HASH_SIZE);
    sha256_update(&c, &e->data_size,  sizeof(e->data_size));
    sha256_final(&c, e->chain_hash);

    if (chain->count == 0) memcpy(chain->root_hash, e->chain_hash, UFT_PROV_HASH_SIZE);
    chain->count++;
    return 0;
}

bool uft_prov_verify(const uft_provenance_chain_t *chain) {
    if (!chain) return false;
    if (chain->count == 0) return true;

    uint8_t prev[UFT_PROV_HASH_SIZE];
    memset(prev, 0, sizeof(prev));

    for (uint32_t i = 0; i < chain->count; i++) {
        const uft_prov_entry_t *e = &chain->entries[i];
        sha256_ctx_t c; sha256_init(&c);
        if (i == 0) sha256_update(&c, chain->root_hash, UFT_PROV_HASH_SIZE);
        else        sha256_update(&c, prev, UFT_PROV_HASH_SIZE);
        sha256_update(&c, &e->action,     sizeof(e->action));
        sha256_update(&c, &e->timestamp,  sizeof(e->timestamp));
        sha256_update(&c, e->input_hash,  UFT_PROV_HASH_SIZE);
        sha256_update(&c, e->output_hash, UFT_PROV_HASH_SIZE);
        sha256_update(&c, &e->data_size,  sizeof(e->data_size));

        uint8_t expected[UFT_PROV_HASH_SIZE];
        sha256_final(&c, expected);
        if (memcmp(expected, e->chain_hash, UFT_PROV_HASH_SIZE) != 0)
            return false;
        memcpy(prev, e->chain_hash, UFT_PROV_HASH_SIZE);
    }
    return true;
}

static void hex_bytes(const uint8_t *b, size_t n, char *out) {
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) {
        out[i*2]   = hex[(b[i] >> 4) & 0xF];
        out[i*2+1] = hex[b[i] & 0xF];
    }
    out[n*2] = '\0';
}

int uft_prov_export_json(const uft_provenance_chain_t *chain,
                          const char *output_path)
{
    if (!chain || !output_path) return -1;
    FILE *f = fopen(output_path, "w");
    if (!f) return -1;

    char hex[65];
    fprintf(f, "{\n  \"version\": 1,\n  \"count\": %u,\n",
            (unsigned)chain->count);
    hex_bytes(chain->root_hash, UFT_PROV_HASH_SIZE, hex);
    fprintf(f, "  \"root_hash\": \"%s\",\n", hex);
    fprintf(f, "  \"media_fingerprint\": \"%s\",\n", chain->media_fingerprint);
    fprintf(f, "  \"entries\": [\n");

    for (uint32_t i = 0; i < chain->count; i++) {
        const uft_prov_entry_t *e = &chain->entries[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"action\": \"%s\",\n", uft_prov_action_name(e->action));
        fprintf(f, "      \"timestamp\": %lld,\n", (long long)e->timestamp);
        hex_bytes(e->input_hash, UFT_PROV_HASH_SIZE, hex);
        fprintf(f, "      \"input_hash\": \"%s\",\n", hex);
        hex_bytes(e->output_hash, UFT_PROV_HASH_SIZE, hex);
        fprintf(f, "      \"output_hash\": \"%s\",\n", hex);
        hex_bytes(e->chain_hash, UFT_PROV_HASH_SIZE, hex);
        fprintf(f, "      \"chain_hash\": \"%s\",\n", hex);
        fprintf(f, "      \"data_size\": %u,\n", e->data_size);
        fprintf(f, "      \"tool\": \"%s\",\n", e->tool_version);
        fprintf(f, "      \"operator\": \"%s\",\n", e->operator_id);
        fprintf(f, "      \"description\": \"%s\"\n", e->description);
        fprintf(f, "    }%s\n", (i + 1 < chain->count) ? "," : "");
    }
    fprintf(f, "  ]\n}\n");

    int ok = !ferror(f);
    fclose(f);
    return ok ? 0 : -1;
}

uft_provenance_chain_t *uft_prov_import_json(const char *path) {
    (void)path;
    /* JSON parser not in tree — deferred. Export-only is sufficient for
     * the forensic audit trail today. */
    return NULL;
}

const char *uft_prov_action_name(uft_prov_action_t action) {
    switch (action) {
        case UFT_PROV_CAPTURE:  return "CAPTURE";
        case UFT_PROV_DECODE:   return "DECODE";
        case UFT_PROV_ANALYZE:  return "ANALYZE";
        case UFT_PROV_CONVERT:  return "CONVERT";
        case UFT_PROV_EXPORT:   return "EXPORT";
        case UFT_PROV_MODIFY:   return "MODIFY";
        case UFT_PROV_VERIFY:   return "VERIFY";
        case UFT_PROV_IMPORT:   return "IMPORT";
        default:                return "UNKNOWN";
    }
}
