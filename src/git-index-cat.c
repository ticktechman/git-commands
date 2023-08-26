/*
 *******************************************************************************
 *
 *        filename: git-index-show.c
 *     description: show .git/index content(header , entries)
 *         created: 2023/08/24
 *          author: ticktechman
 *
 *******************************************************************************
 */

#include <arpa/inet.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define loge(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#define logi(fmt, ...) fprintf(stdout, fmt "\n", ##__VA_ARGS__)
#define log printf

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

#define GIT_SHA1_RAWSZ 20
#define GIT_MAX_RAWSZ GIT_SHA1_RAWSZ
#define CE_NAMEMASK (0x0fff)
#define CE_EXTENDED (0x4000)

#define ondisk_ce_flag_size(flags) \
  ((flags & CE_EXTENDED ? 2 : 1) * sizeof(uint16_t))
#define ondisk_ce_flags_ptr(ce) ((uint16_t*)(ce->data + GIT_MAX_RAWSZ))
#define ondisk_ce_flags(ce) (*(ondisk_ce_flags_ptr(ce)))
#define ondisk_ce_name(ce, flags) \
  ((char*)(ce->data + GIT_MAX_RAWSZ + ondisk_ce_flag_size(flags)))

// network byte order(big endian)
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
  unsigned char data[GIT_MAX_RAWSZ + 2 * sizeof(uint16_t)];
  char name[0];
};

int ce_size(struct ondisk_cache_entry* e) {
  uint16_t flags = ntohs(ondisk_ce_flags(e));
  int len = flags & CE_NAMEMASK;
  char* name = ondisk_ce_name(e, flags);
  len = name - (char*)e + len;
  len = (len + 8) & ~7;
  return len;
}

void print_ce(struct ondisk_cache_entry* e) {
  uint16_t flags = ntohs(ondisk_ce_flags(e));
  const char* name = ondisk_ce_name(e, flags);
  const unsigned char* d = e->data;
  uint32_t mode = ntohl(e->mode);
  uint32_t uid = ntohl(e->uid);
  uint32_t gid = ntohl(e->gid);
  uint32_t sec1 = ntohl(e->ctime.sec);
  uint32_t sec2 = ntohl(e->mtime.sec);
  uint32_t nsec1 = ntohl(e->ctime.nsec);
  uint32_t nsec2 = ntohl(e->mtime.nsec);
  char oid[GIT_MAX_RAWSZ * 2 + 1];
  char* ptr = oid;
  for (int i = 0; i < GIT_MAX_RAWSZ; i++) {
    sprintf(ptr, "%02x", d[i]);
    ptr += 2;
  }
  log("%o %d %d %04x %u.%09u %u.%09u %8d %s %8d %s\n", mode, uid, gid, flags,
      sec1, nsec1, sec2, nsec2, ntohl(e->ino), oid, ntohl(e->size), name);
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    loge("usage: %s <index-file>", argv[0]);
    return EINVAL;
  }

  // open file and map it
  FILE* fp = fopen(argv[1], "rb");
  if (!fp) {
    loge("fail to open: %s", argv[1]);
    return ENFILE;
  }

  struct stat st;
  fstat(fileno(fp), &st);

  char* map = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fileno(fp), 0);
  char* end = map + st.st_size;
  fclose(fp);
  if (!map) {
    loge("fail to map: %s", argv[1]);
    return ENOMEM;
  }

  // show header
  struct cache_header* hdr = (struct cache_header*)map;
  uint32_t signature = ntohl(hdr->hdr_signature);
  if (signature != CACHE_SIGNATURE) {
    loge("invalid signature(%08x) in file", signature);
    goto out_gc;
  }

  logi("#header (1-signature, 2-version, 3-entries)");
  logi("0x%08x %d %d", signature, ntohl(hdr->hdr_version),
       ntohl(hdr->hdr_entries));
  logi("");

  // show entries
  map += sizeof(*hdr);
  size_t entries = ntohl(hdr->hdr_entries);
  if (entries > 0) {
    logi(
        "#entries (1-mode, 2-uid, 3-gid, 4-flags, 5-ctime, 6-mtime, 7-inode, "
        "8-oid, 9-size, 10-name)");
  }
  while (map < end && entries > 0) {
    --entries;
    struct ondisk_cache_entry* ent = (struct ondisk_cache_entry*)map;
    print_ce(ent);
    int sz = ce_size(ent);
    map += sz;
  }

out_gc:
  munmap(map, st.st_size);
  return 0;
}

/******************************************************************************/
