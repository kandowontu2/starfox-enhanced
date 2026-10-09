# Build a private copy of the pinned LGPL SPC sources. Extensions expose
# filter history, output carry and side-effect-free state/input-port access; preserve upstream files
# and license headers. These extensions require no pointer serialization.
set(STARFOX_SPC_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/snes_spc")
file(MAKE_DIRECTORY "${STARFOX_SPC_SOURCE}")
file(GLOB spc_source_files "${snes_spc_SOURCE_DIR}/snes_spc/*")
foreach(source IN LISTS spc_source_files)
    if(NOT IS_DIRECTORY "${source}")
        get_filename_component(name "${source}" NAME)
        if(NOT name STREQUAL "SPC_Filter.h" AND NOT name STREQUAL "SNES_SPC.h")
            configure_file("${source}" "${STARFOX_SPC_SOURCE}/${name}" COPYONLY)
        endif()
    endif()
endforeach()
function(starfox_write_spc_header name contents)
    set(destination "${STARFOX_SPC_SOURCE}/${name}")
    if(EXISTS "${destination}")
        file(READ "${destination}" previous)
        if(previous STREQUAL contents)
            return()
        endif()
    endif()
    file(WRITE "${destination}" "${contents}")
endfunction()
file(READ "${snes_spc_SOURCE_DIR}/snes_spc/SPC_Filter.h" filter_header)
set(filter_anchor "\tvoid clear();")
string(FIND "${filter_header}" "${filter_anchor}" anchor_position)
if(anchor_position EQUAL -1)
    message(FATAL_ERROR "Pinned SPC filter state extension needs review")
endif()
string(REPLACE "${filter_anchor}" [=[
    void clear();
    // Star Fox Enhanced extension: semantic fields, never object bytes.
    void save_history(int out[8]) const {
        out[0] = gain; out[1] = bass;
        for (int i = 0; i < 2; ++i) {
            out[2+i*3] = ch[i].p1;
            out[3+i*3] = ch[i].pp1;
            out[4+i*3] = ch[i].sum;
        }
    }
    void load_history(const int in[8]) {
        gain = in[0]; bass = in[1];
        for (int i = 0; i < 2; ++i) {
            ch[i].p1 = in[2+i*3];
            ch[i].pp1 = in[3+i*3];
            ch[i].sum = in[4+i*3];
        }
    }
]=] filter_header "${filter_header}")
starfox_write_spc_header("SPC_Filter.h" "${filter_header}")

file(READ "${snes_spc_SOURCE_DIR}/snes_spc/SNES_SPC.h" core_header)
set(core_anchor "\tint sample_count() const;")
string(FIND "${core_header}" "${core_anchor}" anchor_position)
if(anchor_position EQUAL -1)
    message(FATAL_ERROR "Pinned SPC carry-buffer extension needs review")
endif()
string(REPLACE "${core_anchor}" [=[
    int sample_count() const;
    // Upstream copy_state also loads SMP registers while saving. Preserve the
    // live CPU-input ports/timers/ROM RAM so inspecting or saving a timeline
    // cannot alter its next instruction. Heap storage avoids a ~66 KiB ARM
    // stack temporary; nothing here is persisted as native object bytes.
    void copy_state_preserving_machine(unsigned char** io, void (*copy)(unsigned char**, void*, size_t)) {
        struct Restore {
            SNES_SPC& owner;
            state_t* saved;
            ~Restore() { owner.m = *saved; delete saved; }
        } restore = {*this, new state_t(m)};
        copy_state(io, copy);
    }
    void save_cpu_input_ports(unsigned char out[4]) const {
        for (int i = 0; i < 4; ++i) out[i] = m.smp_regs[1][r_cpuio0+i];
    }
    void load_cpu_input_ports(const unsigned char in[4]) {
        for (int i = 0; i < 4; ++i) m.smp_regs[1][r_cpuio0+i] = in[i];
    }
    // Star Fox Enhanced extension. Call only between end_frame/set_output.
    void save_output_carry(int& clocks, int& count, sample_t* samples) const {
        clocks = m.extra_clocks;
        count = int(m.extra_pos - m.extra_buf);
        for (int i = 0; i < extra_size; ++i)
            samples[i] = i < count ? m.extra_buf[i] : 0;
    }
    void load_output_carry(int clocks, int count, const sample_t* samples) {
        m.extra_clocks = clocks;
        for (int i = 0; i < extra_size; ++i) m.extra_buf[i] = samples[i];
        m.extra_pos = m.extra_buf + count;
        m.buf_begin = 0;
        m.buf_end = 0;
        dsp.set_output(0, 0);
    }
]=] core_header "${core_header}")
starfox_write_spc_header("SNES_SPC.h" "${core_header}")
