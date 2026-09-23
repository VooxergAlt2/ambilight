#include "network/OtaUpdateService.h"

#include <Arduino.h>
#include <Update.h>
#include <WebServer.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>

#include <esp_random.h>

#include "network/OtaImageValidator.h"

namespace ambilight {
namespace {

void copyMessage(
    std::array<char, 128>& target,
    const char* message) {

    target.fill('\0');

    if (message == nullptr) {
        return;
    }

    std::strncpy(
        target.data(),
        message,
        target.size() - 1U);
}

} // namespace

OtaUpdateService::~OtaUpdateService() {
    stop();
}

bool OtaUpdateService::begin() {
    if (running_) {
        return true;
    }

    server_ =
        new (std::nothrow)
            WebServer(kPort);

    if (server_ == nullptr) {
        ++stats_.startFailures;
        return false;
    }

    server_->on(
        "/update",
        HTTP_OPTIONS,
        [this]() {
            handleOptions();
        });

    server_->on(
        "/update",
        HTTP_POST,
        [this]() {
            handleUploadComplete();
        },
        [this]() {
            handleUploadChunk();
        });

    server_->onNotFound(
        [this]() {
            addCorsHeaders();
            server_->send(
                404,
                "application/json",
                "{\"ok\":false,\"error\":\"not found\"}");
        });

    server_->begin();
    running_ = true;
    ++stats_.starts;
    return true;
}

void OtaUpdateService::stop() {
    if (updateStarted_) {
        Update.abort();
    }

    if (server_ != nullptr) {
        server_->stop();
        delete server_;
        server_ = nullptr;
    }

    running_ = false;
    disarm();
    resetUploadState();
}

void OtaUpdateService::tick(
    std::uint64_t nowUs) {

    if (!running_ ||
        server_ == nullptr) {

        return;
    }

    if (!inProgress_ &&
        armedUntilUs_ != 0 &&
        nowUs > armedUntilUs_) {

        disarm();
    }

    server_->handleClient();
}

bool OtaUpdateService::arm(
    std::uint64_t nowUs) {

    if (!running_ ||
        inProgress_ ||
        rebootPending_) {

        return false;
    }

    std::array<std::uint8_t, 16> random{};
    esp_fill_random(
        random.data(),
        random.size());

    static constexpr char kHex[] =
        "0123456789abcdef";

    for (std::size_t index = 0;
         index < random.size();
         ++index) {

        token_[index * 2U] =
            kHex[random[index] >> 4U];
        token_[index * 2U + 1U] =
            kHex[random[index] & 0x0FU];
    }

    token_.back() = '\0';
    armedUntilUs_ =
        nowUs +
        kArmDurationUs;
    lastSuccess_ = false;
    copyMessage(
        lastMessage_,
        "OTA armed. Upload firmware.bin within 120 seconds.");
    ++stats_.arms;
    return true;
}

void OtaUpdateService::disarm() {
    token_.fill('\0');
    armedUntilUs_ = 0;
}

bool OtaUpdateService::armed(
    std::uint64_t nowUs) const {

    return
        token_[0] != '\0' &&
        armedUntilUs_ != 0 &&
        nowUs <= armedUntilUs_ &&
        !inProgress_ &&
        !rebootPending_;
}

void OtaUpdateService::resetUploadState() {
    inProgress_ = false;
    uploadAuthorized_ = false;
    updateStarted_ = false;
    uploadFailed_ = false;
    responseStatus_ = 400;
    receivedBytes_ = 0;
    prefix_.fill(0);
    prefixBytes_ = 0;
}

void OtaUpdateService::failUpload(
    int httpStatus,
    const char* message,
    bool abortUpdate) {

    if (abortUpdate &&
        updateStarted_) {

        Update.abort();
        updateStarted_ = false;
    }

    uploadFailed_ = true;
    responseStatus_ =
        httpStatus;
    lastSuccess_ = false;
    copyMessage(
        lastMessage_,
        message);
}

bool OtaUpdateService::tokenMatchesRequest() const {
    if (server_ == nullptr ||
        token_[0] == '\0') {

        return false;
    }

    const String supplied =
        server_->arg("token");

    return
        supplied.length() ==
            std::strlen(token_.data()) &&
        std::strcmp(
            supplied.c_str(),
            token_.data()) == 0;
}

void OtaUpdateService::addCorsHeaders() {
    if (server_ == nullptr) {
        return;
    }

    server_->sendHeader(
        "Access-Control-Allow-Origin",
        "*");
    server_->sendHeader(
        "Access-Control-Allow-Methods",
        "POST, OPTIONS");
    server_->sendHeader(
        "Access-Control-Allow-Headers",
        "Content-Type");
    server_->sendHeader(
        "Cache-Control",
        "no-store");
}

void OtaUpdateService::handleOptions() {
    addCorsHeaders();
    server_->send(
        204,
        "text/plain",
        "");
}

bool OtaUpdateService::validateAndStartUpdate() {
    if (!OtaImageValidator::
            validApplicationPrefix(
                prefix_.data(),
                prefixBytes_)) {

        failUpload(
            400,
            "Invalid ESP32-C6 application image. Upload firmware.bin, not firmware.factory.bin.",
            false);
        ++stats_.uploadsRejected;
        return false;
    }

    if (!Update.begin(
            UPDATE_SIZE_UNKNOWN,
            U_FLASH)) {

        failUpload(
            500,
            Update.errorString(),
            false);
        ++stats_.writeFailures;
        return false;
    }

    updateStarted_ = true;

    if (!writeBytes(
            prefix_.data(),
            prefixBytes_)) {

        return false;
    }

    return true;
}

bool OtaUpdateService::writeBytes(
    std::uint8_t* data,
    std::size_t length) {

    if (length == 0) {
        return true;
    }

    if (!updateStarted_ ||
        data == nullptr ||
        Update.write(
            data,
            length) != length) {

        failUpload(
            500,
            Update.errorString());
        ++stats_.writeFailures;
        return false;
    }

    return true;
}

void OtaUpdateService::handleUploadChunk() {
    if (server_ == nullptr) {
        return;
    }

    HTTPUpload& upload =
        server_->upload();

    switch (upload.status) {
    case UPLOAD_FILE_START: {
        resetUploadState();

        const std::uint64_t nowUs =
            static_cast<std::uint64_t>(
                esp_timer_get_time());

        // Authorize before setting inProgress_: armed() deliberately returns
        // false once an upload owns the service.
        uploadAuthorized_ =
            armed(nowUs) &&
            tokenMatchesRequest();

        if (!uploadAuthorized_) {
            failUpload(
                403,
                "OTA is not armed or the one-time token is invalid.",
                false);
            ++stats_.uploadsRejected;
            return;
        }

        inProgress_ = true;

        // Consume the token at upload start. A retry needs an explicit re-arm.
        disarm();
        ++stats_.uploadsStarted;
        copyMessage(
            lastMessage_,
            "OTA upload in progress.");
        break;
    }

    case UPLOAD_FILE_WRITE: {
        if (!uploadAuthorized_ ||
            uploadFailed_) {

            return;
        }

        receivedBytes_ +=
            upload.currentSize;

        std::size_t offset = 0;

        if (!updateStarted_) {
            const std::size_t needed =
                prefix_.size() -
                prefixBytes_;

            const std::size_t copied =
                std::min(
                    needed,
                    upload.currentSize);

            if (copied != 0) {
                std::memcpy(
                    prefix_.data() +
                        prefixBytes_,
                    upload.buf,
                    copied);

                prefixBytes_ += copied;
                offset += copied;
            }

            if (prefixBytes_ ==
                prefix_.size()) {

                if (!validateAndStartUpdate()) {
                    return;
                }
            }
        }

        if (updateStarted_ &&
            offset < upload.currentSize) {

            writeBytes(
                upload.buf + offset,
                upload.currentSize - offset);
        }

        break;
    }

    case UPLOAD_FILE_END:
        if (!uploadAuthorized_ ||
            uploadFailed_) {

            inProgress_ = false;
            return;
        }

        if (!updateStarted_) {
            failUpload(
                400,
                "Firmware image is too short.",
                false);
            ++stats_.uploadsRejected;
            inProgress_ = false;
            return;
        }

        if (!Update.end(true)) {
            failUpload(
                500,
                Update.errorString(),
                false);
            ++stats_.writeFailures;
            inProgress_ = false;
            updateStarted_ = false;
            return;
        }

        updateStarted_ = false;
        inProgress_ = false;
        uploadFailed_ = false;
        responseStatus_ = 200;
        lastSuccess_ = true;
        rebootPending_ = true;
        successAtUs_ =
            static_cast<std::uint64_t>(
                esp_timer_get_time());
        copyMessage(
            lastMessage_,
            "Firmware accepted. Rebooting into the new OTA slot.");
        ++stats_.uploadsCompleted;
        break;

    case UPLOAD_FILE_ABORTED:
        failUpload(
            400,
            "OTA upload aborted.");
        ++stats_.uploadsRejected;
        inProgress_ = false;
        break;
    }
}

void OtaUpdateService::handleUploadComplete() {
    if (server_ == nullptr) {
        return;
    }

    if (inProgress_) {
        failUpload(
            400,
            "OTA upload ended before the firmware image was finalized.");
        inProgress_ = false;
    }

    addCorsHeaders();

    char response[256] = {};
    std::snprintf(
        response,
        sizeof(response),
        "{\"ok\":%s,\"bytes\":%lu,\"message\":\"%s\"}",
        lastSuccess_
            ? "true"
            : "false",
        static_cast<unsigned long>(
            receivedBytes_),
        lastMessage_.data());

    server_->send(
        responseStatus_,
        "application/json",
        response);
}

} // namespace ambilight
