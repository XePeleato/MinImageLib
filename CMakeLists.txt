cmake_minimum_required(VERSION 3.20)
project(MinImageDrv)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(DOKAN_ROOT "${CMAKE_CURRENT_LIST_DIR}/include")

include_directories("${CMAKE_CURRENT_LIST_DIR}/include")

add_executable(MinImageDrv library.cpp MinixFS.cpp MinixFS.h minixfs_operations.cpp SuperBlock.cpp SuperBlock.h Stream.cpp Stream.h types.h const.h Inode.cpp Inode.h V7Direct.cpp V7Direct.h minixfs_operations.h DirEntry.cpp DirEntry.h)
#add_library(MinImageDrv STATIC library.cpp MinixFS.cpp MinixFS.h minixfs_operations.cpp SuperBlock.cpp SuperBlock.h Stream.cpp Stream.h types.h const.h Inode.cpp Inode.h V7Direct.cpp V7Direct.h minixfs_operations.h DirEntry.cpp DirEntry.h)

target_link_libraries(MinImageDrv "${CMAKE_CURRENT_LIST_DIR}/lib/dokan2.lib")
#target_link_libraries(MinImageDrvLib "${CMAKE_CURRENT_LIST_DIR}/lib/dokan1.lib")