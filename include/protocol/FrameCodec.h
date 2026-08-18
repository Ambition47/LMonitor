#ifndef LMONITOR_FRAME_CODEC_H
#define LMONITOR_FRAME_CODEC_H

#include <cstddef>
#include <cstdint>
#include <string>


class FrameCodec {
public:
    // 4-byte unsigned length prefix.
    static constexpr std::size_t HEADER_SIZE =
        sizeof(uint32_t);


    // Prevent unreasonable memory allocation.
    static constexpr uint32_t MAX_FRAME_SIZE =
        1024U * 1024U;


    // Encode:
    //
    // [4-byte network-order length][payload]
    static std::string encode(
        const std::string& payload
    );


    // Try to decode one complete frame from buffer.
    //
    // return true:
    //     one complete frame was extracted
    //
    // return false:
    //     buffer does not contain a complete frame yet
    //
    // On success, consumed bytes are removed from buffer.
    static bool tryDecode(
        std::string& buffer,
        std::string& payload
    );
};

#endif
