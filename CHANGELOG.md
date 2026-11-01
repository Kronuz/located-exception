# Changelog

Notable changes to `located-exception`, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); the library has no releases, so
entries are dated.

## 2026-11-01

### Changed

- The README follows the family README standard: the description as its first sentence,
  Quick start before Installation, a pinned FetchContent tag, and "Not for" limits. The
  Quick start's expected output names the line the throw is on.
- The repository description names C++20 and `std::format`, which the library uses, where
  it said C++17 and {fmt}.

### Removed

- The README diagram (`docs/located-exception.d2`, `docs/located-exception.svg`), which
  drew the three outcomes of a throw that the Quick start shows in code.
