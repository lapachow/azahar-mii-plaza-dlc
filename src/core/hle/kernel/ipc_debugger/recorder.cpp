// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/ipc_debugger/recorder.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/server_port.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/session.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/service/service.h"

namespace IPCDebugger {

namespace {
ObjectInfo GetObjectInfo(const Kernel::Object* object) {
    if (object == nullptr) {
        return {};
    }
    return {object->GetTypeName(), object->GetName(), static_cast<int>(object->GetObjectId())};
}

ObjectInfo GetObjectInfo(const Kernel::Thread* thread) {
    if (thread == nullptr) {
        return {};
    }
    return {thread->GetTypeName(), thread->GetName(), static_cast<int>(thread->GetThreadId())};
}

ObjectInfo GetObjectInfo(const Kernel::Process* process) {
    if (process == nullptr) {
        return {};
    }
    return {process->GetTypeName(), process->GetName(), static_cast<int>(process->process_id)};
}

std::string FormatWords(const std::vector<u32>& words, std::size_t max_words) {
    std::string out;
    for (std::size_t i = 0; i < words.size() && i < max_words; ++i) {
        if (i != 0) {
            out += ' ';
        }
        out += fmt::format("{:08X}", words[i]);
    }
    if (words.size() > max_words) {
        out += " ...";
    }
    return out;
}

// TEMPORARY (Mii Plaza debugging): write every finished IPC call to the log.
void LogRecord(const RequestRecord& record) {
    const std::string& port =
        record.client_port.name.empty() ? record.server_session.name : record.client_port.name;

    // Skip per-frame graphics/audio traffic and portless sessions (file reads).
    if (port.rfind("gsp::Gpu", 0) == 0 || port.rfind("dsp::DSP", 0) == 0 ||
        (record.is_hle && record.function_name.empty())) {
        return;
    }

    LOG_INFO(Kernel, "IPCLOG {} -> {} {}{} req=[{}] rep=[{}]", record.client_process.name, port,
             record.function_name,
             record.status == RequestStatus::HLEUnimplemented ? " (UNIMPLEMENTED)" : "",
             FormatWords(record.untranslated_request_cmdbuf, 16),
             FormatWords(record.translated_reply_cmdbuf, 16));
}
} // namespace

Recorder::Recorder() = default;
Recorder::~Recorder() = default;

bool Recorder::IsEnabled() const {
    // TEMPORARY (Mii Plaza debugging): always record so every call reaches the log.
    return true;
}

void Recorder::RegisterRequest(const std::shared_ptr<Kernel::ClientSession>& client_session,
                               const std::shared_ptr<Kernel::Thread>& client_thread) {
    const u32 thread_id = client_thread->GetThreadId();

    if (auto owner_process = client_thread->owner_process.lock()) {
        RequestRecord record = {/* id */ ++record_count,
                                /* status */ RequestStatus::Sent,
                                /* client_process */ GetObjectInfo(owner_process.get()),
                                /* client_thread */ GetObjectInfo(client_thread.get()),
                                /* client_session */ GetObjectInfo(client_session.get()),
                                /* client_port */ GetObjectInfo(client_session->parent->port.get()),
                                /* server_process */ {},
                                /* server_thread */ {},
                                /* server_session */ GetObjectInfo(client_session->parent->server)};
        record_map.insert_or_assign(thread_id, std::make_unique<RequestRecord>(record));
        client_session_map.insert_or_assign(thread_id, client_session);

        InvokeCallbacks(record);
    }
}

void Recorder::SetRequestInfo(const std::shared_ptr<Kernel::Thread>& client_thread,
                              std::vector<u32> untranslated_cmdbuf,
                              std::vector<u32> translated_cmdbuf,
                              const std::shared_ptr<Kernel::Thread>& server_thread) {
    const u32 thread_id = client_thread->GetThreadId();
    if (!record_map.count(thread_id)) {
        // This is possible when the recorder is enabled after application started
        LOG_ERROR(Kernel, "No request is associated with the thread");
        return;
    }

    auto& record = *record_map[thread_id];
    record.status = RequestStatus::Handling;
    record.untranslated_request_cmdbuf = std::move(untranslated_cmdbuf);
    record.translated_request_cmdbuf = std::move(translated_cmdbuf);

    if (server_thread) {
        if (auto owner_process = server_thread->owner_process.lock()) {
            record.server_process = GetObjectInfo(owner_process.get());
        }
        record.server_thread = GetObjectInfo(server_thread.get());
    } else {
        record.is_hle = true;
    }

    // Function name
    ASSERT_MSG(client_session_map.count(thread_id), "Client session is missing");
    const auto& client_session = client_session_map[thread_id];
    if (client_session->parent->port &&
        client_session->parent->port->GetServerPort()->hle_handler) {

        record.function_name = std::dynamic_pointer_cast<Service::ServiceFrameworkBase>(
                                   client_session->parent->port->GetServerPort()->hle_handler)
                                   ->GetFunctionName({record.untranslated_request_cmdbuf[0]});
    }
    client_session_map.erase(thread_id);

    InvokeCallbacks(record);
}

void Recorder::SetReplyInfo(const std::shared_ptr<Kernel::Thread>& client_thread,
                            std::vector<u32> untranslated_cmdbuf,
                            std::vector<u32> translated_cmdbuf) {
    const u32 thread_id = client_thread->GetThreadId();
    if (!record_map.count(thread_id)) {
        // This is possible when the recorder is enabled after application started
        LOG_ERROR(Kernel, "No request is associated with the thread");
        return;
    }

    auto& record = *record_map[thread_id];
    if (record.status != RequestStatus::HLEUnimplemented) {
        record.status = RequestStatus::Handled;
    }

    record.untranslated_reply_cmdbuf = std::move(untranslated_cmdbuf);
    record.translated_reply_cmdbuf = std::move(translated_cmdbuf);
    LogRecord(record);
    InvokeCallbacks(record);

    record_map.erase(thread_id);
}

void Recorder::SetHLEUnimplemented(const std::shared_ptr<Kernel::Thread>& client_thread) {
    const u32 thread_id = client_thread->GetThreadId();
    if (!record_map.count(thread_id)) {
        // This is possible when the recorder is enabled after application started
        LOG_ERROR(Kernel, "No request is associated with the thread");
        return;
    }

    auto& record = *record_map[thread_id];
    record.status = RequestStatus::HLEUnimplemented;
}

CallbackHandle Recorder::BindCallback(CallbackType callback) {
    std::unique_lock lock(callback_mutex);
    CallbackHandle handle = std::make_shared<CallbackType>(callback);
    callbacks.emplace(handle);
    return handle;
}

void Recorder::UnbindCallback(const CallbackHandle& handle) {
    std::unique_lock lock(callback_mutex);
    callbacks.erase(handle);
}

void Recorder::InvokeCallbacks(const RequestRecord& request) {
    {
        std::shared_lock lock(callback_mutex);
        for (const auto& iter : callbacks) {
            (*iter)(request);
        }
    }
}

void Recorder::SetEnabled(bool enabled_) {
    enabled.store(enabled_, std::memory_order_relaxed);
}

} // namespace IPCDebugger
