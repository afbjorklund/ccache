/*
  MessagePack for C++
*/

typedef string str // string (STR)
typedef string bin // binary (BIN)

/** class Digest */
typedef bin digest

/** class util::Bytes */
typedef bin bytes

/** class nonstd::span<const uint8_t> */
typedef bin span8

/*
  util::Bytes get(const Digest& key);

  bool exists(const Digest& key);

  bool put(const Digest& key, nonstd::span<const uint8_t> value);

  bool remove(const Digest& key);

  bool auth(const std::string& pass);
*/

/**
 * Ccache RPC storage
 */
service ccache_storage {

  /**
   * Get the value associated with `key`. Returns the value on success or
   * std::nullopt if the entry is not present.
   */
  bin get(1:digest key),

  /**
   * Check if `key` exists in storage, without the value. Returns presence.
   */
  bool exists(1:digest key),

  /**
   * Put the value associated with `key` into storage. Returns success.
   */
  bool put(1:digest key, 2:span8 value),

  /**
   * Remove the value associated with `key` from storage. Returns success.
   */
  bool remove(1:digest key),

  /**
   * Authenticate session using `password`. Returns success.
   */
  bool auth(1:str password),
}
