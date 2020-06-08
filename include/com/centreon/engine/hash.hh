#ifndef CCE_HASH_HH
#define CCE_HASH_HH

struct pair_hash {
  template <class T1, class T2>
  std::size_t operator()(const std::pair<T1, T2>& pair) const {
    return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
  }

  std::size_t operator()(const std::pair<std::string, std::string>& pair) const {
    size_t hash = 0;
    hash_combine(hash, pair.first);
    hash_combine(hash, pair.second);
    return hash;
  }

  inline static void hash_combine(std::size_t& seed, const std::string& v) {
    std::hash<std::string> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }
};

#endif
