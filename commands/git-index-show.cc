/*
 *******************************************************************************
 *
 *        filename: git-index-show.cc
 *     description:
 *         created: 2023/08/24
 *          author: ticktechman
 *
 *******************************************************************************
 */

#include <_types/_uint32_t.h>
#include <sys/errno.h>

#include <cstdint>
#include <fstream>
#include <iostream>
using namespace std;

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

#define GIT_SHA256_RAWSZ 32
#define GIT_MAX_RAWSZ GIT_SHA256_RAWSZ

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
  unsigned char data[GIT_MAX_RAWSZ + 2 * sizeof(uint16_t)];
  char name[FLEX_ARRAY];
};

int main(int argc, char* argv[]) {
  if (argc != 2) {
    cerr << "usage: " << argv[0] << " <index-file>" << endl;
    return EINVAL;
  }

  std::ifstream ifs(argv[1], std::ios::in | std::ios::binary);
  if (!ifs) {
    cerr << "fail to open file: " << argv[1] << endl;
    return ENFILE;
  }

  cache_header hdr;
  memset(&hdr, 0x00, sizeof(hdr));
  ifs.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
  cout << "signature=0x" << std::hex << ntohl(hdr.hdr_signature) << " "
       << "version=" << ntohl(hdr.hdr_version) << " "
       << "entries=" << ntohl(hdr.hdr_entries) << endl;

  ifs.close();
  return 0;
}

/******************************************************************************/
