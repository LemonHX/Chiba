# Chiba Runtime

## Needing Contributers for

- [ ] aarch64 windows `ASM` fiber impl
- [ ] android aarch64 testing
- [ ] ios aarch64 testing
- [ ] wasm64 fiber impl

## Building

please use c-compiler caching tool for incremental builds
such as

- `sccache` (written in rust supporting multiple compilers, could use s3 or disk as cache backend)
- `ccache` (written in c, supports only gcc/clang/msvc, could use local disk as cache backend)

please use a better linker instead of using `gold` in posix

- `lld` (llvm project, faster than gold, supports posix/windows)
- `mold` (faster than lld and gold, supports only posix)

## Testing

metrics:

- `ASM` impls:
   - aarch64 darwin
   - aarch64 posix
   - amd64 posix
   - rv32/rv64 posix
   - amd64 windows
- `ucontext` impls:
    - any posix
- `wasm fiber` impls:
   - wasm32 emscripten

