#include "transport/UsbAwaService.h"

#include <cstring>

#include <driver/usb_serial_jtag.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>

namespace ambilight {
namespace {

constexpr char kHandshakeReply[] =
    "\r\nWelcome!\r\nAwa driver 11.";

constexpr std::uint32_t kUsbRxRingBytes = 16384;
constexpr std::uint32_t kUsbTxRingBytes = 2048;

} // namespace

UsbAwaService::~UsbAwaService() {
    stop();
}

bool UsbAwaService::begin() {
    if (ownsDriver_) {
        return true;
    }

    // If another subsystem already installed this driver, sharing ownership
    // would make reads/writes and uninstall semantics ambiguous.
    if (usb_serial_jtag_is_driver_installed()) {
        ++stats_.driverInstallErrors;
        return false;
    }

    usb_serial_jtag_driver_config_t config{};
    config.rx_buffer_size = kUsbRxRingBytes;
    config.tx_buffer_size = kUsbTxRingBytes;
    config.intr_priority = 0;

    const esp_err_t result =
        usb_serial_jtag_driver_install(&config);

    if (result != ESP_OK) {
        ++stats_.driverInstallErrors;
        return false;
    }

    ownsDriver_ = true;
    parser_.reset();
    return true;
}

void UsbAwaService::stop() {
    if (!ownsDriver_) {
        return;
    }

    usb_serial_jtag_driver_uninstall();
    ownsDriver_ = false;
}

bool UsbAwaService::sendHandshakeReply() {
    const std::size_t length =
        sizeof(kHandshakeReply) - 1;

    const int written = usb_serial_jtag_write_bytes(
        kHandshakeReply,
        length,
        pdMS_TO_TICKS(20));

    if (written == static_cast<int>(length)) {
        ++stats_.handshakeReplies;
        return true;
    }

    ++stats_.handshakeReplyFailures;
    return false;
}

UsbAwaPollResult UsbAwaService::poll() {
    UsbAwaPollResult result;

    if (!ownsDriver_) {
        return result;
    }

    const std::uint64_t pollStartedUs =
        static_cast<std::uint64_t>(esp_timer_get_time());

    bool haveCompletedFrame = false;
    std::size_t reads = 0;

    while (reads < kMaxReadsPerPoll) {
        const std::uint64_t beforeReadUs =
            static_cast<std::uint64_t>(esp_timer_get_time());

        if (reads > 0 &&
            beforeReadUs - pollStartedUs >= kPollBudgetUs) {
            result.backlogLikely = true;
            ++stats_.pollBudgetExhaustions;
            break;
        }

        const int received = usb_serial_jtag_read_bytes(
            rxChunk_.data(),
            rxChunk_.size(),
            0);

        if (received <= 0) {
            result.inputDrained = true;
            break;
        }

        ++reads;
        result.bytes += static_cast<std::uint32_t>(received);
        stats_.bytesReceived +=
            static_cast<std::uint64_t>(received);

        for (int index = 0; index < received; ++index) {
            const AwaEvent event = parser_.consume(
                rxChunk_[static_cast<std::size_t>(index)],
                completedFrame_);

            switch (event) {
            case AwaEvent::FrameComplete:
                completedFrame_.receivedUs =
                    static_cast<std::uint64_t>(
                        esp_timer_get_time());

                ++result.completeFrames;
                ++stats_.completeFramesAssembled;
                haveCompletedFrame = true;
                break;

            case AwaEvent::HandshakeRequest:
                sendHandshakeReply();
                break;

            case AwaEvent::SleepRequest:
                ++stats_.sleepRequests;
                break;

            case AwaEvent::None:
                break;
            }
        }
    }

    if (reads == kMaxReadsPerPoll &&
        !result.inputDrained) {
        result.backlogLikely = true;
        ++stats_.readLimitHits;
    }

    if (haveCompletedFrame) {
        if (mailbox_.publish(completedFrame_)) {
            result.mailboxPublished = true;
            ++stats_.mailboxPublications;

            if (result.completeFrames > 1) {
                stats_.collapsedCompleteFrames +=
                    result.completeFrames - 1;
            }

            lastFrameUs_ = completedFrame_.receivedUs;
        }
    }

    const std::uint64_t pollFinishedUs =
        static_cast<std::uint64_t>(esp_timer_get_time());

    result.elapsedUs = static_cast<std::uint32_t>(
        pollFinishedUs - pollStartedUs);

    if (result.bytes > stats_.maxBytesPerPoll) {
        stats_.maxBytesPerPoll = result.bytes;
    }

    if (result.elapsedUs > stats_.maxPollUs) {
        stats_.maxPollUs = result.elapsedUs;
    }

    return result;
}

} // namespace ambilight
