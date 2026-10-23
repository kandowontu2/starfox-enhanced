# Adapted from Esteban PDN's Starwing 3DS v1.1 (0863e36), with author permission.
# Lift one guarded RAM-only SPC instruction pattern; retain the pinned interpreter.
file(READ "${STARFOX_SPC_SOURCE}/SNES_SPC.h" core_header)
set(native_header_anchor "    int sample_count() const;")
string(FIND "${core_header}" "${native_header_anchor}" native_header_position)
if(native_header_position EQUAL -1)
    message(FATAL_ERROR "Pinned SPC native block interface needs review")
endif()
string(REPLACE "${native_header_anchor}" [=[
    int sample_count() const;
    void enable_native_blocks(bool enabled) { native_blocks_ = enabled; }
    unsigned long long native_block_count() const { return native_block_count_; }
private:
    bool native_blocks_ = false;
    unsigned long long native_block_count_ = 0;
public:
]=] core_header "${core_header}")
starfox_write_spc_header("SNES_SPC.h" "${core_header}")

file(READ "${STARFOX_SPC_SOURCE}/SPC_CPU.h" cpu_source)
string(REPLACE "uint8_t* const ram = RAM;" "uint8_t* const ram = RAM;\n    uint8_t const* const native_entry = native_blocks_ ? ram+0x624 : 0;" cpu_source "${cpu_source}")
set(native_anchor "\topcode = *pc;")
string(FIND "${cpu_source}" "${native_anchor}" native_position)
if(native_position EQUAL -1)
    message(FATAL_ERROR "Pinned SPC native block entry needs review")
endif()
string(REPLACE "${native_anchor}" [=[
    #if !SPC_MORE_ACCURACY && defined(NDEBUG)
    // No I/O, ROM writes, self-modification or event boundary may enter here.
    if (pc == native_entry && rel_time <= -53
        && pc[0]==0xaa && pc[3]==0x8a && pc[6]==0x2b && pc[8]==0x2b
        && pc[10]==0xe4 && pc[12]==0x25 && pc[15]==0x08 && pc[17]==0xd6
        && pc[20]==0xfc && pc[21]==0xe4 && pc[23]==0x08 && pc[25]==0xd6
        && pc[28]==0xfc && pc[29]==0x1d && pc[30]==0xd0 && pc[31]==0xe0)
    {
        unsigned bit0=GET_LE16(pc+1), bit1=GET_LE16(pc+4);
        unsigned and_addr=GET_LE16(pc+13);
        unsigned write0=GET_LE16(pc+18)+y;
        unsigned write1=GET_LE16(pc+26)+uint8_t(y+1);
        unsigned code_addr=GET_PC();
        if (unsigned((bit0&0x1fff)-0xf0)>=16 && unsigned((bit1&0x1fff)-0xf0)>=16
            && unsigned(and_addr-0xf0)>=16
            && (dp || (pc[7]<0xf0 && pc[9]<0xf0 && pc[11]<0xf0 && pc[22]<0xf0))
            && unsigned(write0-0x200)<0xfdc0 && unsigned(write1-0x200)<0xfdc0
            && unsigned(write0-code_addr)>=32 && unsigned(write1-code_addr)>=32)
        {
            c=(((ram[bit0&0x1fff]>>(bit0>>13))
                ^ (ram[bit1&0x1fff]>>(bit1>>13)))<<8)&0x100;
            unsigned rotate0=ram[dp+pc[7]]<<1;
            ram[dp+pc[7]]=uint8_t(rotate0|(c>>8&1));c=rotate0;
            unsigned rotate1=ram[dp+pc[9]]<<1;
            ram[dp+pc[9]]=uint8_t(rotate1|(c>>8&1));c=rotate1;
            a=(ram[dp+pc[11]]&ram[and_addr])|pc[16];
            ram[write0]=uint8_t(a);y=uint8_t(y+1);
            a=ram[dp+pc[22]]|pc[24];
            ram[write1]=uint8_t(a);y=uint8_t(y+1);
            nz=x-1;x=uint8_t(nz);
            rel_time+=53;
            if (!x) { rel_time-=2;pc+=32; }
            ++native_block_count_;
            goto loop;
        }
    }

    #endif
    opcode = *pc;
]=] cpu_source "${cpu_source}")
starfox_write_spc_header("SPC_CPU.h" "${cpu_source}")
