# Third Party Libraries

Especially thanks to some Non-Posix compatible OS, we had to vendor in some third party libraries to make Chiba work on those platforms.

- wepoll: A Windows I/O completion port backend for libuv
    - Source URL: https://github.com/piscisaureus/wepoll
    - License: Redistributions of source / binary must retain the copyright and the list of conditions and the disclaimer(in the source).
    - Copyright 2012-2020, Bert Belder <bertbelder@gmail.com>

- pthread-win32: POSIX Threads for Windows
    - Source URL: https://github.com/GerHobbelt/pthread-win32
    - License:
        - Apache License v2 (v3+)
        - LGPL v3 (v2+)

