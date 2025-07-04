#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <tchar.h>
#include <direct.h>
#include <stdint.h>

#ifdef GCC32HACK
#include "win32gcc.h"
#endif

#pragma pack(push, 1)
typedef struct {
  uint32_t offs; /* absolute offset to file in archive */
  uint32_t pack; /* 0 - unpacked; 1 - packed with PKWARE implode */
  uint32_t unsz; /* unpacked size; 0 if not packed */
  uint32_t pksz; /* packed size (size in archive); same as filesize if not packed */
} pak_item;

/* internal structure for inmem blast() callbacks */
typedef struct {
  uint8_t *p;
  uint8_t *u;
  uint32_t ps;
  uint32_t us;
} mem_item;
#pragma pack(pop)

/*
To compile this source code you'll need "blast.c" and "blast.h" files from zlib package.
Get this package from official site: http://zlib.net/
Requered files can be found in /contrib/blast/ folder inside archive.
*/

#include "blast.h"

static unsigned memin(void *how, unsigned char **buf) {
mem_item *mi;
uint32_t sz;
  mi = (mem_item *) how;
  sz = (16 * 1024);
  sz = (sz < mi->ps) ? sz : mi->ps;
  *buf = mi->p;
  mi->p += sz;
  mi->ps -= sz;
  return(sz);
}

static int memout(void *how, unsigned char *buf, unsigned len) {
mem_item *mi;
uint32_t sz;
  mi = (mem_item *) how;
  sz = (len < mi->us) ? len : mi->us;
  memcpy(mi->u, buf, sz);
  mi->u += sz;
  mi->us -= sz;
  return(!mi->us);
}

static char KEY_DEMO[] = "NSIARKPRQPHBTE50GRIH3AYXJP2AMF3FCEYAVQO5QGA0JGIIH2AYXKVOA1VOGGU5GSQKKYEOIAQG1XRX0J4F5OEAEFI4DD3LL45VJTVOA1VOGGUKE50GRI";
static char KEY_FULL[] = "AVQF3FCKE50GRIAYXJP2AMEYO5QGA0JGIIH2NHBTVOA1VOGGU5H3GSSIARKPRQPQKKYEOIAQG1XRX0J4F5OEAEFI4DD3LL45VJTVOA1VOGGUKE50GRIAYX";

void makedirs(char *path) {
char *s;
  if (path) {
    for (s = path; *s; s++) {
      if ((*s == '\\') || (*s == '/')) {
        *s = 0;
        mkdir(path);
        *s = '\\';
      }
    }
  }
}

int main(int argc, char *argv[]) {
char *toc, *htoc, *etoc, *dir, *fname;
uint32_t i, sz, len;
uint8_t *p, *u;
pak_item *item;
FILE *fl, *f;
mem_item mi;
  printf("Arx Fatalis .PAK unpacker v1.3\n(c) CTPAX-X Team 2010-2012,2019\nhttp://www.CTPAX-X.org/\n\n");
  if ((argc < 2) || (argc > 3)) {
    printf(
      "Usage: afunpak <filename.pak> [/all]\n\n"
      "Where:\n"
      "  filename.pak - input Arx Fatalis .PAK archive format file name\n"
      "  /all - create all subfolders even without files (optional argument)\n\n"
    );
    return(1);
  }
  fl = fopen(argv[1], "rb");
  if (!fl) {
    printf("Error: can't open input file.\n\n");
    return(2);
  }
  /* read offset to TOC */
  fread(&sz, 4, 1, fl);
  fseek(fl, 0, SEEK_END);
  len = ftell(fl);
  if ((len - (4 + 4 + 1)) < sz) {
    fclose(fl);
    printf("Error: input file not a .PAK archive (invalid header).\n\n");
    return(3);
  }
  fseek(fl, sz, SEEK_SET);
  /* TOC size */
  fread(&sz, 4, 1, fl);
  if ((ftell(fl) + sz) > len) {
    fclose(fl);
    printf("Error: input file not a .PAK archive (file too small).\n\n");
    return(4);
  }
  toc = (char *) malloc(sz);
  if (!toc) {
    fclose(fl);
    printf("Error: not enough memory for archive file table.\n\n");
    return(5);
  }
  htoc = toc;
  /* read whole TOC */
  fread(toc, 1, sz, fl);
  /* check TOC contents */
  dir = NULL;
  len = *((uint32_t *) toc);
  /* demo - plain */
  if (len == 0) { dir = toc; }
  /* demo - key 1 */
  if (len == *((uint32_t *) KEY_DEMO)) { dir = KEY_DEMO; }
  /* full - key 2 */
  if (len == *((uint32_t *) KEY_FULL)) { dir = KEY_FULL; }
  if (!dir) {
    free(toc);
    fclose(fl);
    printf("Error: input file not a .PAK archive (unknown xor key).\n\n");
    return(6);
  }
  /* decrypt TOC if needed */
  if (dir != toc) {
    /* decrypt TOC */
    len = strlen(dir);
    for (i = 0; i < sz; i++) {
      toc[i] = toc[i] ^ dir[i % len];
    }
    /* --- here you can dump toc[] to file for better understanding TOC structure --- */
  }
  /* calculate end of TOC pointer */
  etoc = toc;
  etoc += sz;
  while (toc < etoc) {
    /* root folder */
    dir = toc;
    len = strlen(dir) + 1;
    /* skip ASCIIZ folder name */
    toc += strlen(toc) + 1;
    /* number of files inside this folder */
    sz = *((uint32_t *) toc);
    toc += 4;
    /* create subfolders if there are at least one file or forced to do so */
    if (sz || (argc == 3)) {
      makedirs(dir);
    }
    while (sz--) {
      i = strlen(toc);
      fname = (char *) malloc(len + i);
      f = NULL;
      if (fname) {
        sprintf(fname, "%s%s", dir, toc);
        f = fopen(fname, "wb");
      }
      if (f) {
        printf("%s", fname);
      } else {
        printf("Error creating: %s%s", dir, toc);
      }
      if (fname) { free(fname); }
      toc += i + 1;
      item = (pak_item *) toc;
      toc += sizeof(item[0]);
      /* output file created */
      if (f) {
        p = (uint8_t *) malloc(item->pksz);
        if (p) {
          mi.ps = item->pksz;
          fseek(fl, item->offs, SEEK_SET);
          fread(p, mi.ps, 1, fl);
          /* PKWARE implode */
          if (item->pack && item->unsz) {
            u = (uint8_t *) malloc(item->unsz);
            if (u) {
              memset(u, 0, item->unsz);
              mi.p = p;
              mi.u = u;
              mi.us = item->unsz;
              blast(memin, &mi, memout, &mi, NULL, NULL);
              if (mi.ps || mi.us) {
                printf("\nWarning: unpacking error - bytes left (in: %u / out: %u).", mi.ps, mi.us);
              }
              free(p);
              p = u;
              mi.ps = item->unsz;
            }
          }
          fwrite(p, mi.ps, 1, f);
          free(p);
        }
        fclose(f);
      }
      printf("\n");
    }
  }
  free(htoc);
  fclose(fl);
  printf("\ndone\n\n");
  return(0);
}
