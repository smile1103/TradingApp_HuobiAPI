/*
MIT License
Copyright (c) 2018 Meng Rao <raomeng1@gmail.com>
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once
#include <atomic>
#include <unistd.h>
#include <sys/syscall.h>

// THR_SIZE must not be less than the max number of threads using tryPush/tryPop, otherwise they could fail forever
// It's preferred to set THR_SIZE twice the max number, because THR_SIZE is the size of an open addressing hash table
// 16 is a good default value for THR_SIZE, as 16 tids fit exactly in a cache line: 16 * 4 = 64

namespace exchange_core
{
  template <class T, uint32_t SIZE, uint32_t THR_SIZE = 16>
  class WFSPSC
  {
  public:
    static_assert(SIZE && !(SIZE & (SIZE - 1)), "SIZE must be a power of 2");
    static_assert(THR_SIZE && !(THR_SIZE & (THR_SIZE - 1)), "THR_SIZE must be a power of 2");

    // shmInit() should only be called for objects allocated in SHM and are zero-initialized
    void shmInit()
    {
    }

    int64_t size()
    {
      return write_idx.load(std::memory_order_relaxed) - read_idx.load(std::memory_order_relaxed);
    }

    // A hint to check if read is likely to wait
    bool empty()
    {
      return size() <= 0;
    }

    void reset()
    {
      write_idx = 0;
      read_idx = 0;
    }

    int64_t getCurrentWriteIdx()
    {
      return write_idx;
    }

    int64_t getCurrentReadIdx()
    {
      return read_idx;
    }

    T *getWritable(int64_t idx)
    {
      auto &blk = blks[idx % SIZE];
      return reinterpret_cast<T *>(&blk.data);
    }

    // Lounger(All in One) version of write, which is neither wait-free nor zero-copy
    template <typename... Args>
    void emplace(Args &&...args)
    {
      int64_t idx = write_idx + 1;
      T *data = getWritable(idx);
      new (data) T(std::forward<Args>(args)...);
      write_idx = idx;
    }

    // zero-copy and wait-free
    // Visitor's signature: void f(T& val), where val is an *unconstructed* object
    template <typename Visitor>
    bool tryVisitPush(Visitor v)
    {
      int64_t idx = write_idx + 1;
      T *data = getWritable(idx);
      v(*data);
      write_idx = idx;
      return true;
    }

    template <typename Type>
    bool tryPush(Type &&val)
    {
      return tryVisitPush(
          [val = std::forward<decltype(val)>(val)](T &data)
          { new (&data) T(std::forward<decltype(val)>(val)); });
    }

    T *getReadable(int64_t idx)
    {
      auto &blk = blks[idx % SIZE];
      return reinterpret_cast<T *>(&blk.data);
    }

    // Lounger(All in One) version of read, which is neither wait-free nor zero-copy
    T pop()
    {
      int64_t idx = read_idx + 1;
      T *data = getReadable(idx);
      T ret = std::move(*data);
      read_idx = idx;
      return ret;
    }

    // zero-copy and wait-free
    // Visitor's signature: void f(T&& val)
    template <typename Visitor>
    bool tryVisitPop(Visitor v)
    {
      int64_t idx = read_idx + 1;
      T *data = getReadable(idx);
      v(std::move(*data));
      read_idx = idx;
      return true;
    }

  private:
    alignas(64) std::atomic<int64_t> write_idx;
    alignas(64) std::atomic<int64_t> read_idx;

    struct
    {
      typename std::aligned_storage<sizeof(T), alignof(T)>::type data;
    } blks[SIZE];
  };

}