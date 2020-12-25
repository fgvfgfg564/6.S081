// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"


struct buf bcache[NBUF];

struct {
  int stack[NBUF * 10];
  int top;
  int count[NBUF];
  struct spinlock lock;
} usage;

// refresh count
// usage.lock must be hold
void refrest_usage()
{
  int temp[NBUF * 10];
  int top2 = 0, tot = 0;
  for (int i = 0; i < NBUF; i++) {
    if (usage.count[i]) top2++;
  }
  tot = top2;
  memset(usage.count, 0, sizeof(usage.count));
  for (int i = usage.top - 1; i >= 0; i--) {
    if (usage.count[usage.stack[i]] == 0) {
      temp[--top2] = usage.stack[i];
      usage.count[usage.stack[i]] = 1;
    }
  }
  memmove(usage.stack, temp, sizeof(temp));
  usage.top = tot;
}

void push_usage(int ind)
{
  acquire(&usage.lock);
  if (usage.top == NBUF * 10) refrest_usage();
  usage.stack[++usage.top] = ind;
  usage.count[ind] ++;
  release(&usage.lock);
}

int pop_usage()
{
  acquire(&usage.lock);
  int res;
  do {
    if (usage.top == 0) {
      release(&usage.lock);
      return -1;
    }
    res = usage.stack[--usage.top];
    usage.count[res] --;
  } while (usage.count[res] != 0 || bcache[res].refcnt != 0);
  release(&usage.lock);
  if (res >= NBUF)
    panic("pop usage");
  return res;
}

void
binit(void)
{
  struct buf *b;
  initlock(&usage.lock, "bcache_usage");
  usage.top = 0;
  memset(usage.count, 0, sizeof(usage.count));

  // Create linked list of buffers
  for (b = bcache; b < bcache + NBUF; b++) {
    push_usage(b - bcache);
    initsleeplock(&b->lock, "buffer");
    initlock(&b->kernel_lock, "bcache");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *b;
  push_off();

  // Is the block already cached?
  for (b = bcache; b < bcache + NBUF; b++) {
    //printf("acquire %d\n", b - bcache);
    acquire(&b->kernel_lock);
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&b->kernel_lock);
      //printf("release %d\n", b - bcache);
      acquiresleep(&b->lock);
      pop_off();
      return b;
    }
    release(&b->kernel_lock);
    //printf("release %d\n", b - bcache);
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  int ind = pop_usage();
  if (ind < 0)
    panic("bget: ind out of range");
  b = bcache + ind;
  //printf("acquire %d\n", b - bcache);
  acquire(&b->kernel_lock);
  if (b < 0)
    panic("bget: no buffers");
  if (b->refcnt != 0)
    panic("bget");
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
  release(&b->kernel_lock);
  //printf("release %d\n", b - bcache);
  acquiresleep(&b->lock);
  pop_off();
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  push_off();
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  //printf("acquire %d\n", b - bcache);
  acquire(&b->kernel_lock);
  b->refcnt--;
  if (b->refcnt == 0)
    push_usage(b - bcache);

  release(&b->kernel_lock);
  pop_off();
}

void
bpin(struct buf *b)
{
  acquire(&b->kernel_lock);
  b->refcnt++;
  release(&b->kernel_lock);
}

void
bunpin(struct buf *b)
{
  acquire(&b->kernel_lock);
  b->refcnt--;
  release(&b->kernel_lock);
}


