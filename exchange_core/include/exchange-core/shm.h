#pragma once
#include <bits/stdc++.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include "WFSPSC.h"
#include "WFSPMC.h"
#include "message.h"
#include <string>

namespace exchange_core
{
  template <class T>
  T *shmmap(const std::string &filename)
  {
    int fd = shm_open(filename.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd == -1)
    {
      std::cerr << "shm_open failed: " << strerror(errno) << std::endl;
      return nullptr;
    }
    if (ftruncate(fd, sizeof(T)))
    {
      std::cerr << "ftruncate failed: " << strerror(errno) << std::endl;
      close(fd);
      return nullptr;
    }
    T *ret = (T *)mmap(0, sizeof(T), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (ret == MAP_FAILED)
    {
      std::cerr << "mmap failed: " << strerror(errno) << std::endl;
      return nullptr;
    }
    ret->shmInit();
    return ret;
  }

  using Queue = WFSPSC<Message, 1024 * 16>;
  using MulticastQueue = WFSPMC<Message, 1024*16>;

  Queue *getQueue(const std::string & name)
  {
    return shmmap<Queue>(name);
  };

  MulticastQueue *getMulticastQueue(const std::string & name)
  {
    return shmmap<MulticastQueue>(name);
  };

}
