#pragma once

// Waits until a producer fence is ready and always closes a non-negative fd.
bool waitAndCloseFence(int fenceFd, int timeoutMs);
