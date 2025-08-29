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

#define BUCKETCNT 13

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
}bcache;

struct {
  struct spinlock lock;

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcachelist[BUCKETCNT];


void
binit(void)
{
  struct buf *b;
  int i;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  for(i = 0; i < BUCKETCNT; i++){
    initlock(&bcachelist[i].lock, "bcache_hash");
    bcachelist[i].head.prev = &bcachelist[i].head;
    bcachelist[i].head.next = &bcachelist[i].head;
  }
  for(i=0,b = bcache.buf; b < bcache.buf+NBUF; b++,i=(i+1)%BUCKETCNT){
    b->next = bcachelist[i].head.next;
    b->prev = &bcachelist[i].head;
    initsleeplock(&b->lock, "buffer");
    bcachelist[i].head.next->prev = b;
    bcachelist[i].head.next = b;

    b->now_hash = i;
  }
}


// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  struct buf *lru = 0;
  uint hash = blockno % BUCKETCNT;
  
  // 1. 首先在当前桶中查找
  acquire(&bcachelist[hash].lock);
  
  // 第一次检查：是否已被缓存
  for(b = bcachelist[hash].head.next; b != &bcachelist[hash].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcachelist[hash].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 2. 未找到，准备寻找LRU缓冲区
  release(&bcachelist[hash].lock);

  // 3. 在其他桶中寻找LRU缓冲区
  for(int i = 0; i < BUCKETCNT; i++){
    if(i == hash) continue;
    
    acquire(&bcachelist[i].lock);
    for(b = bcachelist[i].head.prev; b != &bcachelist[i].head; b = b->prev){
      if(b->refcnt == 0) {
        lru = b;
        break;
      }
    }
    
    if(lru){
      // 从原桶移除找到的LRU缓冲区
      lru->next->prev = lru->prev;
      lru->prev->next = lru->next;
      release(&bcachelist[i].lock);
      break;
    }
    release(&bcachelist[i].lock);
  }

  if(!lru)
    panic("bget: no buffers");

  // 4. 重新获取目标桶锁并双重检查
  acquire(&bcachelist[hash].lock);
  
  // 第二次检查：防止竞态条件
  for(b = bcachelist[hash].head.next; b != &bcachelist[hash].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      // 其他线程已经缓存了该块
      b->refcnt++;
      release(&bcachelist[hash].lock);
      
      // 将找到的LRU缓冲区放回原桶
      acquire(&bcachelist[lru->now_hash].lock);
      lru->next = bcachelist[lru->now_hash].head.next;
      lru->prev = &bcachelist[lru->now_hash].head;
      bcachelist[lru->now_hash].head.next->prev = lru;
      bcachelist[lru->now_hash].head.next = lru;
      release(&bcachelist[lru->now_hash].lock);
      
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 5. 确实需要缓存，设置新缓冲区
  lru->dev = dev;
  lru->blockno = blockno;
  lru->valid = 0;
  lru->refcnt = 1;
  lru->now_hash = hash;

  lru->next = bcachelist[hash].head.next;
  lru->prev = &bcachelist[hash].head;
  bcachelist[hash].head.next->prev = lru;
  bcachelist[hash].head.next = lru;

  release(&bcachelist[hash].lock);
  acquiresleep(&lru->lock);
  return lru;
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

  acquire(&bcachelist[b->now_hash].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcachelist[b->now_hash].head.next;
    b->prev = &bcachelist[b->now_hash].head;
    bcachelist[b->now_hash].head.next->prev = b;
    bcachelist[b->now_hash].head.next = b;
  }
  
  release(&bcachelist[b->now_hash].lock);
}

void
bpin(struct buf *b) {
  acquire(&bcachelist[b->now_hash].lock);
  b->refcnt++;
  release(&bcachelist[b->now_hash].lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcachelist[b->now_hash].lock);
  b->refcnt--;
  release(&bcachelist[b->now_hash].lock);
}
