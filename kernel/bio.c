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

#define BUFHASH_MAP(dev, blockno) (((dev << 15) + blockno) % NBUFMAP_BUCKETS)

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // bufmap is a hash table that maps (dev, blockno) to a buffer.
  struct buf bufmap[NBUFMAP_BUCKETS];
  struct spinlock bufmap_lock[NBUFMAP_BUCKETS]; 
} bcache;

void
binit(void)
{
  initlock(&bcache.lock, "bcache");

  // Initialize bufmap
  for(int i = 0; i < NBUFMAP_BUCKETS; i++) {
    initlock(&bcache.bufmap_lock[i], "bufmap");
    bcache.bufmap[i].next = 0;
  }

  // Create a linked list of buffers.
  // Firstly, put all buffers into the first bucket.
  for(int i = 0; i < NBUF; i++){
    struct buf *b = &bcache.buf[i];

    b->refcnt = 0;
    b->last_use = 0;

    initsleeplock(&b->lock, "buffer");

    b->next = bcache.bufmap[0].next;
    bcache.bufmap[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint key = BUFHASH_MAP(dev, blockno);

  acquire(&bcache.bufmap_lock[key]);

  // Is the block already cached?
  for(b = bcache.bufmap[key].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bufmap_lock[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Release the bucket lock.
  release(&bcache.bufmap_lock[key]);
  // Acquire the global lock.
  acquire(&bcache.lock);

  // To avoid race condition, we need to check again.
  for(b = bcache.bufmap[key].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      acquire(&bcache.bufmap_lock[key]);
      b->refcnt++;
      release(&bcache.bufmap_lock[key]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  // Still not cached.
  // Recycle the least recently used (LRU) unused buffer.
  struct buf* least = 0;
  uint holding_bucket = -1;

  for(int i = 0; i < NBUFMAP_BUCKETS; i++) {

    acquire(&bcache.bufmap_lock[i]);
    int found = 0;

    for(*b = bcache.bufmap[i]; b; b = b->next){
      if(b->next->refcnt == 0 && (!least || b->next->last_use < least->next->last_use)) {
        least = b;
        found = 1;
      }
    }

  if(!found) {
    release(&bcache.bufmap_lock[i]);
  } else {
      if(holding_bucket != -1) 
        release(&bcache.bufmap_lock[holding_bucket]);
      holding_bucket = i;
    }
  }

  if(!least) {
    panic("bget: no buffers");
  }
  b = least->next;
  
  if(holding_bucket != key) {
    // remove the buf from it's original bucket
    least->next = b->next;
    release(&bcache.bufmap_lock[holding_bucket]);
    // rehash and add it to the target bucket
    acquire(&bcache.bufmap_lock[key]);
    b->next = bcache.bufmap[key].next;
    bcache.bufmap[key].next = b;
  }

    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    release(&bcache.bufmap_lock[key]);
    release(&bcache.lock);
    acquiresleep(&b->lock);
    return b;
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
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint key = BUFHASH_MAP(b->dev, b->blockno);

  acquire(&bcache.bufmap_lock[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->last_use = ticks;
  }
  
  release(&bcache.bufmap_lock[key]);
}

void
bpin(struct buf *b) {
  uint key = BUFHASH_MAP(b->dev, b->blockno);

  acquire(&bcache.bufmap_lock[key]);
  b->refcnt++;
  release(&bcache.bufmap_lock[key]);
}

void
bunpin(struct buf *b) {
  uint key = BUFHASH_MAP(b->dev, b->blockno);

  acquire(&bcache.bufmap_lock[key]);
  b->refcnt--;
  release(&bcache.bufmap_lock[key]);
}


