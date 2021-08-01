//
// Created by Eduardo on 31/07/2021.
//

#include <iostream>
#include "MinixFS.h"

namespace minixfs {

    SetupState MinixFS::setup(const std::wstring &filename) {
        mStream = std::make_unique<Stream>();
        mStream->m_file.open(filename.c_str(),std::ifstream::binary | std::ifstream::in);

        if (!mStream->m_file.is_open()) {
            return SetupState::ErrorFile;
        }
        mSuperBlock = new SuperBlock(*mStream);

        if (!mSuperBlock->validate()) {
            return SetupState::ErrorFormat;
        }

        mInodes = new Inodes(*mStream, mSuperBlock->s_ninodes);

        loadEntries();

        return SetupState::Success;
    }

    const Inode *MinixFS::find(const std::wstring& filename) {
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
        std::vector<char> buf(inode.d2_size);

        for (int i = 0; i < V2_NR_TZONES && inode.d2_zone[i] != 0 && toRead != 0; i++) {
            if (i < V2_NR_DZONES) {
                // Direct block.
                toRead -= readBlock(inode.d2_zone[i], buf.data() + read, min(0x1000, size), offset);
                read += 0x1000;
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
        std::copy(buf.data() + offset, buf.data() + size, buffer);
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
        return 0;
    }
}
