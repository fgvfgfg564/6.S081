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
uint lru[NBUF];
struct spinlock lock[NBUCKET];
uint mapi[NTEMP], mapx[NTEMP], mapy[NTEMP], cnt;
struct spinlock maplock;

void
binit(void)
{
  struct buf *b;
  initlock(&maplock, "bcache_map");

  // Create linked list of buffers
  for (b = bcache; b < bcache + NBUF; b++)
    initsleeplock(&b->lock, "buffer");
  for (int i = 0; i < NBUCKET; i++)
    initlock(&lock[i], "bcache_lock");
  cnt = 0;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *b;
  int h = blockno % NBUCKET;
  for (int i = 0; i < cnt; i++) {
    if (mapi[i] == dev && mapx[i] == blockno) {
      h = mapy[i];
      break;
    }
  }
  acquire(&lock[h]);
  for (int i = h * 10; i < h * 10 + 10; i++) {
    b = bcache + i;
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&lock[h]);
      //printf("release %d\n", b - bcache);
      acquiresleep(&b->lock);
      return b;
    }
    //printf("release %d\n", b - bcache);
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  int mini = -1, minx = 0x7fffffff;
  for (int i = h * 10; i < h * 10 + 10; i++) {
    b = bcache + i;
    if (b->refcnt == 0 && lru[i] < minx) {
      minx = lru[i];
      mini = i;
    }
    //printf("release %d\n", b - bcache);
  }
  if (mini >= 0) {
    b = bcache + mini;
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    release(&lock[h]);
    acquiresleep(&b->lock);
    return b;
  }
  //printf("release %d\n", b - bcache);

  release(&lock[h]);
  mini = -1; minx = 0x7fffffff;
  for (int i = 0; i < NBUCKET; i++)
    acquire(&lock[i]);
  for (int i = (h + 1) % NBUCKET; i != h; i = (i + 1) % NBUCKET) {
    for (int j = 0; j < 10; j++) {
      b = bcache + i * 10 + j;
      if (b->refcnt == 0 && lru[i * 10 + j] < minx) {
        minx = lru[i * 10 + j];
        mini = i * 10 + j;
      }
    }
  }
  if (mini >= 0) {
    mapi[cnt] = mini / 10;
    mapx[cnt] = dev;
    mapy[cnt] = blockno;

    b = bcache + mini;
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    for (int i = 0; i < NBUCKET; i++)
      release(&lock[i]);
    acquiresleep(&b->lock);
    return b;
  }
  panic("bget: no buffers");
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
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  //printf("acquire %d\n", b - bcache);
  int h = (b - bcache) / 10;
  acquire(&lock[h]);
  b->refcnt--;
  lru[b-bcache] = ticks;
  release(&lock[h]);

  //printf("release %d\n", b - bcache);
}

void
bpin(struct buf *b)
{
  int h = (b - bcache) / 10;
  acquire(&lock[h]);
  b->refcnt++;
  release(&lock[h]);
}

void
bunpin(struct buf *b)
{
  int h = (b - bcache) / 10;
  acquire(&lock[h]);
  b->refcnt--;
  release(&lock[h]);
}


