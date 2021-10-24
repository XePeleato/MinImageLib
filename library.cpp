#include "library.h"

#include <iostream>
#include "MinixFS.h"
#include "minixfs_operations.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

void runDokan(minixfs::MinixFS& fs) {
    DOKAN_OPTIONS dokanOptions;
    ZeroMemory(&dokanOptions, sizeof(dokanOptions));

    dokanOptions.Version = DOKAN_VERSION;
    dokanOptions.ThreadCount = 0;
    dokanOptions.Timeout = 0;
    dokanOptions.GlobalContext = reinterpret_cast<ULONG64>(&fs);
    dokanOptions.MountPoint = L"W:\\";
    dokanOptions.Options |= DOKAN_OPTION_ALT_STREAM;
    //dokanOptions.Options |= DOKAN_OPTION_WRITE_PROTECT;
    dokanOptions.Options |= DOKAN_OPTION_CURRENT_SESSION;
    dokanOptions.Options |= DOKAN_OPTION_STDERR | DOKAN_OPTION_DEBUG;

    DOKAN_OPERATIONS dokanOperations;
    ZeroMemory(&dokanOperations, sizeof(DOKAN_OPERATIONS));
    minixfs::setup(dokanOperations);

    auto status = DokanMain(&dokanOptions, &dokanOperations);
}


void hello() {
    auto *fs = new minixfs::MinixFS();
    auto status = fs->setup(L"C:\\Users\\Eduardo\\fda.img");
    std::cout << "Hello, World!" << std::endl;
    runDokan(*fs);
}

int main(int argc, char **argv) {
    hello();
}


