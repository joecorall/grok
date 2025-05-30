#include <BufferPool.h>

BufferPool::BufferPool() {}

BufferPool::~BufferPool()
{
  for(auto& p : pool)
    p.second.dealloc();
}

GrkIOBuf BufferPool::get(size_t len)
{
  for(auto iter = pool.begin(); iter != pool.end(); ++iter)
  {
    if(iter->second.alloc_len >= len)
    {
      auto b = iter->second;
      b.len = len;
      pool.erase(iter);
      // printf("Buffer pool get  %p\n", b.data);
      return b;
    }
  }
  GrkIOBuf rc;
  rc.alloc(len);

  return rc;
}
void BufferPool::put(GrkIOBuf b)
{
  assert(b.data);
  assert(pool.find(b.data) == pool.end());
  pool[b.data] = b;
}
