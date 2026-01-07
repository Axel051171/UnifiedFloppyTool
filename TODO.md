# UFT Development Roadmap

## Current: v3.7.0 ✓

### Completed
- [x] IPF container support (read/write/validate)
- [x] Safety headers (safe_parse, path_safe, crc_validate)
- [x] Performance utilities (mempool, strbuf, perf)
- [x] 93%+ header compliance
- [x] All atoi → safe parse migration
- [x] switch/default coverage

## Next: v3.8.0

### Planned Features
- [ ] WOZ 2.0 format support
- [ ] Enhanced Apple II GCR decoding
- [ ] Fuzz testing infrastructure
- [ ] Memory analysis (Valgrind/ASan) CI gate

### Technical Debt
- [ ] Remaining 7% headers (Qt dependencies)
- [ ] Performance profiling of hot paths
- [ ] Documentation generation (Doxygen)

## Future

### v4.0.0
- [ ] Plugin architecture for formats
- [ ] Python bindings
- [ ] Web-based viewer
