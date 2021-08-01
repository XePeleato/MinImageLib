//
// Created by Eduardo on 31/07/2021.
//

#ifndef MINIMAGEDRV_MINIXFS_H
#define MINIMAGEDRV_MINIXFS_H

#include <dokan/dokan.h>
#include <dokan/fileinfo.h>
#include <fstream>
#include <mutex>
#include <map>
#include <filesystem>
#include "Stream.h"
#include "SuperBlock.h"
#include "Inode.h"
#include "DirEntry.h"

namespace minixfs {
    enum class SetupState {
        ErrorFile,
        ErrorFormat,
        Success
    };

    class MinixFS {
    public:
        MinixFS() = default;
        SetupState setup(const std::wstring &filename);
        const Inode * find(const std::wstring& filename);
        size_t readInode(const Inode& inode, void *buffer, size_t size, size_t offset) const;
        size_t readBlock(zone_t block, void *buffer, size_t size, size_t offset) const;
        size_t readIndirectBlock(zone_t block, void *buffer, size_t size, size_t offset) const;
        std::vector<DirEntry*> listFolder(const std::filesystem::path &path) const;
        void loadEntries();
        void loadEntry(ino_t inode, std::filesystem::path &path);
    private:
        SuperBlock *mSuperBlock;
        Inodes *mInodes;

        std::map<std::wstring, ino_t> mEntryMap;
        std::unique_ptr<Stream> mStream;

        std::string &getFileName(Inode &ino) const;

        size_t getFreeSpace() const;
    };
}

#endif //MINIMAGEDRV_MINIXFS_H
