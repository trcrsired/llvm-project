// Legacy header imported through a namespace-scope [[unsafe]] fence.
inline void legacy_helper() noexcept {}

inline void legacy_entry() noexcept { legacy_helper(); }
