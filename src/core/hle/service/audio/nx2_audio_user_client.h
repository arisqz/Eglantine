// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>

#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/service/audio/audio_in_manager.h"
#include "core/hle/service/audio/audio_out_manager.h"
#include "core/hle/service/audio/audio_renderer_manager.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"
#include "core/memory.h"

namespace Service::Audio {

// Switch 2 "aud:u" (nn::audio::CreateUserClientSessionByHipc). The SDK reaches the Switch 1
// audio interfaces through it: command 2 returns an object used exactly like
// IAudioRendererManager (vtable slot 1 = GetWorkBufferSize with a Switch 1 parameter block), so
// it is backed by Eden's audren:u implementation. Command 1 is nn::audio::OpenAudioOut's
// manager, called like IAudioOutManager (slot 1 = OpenAudioOut), backed by audout:u. The nx2: name
// keeps unknown commands auto-stubbed and logged.
class INx2AudioUserClient final : public ServiceFramework<INx2AudioUserClient> {
public:
    explicit INx2AudioUserClient(Core::System& system_)
        : ServiceFramework{system_, "nx2:aud:u"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {1, &INx2AudioUserClient::GetAudioOutManager, "GetAudioOutManager (guessed)"},
            {2, &INx2AudioUserClient::GetAudioRendererManager, "GetAudioRendererManager (guessed)"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }

private:
    void GetAudioOutManager(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u32 arg = rp.Pop<u32>();
        LOG_WARNING(Service_Audio, "(STUBBED) Switch 2 aud:u 1 arg={}, returning IAudioOutManager",
                    arg);
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IAudioOutManager>(ctx, system);
    }

    void GetAudioRendererManager(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u32 arg = rp.Pop<u32>();
        LOG_WARNING(Service_Audio, "(STUBBED) Switch 2 aud:u 2 arg={}, returning IAudioRendererManager",
                    arg);
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IAudioRendererManager>(ctx, system);
    }
};

// Logs a Switch 2 audio request's first raw words and its buffer and handle descriptors.
inline void LogNx2AudioRequest(HLERequestContext& ctx, std::string_view object) {
    IPC::RequestParser rp{ctx};
    std::string words;
    for (u32 i = 0; i < 8; ++i) {
        words += fmt::format(" {:08x}", rp.Pop<u32>());
    }
    std::string buffers;
    for (const auto& a : ctx.BufferDescriptorA()) buffers += fmt::format(" A{:#x}", a.Size());
    for (const auto& b : ctx.BufferDescriptorB()) buffers += fmt::format(" B{:#x}", b.Size());
    for (const auto& x : ctx.BufferDescriptorX()) buffers += fmt::format(" X{:#x}", x.Size());
    for (const auto& c : ctx.BufferDescriptorC()) buffers += fmt::format(" C{:#x}", c.Size());
    LOG_WARNING(Service_Audio, "(STUBBED) Switch 2 {} {}: raw{} |{} copy {:#x} move {:#x}", object,
                ctx.GetCommand(), words, buffers, ctx.GetCopyHandle(0), ctx.GetMoveHandle(0));
}

// Logs an unreversed Switch 2 audio request and answers success with zeroed outputs.
inline void LogAndZeroNx2AudioRequest(HLERequestContext& ctx, std::string_view object) {
    LogNx2AudioRequest(ctx, object);
    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);
    rb.Push<u32>(0);
    rb.Push<u32>(0);
    rb.Push<u32>(0);
    rb.Push<u32>(0);
}

// Switch 2 "auddmg:u" (nn::audio::devmgr::IUserSessionManager). Command 0 (with the process ID)
// returns the IUserSession the devmgr calls go through; without it the SDK calls through a null
// object. Command IDs are from the SDK's proxy vtable; one virtual device per direction
// ("MainAudioOut"/"MainAudioIn", the audio sysmodule's names) is reported, available and
// supporting any stream format. Other
// commands stay auto-stubbed and logged under the nx2: name.
class INx2AudioDeviceManager final : public ServiceFramework<INx2AudioDeviceManager> {
public:
    explicit INx2AudioDeviceManager(Core::System& system_)
        : ServiceFramework{system_, "nx2:auddmg"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &INx2AudioDeviceManager::ListVirtualDeviceNames, "ListVirtualDeviceNames"},
            {200, &INx2AudioDeviceManager::ReturnTrue, "IsVirtualDeviceAvailable"},
            {300, &INx2AudioDeviceManager::ReturnTrue, "IsVirtualDeviceStreamFormatSupported"},
            {401, &INx2AudioDeviceManager::GetVirtualDeviceProfile, "GetVirtualDeviceProfile"},
            {700, &INx2AudioDeviceManager::AttachStream, "AttachStream"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }

private:
    static constexpr std::size_t NameSize = 0x100;
    static constexpr u32 OutputFunctionality = 1;

    static std::string_view DeviceName(u32 functionality) {
        return functionality == OutputFunctionality ? "MainAudioOut" : "MainAudioIn";
    }

    void ListVirtualDeviceNames(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        // The bool argument comes first, padded to a word, then the functionality.
        const bool flag = (rp.Pop<u32>() & 0xff) != 0;
        const u32 functionality = rp.Pop<u32>();
        LOG_WARNING(Service_Audio,
                    "(STUBBED) Switch 2 devmgr ListVirtualDeviceNames functionality={} flag={}",
                    functionality, flag);
        std::array<char, NameSize> name{};
        DeviceName(functionality).copy(name.data(), name.size() - 1);
        const bool fits = ctx.GetWriteBufferSize() >= name.size();
        if (fits) {
            ctx.WriteBuffer(name);
        }
        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push<s32>(fits ? 1 : 0);
    }

    void GetVirtualDeviceProfile(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u32 functionality = rp.Pop<u32>();
        LOG_WARNING(Service_Audio, "(STUBBED) Switch 2 devmgr GetVirtualDeviceProfile functionality={}",
                    functionality);
        // nn::audio's OutputStream open turns the nanoseconds at 0x218 into a minimum buffer
        // frame count (the stream's buffer must hold at least that); 5 ms. The u32 at 0x220 is
        // passed to sysmix::CreateStream, which rejects 0 (a device ID). 0x108 is left zero.
        std::array<u8, 0x228> profile{};
        DeviceName(functionality).copy(reinterpret_cast<char*>(profile.data()), NameSize - 1);
        const s64 minimum_buffer_ns = 5'000'000;
        const u32 device_id = functionality == OutputFunctionality ? 1 : 2;
        std::memcpy(profile.data() + 0x218, &minimum_buffer_ns, sizeof(s64));
        std::memcpy(profile.data() + 0x220, &device_id, sizeof(u32));
        ctx.WriteBuffer(profile);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    // Attaches a sysmix stream (by its ID) to the named virtual device; nothing to route here.
    void AttachStream(HLERequestContext& ctx) {
        LogNx2AudioRequest(ctx, "devmgr");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void ReturnTrue(HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.Push<u32>(1);
    }
};

// Switch 2 "audsmx:u" (nn::audio::sysmix, behind nn::audio::OpenOutputStream and the other
// OutputStream calls): IUserSessionManager command 0 returns an IUserSession, whose command 1
// creates an IExternalStreamSession (commands 0-12 from the SDK's proxy vtables).
//
// The stream's buffer (given to CreateStream, read by the service from the client's memory) is a
// ring of frames with two control blocks: the service's at +0x40 (frames played in total, then
// the read position) and the client's at +0x1000 (frames written in total, then the write
// position). Positions are in frames modulo the capacity command 0 sets plus one (one slot stays
// empty); the client starts with the ring full. Here the service drains it in real time without
// playing it and signals the data event (command 7) so the client refills it. Other commands are
// logged and answered with zeros.
class INx2AudioExternalStreamSession final
    : public ServiceFramework<INx2AudioExternalStreamSession> {
public:
    explicit INx2AudioExternalStreamSession(Core::System& system_, u64 buffer_)
        : ServiceFramework{system_, "nx2:audsmx:stream"}, m_context{system_, "nx2:audsmx:stream"},
          m_data_event{m_context.CreateEvent("nx2:audsmx:stream data")} {
        m_state->buffer = buffer_;
        m_state->event = m_data_event;
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &INx2AudioExternalStreamSession::Open, "Open (guessed)"},
            {1, &INx2AudioExternalStreamSession::Log, "Unknown1"},
            {2, &INx2AudioExternalStreamSession::Log, "Unknown2"},
            {3, &INx2AudioExternalStreamSession::Log, "Unknown3"},
            {4, &INx2AudioExternalStreamSession::Log, "Unknown4"},
            {5, &INx2AudioExternalStreamSession::Log, "Unknown5"},
            {6, &INx2AudioExternalStreamSession::Log, "Unknown6"},
            {7, &INx2AudioExternalStreamSession::AcquireDataEvent, "AcquireStreamDataEvent (guessed)"},
            {8, &INx2AudioExternalStreamSession::Log, "Unknown8"},
            {9, &INx2AudioExternalStreamSession::Log, "Unknown9"},
            {10, &INx2AudioExternalStreamSession::Log, "Unknown10"},
            {11, &INx2AudioExternalStreamSession::Log, "Unknown11"},
            {12, &INx2AudioExternalStreamSession::Log, "Unknown12"},
        };
        // clang-format on
        RegisterHandlers(functions);
        m_drain_timer = Core::Timing::CreateEvent(
            "nx2:audsmx:stream drain",
            [state = m_state, &system = system](
                s64, std::chrono::nanoseconds) -> std::optional<std::chrono::nanoseconds> {
                Drain(*state, system);
                return std::nullopt;
            });
        system.CoreTiming().ScheduleLoopingEvent(DrainInterval, DrainInterval, m_drain_timer);
    }

    ~INx2AudioExternalStreamSession() override {
        system.CoreTiming().UnscheduleEvent(m_drain_timer);
        {
            std::scoped_lock lk{m_state->mutex};
            m_state->event = nullptr;
        }
        m_context.CloseEvent(m_data_event);
    }

private:
    static constexpr std::chrono::milliseconds DrainInterval{10};
    static constexpr u32 FramesPerDrain = 480; // 10 ms at 48 kHz
    static constexpr u64 PlayedFrames = 0x40;
    static constexpr u64 ReadPosition = 0x44;
    static constexpr u64 WrittenFrames = 0x1000;
    static constexpr u64 WritePosition = 0x1004;

    // Shared with the drain timer, which can outlive the session by one call.
    struct State {
        std::mutex mutex;
        u64 buffer{};
        u32 capacity{};
        u32 drains{};
        Kernel::KEvent* event{};
    };

    static void Drain(State& state, Core::System& system) {
        std::scoped_lock lk{state.mutex};
        if (state.capacity == 0 || state.event == nullptr) {
            return;
        }
        auto& memory = system.ApplicationMemory();
        const u32 modulus = state.capacity + 1;
        const u32 write = memory.Read32(state.buffer + WritePosition);
        const u32 read = memory.Read32(state.buffer + ReadPosition);
        if (state.drains++ % 500 == 0) {
            LOG_INFO(Service_Audio,
                     "Switch 2 sysmix stream: written {} (position {}), played {} (position {})",
                     memory.Read32(state.buffer + WrittenFrames), write,
                     memory.Read32(state.buffer + PlayedFrames), read);
        }
        if (write >= modulus || read >= modulus) {
            return;
        }
        const u32 filled = (write + modulus - read) % modulus;
        const u32 frames = (std::min)(filled, FramesPerDrain);
        if (frames != 0) {
            memory.Write32(state.buffer + ReadPosition, (read + frames) % modulus);
            // The played-frame total the SDK's stream statistics (the playback clock) read.
            memory.Write32(state.buffer + PlayedFrames,
                           memory.Read32(state.buffer + PlayedFrames) + frames);
        }
        state.event->Signal(system.Kernel());
    }

    // Raw input: sample format, channel count, sample rate, capacity in frames, ...
    void Open(HLERequestContext& ctx) {
        LogNx2AudioRequest(ctx, "sysmix stream");
        IPC::RequestParser rp{ctx};
        rp.Pop<u32>();
        rp.Pop<u32>();
        rp.Pop<u32>();
        const u32 capacity = rp.Pop<u32>();
        {
            std::scoped_lock lk{m_state->mutex};
            m_state->capacity = capacity;
        }
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    // Raw input: notification mode, frame threshold.
    void AcquireDataEvent(HLERequestContext& ctx) {
        LogNx2AudioRequest(ctx, "sysmix stream");
        IPC::ResponseBuilder rb{ctx, 2, 1, 0};
        rb.Push(ResultSuccess);
        rb.PushCopyObjects(ctx, m_data_event->GetReadableEvent());
    }

    void Log(HLERequestContext& ctx) {
        LogAndZeroNx2AudioRequest(ctx, "sysmix stream");
    }

    KernelHelpers::ServiceContext m_context;
    Kernel::KEvent* m_data_event;
    std::shared_ptr<State> m_state = std::make_shared<State>();
    std::shared_ptr<Core::Timing::EventType> m_drain_timer;
};

class INx2AudioSysmixSession final : public ServiceFramework<INx2AudioSysmixSession> {
public:
    explicit INx2AudioSysmixSession(Core::System& system_)
        : ServiceFramework{system_, "nx2:audsmx"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {1, &INx2AudioSysmixSession::CreateStream, "CreateStream (guessed)"},
            {2, &INx2AudioSysmixSession::Log, "Unknown2"},
            {3, &INx2AudioSysmixSession::Log, "Unknown3"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }

private:
    // Raw input: device ID, flags, ..., the stream buffer's address and size (the service reads
    // the client's memory directly; it has its process handle). The SDK reads a u32 (the stream
    // ID) before the object.
    void CreateStream(HLERequestContext& ctx) {
        LogNx2AudioRequest(ctx, "sysmix session");
        IPC::RequestParser rp{ctx};
        rp.Pop<u32>();
        rp.Pop<u32>();
        rp.Pop<u32>();
        rp.Pop<u32>();
        const u64 buffer = rp.Pop<u64>();
        IPC::ResponseBuilder rb{ctx, 3, 0, 1};
        rb.Push(ResultSuccess);
        rb.Push<u32>(++next_stream_id);
        rb.PushIpcInterface<INx2AudioExternalStreamSession>(ctx, system, buffer);
    }

    u32 next_stream_id{};

    void Log(HLERequestContext& ctx) {
        LogAndZeroNx2AudioRequest(ctx, "sysmix session");
    }
};

class INx2AudioSysmixClient final : public ServiceFramework<INx2AudioSysmixClient> {
public:
    explicit INx2AudioSysmixClient(Core::System& system_)
        : ServiceFramework{system_, "nx2:audsmx:u"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &INx2AudioSysmixClient::OpenSession, "OpenSession (guessed)"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }

private:
    void OpenSession(HLERequestContext& ctx) {
        LOG_WARNING(Service_Audio, "(STUBBED) Switch 2 audsmx:u 0, returning a sysmix session");
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<INx2AudioSysmixSession>(ctx, system);
    }
};

class INx2AudioDeviceManagerClient final : public ServiceFramework<INx2AudioDeviceManagerClient> {
public:
    explicit INx2AudioDeviceManagerClient(Core::System& system_)
        : ServiceFramework{system_, "nx2:auddmg:u"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &INx2AudioDeviceManagerClient::OpenDeviceManager, "OpenDeviceManager (guessed)"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }

private:
    void OpenDeviceManager(HLERequestContext& ctx) {
        LOG_WARNING(Service_Audio, "(STUBBED) Switch 2 auddmg:u 0, returning a device manager");
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<INx2AudioDeviceManager>(ctx, system);
    }
};

} // namespace Service::Audio
