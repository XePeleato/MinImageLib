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
        SetupState setup(const std::string &filename);
        Inode * find(const std::wstring& filename);
        size_t readInode(const Inode& inode, void *buffer, size_t size, size_t offset) const;
        size_t readBlock(zone_t block, void *buffer, size_t size, size_t offset) const;
        size_t readIndirectBlock(zone_t block, void *buffer, size_t size, size_t offset) const;
        std::vector<DirEntry*> listFolder(const std::filesystem::path &path) const;
        void loadEntries();
        void loadEntry(ino_t inode, std::filesystem::path &path);
        size_t getFreeSpace() const;
        size_t getTotalSpace() const;
        void createFile(std::wstring &path, bool isDir);
        NTSTATUS
        writeFile(std::wstring &filename, const void *buffer, size_t NumberOfBytesToWrite, unsigned long *NumberOfBytesWritten,
                  size_t Offset);
    private:
        SuperBlock *mSuperBlock;

        std::vector<unsigned char> mInodeBitmap;

        std::vector<unsigned char> mBlockBitmap;
        Inodes *mInodes;
        std::map<std::wstring, ino_t> mEntryMap;

        std::unique_ptr<Stream> mStream;
        bool getBlock(zone_t block) const;
        bool getInode(ino_t ino) const;
        void setBlock(zone_t block, bool set);

        void setInode(ino_t ino, bool set);
    };
}

#endif //MINIMAGEDRV_MINIXFS_H
