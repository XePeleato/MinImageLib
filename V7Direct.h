//
// Created by Eduardo on 31/07/2021.
//

#ifndef MINIMAGEDRV_V7DIRECT_H
#define MINIMAGEDRV_V7DIRECT_H

#include <sys/dir.h>
#include "Stream.h"

namespace minixfs {
    class V7Direct {
    public:
        V7Direct() = default;
        explicit V7Direct(Stream& file);
        ino_t d_ino;
        char d_name[DIRSIZ];
    };
}

#endif //MINIMAGEDRV_V7DIRECT_H
