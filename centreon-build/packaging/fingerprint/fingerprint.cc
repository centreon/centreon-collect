#include <cstring>
#include <sys/types.h>
#include <ifaddrs.h>
#include <iomanip>
#include <iostream>
#include <linux/if_packet.h>
#include <vector>

/*
** The following SHA-256 implementation is based on Public Domain work
** released by Ulrich Drepper <drepper@redhat.com>.
*/

/* Structure to save state of computation between the single steps.  */
struct sha256_ctx {
  uint32_t H[8];
  uint32_t total[2];
  uint32_t buflen;
  char buffer[128];/* NB: always correctly aligned for uint32_t.  */
};

#if __BYTE_ORDER == __LITTLE_ENDIAN
# define SWAP(n) \
  (((n) << 24) | (((n) & 0xff00) << 8) | (((n) >> 8) & 0xff00) | ((n) >> 24))
#else
# define SWAP(n) (n)
#endif

/*
** This array contains the bytes used to pad the buffer to the next
** 64-byte boundary.  (FIPS 180-2:5.1.1)
*/
static unsigned char const fillbuf[64] = { 0x80, 0 /* , 0, 0, ...  */ };

// Constants for SHA256 from FIPS 180-2:4.2.2.
static uint32_t const K[64] = {
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
  0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
  0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
  0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
  0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
  0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
  0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
  0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
  0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/*
** Process LEN bytes of BUFFER, accumulating context into CTX.
** It is assumed that LEN % 64 == 0.
*/
static inline void sha256_process_block(
                     void const* buffer,
                     size_t len,
                     struct sha256_ctx *ctx) {
  uint32_t const* words(static_cast<uint32_t const*>(buffer));
  size_t nwords = len / sizeof (uint32_t);
  uint32_t a = ctx->H[0];
  uint32_t b = ctx->H[1];
  uint32_t c = ctx->H[2];
  uint32_t d = ctx->H[3];
  uint32_t e = ctx->H[4];
  uint32_t f = ctx->H[5];
  uint32_t g = ctx->H[6];
  uint32_t h = ctx->H[7];

  /*
  ** First increment the byte count.  FIPS 180-2 specifies the possible
  ** length of the file up to 2^64 bits.  Here we only compute the
  ** number of bytes.  Do a double word increment.
  */
  ctx->total[0] += len;
  if (ctx->total[0] < len)
    ++ctx->total[1];

  /*
  ** Process all bytes in the buffer with 64 bytes in each round of
  ** the loop.
  */
  while (nwords > 0) {
    uint32_t W[64];
    uint32_t a_save = a;
    uint32_t b_save = b;
    uint32_t c_save = c;
    uint32_t d_save = d;
    uint32_t e_save = e;
    uint32_t f_save = f;
    uint32_t g_save = g;
    uint32_t h_save = h;

    /* Operators defined in FIPS 180-2:4.1.2.  */
#define Ch(x, y, z) ((x & y) ^ (~x & z))
#define Maj(x, y, z) ((x & y) ^ (x & z) ^ (y & z))
#define S0(x) (CYCLIC (x, 2) ^ CYCLIC (x, 13) ^ CYCLIC (x, 22))
#define S1(x) (CYCLIC (x, 6) ^ CYCLIC (x, 11) ^ CYCLIC (x, 25))
#define R0(x) (CYCLIC (x, 7) ^ CYCLIC (x, 18) ^ (x >> 3))
#define R1(x) (CYCLIC (x, 17) ^ CYCLIC (x, 19) ^ (x >> 10))

    /*
    ** It is unfortunate that C does not provide an operator for
    ** cyclic rotation.  Hope the C compiler is smart enough.
    */
#define CYCLIC(w, s) ((w >> s) | (w << (32 - s)))

    /* Compute the message schedule according to FIPS 180-2:6.2.2 step 2.  */
    for (unsigned int t = 0; t < 16; ++t) {
      W[t] = SWAP (*words);
      ++words;
    }
    for (unsigned int t = 16; t < 64; ++t)
      W[t] = R1 (W[t - 2]) + W[t - 7] + R0 (W[t - 15]) + W[t - 16];

    /* The actual computation according to FIPS 180-2:6.2.2 step 3.  */
    for (unsigned int t = 0; t < 64; ++t) {
      uint32_t T1 = h + S1 (e) + Ch (e, f, g) + K[t] + W[t];
      uint32_t T2 = S0 (a) + Maj (a, b, c);
      h = g;
      g = f;
      f = e;
      e = d + T1;
      d = c;
      c = b;
      b = a;
      a = T1 + T2;
    }

    /*
    ** Add the starting values of the context according to
    ** FIPS 180-2:6.2.2 step 4.
    */
    a += a_save;
    b += b_save;
    c += c_save;
    d += d_save;
    e += e_save;
    f += f_save;
    g += g_save;
    h += h_save;

    /* Prepare for the next round. */
    nwords -= 16;
  }

  /* Put checksum in context given as argument.  */
  ctx->H[0] = a;
  ctx->H[1] = b;
  ctx->H[2] = c;
  ctx->H[3] = d;
  ctx->H[4] = e;
  ctx->H[5] = f;
  ctx->H[6] = g;
  ctx->H[7] = h;
}

/*
** Initialize structure containing state of computation.
** (FIPS 180-2:5.3.2).
*/
static inline void sha256_init_ctx (struct sha256_ctx* ctx) {
  ctx->H[0] = 0x6a09e667;
  ctx->H[1] = 0xbb67ae85;
  ctx->H[2] = 0x3c6ef372;
  ctx->H[3] = 0xa54ff53a;
  ctx->H[4] = 0x510e527f;
  ctx->H[5] = 0x9b05688c;
  ctx->H[6] = 0x1f83d9ab;
  ctx->H[7] = 0x5be0cd19;
  ctx->total[0] = ctx->total[1] = 0;
  ctx->buflen = 0;
}

/*
** Process the remaining bytes in the internal buffer and the usual
** prolog according to the standard and write the result to RESBUF.
**
** IMPORTANT: On some systems it is required that RESBUF is correctly
** aligned for a 32 bits value.
*/
static inline void* sha256_finish_ctx(
                      struct sha256_ctx* ctx,
                      void* resbuf) {
  /* Take yet unprocessed bytes into account. */
  uint32_t bytes = ctx->buflen;
  size_t pad;

  /* Now count remaining bytes. */
  ctx->total[0] += bytes;
  if (ctx->total[0] < bytes)
    ++ctx->total[1];

  pad = bytes >= 56 ? 64 + 56 - bytes : 56 - bytes;
  for (int i(0); i < pad; ++i)
    ctx->buffer[bytes + i] = fillbuf[i];

  /* Put the 64-bit file length in *bits* at the end of the buffer. */
  *(uint32_t*)&ctx->buffer[bytes + pad + 4] = SWAP (ctx->total[0] << 3);
  *(uint32_t*)&ctx->buffer[bytes + pad] = SWAP ((ctx->total[1] << 3) |
                                                (ctx->total[0] >> 29));

  /* Process last bytes. */
  sha256_process_block(ctx->buffer, bytes + pad + 8, ctx);

  /* Put result from CTX in first 32 bytes following RESBUF. */
  for (unsigned int i = 0; i < 8; ++i)
    ((uint32_t*)resbuf)[i] = SWAP (ctx->H[i]);

  return (resbuf);
}

static inline void sha256_process_bytes(
                     void const* buffer,
                     size_t len,
                     struct sha256_ctx* ctx) {
  /*
  ** When we already have some bits in our internal buffer concatenate
  ** both inputs first.
  */
  if (ctx->buflen != 0) {
    size_t left_over = ctx->buflen;
    size_t add = 128 - left_over > len ? len : 128 - left_over;

    for (int i(0); i < add; ++i)
      ctx->buffer[left_over + i] = static_cast<char const*>(buffer)[i];
    ctx->buflen += add;

    if (ctx->buflen > 64) {
      sha256_process_block (ctx->buffer, ctx->buflen & ~63, ctx);

      ctx->buflen &= 63;
      /* The regions in the following copy operation cannot overlap.  */
      for (int i(0); i < ctx->buflen; ++i)
        ctx->buffer[i] = ctx->buffer[((left_over + add) & ~63) + i];
    }

    buffer = (char const*)buffer + add;
    len -= add;
  }

  /* Process available complete blocks.  */
  if (len >= 64) {
    /*
    ** To check alignment gcc has an appropriate operator. Other
    ** compilers don't.
    */
      #if __GNUC__ >= 2
# define UNALIGNED_P(p) (((uintptr_t) p) % __alignof__ (uint32_t) != 0)
      #else
# define UNALIGNED_P(p) (((uintptr_t) p) % sizeof (uint32_t) != 0)
      #endif
    if (UNALIGNED_P (buffer))
      while (len > 64) {
        for (int i(0); i < 64; ++i)
          ctx->buffer[i] = static_cast<char const*>(buffer)[i];
        sha256_process_block(ctx->buffer, 64, ctx);
        buffer = (char const*)buffer + 64;
        len -= 64;
      }
    else {
      sha256_process_block(buffer, len & ~63, ctx);
      buffer = (char const*) buffer + (len & ~63);
      len &= 63;
    }
  }

  /* Move remaining bytes into internal buffer.  */
  if (len > 0) {
    size_t left_over = ctx->buflen;

    for (int i(0); i < len; ++i)
      ctx->buffer[left_over + i] = static_cast<char const*>(buffer)[i];
    left_over += len;
    if (left_over >= 64) {
      sha256_process_block(ctx->buffer, 64, ctx);
      left_over -= 64;
      for (int i(0); i < left_over; ++i)
        ctx->buffer[i] = ctx->buffer[64 + i];
    }
    ctx->buflen = left_over;
  }
}

template    <typename T>
class       dynarray {
public:
            dynarray() : _data(NULL), _size(0) {}
            dynarray(dynarray const& d) : _data(NULL), _size(0) {
    operator=(d);
  }
  T*        data() {
    return (_data);
  }
  dynarray& operator=(dynarray const& d) {
    if (this != &d) {
      delete [] _data;
      if (d._data) {
        _data = new T[d._size];
        _size = d._size;
        for (int i(0); i < _size; ++i)
          _data[i] = d._data[i];
      }
      else {
        _data = NULL;
        _size = 0;
      }
    }
    return (*this);
  }
  T&        operator[](int index) {
    return (_data[index]);
  }
  bool      operator<(dynarray const& other) const {
    int i(0);
    for (; (i < _size) && (i < other._size); ++i)
      if (_data[i] != other._data[i])
        return (_data[i] < other._data[i]);
    return ((i == _size) && (_size != other._size));
  }
  void      resize(int sz) {
    T* d(new T[sz]);
    for (int i(0), limit((sz < _size) ? sz : _size); i < limit; ++i)
      d[i] = _data[i];
    delete [] _data;
    _data = d;
    _size = sz;
    return ;
  }
  int       size() const {
    return (_size);
  }

private:
  T*        _data;
  int       _size;
};

template <typename T>
static void sort(T* data, int size) {
  if (size > 1) {
    T p(data[size - 1]);
    int i(0);
    for (int j(0); j < size - 1; ++j)
      if (data[j] < p) {
        T t(data[j]);
        data[j] = data[i];
        data[i] = t;
        ++i;
      }
    data[size - 1] = data[i];
    data[i] = p;
    sort(data, i);
    sort(data + i + 1, size - i - 1);
  }
  return ;
}

int main() {
  dynarray<dynarray<unsigned char> > a;
  struct ifaddrs* ifap;
  getifaddrs(&ifap);
  for (struct ifaddrs* ptr(ifap); ptr; ptr = ptr->ifa_next) {
    if (ptr->ifa_addr->sa_family == AF_PACKET) {
      struct sockaddr_ll* sll((struct sockaddr_ll*)ptr->ifa_addr);
      char null_addr[42];
      memset(null_addr, 0, sizeof(null_addr));
      if (memcmp(sll->sll_addr, null_addr, sll->sll_halen)) {
        a.resize(a.size() + 1);
        dynarray<unsigned char>& d(a[a.size() - 1]);
        d.resize(sll->sll_halen);
        for (int i(0); i < sll->sll_halen; ++i)
          d[i] = sll->sll_addr[i];
      }
    }
  }
  freeifaddrs(ifap);
  if (a.size() <= 0) {
    std::cout << "Fingerprint generation failed." << std::endl;
    return (1);
  }
  sort(a.data(), a.size());

  dynarray<unsigned char> b;
  for (int i(0); i < a.size(); ++i)
    for (int j(0); j < a[i].size(); ++j) {
      b.resize(b.size() + 1);
      b[b.size() - 1] = a[i][j];
    }

  unsigned char buffer[32];
  {
    sha256_ctx ctx;
    sha256_init_ctx(&ctx);
    sha256_process_bytes(b.data(), b.size(), &ctx);
    sha256_finish_ctx(&ctx, buffer);
    b[0] ^= 42;
    for (int i(1); i < b.size(); ++i)
      b[i] ^= b[i - 1];
  }
  for (int i(0); i < 32; ++i)
    std::cout << std::setw(2) << std::hex << std::right
              << std::setfill('0') << (int)buffer[i];
  std::cout << "\n";


  return (0);
}
