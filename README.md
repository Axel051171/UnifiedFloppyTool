# UnifiedFloppyTool

[![Release](https://img.shields.io/github/v/release/Axel051171/UnifiedFloppyTool)](https://github.com/Axel051171/UnifiedFloppyTool/releases)
[![Build](https://github.com/Axel051171/UnifiedFloppyTool/actions/workflows/ci.yml/badge.svg)](https://github.com/Axel051171/UnifiedFloppyTool/actions)

**„Kein Bit verloren. Keine stille Veränderung. Keine erfundenen Daten."**

Open-source forensic floppy disk preservation tool — a Qt 6 C/C++ desktop
application for archives, museums, retrocomputing enthusiasts, digital
forensics and copy-protection research. It reads, analyses and converts
disk images of 8- and 16-bit computers and captures flux from floppy
controllers such as the Greaseweazle.

## Download

Ready-to-run builds for Linux, macOS and Windows are on the
**[releases page](https://github.com/Axel051171/UnifiedFloppyTool/releases/latest)**,
each with a `SHA256SUMS.txt`.

> **macOS:** if the system reports the app as damaged, run
> `xattr -cr UnifiedFloppyTool.app`.

## Building from source

This repository holds exactly the source needed to compile the
application. See **[BUILDING.md](BUILDING.md)**.

## About this repository

Development happens in a separate repository; this one receives the
source of each published version. A `RELEASE_SOURCE.txt`, where present,
names the development commit a tree was exported from.

Bug reports and questions are welcome as
[issues](https://github.com/Axel051171/UnifiedFloppyTool/issues).

## License

See [LICENSE](LICENSE).
