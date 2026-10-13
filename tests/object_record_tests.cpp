#include "starfox/simulation/object_pool.hpp"
#include "starfox/simulation/map_vm.hpp"
#include "starfox/platform/nintendo_3ds/frame_profile.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string_view>

namespace starfox::simulation {
// Independent pre-cache bytewise bridge. Production has no bypass switch;
// compare complete CPU and pool state, including open bus and generations.
struct ObjectBridgeTestAccess {
    static void push(MapVm& map,bool cached,bool reuse_imported=false) {
        if(cached) {map.sync_objects_to_cpu(reuse_imported);return;}
        const auto active=map.objects_->active_handles(),free=map.objects_->free_handles();
        map.cpu_.write16(map.active_list_,active.empty()?0:map.original_object_pointer(active.front()));
        map.cpu_.write16(map.free_list_,free.empty()?0:map.object_base_+(free.front()-1)*map.object_size_);
        for(std::size_t n=0;n<active.size();++n) {
            const auto handle=active[n];const auto base=map.original_object_pointer(handle);
            const auto extended=map.extended_object_base_+(handle-1)*map.object_size_;
            map.cpu_.write16(base,n+1<active.size()?map.original_object_pointer(active[n+1]):0);
            map.cpu_.write16(base+2,n?map.original_object_pointer(active[n-1]):0);
            for(unsigned i=4;i<map.object_size_;++i)
                map.cpu_.write8(base+i,map.objects_->read_base_byte(handle,i));
            const auto& object=map.objects_->at(handle);
            map.cpu_.write16(base+6,map.original_object_pointer(object.attached));
            map.cpu_.write16(base+25,map.original_object_pointer(object.immune_object));
            map.cpu_.write16(base+27,map.original_object_pointer(object.collision_object));
            for(unsigned i=0;i<map.extended_object_bytes_;++i) map.cpu_.write8(extended+i,object.extended[i]);
            map.cpu_.write16(extended+19,map.original_object_pointer(object.fire_object));
        }
        for(std::size_t n=0;n<free.size();++n) map.cpu_.write16(map.object_base_+(free[n]-1)*map.object_size_,
            n+1<free.size()?map.object_base_+(free[n+1]-1)*map.object_size_:0);
    }
    static void pull(MapVm& map,bool cached) {
        if(cached) {map.sync_objects_from_cpu();return;}
        const auto list=[&](std::uint16_t pointer) {
            std::vector<ObjectHandle> out;std::array<bool,kMaximumObjects+1> seen{};
            while(pointer) {
                const auto h=map.native_object_handle(pointer);
                if(!h || seen[h]) throw std::runtime_error("invalid oracle list");
                seen[h]=true;out.push_back(h);pointer=map.cpu_.read16(pointer);
            }
            return out;
        };
        const auto active=list(map.cpu_.read16(map.active_list_)),free=list(map.cpu_.read16(map.free_list_));
        map.objects_->restore_lists(active,free);
        for(const auto h:active) {
            const auto base=map.original_object_pointer(h);
            const auto extended=map.extended_object_base_+(h-1)*map.object_size_;
            for(unsigned i=4;i<map.object_size_;++i) map.objects_->write_base_byte(h,i,map.cpu_.read8(base+i));
            auto& object=map.objects_->at(h);
            object.attached=map.object_handle(map.cpu_.read16(base+6));
            object.immune_object=map.object_handle(map.cpu_.read16(base+25));
            object.collision_object=map.object_handle(map.cpu_.read16(base+27));
            for(unsigned i=0;i<map.extended_object_bytes_;++i)
                map.objects_->write_path_byte(h,static_cast<std::uint8_t>(0x80+i),map.cpu_.read8(extended+i));
            object.fire_object=map.object_handle(map.cpu_.read16(extended+19));
        }
    }
    static auto hits(const MapVm& map) {return map.object_bridge_->push_hits+map.object_bridge_->pull_hits;}
    static auto image_hits(const MapVm& map) {return map.object_bridge_->image_hits;}
    static auto cache_bytes() {return sizeof(MapVm::ObjectBridgeCache);}
    static auto cpu_state(const MapVm& map) {return map.cpu_.save_state();}
    static auto call_strategy(MapVm& map,std::uint32_t address,ObjectHandle object,bool reuse) {
        return reuse?map.call_native_object_routine_reusing_import(address,object,1000):
            map.call_native_object_routine(address,object);
    }
    static auto call_bytewise(MapVm& map,std::uint32_t address,ObjectHandle object) {
        push(map,false);
        Wdc65816Registers registers;
        registers.x=map.original_object_pointer(object);registers.y=registers.x;
        registers.data_bank=0x7e;registers.status=0x24;
        const auto instructions=map.cpu_.call_long(address,registers,1000);
        pull(map,false);map.sync_display_from_cpu();return instructions;
    }
    static auto call_transfer(MapVm& map, std::uint32_t address,
        Wdc65816Registers& registers, bool cached, bool reuse,
        std::size_t limit = 1000, bool service_transfer = true) {
        if (cached) return reuse
            ? map.call_native_routine_reusing_import(address, registers, limit, service_transfer)
            : map.call_native_routine(address, registers, limit, service_transfer);
        push(map, false);
        const auto instructions = map.cpu_.call_long(address, registers, limit, service_transfer);
        pull(map, false);
        map.sync_display_from_cpu();
        return instructions;
    }
};
}

namespace {
using namespace starfox::simulation;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error{message};
}
std::uint64_t bridge_clock_calls{};
std::uint64_t bridge_clock() noexcept { return ++bridge_clock_calls; }
template<class F> void rejects(F operation) {
    bool rejected = false;
    try { operation(); } catch (const std::exception&) { rejected = true; }
    require(rejected, "invalid record/list was accepted");
}
void test_records(ObjectMemoryLayout layout) {
    const std::size_t base_size = layout == ObjectMemoryLayout::starfox_ex ? 57 : 56;
    const std::size_t extended_size = layout == ObjectMemoryLayout::starfox_ex ? 56 : 54;
    std::mt19937 random{0x65816};
    for (unsigned trial = 0; trial < 2048; ++trial) {
        ObjectPool bytewise{3, layout};
        const auto handle = bytewise.allocate_after();
        for (std::uint16_t i = 4; i < base_size; ++i)
            bytewise.write_base_byte(handle, i, static_cast<std::uint8_t>(random()));
        for (std::uint8_t i = 0; i < extended_size; ++i)
            bytewise.write_path_byte(handle, 0x80U + i, static_cast<std::uint8_t>(random()));
        auto& original = bytewise.at(handle);
        original.strategy_address |= std::uint32_t{static_cast<std::uint8_t>(random())} << 24U;
        original.open_al = static_cast<std::uint8_t>(random());
        original.fire_object = static_cast<ObjectHandle>(random());
        original.colour_table = static_cast<std::uint16_t>(random());
        original.sound1 = static_cast<std::uint8_t>(random());
        original.extended[54] = static_cast<std::uint8_t>(random());
        original.extended[55] = static_cast<std::uint8_t>(random());
        ObjectPool bulk = bytewise;

        std::array<std::uint8_t, 59> record;
        record.fill(0xa5);
        bulk.read_base_record(handle, std::span{record}.first(base_size));
        require(std::all_of(record.begin(), record.begin() + 4,
            [](auto byte) { return byte == 0xa5; }), "base export overwrote list pointers");
        require(std::all_of(record.begin() + base_size, record.end(),
            [](auto byte) { return byte == 0xa5; }), "base export overran record");
        for (std::uint16_t i = 4; i < base_size; ++i)
            require(record[i] == bytewise.read_base_byte(handle, i), "base export differs from byte accessor");
        for (auto& byte : record) byte = static_cast<std::uint8_t>(random());
        for (std::uint16_t i = 4; i < base_size; ++i)
            bytewise.write_base_byte(handle, i, record[i]);
        bulk.write_base_record(handle, std::span{record}.first(base_size));
        require(bytewise.save_state() == bulk.save_state(), "base import changed semantic/host fields");
        for (auto& byte : record) byte = static_cast<std::uint8_t>(random());
        for (std::uint8_t i = 0; i < extended_size; ++i)
            bytewise.write_path_byte(handle, 0x80U + i, record[i]);
        bulk.write_extended_record(handle, std::span{record}.first(extended_size));
        require(bytewise.save_state() == bulk.save_state(), "extended import changed unknown/shifted fields");

        if (trial == 0) {
            const auto before = bulk.save_state();
            rejects([&] { bulk.write_base_record(handle, std::span{record}.first(base_size - 1)); });
            rejects([&] { bulk.write_base_record(handle, record); });
            rejects([&] { bulk.write_extended_record(handle, std::span{record}.first(extended_size - 1)); });
            rejects([&] { bulk.write_extended_record(handle, record); });
            rejects([&] { bulk.read_base_record(2, std::span{record}.first(base_size)); });
            require(before == bulk.save_state(), "invalid record mutated pool");
        }
    }
}
void test_lists() {
    ObjectPool pool{8};
    std::array<std::uint64_t, 9> generation{};
    for (unsigned i = 0; i < 4; ++i) {
        const auto handle = pool.allocate_after();
        pool.at(handle).shape = static_cast<std::uint16_t>(100 + handle);
        generation[handle] = pool.generation(handle);
    }
    pool.restore_lists({6, 3, 5, 1}, {8, 4, 7, 2});
    require(pool.active_handles() == std::vector<ObjectHandle>({6, 3, 5, 1}), "active order changed");
    require(pool.free_handles() == std::vector<ObjectHandle>({8, 4, 7, 2}), "free order changed");
    require(pool.generation(1) == generation[1] && pool.generation(3) == generation[3], "retained generation changed");
    require(pool.generation(5) == 5 && pool.generation(6) == 6, "activation generation order changed");
    require(pool.at(1).shape == 101 && pool.at(3).shape == 103, "retained objects cleared");
    require(pool.at(5) == GameObject{} && pool.at(6) == GameObject{}, "free objects gained data");
    require(pool.lists_match(std::array<ObjectHandle,4>{6,3,5,1},std::array<ObjectHandle,4>{8,4,7,2}),"exact lists missed");
    require(!pool.lists_match(std::array<ObjectHandle,4>{6,5,3,1},std::array<ObjectHandle,4>{8,4,7,2}),"active reorder matched");
    require(!pool.lists_match(std::array<ObjectHandle,4>{6,3,5,1},std::array<ObjectHandle,4>{8,7,4,2}),"free reorder matched");
    const auto saved = pool.save_state();
    pool.restore_lists({6, 3, 5, 1}, {8, 4, 7, 2});
    require(saved == pool.save_state(), "unchanged lists changed generations/state");
    rejects([&] { pool.restore_lists({6, 3, 5, 1}, {8, 4, 7, 6}); });
    rejects([&] { pool.restore_lists({9, 3, 5, 1}, {8, 4, 7, 2}); });
    rejects([&] { pool.restore_lists({0, 3, 5, 1}, {8, 4, 7, 2}); });
    rejects([&] { pool.restore_lists({6, 3, 5, 1}, {8, 4, 7}); });
    require(saved == pool.save_state(), "invalid lists mutated pool");
    const auto reused = pool.allocate_after(3);
    require(reused == 8 && pool.generation(reused) == 7, "free reuse/generation changed");
}
void test_bridge(ObjectMemoryLayout layout,bool reuse_imported=false) {
    const bool ex=layout==ObjectMemoryLayout::starfox_ex;
    const unsigned capacity=ex?80:70,stride=ex?57:56,extended_size=ex?56:54;
    const auto symbols=starfox::assets::SymbolMap::parse(ex?
        "AL_SIZE $000039\nNUMBER_AL $000050\nALLST $001a00\nALFREELST $001a02\n":
        "AL_SIZE $000038\nNUMBER_AL $000046\nALLST $001a00\nALFREELST $001a02\n");
    const starfox::assets::RomImage rom{std::vector<std::uint8_t>(32768,0xea)};
    ObjectPool a{capacity,layout},b{capacity,layout};
    MapVm cached{rom,MapDatabase{rom,0x008000,0x008100},a,&symbols};
    MapVm oracle{rom,MapDatabase{rom,0x008000,0x008100},b,&symbols};
    using namespace starfox::platform::nintendo_3ds;
    FrameProfile bridge_profile(bridge_clock, 1000);
    ScopedFrameProfileActivation bridge_activation(bridge_profile);
    const auto profiled_bridge = [&](FramePhase phase, auto operation) {
        const auto before = bridge_profile.totals()[unsigned(phase)];
        const auto clocks_before = bridge_clock_calls;
        operation();
        const auto after = bridge_profile.totals()[unsigned(phase)];
#if defined(STARFOX_3DS_PROFILE_FRAMES)
        require(after.calls == before.calls + 1 && after.ticks == before.ticks + 1
            && after.maximum == 1 && bridge_clock_calls == clocks_before + 2,
            "Object bridge timing omitted/double-counted a full call or image hit");
#else
        require(after == before && bridge_clock_calls == clocks_before,
            "Disabled object bridge marker read the clock or changed counters");
#endif
    };
    unsigned bridge_step{};
    const auto same=[&] {
        ++bridge_step;
        require(a.save_state()==b.save_state(),"cached bridge changed pool/semantic mirrors/generations");
        const auto actual=cached.save_state(),expected=oracle.save_state();
        require(actual.size()==expected.size(),"cached bridge changed state size");
        if(actual!=expected) {
            const auto mismatch=std::mismatch(actual.begin(),actual.end(),expected.begin(),expected.end());
            std::cerr << (ex?"EX":"Original") << " bridge step " << bridge_step
                << " state offset " << (mismatch.first-actual.begin())
                << " actual " << unsigned(*mismatch.first) << " expected " << unsigned(*mismatch.second) << '\n';
            const auto cpu_actual=ObjectBridgeTestAccess::cpu_state(cached),cpu_expected=ObjectBridgeTestAccess::cpu_state(oracle);
            unsigned differences{};
            for(std::size_t i=0;i<cpu_actual.size() && i<cpu_expected.size() && differences<16;++i) {
                if(cpu_actual[i]!=cpu_expected[i]) {
                    ++differences;std::cerr << "CPU offset " << i << " actual " << unsigned(cpu_actual[i])
                        << " expected " << unsigned(cpu_expected[i]) << '\n';
                }
            }
            throw std::runtime_error{"cached bridge changed CPU memory/open bus"};
        }
    };
    const auto push=[&] {
        profiled_bridge(FramePhase::object_export,[&] {ObjectBridgeTestAccess::push(cached,true,reuse_imported);});
        ObjectBridgeTestAccess::push(oracle,false);same();
    };
    const auto pull=[&] {
        profiled_bridge(FramePhase::object_import,[&] {ObjectBridgeTestAccess::pull(cached,true);});
        ObjectBridgeTestAccess::pull(oracle,false);same();
    };
    // Empty/full pools, repeated cache hits and deliberately inconsistent host
    // mirrors: export alone must never qualify a skipped semantic import.
    push();pull();
    for(unsigned n=0;n<capacity;++n) {
        const auto h=a.allocate_after();require(h==b.allocate_after(),"fixture allocation differs");
        a.at(h).world_x=b.at(h).world_x=static_cast<std::int16_t>(n*173);
        a.at(h).sound1=b.at(h).sound1=17;
    }
    push();pull();push();pull();push();pull();
    require(ObjectBridgeTestAccess::hits(cached)>capacity,"bridge did not exercise exact reuse");
    if(reuse_imported) {
        require(ObjectBridgeTestAccess::image_hits(cached)>0,"complete imported image was never reused");
        // Import follows NEXT, but export also repairs PREV. Do not silently
        // retain a corrupted previous link inside the consecutive-call path.
        const auto pointer=0x0338U+(a.first_active()-1)*stride;
        cached.write_native_word(pointer+2,0x1234);oracle.write_native_word(pointer+2,0x1234);
        pull();const auto before=ObjectBridgeTestAccess::image_hits(cached);
        push();pull();
        require(ObjectBridgeTestAccess::image_hits(cached)==before,"noncanonical PREV image was reused");
        // A source WRAM write can use any low-bank mirror, not just bank 7E.
        const auto mirror=0x800000U+pointer+12U;
        cached.write_native_byte(mirror,0x51);oracle.write_native_byte(mirror,0x51);
        const auto before_write=ObjectBridgeTestAccess::image_hits(cached);
        push();pull();
        require(ObjectBridgeTestAccess::image_hits(cached)==before_write,"mirrored native write did not revoke import reuse");
        cached.write_native_byte(0x002100U,0x0f);oracle.write_native_byte(0x002100U,0x0f);
        const auto before_io=ObjectBridgeTestAccess::image_hits(cached);
        push();pull();
        require(ObjectBridgeTestAccess::image_hits(cached)==before_io,"I/O write did not revoke import reuse");
    }
    std::mt19937 random{0x51a65816U};
    for(unsigned trial=0;trial<256;++trial) {
        const auto h=a.first_active();
        if(h) {
            const auto value=static_cast<std::uint8_t>(random());
            a.write_base_byte(h,4+trial%(stride-4),value);b.write_base_byte(h,4+trial%(stride-4),value);
            a.at(h).sound1=b.at(h).sound1=static_cast<std::uint8_t>(value+1); // Mirror divergence.
            a.at(h).extended[55]=b.at(h).extended[55]=value; // Original's unexported tail.
        }
        if(trial%13==0 && h) {require(a.remove(h)==b.remove(h),"fixture removal differs");}
        if(trial%17==0) {require(a.allocate_after()==b.allocate_after(),"fixture reuse differs");}
        if(trial%19==0) {
            auto active=a.active_handles(),free=a.free_handles();
            std::reverse(active.begin(),active.end());std::reverse(free.begin(),free.end());
            a.restore_lists(active,free);b.restore_lists(active,free);
        }
        push();
        if(const auto active=a.first_active()) {
            const auto address=trial%2?0x0338U+(active-1)*stride+4+trial%(stride-4):
                0x7e2000U+(active-1)*stride+trial%extended_size;
            cached.write_native_byte(address,static_cast<std::uint8_t>(trial*91));
            oracle.write_native_byte(address,static_cast<std::uint8_t>(trial*91));
        }
        pull();push();pull();
        if(trial%31==0) {
            const auto state=cached.save_state();cached.load_state(state);oracle.load_state(state);
            push();pull(); // Fresh caches after restored CPU state.
        }
    }
    // Corrupt a native list, without letting a stale cache turn it into a hit.
    push();const auto first=a.first_active();const auto pointer=static_cast<std::uint16_t>(0x0338+(first-1)*stride);
    cached.write_native_word(pointer,pointer);oracle.write_native_word(pointer,pointer);
    const auto before=a.save_state();
    rejects([&]{ObjectBridgeTestAccess::pull(cached,true);});
    rejects([&]{ObjectBridgeTestAccess::pull(oracle,false);});
    require(before==a.save_state() && a.save_state()==b.save_state(),"invalid native list mutated cached pool");
    require(ObjectBridgeTestAccess::cache_bytes()<32768,"object bridge scratch exceeded bounded heap budget");
}
void test_bridge_io(ObjectMemoryLayout layout) {
    // Unsupported/custom I/O record locations must retain ordered callbacks;
    // the WRAM-only cache must not add, remove or reorder those accesses.
    const auto symbols=starfox::assets::SymbolMap::parse(layout==ObjectMemoryLayout::starfox_ex?
        "AL_SIZE $000039\nNUMBER_AL $000002\nXALBLKS $002100\n":
        "AL_SIZE $000038\nNUMBER_AL $000002\nXALBLKS $002100\n");
    const starfox::assets::RomImage rom{std::vector<std::uint8_t>(32768,0xea)};
    ObjectPool a{2,layout},b{2,layout};
    MapVm cached{rom,MapDatabase{rom,0x008000,0x008100},a,&symbols};
    MapVm oracle{rom,MapDatabase{rom,0x008000,0x008100},b,&symbols};
    for(unsigned n=0;n<2;++n) {
        const auto h=a.allocate_after();require(h==b.allocate_after(),"I/O allocation differs");
        for(unsigned i=0;i<kExtendedObjectBytes;++i)
            a.at(h).extended[i]=b.at(h).extended[i]=static_cast<std::uint8_t>(i*17+n);
    }
    for(unsigned n=0;n<8;++n) {
        ObjectBridgeTestAccess::push(cached,true,true);ObjectBridgeTestAccess::push(oracle,false);
        require(cached.save_state()==oracle.save_state(),"I/O export changed callbacks/open bus");
        ObjectBridgeTestAccess::pull(cached,true);ObjectBridgeTestAccess::pull(oracle,false);
        require(a.save_state()==b.save_state(),"I/O import changed objects");
        require(cached.save_state()==oracle.save_state(),"I/O import changed callbacks/open bus");
    }
    require(ObjectBridgeTestAccess::hits(cached)==0,"I/O records entered WRAM cache");
    require(ObjectBridgeTestAccess::image_hits(cached)==0,"I/O image entered import reuse");
}
void test_consecutive_calls(ObjectMemoryLayout layout) {
    const bool ex=layout==ObjectMemoryLayout::starfox_ex;
    const unsigned stride=ex?57:56;
    const auto symbols=starfox::assets::SymbolMap::parse(ex?
        "AL_SIZE $000039\nNUMBER_AL $000008\nALLST $001a00\nALFREELST $001a02\n":
        "AL_SIZE $000038\nNUMBER_AL $000008\nALLST $001a00\nALFREELST $001a02\n");
    std::vector<std::uint8_t> code(32768,0xea);
    // 8-bit INC of both world-X bytes through the current native X register.
    // Real interpreter writes, not a helper impersonating a strategy result.
    const std::array<std::uint8_t,7> routine{0xfe,0x0c,0x00,0xfe,0x0d,0x00,0x6b};
    std::copy(routine.begin(),routine.end(),code.begin());
    const starfox::assets::RomImage rom{std::move(code)};
    ObjectPool a{8,layout},b{8,layout};
    MapVm cached{rom,MapDatabase{rom,0x008100,0x008200},a,&symbols};
    MapVm oracle{rom,MapDatabase{rom,0x008100,0x008200},b,&symbols};
    for(unsigned n=0;n<4;++n) require(a.allocate_after()==b.allocate_after(),"strategy fixture allocation differs");
    std::uint64_t image_hits{};
    for(unsigned n=0;n<128;++n) {
        const auto object=a.first_active();
        if(n%8==0) a.at(object).world_z=b.at(object).world_z=static_cast<std::int16_t>(n*157);
        if(n%11==0) {
            const auto base=0x7e0338U+(object-1)*stride;
            cached.write_native_byte(base+4,n);oracle.write_native_byte(base+4,n);
        }
        if(n%13==0) {
            require(a.remove(object)==b.remove(object),"strategy fixture removal differs");
            require(a.allocate_after()==b.allocate_after(),"strategy fixture reuse differs");
        }
        const auto selected=a.first_active();
        const auto before_hits=ObjectBridgeTestAccess::image_hits(cached);
        require(ObjectBridgeTestAccess::call_strategy(cached,0x008000,selected,n!=0)
            ==ObjectBridgeTestAccess::call_bytewise(oracle,0x008000,selected),"consecutive strategies changed instructions");
        image_hits+=ObjectBridgeTestAccess::image_hits(cached)-before_hits;
        require(a.save_state()==b.save_state(),"consecutive strategies changed objects/lists/generations");
        require(cached.save_state()==oracle.save_state(),"consecutive strategies changed CPU/VM state");
        if(n%23==0) {
            const auto state=cached.save_state();cached.load_state(state);oracle.load_state(state);
        }
    }
    require(image_hits>32,"actual interpreter calls did not reuse the imported image");
}
void test_consecutive_transfers(ObjectMemoryLayout layout) {
    const bool ex = layout == ObjectMemoryLayout::starfox_ex;
    const unsigned stride = ex ? 57 : 56;
    const auto symbols = starfox::assets::SymbolMap::parse(ex
        ? "AL_SIZE $000039\nNUMBER_AL $000008\nALLST $001a00\nALFREELST $001a02\n"
        : "AL_SIZE $000038\nNUMBER_AL $000008\nALLST $001a00\nALFREELST $001a02\n");
    std::vector<std::uint8_t> code(32768, 0xea);
    // INC world-X, emit an APU command, update INIDISP and return altered A/Y.
    // Caller-supplied registers and transfer servicing must not be lost when
    // reusing the object image. The second entry never returns, testing expiry.
    const std::array<std::uint8_t, 16> routine{
        0xfe, 0x0c, 0x00, 0xe8, 0xa9, 0x37, 0x8d, 0x40,
        0x21, 0xa9, 0x0f, 0x8d, 0x00, 0x21, 0xc8, 0x6b};
    std::copy(routine.begin(), routine.end(), code.begin());
    code[0x40] = 0x80; code[0x41] = 0xfe; // BRA -2
    const starfox::assets::RomImage rom{std::move(code)};
    ObjectPool a{8, layout}, b{8, layout};
    MapVm cached{rom, MapDatabase{rom, 0x008100, 0x008200}, a, &symbols};
    MapVm oracle{rom, MapDatabase{rom, 0x008100, 0x008200}, b, &symbols};
    for (unsigned n = 0; n < 4; ++n)
        require(a.allocate_after() == b.allocate_after(), "transfer allocation differs");
    std::uint64_t image_hits{};
    const auto same_registers = [](const auto& x, const auto& y) {
        return x.a == y.a && x.x == y.x && x.y == y.y && x.direct == y.direct
            && x.stack == y.stack && x.data_bank == y.data_bank && x.status == y.status;
    };
    for (unsigned n = 0; n < 128; ++n) {
        const auto object = a.first_active();
        if (n % 8 == 0) a.at(object).world_z = b.at(object).world_z = n * 157;
        if (n % 11 == 0) {
            const auto base = 0x800338U + (object - 1) * stride;
            cached.write_native_byte(base + 4, n); oracle.write_native_byte(base + 4, n);
        }
        if (n % 13 == 0) {
            require(a.remove(object) == b.remove(object), "transfer removal differs");
            require(a.allocate_after() == b.allocate_after(), "transfer reuse differs");
        }
        if (n % 17 == 0) {
            cached.write_native_byte(0x002100, 0x0a); oracle.write_native_byte(0x002100, 0x0a);
        }
        Wdc65816Registers actual;
        actual.x = 0x0338U + (a.first_active() - 1) * stride;
        actual.y = n; actual.status = 0x24;
        auto expected = actual;
        const auto before = ObjectBridgeTestAccess::image_hits(cached);
        const bool service_transfer = (n % 2) != 0;
        require(ObjectBridgeTestAccess::call_transfer(cached, 0x008000, actual, true, n != 0, 1000, service_transfer)
            == ObjectBridgeTestAccess::call_transfer(oracle, 0x008000, expected, false, false, 1000, service_transfer),
            "consecutive transfer instructions differ");
        image_hits += ObjectBridgeTestAccess::image_hits(cached) - before;
        require(same_registers(actual, expected), "transfer returned different registers");
        require(a.save_state() == b.save_state(), "transfer changed objects/lists/generations");
        require(cached.save_state() == oracle.save_state(), "transfer changed CPU/PPU/APU/VM state");
        if (n % 23 == 0) {
            const auto state = cached.save_state(); cached.load_state(state); oracle.load_state(state);
        }
        if (n == 63) {
            rejects([&] { ObjectBridgeTestAccess::call_transfer(cached, 0x008040, actual, true, true, 16); });
            rejects([&] { ObjectBridgeTestAccess::call_transfer(oracle, 0x008040, expected, false, false, 16); });
            require(cached.save_state() == oracle.save_state(), "failed transfer changed CPU/VM state");
            const auto failed_hits = ObjectBridgeTestAccess::image_hits(cached);
            actual = expected = Wdc65816Registers{}; actual.status = expected.status = 0x24;
            actual.x = expected.x = 0x0338U + (a.first_active() - 1) * stride;
            ObjectBridgeTestAccess::call_transfer(cached, 0x008000, actual, true, true);
            ObjectBridgeTestAccess::call_transfer(oracle, 0x008000, expected, false, false);
            require(ObjectBridgeTestAccess::image_hits(cached) == failed_hits, "failed transfer left an import lease");
            require(cached.save_state() == oracle.save_state(), "transfer recovery changed CPU/VM state");
        }
    }
    require(image_hits > 32, "transfer interpreter calls never reused the imported image");
}
void benchmark() {
    ObjectPool pool{1, ObjectMemoryLayout::starfox_ex};
    const auto handle = pool.allocate_after();
    std::array<std::uint8_t, 57> bytes{};
    std::uint64_t checksum{};
    for (const bool bulk : {false, true, true, false}) {
        const auto begin = std::chrono::steady_clock::now();
        for (unsigned iteration = 0; iteration < 200000; ++iteration) {
            pool.at(handle).count = static_cast<std::uint8_t>(iteration);
            if (bulk) {
                pool.read_base_record(handle, bytes);
                pool.write_base_record(handle, bytes);
                pool.write_extended_record(handle, std::span{bytes}.first(56));
            } else {
                for (std::uint16_t i = 4; i < 57; ++i) bytes[i] = pool.read_base_byte(handle, i);
                for (std::uint16_t i = 4; i < 57; ++i) pool.write_base_byte(handle, i, bytes[i]);
                for (std::uint8_t i = 0; i < 56; ++i) pool.write_path_byte(handle, 0x80U + i, bytes[i]);
            }
            checksum += pool.at(handle).count;
        }
        std::cout << (bulk ? "bulk" : "bytewise") << ','
            << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count()
            << ',' << checksum << '\n';
    }
}
} // namespace
int main(int argc, char** argv) try {
    test_records(ObjectMemoryLayout::original);
    test_records(ObjectMemoryLayout::starfox_ex);
    test_lists();
    test_bridge(ObjectMemoryLayout::original);
    test_bridge(ObjectMemoryLayout::starfox_ex);
    test_bridge(ObjectMemoryLayout::original,true);
    test_bridge(ObjectMemoryLayout::starfox_ex,true);
    test_bridge_io(ObjectMemoryLayout::original);
    test_bridge_io(ObjectMemoryLayout::starfox_ex);
    test_consecutive_calls(ObjectMemoryLayout::original);
    test_consecutive_calls(ObjectMemoryLayout::starfox_ex);
    test_consecutive_transfers(ObjectMemoryLayout::original);
    test_consecutive_transfers(ObjectMemoryLayout::starfox_ex);
    if (argc == 2 && std::string_view{argv[1]} == "--benchmark") benchmark();
    std::cout << "PASS original/EX native object records, exact bridge/import reuse, I/O ordering and list generations; cache_bytes="
        << ObjectBridgeTestAccess::cache_bytes() << '\n';
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
