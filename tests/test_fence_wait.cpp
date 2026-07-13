#include "../native/platform/harmony/fence_wait.h"

#include <assert.h>
#include <errno.h>
#include <unistd.h>

int main() {
  assert(waitAndCloseFence(-1, 10));

  int readyPipe[2];
  assert(pipe(readyPipe) == 0);
  const char byte = 'x';
  assert(write(readyPipe[1], &byte, 1) == 1);
  close(readyPipe[1]);
  assert(waitAndCloseFence(readyPipe[0], 50));
  assert(close(readyPipe[0]) == -1 && errno == EBADF);

  int blockedPipe[2];
  assert(pipe(blockedPipe) == 0);
  assert(!waitAndCloseFence(blockedPipe[0], 1));
  assert(close(blockedPipe[0]) == -1 && errno == EBADF);
  close(blockedPipe[1]);
  return 0;
}
