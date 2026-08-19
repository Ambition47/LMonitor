#include "network/TcpConnection.h"

#include "log/Logger.h"
#include "protocol/FrameCodec.h"

#include <cerrno>
#include <exception>
#include <string>
#include <utility>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>


namespace {

constexpr std::size_t BUFFER_SIZE =
    4096;

}  // namespace


// ============================================================
// Constructor
// ============================================================

TcpConnection::TcpConnection(
    EventLoop& eventLoop,
    int fd,
    std::string clientName
)
    : eventLoop_(eventLoop),
      fd_(fd),
      clientName_(
          std::move(
              clientName
          )
      ),
      channel_(fd) {

    channel_.setEvents(
        EPOLLIN |
        EPOLLRDHUP
    );


    channel_.setReadCallback(
        [this]() {
            handleRead();
        }
    );


    channel_.setCloseCallback(
        [this]() {
            handleClose();
        }
    );


    channel_.setErrorCallback(
        [this]() {
            handleError();
        }
    );


    eventLoop_.addChannel(
        &channel_
    );
}


// ============================================================
// Destructor
// ============================================================

TcpConnection::~TcpConnection() {

    try {

        eventLoop_.removeChannel(
            &channel_
        );

    } catch (...) {
        // Destructor must not throw.
    }


    if (fd_ >= 0) {

        close(
            fd_
        );

        fd_ =
            -1;
    }
}


// ============================================================
// Accessors
// ============================================================

int TcpConnection::fd() const {

    return fd_;
}


const std::string&
TcpConnection::clientName() const {

    return clientName_;
}


// ============================================================
// Callback registration
// ============================================================

void TcpConnection::setMessageCallback(
    MessageCallback callback
) {

    messageCallback_ =
        std::move(
            callback
        );
}


void TcpConnection::setCloseCallback(
    CloseCallback callback
) {

    closeCallback_ =
        std::move(
            callback
        );
}


// ============================================================
// Read available socket data
// ============================================================

void TcpConnection::handleRead() {

    if (closing_) {
        return;
    }


    char buffer[
        BUFFER_SIZE
    ];


    while (true) {

        const ssize_t bytesReceived =
            recv(
                fd_,
                buffer,
                sizeof(buffer),
                0
            );


        if (bytesReceived > 0) {

            pendingData_.append(
                buffer,
                static_cast<std::size_t>(
                    bytesReceived
                )
            );


            processPendingData();

            continue;
        }


        if (bytesReceived == 0) {

            requestClose();

            return;
        }


        // ----------------------------------------------------
        // recv < 0
        // ----------------------------------------------------

        if (errno == EINTR) {
            continue;
        }


        if (errno == EAGAIN ||
            errno == EWOULDBLOCK) {

            return;
        }


        Logger::instance().error(
            "recv failed from " +
            clientName_ +
            ", fd=" +
            std::to_string(
                fd_
            )
        );


        requestClose();

        return;
    }
}


// ============================================================
// Channel close event
// ============================================================

void TcpConnection::handleClose() {

    requestClose();
}


// ============================================================
// Channel error event
// ============================================================

void TcpConnection::handleError() {

    if (closing_) {
        return;
    }


    Logger::instance().error(
        "Socket error from " +
        clientName_ +
        ", fd=" +
        std::to_string(
            fd_
        )
    );


    requestClose();
}


// ============================================================
// Parse complete length-prefixed frames
// ============================================================

void TcpConnection::processPendingData() {

    while (true) {

        std::string message;


        try {

            if (!FrameCodec::tryDecode(
                    pendingData_,
                    message
                )) {

                return;
            }

        } catch (
            const std::exception& e
        ) {

            Logger::instance().error(
                "Invalid frame from " +
                clientName_ +
                ": " +
                std::string(
                    e.what()
                )
            );


            requestClose();

            return;
        }


        if (messageCallback_) {

            messageCallback_(
                clientName_,
                message
            );
        }
    }
}


// ============================================================
// Deferred close request
// ============================================================

void TcpConnection::requestClose() {

    if (closing_) {
        return;
    }


    closing_ =
        true;


    if (closeCallback_) {

        closeCallback_(
            fd_
        );

    } else {

        Logger::instance().warning(
            "TcpConnection close requested without close callback: " +
            clientName_
        );
    }
}
