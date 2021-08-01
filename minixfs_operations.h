//
// Created by Eduardo on 31/07/2021.
//

#ifndef MINIMAGEDRV_MINIXFS_OPERATIONS_H
#define MINIMAGEDRV_MINIXFS_OPERATIONS_H
#include <dokan/dokan.h>

namespace minixfs {
    void setup(DOKAN_OPERATIONS &dokanOperations);
}
#endif //MINIMAGEDRV_MINIXFS_OPERATIONS_H
