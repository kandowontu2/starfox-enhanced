#include "starfox/audio/spc700_audio.hpp"
#include "starfox/state/archive.hpp"
#include "starfox/state/container.hpp"
#include "starfox/platform/nintendo_3ds/frame_profile.hpp"

#include <spc.h>
#include <SPC_Filter.h>
#include <SNES_SPC.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace starfox::audio {
namespace {

constexpr std::size_t kSpcHeaderSize = 0x100U;
constexpr std::size_t kSpcRamSize = 0x10000U;
constexpr std::size_t kSpcRamOffset = kSpcHeaderSize;
constexpr std::size_t kSpcDspOffset = 0x10100U;
constexpr std::size_t kPcLowOffset = 0x25U;
constexpr std::size_t kPcHighOffset = 0x26U;
constexpr std::size_t kAccumulatorOffset = 0x27U;
constexpr std::size_t kXOffset = 0x28U;
constexpr std::size_t kYOffset = 0x29U;
constexpr std::size_t kPswOffset = 0x2aU;
constexpr std::size_t kStackOffset = 0x2bU;
constexpr std::size_t kSmpRegistersOffset = kSpcRamOffset + 0xf0U;
constexpr std::size_t kDspFlags = 0x6cU;
constexpr int kClocksPerLogicTick = spc_clock_rate / 20;
static_assert(kClocksPerLogicTick == 51'200);
static_assert(Spc700Audio::stereo_frames_per_logic_tick
              == static_cast<std::size_t>(spc_sample_rate / 20));

void throw_spc_error(const char* operation, spc_err_t error) {
    if (error != nullptr) {
        throw std::runtime_error{
            std::string{operation} + ": " + error};
    }
}

// The pinned core passes io only to this callback, including extension bytes.
// Keep the bounds in a stack-local context rather than global callback state.
struct CoreStateCopy {
    std::vector<std::uint8_t> output;
    std::span<const std::uint8_t> input;
    bool reading{};
    static void copy(unsigned char** io, void* fields, std::size_t count) {
        auto& self = *reinterpret_cast<CoreStateCopy*>(*io);
        if (self.reading) {
            if (count > self.input.size())
                throw std::runtime_error{"Truncated SPC core state"};
            std::memcpy(fields, self.input.data(), count);
            self.input = self.input.subspan(count);
        } else {
            if (count > spc_state_size - self.output.size())
                throw std::runtime_error{"Oversized SPC core state"};
            auto* first = static_cast<const std::uint8_t*>(fields);
            self.output.insert(self.output.end(), first, first + count);
        }
    }
};

} // namespace

struct Spc700Audio::Impl {
    enum class CommandStream {
        music,
        effects,
    };

    enum class UploadState {
        waiting_for_address_low,
        waiting_for_address_high,
        waiting_for_transfer_flag,
        waiting_for_token,
        waiting_for_data,
    };

    SNES_SPC* spc{spc_new()};
    SPC_Filter* filter{spc_filter_new()};
    std::array<std::uint8_t, kSpcRamSize> aram{};
    std::array<std::uint8_t, 4> cpu_ports{};
    UploadState upload_state{UploadState::waiting_for_address_low};
    std::uint16_t upload_address{};
    std::uint16_t execute_address{0x0400U};
    bool transfer_enabled{};
    bool uploading{true};
    bool loaded{};
    std::size_t upload_count{};

    Impl() {
        if (spc == nullptr || filter == nullptr) {
            if (filter != nullptr) spc_filter_delete(filter);
            if (spc != nullptr) spc_delete(spc);
            throw std::runtime_error{"could not allocate SPC700 audio emulator"};
        }
        aram.fill(0xffU);
        spc_filter_clear(filter);
    }

    ~Impl() {
        spc_filter_delete(filter);
        spc_delete(spc);
    }

    void save(state::Writer& writer) const {
        CoreStateCopy core;
        auto* context = reinterpret_cast<unsigned char*>(&core);
        spc->copy_state_preserving_machine(&context, CoreStateCopy::copy);
        std::array<int, 8> history{};
        static_assert(sizeof(int) == sizeof(std::int32_t));
        filter->save_history(history.data());
        int clocks{}, carry_count{};
        std::array<short, SNES_SPC::extra_size> carry{};
        spc->save_output_carry(clocks, carry_count, carry.data());
        writer(aram, cpu_ports, static_cast<std::uint8_t>(upload_state),
            upload_address, execute_address, transfer_enabled, uploading, loaded,
            static_cast<std::uint64_t>(upload_count), history, core.output,
            clocks, carry_count, carry);
    }

    void load(state::Reader& reader) {
        std::uint8_t upload{};
        std::uint64_t count{};
        std::array<int, 8> history{};
        std::vector<std::uint8_t> core_bytes;
        int clocks{}, carry_count{};
        std::array<short, SNES_SPC::extra_size> carry{};
        reader(aram, cpu_ports, upload, upload_address, execute_address,
            transfer_enabled, uploading, loaded, count, history, core_bytes,
            clocks, carry_count, carry);
        if (upload > static_cast<std::uint8_t>(UploadState::waiting_for_data)
            || count > std::numeric_limits<std::size_t>::max()
            || (loaded && uploading) || history[0] != spc_filter_gain_unit
            || history[1] != spc_filter_bass_norm || core_bytes.size() > spc_state_size
            || carry_count < 0 || carry_count > SNES_SPC::extra_size
            || (carry_count & 1) != 0 || clocks < 0 || clocks > kClocksPerLogicTick + 31)
            throw std::runtime_error{"Invalid SPC host state"};
        upload_state = static_cast<UploadState>(upload);
        upload_count = static_cast<std::size_t>(count);
        CoreStateCopy core{{}, core_bytes, true};
        auto* context = reinterpret_cast<unsigned char*>(&core);
        spc_copy_state(spc, &context, CoreStateCopy::copy);
        if (!core.input.empty()) throw std::runtime_error{"Trailing SPC core state"};
        filter->load_history(history.data());
        spc->load_output_carry(clocks, carry_count, carry.data());
    }

    void begin_upload() noexcept {
        if (loaded) {
            // A real SPC reset returns to the IPL without clearing ARAM. The
            // running driver modifies its work area after the initial upload,
            // so preserve that live RAM before applying the next bank overlay.
            std::array<std::uint8_t, spc_file_size> snapshot{};
            spc_init_header(snapshot.data());
            spc_save_spc(spc, snapshot.data());
            std::copy(snapshot.begin() + kSpcRamOffset,
                      snapshot.begin() + kSpcRamOffset + kSpcRamSize,
                      aram.begin());
        }
        uploading = true;
        loaded = false;
        upload_state = UploadState::waiting_for_address_low;
        upload_address = 0;
        transfer_enabled = false;
        upload_count = 0;
        // Resetting the SPC into its IPL leaves ARAM intact. Level sound
        // banks are overlays on the base sound0 driver loaded during boot.
    }

    void load_driver() {
        std::array<std::uint8_t, spc_file_size> snapshot{};
        spc_init_header(snapshot.data());
        snapshot[kPcLowOffset] = static_cast<std::uint8_t>(execute_address);
        snapshot[kPcHighOffset] = static_cast<std::uint8_t>(execute_address >> 8U);

        // The IPL enters downloaded code with an empty accumulator/index
        // context and its reset stack. Star Fox's driver initializes all of
        // its own SMP and DSP state immediately from this entry point.
        snapshot[kAccumulatorOffset] = 0;
        snapshot[kXOffset] = 0;
        snapshot[kYOffset] = 0;
        snapshot[kPswOffset] = 0;
        snapshot[kStackOffset] = 0xefU;
        std::copy(aram.begin(), aram.end(), snapshot.begin() + kSpcRamOffset);

        // SPC files mirror the SMP I/O registers into RAM $f0-$ff. Disable
        // the absent IPL mapping and start from the hardware-reset DSP state;
        // the uploaded driver replaces these values during its prologue.
        std::fill(snapshot.begin() + kSmpRegistersOffset,
                  snapshot.begin() + kSmpRegistersOffset + 16U, 0U);
        snapshot[kSpcDspOffset + kDspFlags] = 0xe0U;
        throw_spc_error("spc_load_spc",
                        spc_load_spc(spc, snapshot.data(), snapshot.size()));
        spc_filter_clear(filter);
        loaded = true;
        uploading = false;
    }

    void consume_upload_write(const simulation::ApuPortWrite& write) {
        cpu_ports[write.port] = write.value;
        switch (upload_state) {
        case UploadState::waiting_for_address_low:
            if (write.port == 2U) {
                upload_address = write.value;
                upload_state = UploadState::waiting_for_address_high;
            }
            break;
        case UploadState::waiting_for_address_high:
            if (write.port == 3U) {
                upload_address = static_cast<std::uint16_t>(
                    upload_address | (static_cast<std::uint16_t>(write.value) << 8U));
                upload_state = UploadState::waiting_for_transfer_flag;
            } else if (write.port == 2U) {
                upload_address = write.value;
            }
            break;
        case UploadState::waiting_for_transfer_flag:
            if (write.port == 1U) {
                transfer_enabled = write.value != 0U;
                upload_state = UploadState::waiting_for_token;
            } else if (write.port == 0U) {
                // The IPL's final execute packet is ordered differently from
                // a data block: address on ports 2/3, then the token on port
                // 0, with the zero high byte on port 1 written afterwards.
                // Therefore no transfer flag precedes this token.
                transfer_enabled = false;
                execute_address = upload_address;
                load_driver();
            } else if (write.port == 2U) {
                upload_address = write.value;
                upload_state = UploadState::waiting_for_address_high;
            }
            break;
        case UploadState::waiting_for_token:
            if (write.port == 0U) {
                if (!transfer_enabled) {
                    execute_address = upload_address;
                    load_driver();
                } else {
                    upload_state = UploadState::waiting_for_data;
                }
            }
            break;
        case UploadState::waiting_for_data:
            if (write.port == 1U) {
                aram[upload_address++] = write.value;
                ++upload_count;
            } else if (write.port == 2U) {
                upload_address = write.value;
                upload_state = UploadState::waiting_for_address_high;
            }
            break;
        }
    }

    void render(
        std::vector<std::int16_t>& output,
        std::span<const simulation::ApuPortWrite> writes,
        bool split_upload_restarts = false,
        std::size_t* rendered_frames = nullptr,
        CommandStream command_stream = CommandStream::music) {
        output.resize(Spc700Audio::stereo_frames_per_logic_tick * 2U);
        std::fill(output.begin(), output.end(), 0);
        if (loaded) {
            spc_set_output(spc, output.data(), static_cast<int>(output.size()));
        }
        int last_clock = 0;
        auto finish_frame = [&] {
            if (!loaded) return;
            {
                STARFOX_3DS_FRAME_PHASE(spc_emulate);
                spc_end_frame(spc, kClocksPerLogicTick);
            }
            if (rendered_frames != nullptr) ++*rendered_frames;
            // A later upload calls spc_load_spc and replaces the output
            // buffer. Clear this completed bank's samples now; startup audio
            // is intentionally inaudible and only its emulated state matters.
            std::fill(output.begin(), output.end(), 0);
            last_clock = 0;
        };

        // A fresh sound bank begins with the source driver's explicit $ff
        // restart command. The subsequent writes are another IPL upload.
        for (const auto& write : writes) {
            if (!uploading && write.port == 0U && write.value == 0xffU) {
                if (split_upload_restarts) finish_frame();
                begin_upload();
            }
            const auto was_loaded = loaded;
            if (uploading) {
                consume_upload_write(write);
            } else {
                // The running Star Fox driver uses port 0 for BGM control,
                // ports 1/2 for continuous player/near-object audio (engine,
                // tunnel and positional sounds), and port 3 for its queued
                // one-shot effects. Keep the BGM bus on one SPC instance and
                // all three effect buses on the other so MSU replacement can
                // never discard an Arwing engine or let an effect key-off a
                // music voice.
                // Star Fox normally reserves port 3 for effects, but its
                // $01/$02 PAUSESND commands are global driver controls. The
                // PC port deliberately runs music and effects on independent
                // SPC instances, so deliver those two controls to both stems;
                // otherwise gameplay pauses silence neither music driver.
                const auto global_pause_command = write.port == 3U
                    && (write.value == 0x01U || write.value == 0x02U);
                const auto accepts_command = command_stream
                        == CommandStream::effects
                    ? write.port != 0U
                    : write.port == 0U || global_pause_command;
                if (!accepts_command) continue;
                const auto clock = std::clamp(
                    static_cast<int>(write.clock_offset), last_clock,
                    kClocksPerLogicTick);
                {
                    STARFOX_3DS_FRAME_PHASE(spc_emulate);
                    spc_write_port(spc, clock, write.port, write.value);
                }
                last_clock = clock;
            }
            if (!was_loaded && loaded) {
                // Loading an SPC state resets the library's output buffer.
                // Attach this tick's buffer immediately so initialization and
                // later timestamped port traffic are rendered, not discarded.
                spc_set_output(spc, output.data(), static_cast<int>(output.size()));
                last_clock = 0;
            }
        }

        if (!loaded) return;
        {
            STARFOX_3DS_FRAME_PHASE(spc_emulate);
            spc_end_frame(spc, kClocksPerLogicTick);
        }
        if (rendered_frames != nullptr) ++*rendered_frames;
        const auto generated = std::clamp(spc_sample_count(spc), 0,
                                          static_cast<int>(output.size()));
        if (generated < static_cast<int>(output.size())) {
            std::fill(output.begin() + generated, output.end(), 0);
        }
        {
            STARFOX_3DS_FRAME_PHASE(spc_filter);
            spc_filter_run(filter, output.data(), static_cast<int>(output.size()));
        }
    }
};

Spc700Audio::Spc700Audio()
    : music_impl_(std::make_unique<Impl>()),
      effects_impl_(std::make_unique<Impl>()) {}
Spc700Audio::~Spc700Audio() = default;
Spc700Audio::Spc700Audio(Spc700Audio&&) noexcept = default;
Spc700Audio& Spc700Audio::operator=(Spc700Audio&&) noexcept = default;

std::vector<std::uint8_t> Spc700Audio::save_state() const {
    state::Writer writer;
    music_impl_->save(writer);
    effects_impl_->save(writer);
    writer(last_music_samples_, last_effect_samples_);
    // Optional trailing extension preserves the actual CPU->SMP input ports.
    // The pinned core's old format merges them with SMP output registers.
    // Older states without this extension retain their original load behavior.
    std::array<std::uint8_t,4> music_inputs{},effects_inputs{};
    music_impl_->spc->save_cpu_input_ports(music_inputs.data());
    effects_impl_->spc->save_cpu_input_ports(effects_inputs.data());
    writer(music_inputs,effects_inputs);
    return state::pack(0x53504301U, 0U, writer.bytes());
}

void Spc700Audio::load_state(std::span<const std::uint8_t> bytes) {
    state::Reader reader{state::unpack(bytes, 0x53504301U, 0U)};
    Spc700Audio restored;
    restored.music_impl_->load(reader);
    restored.effects_impl_->load(reader);
    reader(restored.last_music_samples_, restored.last_effect_samples_);
    if(!reader.empty()) {
        std::array<std::uint8_t,4> music_inputs{},effects_inputs{};reader(music_inputs,effects_inputs);
        restored.music_impl_->spc->load_cpu_input_ports(music_inputs.data());
        restored.effects_impl_->spc->load_cpu_input_ports(effects_inputs.data());
    }
    reader.finish();
    const auto samples = restored.last_music_samples_.size();
    if (samples != restored.last_effect_samples_.size()
        || (samples != 0U && samples != stereo_frames_per_logic_tick * 2U))
        throw std::runtime_error{"Invalid SPC stem buffer size"};
    *this = std::move(restored);
}

void Spc700Audio::render_stems_logic_tick(
    std::span<const simulation::ApuPortWrite> writes) {
    {
        STARFOX_3DS_FRAME_PHASE(music);
        music_impl_->render(last_music_samples_,
            writes, false, nullptr, Impl::CommandStream::music);
    }
    {
        STARFOX_3DS_FRAME_PHASE(effects);
        effects_impl_->render(last_effect_samples_,
            writes, false, nullptr, Impl::CommandStream::effects);
    }
    if (last_music_samples_.size() != last_effect_samples_.size()) {
        throw std::runtime_error{"SPC music/effect stem size mismatch"};
    }
}

std::vector<std::int16_t> Spc700Audio::render_logic_tick(
    std::span<const simulation::ApuPortWrite> writes) {
    render_stems_logic_tick(writes);
    auto mixed = last_music_samples_;
    for (std::size_t index = 0; index < mixed.size(); ++index) {
        const auto sample = static_cast<std::int32_t>(last_music_samples_[index])
            + static_cast<std::int32_t>(last_effect_samples_[index]);
        mixed[index] = static_cast<std::int16_t>(std::clamp(
            sample,
            static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::min()),
            static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::max())));
    }
    return mixed;
}

std::size_t Spc700Audio::prime_upload_sequence(
    std::span<const simulation::ApuPortWrite> writes) {
    std::size_t music_frames{};
    std::size_t effect_frames{};
    music_impl_->render(last_music_samples_,
        writes, true, &music_frames, Impl::CommandStream::music);
    effects_impl_->render(last_effect_samples_,
        writes, true, &effect_frames, Impl::CommandStream::effects);
    if (music_frames != effect_frames) {
        throw std::runtime_error{"SPC music/effect upload cadence mismatch"};
    }
    last_music_samples_.clear();
    last_effect_samples_.clear();
    return music_frames;
}

bool Spc700Audio::driver_loaded() const noexcept {
    return music_impl_->loaded && effects_impl_->loaded;
}
std::size_t Spc700Audio::uploaded_bytes() const noexcept {
    return std::min(music_impl_->upload_count, effects_impl_->upload_count);
}

std::array<std::uint8_t, 4> Spc700Audio::output_ports() const noexcept {
    if (!driver_loaded()) return {};
    return {
        static_cast<std::uint8_t>(spc_read_port(music_impl_->spc, 0, 0)),
        static_cast<std::uint8_t>(spc_read_port(effects_impl_->spc, 0, 1)),
        static_cast<std::uint8_t>(spc_read_port(effects_impl_->spc, 0, 2)),
        static_cast<std::uint8_t>(spc_read_port(effects_impl_->spc, 0, 3)),
    };
}

Spc700Audio::State Spc700Audio::state() const {
    if (!driver_loaded()) return {};
    std::array<std::uint8_t, spc_file_size> snapshot{};
    spc_init_header(snapshot.data());
    spc_save_spc(music_impl_->spc, snapshot.data());
    return {
        static_cast<std::uint16_t>(snapshot[kPcLowOffset]
            | (static_cast<std::uint16_t>(snapshot[kPcHighOffset]) << 8U)),
        snapshot[kAccumulatorOffset],
        snapshot[kXOffset],
        snapshot[kYOffset],
        snapshot[kPswOffset],
        snapshot[kStackOffset],
        snapshot[kSpcDspOffset + kDspFlags],
        snapshot[kSpcDspOffset + 0x4cU],
        static_cast<std::int8_t>(snapshot[kSpcDspOffset + 0x0cU]),
        static_cast<std::int8_t>(snapshot[kSpcDspOffset + 0x1cU]),
        output_ports(),
    };
}

} // namespace starfox::audio
