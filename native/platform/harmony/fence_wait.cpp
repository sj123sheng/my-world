#include "fence_wait.h"

#include <errno.h>
#include <poll.h>
#include <unistd.h>

bool waitAndCloseFence(int fenceFd, int timeoutMs) {
  if (fenceFd < 0) {
    return true;
  }

  pollfd descriptor{fenceFd, POLLIN, 0};
  int result;
  do {
    result = poll(&descriptor, 1, timeoutMs);
  } while (result < 0 && (errno == EINTR || errno == EAGAIN));

  const bool ready = result > 0 &&
      (descriptor.revents & (POLLIN | POLLERR | POLLHUP)) != 0;
  close(fenceFd);
  return ready;
}
