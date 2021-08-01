//
// Created by Eduardo on 31/07/2021.
//

#include "V7Direct.h"

namespace minixfs {


    V7Direct::V7Direct(Stream &file) {
        std::lock_guard<std::mutex> lock(file.m_fileMutex);
        //file.m_file.seekg(0x4000, std::ifstream::beg);
        file.m_file.read((char *) &d_ino, sizeof(d_ino));
        file.m_file.read(d_name, DIRSIZ);
    }
}