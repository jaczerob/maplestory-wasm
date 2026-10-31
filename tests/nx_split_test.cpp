#include "doctest/doctest.h"
#include "nlnx/audio.hpp"
#include "nlnx/bitmap.hpp"
#include "nlnx/file.hpp"
#include "nlnx/file_impl.hpp"
#include "nlnx/node.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>
#include <vector>

namespace
{
    using Bytes = std::vector<uint8_t>;

    template <typename T>
    void put(Bytes& bytes, size_t offset, T value)
    {
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }

    struct Fixture
    {
        std::filesystem::path root = std::filesystem::temp_directory_path() /
            ("kitty-nx-split-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::path metadata = root / "shared" / "Map.meta";

        Fixture()
        {
            std::filesystem::create_directories(root / "shared");
            std::filesystem::create_directories(root / "client");
            Bytes bytes(159);
            put<uint32_t>(bytes, 0, 0x34474B50);
            put<uint32_t>(bytes, 4, 3);
            put<uint64_t>(bytes, 8, 52);
            put<uint32_t>(bytes, 16, 2);
            put<uint64_t>(bytes, 20, 112);
            put<uint32_t>(bytes, 28, 1);
            put<uint64_t>(bytes, 32, 143);
            put<uint32_t>(bytes, 40, 1);
            put<uint64_t>(bytes, 44, 151);

            put<uint32_t>(bytes, 52, 0);
            put<uint32_t>(bytes, 56, 1);
            put<uint16_t>(bytes, 60, 2);
            put<uint16_t>(bytes, 62, 0);
            put<uint32_t>(bytes, 72, 1);
            put<uint32_t>(bytes, 76, 0);
            put<uint16_t>(bytes, 80, 0);
            put<uint16_t>(bytes, 82, 6);
            put<uint32_t>(bytes, 84, 0);
            put<uint32_t>(bytes, 88, 3);
            put<uint32_t>(bytes, 92, 0);
            put<uint32_t>(bytes, 96, 0);
            put<uint16_t>(bytes, 100, 0);
            put<uint16_t>(bytes, 102, 5);
            put<uint32_t>(bytes, 104, 0);
            put<uint16_t>(bytes, 108, 1);
            put<uint16_t>(bytes, 110, 1);

            put<uint64_t>(bytes, 112, 128);
            put<uint64_t>(bytes, 120, 136);
            put<uint16_t>(bytes, 128, 6);
            std::memcpy(bytes.data() + 130, "Bitmap", 6);
            put<uint16_t>(bytes, 136, 5);
            std::memcpy(bytes.data() + 138, "Audio", 5);
            put<uint64_t>(bytes, 143, 159);
            put<uint64_t>(bytes, 151, 159);

            std::ofstream(metadata, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            std::ofstream(root / "client" / "Map.media", std::ios::binary).write("abc", 3);
        }

        ~Fixture() { std::filesystem::remove_all(root); }
    };
}

TEST_CASE("NX native reader maps split bitmap and audio payloads")
{
    Fixture fixture;
    CHECK(nl::file::exists((fixture.root / "Map.nx").string()));
    CHECK_FALSE(nl::file::exists((fixture.root / "Missing.nx").string()));
    nl::file file(fixture.metadata.string());
    auto root = file.root();
    auto bitmap = root["Bitmap"].get_bitmap();
    auto audio = root["Audio"].get_audio();
    REQUIRE(bitmap);
    CHECK(bitmap.id() == 159);
    REQUIRE(audio);
    REQUIRE(audio.length() == 3);
    CHECK(std::memcmp(audio.data(), "abc", 3) == 0);

    nl::_file_data data;
    data.file_handle = ::open(fixture.metadata.string().c_str(), O_RDONLY);
    data.media_handle = ::open((fixture.root / "client" / "Map.media").string().c_str(), O_RDONLY);
    data.metadata_size = 159;
    data.media_size = 3;
    char across_boundary[5]{};
    REQUIRE(nl::io_read(&data, across_boundary, sizeof(across_boundary), 157));
    CHECK(std::memcmp(across_boundary, "\0\0abc", 5) == 0);
    CHECK_FALSE(nl::io_read(&data, across_boundary, 6, 157));
    ::close(data.file_handle);
    ::close(data.media_handle);
    CHECK(std::memcmp(audio.data(), "abc", 3) == 0);
}

TEST_CASE("NX native reader rejects truncated split metadata and missing media")
{
    Fixture fixture;
    std::filesystem::resize_file(fixture.metadata, 52);
    CHECK_THROWS_AS(nl::file(fixture.metadata.string()), std::runtime_error);

    Fixture no_media;
    std::filesystem::remove(no_media.root / "client" / "Map.media");
    CHECK_THROWS_AS(nl::file(no_media.metadata.string()), std::runtime_error);
}
