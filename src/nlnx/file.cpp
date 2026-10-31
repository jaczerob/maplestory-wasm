//////////////////////////////////////////////////////////////////////////////
// NoLifeNx - Part of the NoLifeStory project                               //
// Copyright © 2013 Peter Atashian                                          //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////

#include "file_impl.hpp"
#include "node_impl.hpp"

// LazyFS integration for WASM
#ifdef MS_PLATFORM_WASM
#include <LazyFS/LazyFileLoader.h>
#include <LazyFS/LazyFS.h>
#endif

#ifdef _WIN32
#  ifdef _MSC_VER
#    include <codecvt>
#  else
#    include <clocale>
#    include <cstdlib>
#  endif
#  ifdef __MINGW32__
#    include <windows.h>
#  else
#    include <Windows.h>
#  endif // __MINGW32__
#else
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <unistd.h>
#endif
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <limits>
#include <cerrno>

namespace nl {
    namespace {
#ifndef MS_PLATFORM_WASM
        std::filesystem::path metadata_path(const std::string& name) {
            std::filesystem::path path(name);
            if (path.extension() != ".nx") return path;
            auto stem = path.stem().string();
            for (const auto& candidate : {path.parent_path() / "shared" / (stem + ".meta"),
                                         path.parent_path() / "assets" / "shared" / (stem + ".meta")})
                if (std::filesystem::is_regular_file(candidate)) return candidate;
            return path.parent_path() / "shared" / (stem + ".meta");
        }

        std::filesystem::path media_path(const std::filesystem::path& metadata) {
            return metadata.parent_path().parent_path() / "client" / (metadata.stem().string() + ".media");
        }
#endif
    }
    file::file(std::string name) {
        open(name);
    }
    file::~file() {
        close();
    }
    file::file(file &&other) : m_data(other.m_data) {
        other.m_data = nullptr;
    }
    file & file::operator=(file &&other) {
        if (this != &other) {
            close();
            m_data = other.m_data;
            other.m_data = nullptr;
        }
        return *this;
    }
    bool file::exists(std::string const & name) {
#ifdef MS_PLATFORM_WASM
        return true;
#else
        return std::filesystem::is_regular_file(metadata_path(name));
#endif
    }
    void file::open(std::string name) {
        close();
        m_data = new data();
        try {
#ifdef _WIN32
        const auto meta_path = metadata_path(name);
#  ifdef _MSC_VER
        std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
        auto str = convert.from_bytes(meta_path.string());
#  else
        std::setlocale(LC_ALL, "en_US.utf8");
        auto str = std::wstring(meta_path.string().size(), 0);
        auto len = std::mbstowcs(const_cast<wchar_t *>(str.c_str()), meta_path.string().c_str(), str.size());
        str.resize(len);
#  endif
#  if WINAPI_FAMILY == WINAPI_FAMILY_APP
        m_data->file_handle = ::CreateFile2(str.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
#  else
        m_data->file_handle = ::CreateFileW(str.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, nullptr);
#  endif
        if (m_data->file_handle == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Failed to open metadata for " + name);
        LARGE_INTEGER file_size;
        if (!::GetFileSizeEx(m_data->file_handle, &file_size) || file_size.QuadPart < 0)
            throw std::runtime_error("Failed to get metadata size for " + name);
        m_data->metadata_size = static_cast<size_t>(file_size.QuadPart);
#  if WINAPI_FAMILY == WINAPI_FAMILY_APP
        m_data->map = ::CreateFileMappingFromApp(m_data->file_handle, 0, PAGE_READONLY, 0, nullptr);
#  else
        m_data->map = ::CreateFileMappingW(m_data->file_handle, 0, PAGE_READONLY, 0, 0, nullptr);
#  endif
        if (!m_data->map)
            throw std::runtime_error("Failed to create file mapping of file " + name);
#  if WINAPI_FAMILY == WINAPI_FAMILY_APP
        m_data->base = ::MapViewOfFileFromApp(m_data->map, FILE_MAP_READ, 0, 0);
#  else
        m_data->base = ::MapViewOfFile(m_data->map, FILE_MAP_READ, 0, 0, 0);
#  endif
        if (!m_data->base)
            throw std::runtime_error("Failed to map view of file " + name);
        const auto payload_path = media_path(meta_path);
#  ifdef _MSC_VER
        auto media_str = convert.from_bytes(payload_path.string());
#  else
        auto media_str = std::wstring(payload_path.string().size(), 0);
        auto media_len = std::mbstowcs(const_cast<wchar_t *>(media_str.c_str()), payload_path.string().c_str(), media_str.size());
        media_str.resize(media_len);
#  endif
#  if WINAPI_FAMILY == WINAPI_FAMILY_APP
        m_data->media_handle = ::CreateFile2(media_str.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
#  else
        m_data->media_handle = ::CreateFileW(media_str.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, nullptr);
#  endif
        if (m_data->media_handle != INVALID_HANDLE_VALUE) {
            if (!::GetFileSizeEx(m_data->media_handle, &file_size) || file_size.QuadPart < 0)
                throw std::runtime_error("Failed to get media size for " + name);
            m_data->media_size = static_cast<size_t>(file_size.QuadPart);
#  if WINAPI_FAMILY == WINAPI_FAMILY_APP
            m_data->media_map = ::CreateFileMappingFromApp(m_data->media_handle, 0, PAGE_READONLY, 0, nullptr);
            if (m_data->media_map && m_data->media_size)
                m_data->media_base = ::MapViewOfFileFromApp(m_data->media_map, FILE_MAP_READ, 0, 0, m_data->media_size);
#  else
            m_data->media_map = ::CreateFileMappingW(m_data->media_handle, 0, PAGE_READONLY, 0, 0, nullptr);
            if (m_data->media_map && m_data->media_size)
                m_data->media_base = ::MapViewOfFile(m_data->media_map, FILE_MAP_READ, 0, 0, 0);
#  endif
            if (m_data->media_size && !m_data->media_base)
                throw std::runtime_error("Failed to map media for " + name);
        } else if (::GetLastError() != ERROR_FILE_NOT_FOUND && ::GetLastError() != ERROR_PATH_NOT_FOUND) {
            throw std::runtime_error("Failed to open media for " + name);
        }
#else
#ifdef MS_PLATFORM_WASM
        // WASM: Use LazyFS instead of mmap
        
        // Ensure the file is registered with LazyFS before loading
        LazyFS::RegisterFile(name, "/assets/" + name);

        size_t file_size = 0;
        m_data->base = LazyFS::LazyFileLoader::load_file(name, file_size);
        if (!m_data->base || file_size == 0)
            throw std::runtime_error("Failed to load file via LazyFS: " + name);
        m_data->size = file_size;
        m_data->file_handle = -1; // No file handle for LazyFS
#else
        const auto meta_path = metadata_path(name);
        m_data->file_handle = ::open(meta_path.string().c_str(), O_RDONLY);
        if (m_data->file_handle == -1)
            throw std::runtime_error("Failed to open metadata for " + name);
        struct stat finfo;
        if (::fstat(m_data->file_handle, &finfo) == -1)
            throw std::runtime_error("Failed to obtain metadata information for " + name);
        m_data->metadata_size = static_cast<size_t>(finfo.st_size);
        m_data->base = ::mmap(nullptr, m_data->metadata_size, PROT_READ, MAP_SHARED, m_data->file_handle, 0);
        if (reinterpret_cast<intptr_t>(m_data->base) == -1)
            throw std::runtime_error("Failed to map metadata for " + name);
        const auto payload_path = media_path(meta_path);
        m_data->media_handle = ::open(payload_path.string().c_str(), O_RDONLY);
        if (m_data->media_handle == -1 && errno != ENOENT)
            throw std::runtime_error("Failed to open media for " + name);
        if (m_data->media_handle != -1) {
            if (::fstat(m_data->media_handle, &finfo) == -1)
                throw std::runtime_error("Failed to obtain media information for " + name);
            m_data->media_size = static_cast<size_t>(finfo.st_size);
            if (m_data->media_size) {
                m_data->media_base = ::mmap(nullptr, m_data->media_size, PROT_READ, MAP_SHARED, m_data->media_handle, 0);
                if (m_data->media_base == MAP_FAILED)
                    throw std::runtime_error("Failed to map media for " + name);
            }
        }
#endif
#endif

        // Initialize tables
#ifdef MS_PLATFORM_WASM
        // WASM: Store offsets directly
        m_data->header = 0; // Header is at start of file
        
        // We need to read the header to get offsets
        // LazyFileLoader handles this read
        auto loader = static_cast<LazyFS::LazyFileLoader*>(const_cast<void*>(m_data->base));
        auto header_ptr = loader->get_data_at<header>(0);
        if (!header_ptr) throw std::runtime_error("Failed to read header for " + name);
        
        if (header_ptr->magic != 0x34474B50) {
            std::cout << "[NoLifeNx] Invalid Magic for " << name << ": 0x" << std::hex << header_ptr->magic << std::dec << std::endl;
            // Print first few chars to check for HTML/Text
            std::cout << "[NoLifeNx] First 4 chars: " << 
                (char)(header_ptr->magic & 0xFF) << 
                (char)((header_ptr->magic >> 8) & 0xFF) << 
                (char)((header_ptr->magic >> 16) & 0xFF) << 
                (char)((header_ptr->magic >> 24) & 0xFF) << std::endl;
            throw std::runtime_error(name + " is not a PKG4 NX file");
        }
            
        m_data->node_table = header_ptr->node_offset;
        m_data->string_table = header_ptr->string_offset;
        m_data->bitmap_table = header_ptr->bitmap_offset;
        m_data->audio_table = header_ptr->audio_offset;
        
        // Debug: check if struct packing is correct
        auto raw_bytes = static_cast<const uint8_t*>(static_cast<const void*>(header_ptr));
        uint32_t raw_bitmap_count = 0;
        memcpy(&raw_bitmap_count, raw_bytes + 28, sizeof(uint32_t));
        uint64_t raw_bitmap_offset = 0;
        memcpy(&raw_bitmap_offset, raw_bytes + 32, sizeof(uint64_t));
        
        std::cout << "[file::open] " << name
                  << " sizeof(header)=" << sizeof(header)
                  << " struct_bitmap_count=" << header_ptr->bitmap_count
                  << " raw_bitmap_count@28=" << raw_bitmap_count
                  << " struct_bitmap_offset=" << header_ptr->bitmap_offset
                  << " raw_bitmap_offset@32=" << raw_bitmap_offset
                  << std::endl;
#else
        if (m_data->metadata_size < sizeof(header))
            throw std::runtime_error("Truncated NX metadata header: " + name);
        m_data->header = reinterpret_cast<header const *>(m_data->base);
        if (m_data->header->magic != 0x34474B50)
            throw std::runtime_error(name + " is not a PKG4 NX file");
        const auto table_fits = [this](uint64_t offset, uint64_t count, size_t element) {
            return offset <= m_data->metadata_size && count <= (m_data->metadata_size - offset) / element;
        };
        if (!table_fits(m_data->header->node_offset, m_data->header->node_count, sizeof(node::data)) ||
            !table_fits(m_data->header->string_offset, m_data->header->string_count, sizeof(uint64_t)) ||
            !table_fits(m_data->header->bitmap_offset, m_data->header->bitmap_count, sizeof(uint64_t)) ||
            !table_fits(m_data->header->audio_offset, m_data->header->audio_count, sizeof(uint64_t)))
            throw std::runtime_error("NX metadata table outside metadata component: " + name);
        if ((m_data->header->bitmap_count || m_data->header->audio_count) && !m_data->media_base)
            throw std::runtime_error("NX payload table has no media component: " + name);
        m_data->node_table = reinterpret_cast<node::data const *>(reinterpret_cast<char const *>(m_data->base) + m_data->header->node_offset);
        m_data->string_table = reinterpret_cast<uint64_t const *>(reinterpret_cast<char const *>(m_data->base) + m_data->header->string_offset);
        m_data->bitmap_table = reinterpret_cast<uint64_t const *>(reinterpret_cast<char const *>(m_data->base) + m_data->header->bitmap_offset);
        m_data->audio_table = reinterpret_cast<uint64_t const *>(reinterpret_cast<char const *>(m_data->base) + m_data->header->audio_offset);
        const uint64_t logical_size = m_data->metadata_size + m_data->media_size;
        for (uint32_t i = 0; i < m_data->header->bitmap_count; ++i)
            if (m_data->bitmap_table[i] < m_data->metadata_size || m_data->bitmap_table[i] >= logical_size)
                throw std::runtime_error("NX bitmap offset outside media component: " + name);
        for (uint32_t i = 0; i < m_data->header->audio_count; ++i)
            if (m_data->audio_table[i] < m_data->metadata_size || m_data->audio_table[i] >= logical_size)
                throw std::runtime_error("NX audio offset outside media component: " + name);
        for (uint32_t i = 0; i < m_data->header->node_count; ++i) {
            const auto& item = m_data->node_table[i];
            if (item.type == node::type::audio &&
                (item.audio.index >= m_data->header->audio_count ||
                 item.audio.length > logical_size - m_data->audio_table[item.audio.index]))
                throw std::runtime_error("NX audio payload outside media component: " + name);
        }
#endif
        } catch (...) {
            close();
            throw;
        }
    }
    void file::close() {
        if (!m_data) return;
#ifdef _WIN32
        ::UnmapViewOfFile(m_data->base);
        ::CloseHandle(m_data->map);
        ::CloseHandle(m_data->file_handle);
        if (m_data->media_base) ::UnmapViewOfFile(m_data->media_base);
        if (m_data->media_map) ::CloseHandle(m_data->media_map);
        if (m_data->media_handle && m_data->media_handle != INVALID_HANDLE_VALUE) ::CloseHandle(m_data->media_handle);
#else
#ifdef MS_PLATFORM_WASM
        // WASM: Free LazyFS-loaded file
        if (m_data->base) {
            LazyFS::LazyFileLoader::free_file(const_cast<void *>(m_data->base));
        }
#else
        if (m_data->base && m_data->metadata_size)
            ::munmap(const_cast<void *>(m_data->base), m_data->metadata_size);
        if (m_data->media_base && m_data->media_size)
            ::munmap(const_cast<void *>(m_data->media_base), m_data->media_size);
        if (m_data->file_handle != -1)
            ::close(m_data->file_handle);
        if (m_data->media_handle != -1)
            ::close(m_data->media_handle);
#endif
#endif
        delete m_data;
        m_data = nullptr;
    }
    node file::root() const {
#ifdef MS_PLATFORM_WASM
        // Root node is at node_table offset
        return {reinterpret_cast<node::data const*>(static_cast<uintptr_t>(m_data->node_table)), m_data};
#else
        return {m_data->node_table, m_data};
#endif
    }
    file::operator node() const {
        return root();
    }
    uint32_t file::string_count() const {
#ifdef MS_PLATFORM_WASM
        auto loader = static_cast<LazyFS::LazyFileLoader*>(const_cast<void*>(m_data->base));
        auto h = loader->get_data_at<header>(m_data->header);
        return h ? h->string_count : 0;
#else
        return m_data->header->string_count;
#endif
    }
    uint32_t file::bitmap_count() const {
#ifdef MS_PLATFORM_WASM
        auto loader = static_cast<LazyFS::LazyFileLoader*>(const_cast<void*>(m_data->base));
        auto h = loader->get_data_at<header>(m_data->header);
        return h ? h->bitmap_count : 0;
#else
        return m_data->header->bitmap_count;
#endif
    }
    uint32_t file::audio_count() const {
#ifdef MS_PLATFORM_WASM
        auto loader = static_cast<LazyFS::LazyFileLoader*>(const_cast<void*>(m_data->base));
        auto h = loader->get_data_at<header>(m_data->header);
        return h ? h->audio_count : 0;
#else
        return m_data->header->audio_count;
#endif
    }
    uint32_t file::node_count() const {
#ifdef MS_PLATFORM_WASM
        auto loader = static_cast<LazyFS::LazyFileLoader*>(const_cast<void*>(m_data->base));
        auto h = loader->get_data_at<header>(m_data->header);
        return h ? h->node_count : 0;
#else
        return m_data->header->node_count;
#endif
    }
    std::string file::get_string(uint32_t i) const {
#ifdef MS_PLATFORM_WASM
        auto loader = static_cast<LazyFS::LazyFileLoader*>(const_cast<void*>(m_data->base));
        
        // Resolve string table entry
        size_t table_entry_offset = m_data->string_table + i * sizeof(uint64_t);
        auto offset_ptr = loader->get_data_at<uint64_t>(table_entry_offset);
        if (!offset_ptr) return {};
        uint64_t string_offset = *offset_ptr;
        
        // Read string length and content (similar logic to node.cpp)
        auto len_ptr = loader->get_data_at<uint16_t>(string_offset);
        if (!len_ptr) return {};
        uint16_t len = *len_ptr;
        
        auto char_ptr = static_cast<const char*>(loader->get_contiguous_data(string_offset + 2, len));
        if (!char_ptr) return {};
        
        return {char_ptr, len};
#else
        const auto offset = m_data->string_table[i];
        if (offset > m_data->metadata_size || m_data->metadata_size - offset < sizeof(uint16_t)) return {};
        auto const s = reinterpret_cast<char const *>(m_data->base) + offset;
        const auto length = *reinterpret_cast<uint16_t const *>(s);
        if (length > m_data->metadata_size - offset - sizeof(uint16_t)) return {};
        return {s + sizeof(uint16_t), length};
#endif
    }

    bool io_read(const _file_data *fd, void *buf, size_t size, uint64_t offset) {
        if (!fd || !buf || size == 0)
            return false;
#ifdef MS_PLATFORM_WASM
        // Use LazyFS to fetch the data
        auto loader = static_cast<LazyFS::LazyFileLoader*>(const_cast<void*>(fd->base));
        if (!loader)
            return false;
        const void* data = loader->get_contiguous_data(offset, size);
        if (!data)
            return false;
        memcpy(buf, data, size);
        return true;
#else
        if (offset > fd->metadata_size + fd->media_size || size > fd->metadata_size + fd->media_size - offset)
            return false;
        auto *destination = static_cast<char *>(buf);
        size_t remaining = size;
        if (offset < fd->metadata_size) {
            const size_t count = std::min(remaining, fd->metadata_size - static_cast<size_t>(offset));
            if (pread(fd->file_handle, destination, count, static_cast<off_t>(offset)) != static_cast<ssize_t>(count))
                return false;
            destination += count;
            remaining -= count;
            offset += count;
        }
        if (remaining && (fd->media_handle == -1 ||
            pread(fd->media_handle, destination, remaining, static_cast<off_t>(offset - fd->metadata_size)) != static_cast<ssize_t>(remaining)))
            return false;
        return true;
#endif
    }
}
