set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /home/claude/QtWin/6.5.3/mingw_64)
set(CMAKE_PREFIX_PATH /home/claude/QtWin/6.5.3/mingw_64)

set(Qt6_DIR /home/claude/QtWin/6.5.3/mingw_64/lib/cmake/Qt6)
set(Qt6CoreTools_DIR /home/claude/Qt/6.5.3/gcc_64/lib/cmake/Qt6CoreTools)
set(Qt6GuiTools_DIR /home/claude/Qt/6.5.3/gcc_64/lib/cmake/Qt6GuiTools)
set(Qt6WidgetsTools_DIR /home/claude/Qt/6.5.3/gcc_64/lib/cmake/Qt6WidgetsTools)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
