// Rewind buffer: see XboxRewind.h. Plain C++, tested on PC.
#include "XboxRewind.h"

#include <string.h>
#include <deque>
#include <vector>

namespace {

std::vector<unsigned char> g_cur;                    // the newest state
std::deque<std::vector<unsigned char> > g_steps;     // newest last: cur XOR the state before it
size_t g_size = 0, g_budget = 0, g_used = 0;
bool g_have = false;

void putU32(std::vector<unsigned char> &out, size_t v)
{
  for (int i = 0; i < 4; i++)
    out.push_back((unsigned char)(v >> (i * 8)));
}

size_t getU32(const unsigned char *p)
{
  return (size_t)p[0] | ((size_t)p[1] << 8) | ((size_t)p[2] << 16) | ((size_t)p[3] << 24);
}

// a XOR b as runs: [zeros (4 bytes)][count (4 bytes)][that many bytes]...
// A changed run ends at 8 unchanged bytes in a row.
void encodeXor(const unsigned char *a, const unsigned char *b, size_t n, std::vector<unsigned char> &out)
{
  out.clear();
  size_t i = 0;
  while (i < n) {
    size_t z = i;
    while (z < n && a[z] == b[z])
      z++;
    if (z == n)
      break;
    size_t end = z, same = 0;
    while (end < n && same < 8) {
      same = a[end] == b[end] ? same + 1 : 0;
      end++;
    }
    if (same == 8)
      end -= 8;
    putU32(out, z - i);
    putU32(out, end - z);
    for (size_t k = z; k < end; k++)
      out.push_back((unsigned char)(a[k] ^ b[k]));
    i = end;
  }
}

void applyXor(unsigned char *s, size_t n, const std::vector<unsigned char> &d)
{
  size_t pos = 0, i = 0;
  while (i + 8 <= d.size()) {
    pos += getU32(&d[i]);
    size_t count = getU32(&d[i + 4]);
    i += 8;
    for (size_t k = 0; k < count && pos < n && i < d.size(); k++)
      s[pos++] ^= d[i++];
  }
}

} // namespace

void rewindReset(size_t stateSize, size_t budget)
{
  g_steps.clear();
  g_used = 0;
  g_have = false;
  g_size = stateSize;
  g_budget = budget;
  if (g_cur.size() != stateSize)
    std::vector<unsigned char>(stateSize).swap(g_cur);
}

void rewindPush(const unsigned char *state)
{
  if (!g_size)
    return;
  if (!g_have) {
    memcpy(&g_cur[0], state, g_size);
    g_have = true;
    return;
  }
  g_steps.push_back(std::vector<unsigned char>());
  encodeXor(state, &g_cur[0], g_size, g_steps.back());
  std::vector<unsigned char>(g_steps.back()).swap(g_steps.back());   // no spare capacity kept
  g_used += g_steps.back().size();
  memcpy(&g_cur[0], state, g_size);
  while (g_used > g_budget && !g_steps.empty()) {   // full: the oldest go
    g_used -= g_steps.front().size();
    g_steps.pop_front();
  }
}

bool rewindPop(unsigned char *state)
{
  if (!g_have)
    return false;
  bool older = !g_steps.empty();
  if (older) {
    applyXor(&g_cur[0], g_size, g_steps.back());
    g_used -= g_steps.back().size();
    g_steps.pop_back();
  }
  memcpy(state, &g_cur[0], g_size);
  return older;
}

int rewindSteps()
{
  return (int)g_steps.size();
}

bool rewindHasState()
{
  return g_have;
}
