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

#define NBUCKET  13
#define NBUF_PER_BUCKET NBUF / NBUCKET

struct bucket {
  struct spinlock lock;
  struct buf head;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct bucket bucket[NBUCKET];
} bcache;

void
binit(void)
{
  int i, j, left, bk_cnt;
  char lock_name[16];
  struct buf *b;
  struct bucket *bk;

  initlock(&bcache.lock, "bcache");
  b = bcache.buf;
  left = NBUF % NBUCKET;
  for (i = 0; i < NBUCKET; i++) {
    bk = &bcache.bucket[i];

    snprintf(lock_name, 16, "bcache_bucket%d", i);
    initlock(&bk->lock, lock_name);

    bk->head.prev = &bk->head;
    bk->head.next = &bk->head;
    bk_cnt = left-- > 0? NBUF_PER_BUCKET+1 : NBUF_PER_BUCKET;
    for(j = 0; j < bk_cnt; j++) {
      initsleeplock(&b->lock, "buffer");
      b->next = bk->head.next;
      b->prev = &bk->head;
      bk->head.next->prev = b;
      bk->head.next = b;
      b->refcnt = 0;
      b++;
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  int i, index, lrub_index = 0, min_ticks = -1;
  struct buf *b, *lrub = 0;
  struct bucket *bk;

  index = blockno % NBUCKET;
  bk = &bcache.bucket[index];

  acquire(&bk->lock);
  // Is the block already cached?
  for(b = bk->head.next; b != &bk->head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bk->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  release(&bk->lock);
  // acquire(&bcache.lock);
  for(i = 0; i < NBUCKET; i++) {
    bk = &bcache.bucket[i];
    acquire(&bk->lock);
    for(b = bk->head.next; b != &bk->head; b = b->next) {
      if(b->refcnt == 0) {
        if(min_ticks == -1 || min_ticks > b->ticks) {
          lrub = b;
          min_ticks = b->ticks;
          lrub_index = i;
        }
      }
    }
  }

  if(!lrub) {
    for(i = 0; i < NBUCKET; i++) 
      release(&bcache.bucket[i].lock);   
    // release(&bcache.lock); 
    panic("bget: no buffers");
  }

  for(i = 0; i < NBUCKET; i++) {
    if (i != lrub_index && i != index) {
      bk = &bcache.bucket[i];
      release(&bk->lock);   
    }
  }

  lrub->dev = dev;
  lrub->blockno = blockno;
  lrub->valid = 0;
  lrub->refcnt = 1;

  if(lrub_index == index) {
    release(&bcache.bucket[index].lock);
    // release(&bcache.lock);
    acquiresleep(&lrub->lock);
    return lrub;
  }

  lrub->prev->next = lrub->next;
  lrub->next->prev = lrub->prev;
  
  bk = &bcache.bucket[index];
  lrub->prev = &bk->head;
  lrub->next = bk->head.next;
  bk->head.next->prev = lrub;
  bk->head.next = lrub;
  release(&bcache.bucket[index].lock);
  release(&bcache.bucket[lrub_index].lock);
  // release(&bcache.lock);
  acquiresleep(&lrub->lock);
  return lrub;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
    b->ticks = ticks;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  
  b->ticks = ticks;
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  struct bucket *bk;

  if(!holdingsleep(&b->lock))
    panic("brelse");
  releasesleep(&b->lock);

  bk = &bcache.bucket[b->blockno % NBUCKET]; 
  acquire(&bk->lock);
  b->refcnt--;
  release(&bk->lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


