/*
 *******************************************************************************
 *
 *        filename: git-index-show.c
 *     description:
 *         created: 2023/08/24
 *          author: ticktechman
 *
 *******************************************************************************
 */

#include <arpa/inet.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define CACHE_SIGNATURE 0x44495243 /* "DIRC" */
struct cache_header {
  uint32_t hdr_signature;
  uint32_t hdr_version;
  uint32_t hdr_entries;
};

struct cache_time {
  uint32_t sec;
  uint32_t nsec;
};

#define GIT_SHA256_RAWSZ 20
#define GIT_MAX_RAWSZ GIT_SHA256_RAWSZ
#define CE_NAMEMASK (0x0fff)
#define CE_EXTENDED (0x4000)

#define align_padding_size(size, len) ((size + (len) + 8) & ~7) - (size + len)
#define align_flex_name(STRUCT, len) \
  ((offsetof(struct STRUCT, data) + (len) + 8) & ~7)
#define ondisk_cache_entry_size(len) align_flex_name(ondisk_cache_entry, len)
#define ondisk_data_size(flags, len) \
  (GIT_MAX_RAWSZ + ((flags & CE_EXTENDED) ? 2 : 1) * sizeof(uint16_t) + len)
#define ondisk_data_size_max(len) (ondisk_data_size(CE_EXTENDED, len))

#define ondisk_ce_flags_ptr(ce) ((uint16_t*)(ce->data + GIT_MAX_RAWSZ))
#define ondisk_ce_flags(ce) (*(ondisk_ce_flags_ptr(ce)))
#define ondisk_ce_name(ce) \
  ((char*)(ce->data + GIT_MAX_RAWSZ + sizeof(uint16_t)))

#define FLEX_ARRAY /* empty */
struct ondisk_cache_entry {
  struct cache_time ctime;
  struct cache_time mtime;
  uint32_t dev;
  uint32_t ino;
  uint32_t mode;
  uint32_t uid;
  uint32_t gid;
  uint32_t size;
  /*
   * unsigned char hash[hashsz];
   * uint16_t flags;
   * if (flags & CE_EXTENDED)
   *	uint16_t flags2;
   */
  unsigned char data[GIT_MAX_RAWSZ + 1 * sizeof(uint16_t)];
  char name[0];
};

static inline uint16_t get_be16(const void* ptr) {
  const unsigned char* p = ptr;
  return (uint16_t)p[0] << 8 | (uint16_t)p[1] << 0;
}

static inline uint32_t get_be32(const void* ptr) {
  const unsigned char* p = ptr;
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 |
         (uint32_t)p[3] << 0;
}

int ce_size(struct ondisk_cache_entry* e) {
  uint16_t flags = ntohs(ondisk_ce_flags(e));
  int len = flags & CE_NAMEMASK;
  char* name = ondisk_ce_name(e);
  len = offsetof(struct ondisk_cache_entry, data) + GIT_MAX_RAWSZ +
        sizeof(uint16_t) + len;
  len = (len + 8) & ~7;
  return len;
}

void print_ce(struct ondisk_cache_entry* e) {
  const unsigned char* name = e->data + 20 + sizeof(uint16_t);
  const unsigned char* d = e->data;
  uint32_t mode = ntohl(e->mode);
  uint32_t uid = ntohl(e->uid);
  uint32_t gid = ntohl(e->gid);
  uint16_t flags = ntohs(ondisk_ce_flags(e));
  uint32_t sec1 = ntohl(e->mtime.sec);
  uint32_t sec2 = ntohl(e->ctime.sec);
  uint32_t nsec1 = ntohl(e->mtime.nsec);
  uint32_t nsec2 = ntohl(e->ctime.nsec);
  char oid[41];
  sprintf(oid,
          "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%"
          "02x%02x%02x%02x",
          d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9], d[10],
          d[11], d[12], d[13], d[14], d[15], d[16], d[17], d[18], d[19]);
  printf("%o %d %d %04x %u.%u %u.%u %8d %s %8d %s\n", mode, uid, gid, flags,
         sec1, nsec1, sec2, nsec2, ntohl(e->ino), oid, ntohl(e->size), name);
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <index-file>\n", argv[0]);
    return EINVAL;
  }

  FILE* fp = fopen(argv[1], "rb");
  if (!fp) {
    fprintf(stderr, "fail to open: %s\n", argv[1]);
    return ENFILE;
  }

  struct stat st;
  fstat(fileno(fp), &st);

  char* map = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fileno(fp), 0);
  char* end = map + st.st_size;
  fclose(fp);
  if (!map) {
    fprintf(stderr, "fail to map: %s\n", argv[1]);
    return ENOMEM;
  }

  struct cache_header* hdr = (struct cache_header*)map;
  printf("#header (1-signature, 2-version, 3-entries)\n");
  printf("0x%08x %d %d\n", ntohl(hdr->hdr_signature), ntohl(hdr->hdr_version),
         ntohl(hdr->hdr_entries));
  printf("\n");

  map += sizeof(*hdr);
  size_t entries = ntohl(hdr->hdr_entries);
  if (entries > 0) {
    printf(
        "#entries (1-mode, 2-uid, 3-gid, 4-flags, 5-ctime, 6-mtime, 7-inode, "
        "8-oid, 9-size, 10-name)\n");
  }
  while (map < end && entries > 0) {
    --entries;
    struct ondisk_cache_entry* ent = (struct ondisk_cache_entry*)map;
    print_ce(ent);
    int sz = ce_size(ent);
    map += sz;
  }

  munmap(map, st.st_size);
  return 0;
}

/******************************************************************************/
