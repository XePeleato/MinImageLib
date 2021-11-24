//
// Created by Eduardo on 31/07/2021.
//

#include <iostream>
#include <bitset>
#include "MinixFS.h"

namespace minixfs {

    SetupState MinixFS::setup(const std::wstring &filename) {
        mStream = std::make_unique<Stream>();
        mStream->m_file.open(filename.c_str(),std::ifstream::binary | std::ifstream::in | std::ofstream::out);

        if (!mStream->m_file.is_open()) {
            return SetupState::ErrorFile;
        }
        mSuperBlock = new SuperBlock(*mStream);

        if (!mSuperBlock->validate()) {
            return SetupState::ErrorFormat;
        }

        {
            std::lock_guard<std::mutex> lock(mStream->m_fileMutex);
            mInodeBitmap = std::vector<unsigned char>(0x1000);
            mStream->m_file.seekg(0x2000, std::ifstream::beg);
            mStream->m_file.read(reinterpret_cast<char*>(mInodeBitmap.data()), 0x1000);

            mBlockBitmap = std::vector<unsigned char>(0x1000);
            mStream->m_file.seekg(0x3000, std::ifstream::beg);
            mStream->m_file.read(reinterpret_cast<char*>(mBlockBitmap.data()), 0x1000);
        }

        mInodes = new Inodes(*mStream, mSuperBlock->s_ninodes);

        loadEntries();

        return SetupState::Success;
    }

    Inode *MinixFS::find(const std::wstring& filename) {
        std::wcout << L"Finding " + filename + L'\n';
        auto node = mEntryMap.find(filename);
        if (node != mEntryMap.end()) {
            std::wcout << L"Found " + filename + L'\n';
            return &mInodes->operator[](node->second);
        }
        std::wcout << L"Not Found " + filename + L'\n';
        return nullptr;
    }

    size_t MinixFS::readInode(const Inode& inode, void *buffer, size_t size, size_t offset) const {
        size_t read = 0;
        size_t toRead = size;
        std::vector<char> buf(size);

        for (int i = 0; i < V2_NR_TZONES && inode.d2_zone[i] != 0 && toRead > 0; i++) {
            if (i < V2_NR_DZONES) {
                // Direct block.
                toRead -= readBlock(inode.d2_zone[i], buf.data() + read, min(0x1000, size), offset);
                read += min(0x1000, inode.d2_size);
            }
            else if (i == V2_NR_DZONES + 1) {
                // Indirect block
                read += readIndirectBlock(inode.d2_zone[i], buf.data() + read, min(0x1000, size), offset);
            }
            else {
                if (i == V2_NR_DZONES + 2) {
                    // do nothing...
                }
            }
        }
        // handle offset/size here
        memcpy_s(buffer, size, buf.data() + offset, size);
        return read;
    }

    size_t MinixFS::readBlock(zone_t block, void *buffer, size_t size, size_t offset) const {
        std::lock_guard<std::mutex> lock(mStream->m_fileMutex);
        mStream->m_file.seekg(0x1000 * block + offset, std::ifstream::beg);

        mStream->m_file.read(static_cast<char *>(buffer), size);
        return size;
    }

    size_t MinixFS::readIndirectBlock(zone_t block, void *buffer, size_t size, size_t offset) const {
        size_t read = 0;
        std::vector<zone_t> zones(1024);
        readBlock(block, zones.data(), 0x1000, 0);

        for (int i = 0; i < 1024; i++) {
            auto direct = zones[i];
            readBlock(direct, static_cast<char *>(buffer) + 0x1000 * i, size, offset);
            read += 1024;
        }
        return read;
    }

    void MinixFS::loadEntries() {
        std::filesystem::path root("\\");
        mEntryMap.emplace(root, 1);
        loadEntry(1, root);
    }

    void MinixFS::createFile(std::wstring &path, bool isDir) {
        auto existing = find(path);

        if (existing) {
            std::wcout << L"FILE  " + path  + L"ALREADY CREATED...\n";
        }

        ino_t inode = 1;
        while (getInode(inode)) {
            inode++;
        }
        setInode(inode, true);
        Inode ino = mInodes->operator[](inode);
        ino.d2_size = 0;
        ino.d2_nlinks = isDir ? 2 : 1;
        ino.d2_mode = 7 + (isDir ? I_DIRECTORY : I_REGULAR);

        mInodes->operator[](inode) = ino;

        // Write inode bitmap value
        std::lock_guard<std::mutex> lock(mStream->m_fileMutex);

        mStream->m_file.seekg(0x2000 + inode / 8, std::ifstream::beg);
        mStream->m_file.write(reinterpret_cast<char*>(mInodeBitmap.data() + inode / 8), 1);

        // Write inode
        mStream->m_file.seekg(0x4000 + 64 * (inode - 1), std::ifstream::beg);
        mStream->m_file.write(reinterpret_cast<const char *>(&ino), 64);
        // -----------------------------------------------------------------------

        mEntryMap.emplace(path, inode);

        auto parentPath = std::filesystem::path(path).parent_path();
        auto parent = find(parentPath);
        auto parentSize = parent->d2_size;
        auto blocks = parent->d2_zone;

        zone_t lastBlock = 0;
        while (blocks[lastBlock] != 0) {
            lastBlock++;
        }
        lastBlock = blocks[lastBlock - 1];


        auto filename = std::filesystem::path(path).filename().u8string();

        // Write directory entry
        V7Direct child;
        const char * filenamecstr = reinterpret_cast<const char *>(filename.c_str());
        strncpy_s(child.d_name, filenamecstr, _TRUNCATE);
        child.d_ino = inode;
        mStream->m_file.seekg(0x1000 * lastBlock + parentSize, std::ifstream::beg);
        mStream->m_file.write(reinterpret_cast<const char *>(&child), 64);

        // Extend parent
        parent->d2_size += 64;
        auto parentEntry = mEntryMap.find(parentPath.wstring());
        mStream->m_file.seekg(0x4000 + 64 * (parentEntry->second - 1), std::ifstream::beg);
        mStream->m_file.write(reinterpret_cast<const char *>(parent), 64);

    }

    NTSTATUS MinixFS::writeFile(std::wstring &filename,
                                const void *buffer,
                                size_t NumberOfBytesToWrite,
                                unsigned long *NumberOfBytesWritten,
                                size_t Offset) {

        // First check if file is new (no data blocks)
        printf("Offset: %zu\n", Offset);
        auto ino = find(filename);

        int neededBlocks = (NumberOfBytesToWrite + 0xFFF) / 0x1000;
        int directBlocks = min(7, neededBlocks);

        if (neededBlocks > 7 + 1024 + pow(1024, 2)) {
            *NumberOfBytesWritten = 0;
            return STATUS_SUCCESS;
        }

        std::lock_guard<std::mutex> lock(mStream->m_fileMutex);
        for (int i = 0, reserved = 0; i < mSuperBlock->s_zones && reserved < directBlocks; i++) {
            if (!getBlock(i)) {
                setBlock(i, true);
                ino->d2_zone[reserved++] = i;
                mStream->m_file.seekg(0x3000 + i / 8, std::ifstream::beg);
                mStream->m_file.write(reinterpret_cast<char*>(mBlockBitmap.data() + i / 8), 1);
            }
        }

        // Actual writing
        for (int i = 0; i < (NumberOfBytesToWrite + 0xFFF) / 0x1000; i++) {
            if (i < 7) {
                // Direct blocks
                mStream->m_file.seekg(0x1000 * ino->d2_zone[i], std::ifstream::beg);
                mStream->m_file.write(reinterpret_cast<const char *>(buffer) + i * 0x1000,
                                      min(NumberOfBytesToWrite, 0x1000));
            }
        }
        *NumberOfBytesWritten = NumberOfBytesToWrite;

        // Write inode
        ino->d2_size += NumberOfBytesToWrite;
        auto entry = mEntryMap.find(filename);
        mStream->m_file.seekg(0x4000 + 64 * (entry->second - 1), std::ifstream::beg);
        mStream->m_file.write(reinterpret_cast<const char *>(ino), 64);

        return STATUS_SUCCESS;

    }

    void MinixFS::loadEntry(ino_t inode, std::filesystem::path &path) {
        auto dir = mInodes->operator[](inode);
        auto dirSize = dir.d2_size;
        auto totalEntries = dirSize / 64;
        std::vector<V7Direct> entries(totalEntries);
        readInode(dir, entries.data(), dirSize, 0);

        for (int i = 0; i < totalEntries; i++) {
            auto entry = entries[i];
            std::cout << "New entry: ";
            std::cout << entry.d_name;
            std::cout << std::endl;

            auto thisPath = std::filesystem::path(path);
            thisPath /= path;
            if (strcmp(entry.d_name, ".") != 0) {
                thisPath /= entry.d_name;
            }
            mEntryMap.emplace(thisPath, entry.d_ino);

            auto childIno = mInodes->operator[](entry.d_ino);

            if ((childIno.d2_mode & I_TYPE) == I_DIRECTORY) {
                if (strcmp(entry.d_name, ".") == 0) continue;
                if (strcmp(entry.d_name, "..") == 0) continue;
                loadEntry(entry.d_ino, thisPath);
            }
        }
    }

    std::vector<DirEntry*> MinixFS::listFolder(const std::filesystem::path &path) const {
        std::vector<DirEntry*> ret;
        auto node = mEntryMap.find(path);

        if (node != mEntryMap.end()) {
            auto dir = mInodes->operator[](node->second);
            auto dirSize = dir.d2_size;
            auto totalEntries = dirSize / 64;
            std::vector<V7Direct> entries(totalEntries);
            readInode(dir, entries.data(), dirSize, 0);

            for (int i = 0; i < totalEntries; i++) {
                ret.push_back(new DirEntry(mInodes->operator[](entries[i].d_ino), entries[i]));
            }
        }
        return ret;
    }

    size_t MinixFS::getFreeSpace() const {
        auto totalSpace = mSuperBlock->s_zones * 0x1000;
        auto used = 0;
        for (int i = 0; i < mSuperBlock->s_zones; i++) {
            if (getBlock(i)) {
                used += 0x1000;
            }
        }
        return totalSpace - used;
    }

    size_t MinixFS::getTotalSpace() const {
        return mSuperBlock->s_zones * 0x1000;
    }

    bool MinixFS::getBlock(zone_t zone) const {
        int base = mSuperBlock->s_firstdatazone;
        if (zone < base) {
            return true;
        }
        zone -= base;
        int w = zone / 8;
        int s = zone % 8;

        return ((mBlockBitmap[w] >> s) & 1) != 0;
    }

    bool MinixFS::getInode(ino_t ino) const {
        int w = ino / 8;
        int s = ino % 8;
        return ((mInodeBitmap[w] >> s) & 1) != 0;
    }

    void MinixFS::setBlock(zone_t block, bool set) {
        int w = block / 8;
        int s = block % 8;
        int base = mSuperBlock->s_firstdatazone;
        if (w < base) {
            return;
        }
        block-= base;

        if (set) {
            mBlockBitmap[w] |= 1UL << s;
        } else {
            mBlockBitmap[w] &= ~(1UL << s);
        }
    }

    void MinixFS::setInode(ino_t ino, bool set) {
        int w = ino / 8;
        int s = ino % 8;
        if (set) {
            mInodeBitmap[w] |= 1UL << s;
        } else {
            mInodeBitmap[w] &= ~(1UL << s);
        }
    }
}
