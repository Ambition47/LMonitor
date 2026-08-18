#include "protocol/FrameCodec.h"

#include <cstring>
#include <stdexcept>

#include <arpa/inet.h>


std::string FrameCodec::encode(
    const std::string& payload
) {
    if (payload.size() >
        MAX_FRAME_SIZE) {

        throw std::length_error(
            "Frame payload exceeds maximum size"
        );
    }


    const uint32_t payloadLength =
        static_cast<uint32_t>(
            payload.size()
        );


    const uint32_t networkLength =
        htonl(
            payloadLength
        );


    std::string frame;

    frame.resize(
        HEADER_SIZE +
        payload.size()
    );


    // Copy 4-byte length header.
    std::memcpy(
        frame.data(),
        &networkLength,
        HEADER_SIZE
    );


    // Copy payload immediately after header.
    if (!payload.empty()) {

        std::memcpy(
            frame.data() +
                HEADER_SIZE,
            payload.data(),
            payload.size()
        );
    }


    return frame;
}


bool FrameCodec::tryDecode(
    std::string& buffer,
    std::string& payload
) {
    // --------------------------------------------------------
    // Step 1:
    // Header itself may be split across multiple TCP packets.
    // --------------------------------------------------------

    if (buffer.size() <
        HEADER_SIZE) {

        return false;
    }


    uint32_t networkLength = 0;


    std::memcpy(
        &networkLength,
        buffer.data(),
        HEADER_SIZE
    );


    const uint32_t payloadLength =
        ntohl(
            networkLength
        );


    // --------------------------------------------------------
    // Step 2:
    // Validate length before allocating/extracting.
    // --------------------------------------------------------

    if (payloadLength >
        MAX_FRAME_SIZE) {

        throw std::runtime_error(
            "Received frame exceeds maximum size"
        );
    }


    const std::size_t completeFrameSize =
        HEADER_SIZE +
        static_cast<std::size_t>(
            payloadLength
        );


    // --------------------------------------------------------
    // Step 3:
    // Body may not have arrived completely yet.
    // --------------------------------------------------------

    if (buffer.size() <
        completeFrameSize) {

        return false;
    }


    // --------------------------------------------------------
    // Step 4:
    // Extract exactly one payload.
    // --------------------------------------------------------

    payload.assign(
        buffer.data() +
            HEADER_SIZE,
        static_cast<std::size_t>(
            payloadLength
        )
    );


    // --------------------------------------------------------
    // Step 5:
    // Remove consumed frame.
    //
    // Remaining bytes may already contain the next frame.
    // --------------------------------------------------------

    buffer.erase(
        0,
        completeFrameSize
    );


    return true;
}
