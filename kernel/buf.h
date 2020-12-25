struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  struct spinlock kernel_lock;
  uint refcnt;
  uchar data[BSIZE];
  struct buf* prev, next;
};

