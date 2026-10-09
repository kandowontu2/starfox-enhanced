namespace {

std::string atomic_file_bytes(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{input}, {}};
}

std::string atomic_collision_path;
SDL_IOStream* SDLCALL collision_open(const char* path, const char* mode) {
    if (atomic_collision_path.empty()) {
        atomic_collision_path = path;
        auto* sentinel = SDL_IOFromFile(path, "wbx");
        require(sentinel != nullptr, "collision fixture could not create its sentinel");
        require(SDL_WriteIO(sentinel, "other writer", 12) == 12 && SDL_CloseIO(sentinel),
            "collision fixture did not write its sentinel");
        return nullptr;
    }
    return SDL_IOFromFile(path, mode);
}

void check_atomic_files() {
    using starfox::app::AtomicFile;
    using starfox::app::AtomicFileIo;
    const auto root = std::filesystem::temp_directory_path() /
        ("sfe-atomic-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    require(std::filesystem::create_directory(root), "atomic test directory already exists");
    const auto path = root / std::filesystem::path{u8"settings-\u96ea.cfg"};
    {
        AtomicFile file{path};
        require(file.write("original complete settings\n") && file.commit(),
            "initial atomic config commit failed");
        require(!file.commit() && !file.write("stale"), "committed transaction was reused");
    }
    const auto original = atomic_file_bytes(path);
    const auto no_temporaries = [&] {
        const auto count = std::distance(std::filesystem::directory_iterator{root},
            std::filesystem::directory_iterator{});
        require(count == 1, "atomic save leaked or removed a temporary file");
    };
    {
        AtomicFile interrupted{path};
        require(interrupted.write("partial new settings"), "abandoned write failed");
        require(atomic_file_bytes(path) == original, "unfinished write truncated the old config");
    }
    no_temporaries();
    for (unsigned fault = 0; fault != 5; ++fault) {
        AtomicFileIo io;
        switch (fault) {
        case 0:
            io.open = +[](const char*, const char*) -> SDL_IOStream* { return nullptr; };
            break;
        case 1:
            io.write = +[](SDL_IOStream* stream, const void* bytes, std::size_t size) {
                return SDL_WriteIO(stream, bytes, size / 2);
            };
            break;
        case 2:
            io.flush = +[](SDL_IOStream*) { return false; };
            break;
        case 3:
            io.close = +[](SDL_IOStream* stream) {
                SDL_CloseIO(stream);
                return false;
            };
            break;
        case 4:
            io.rename = +[](const char*, const char*) { return false; };
            break;
        }
        {
            AtomicFile failed{path, io};
            const bool wrote = failed.write("complete replacement\n");
            const bool committed = failed.commit();
            require(!committed && (fault >= 2 || !wrote), "injected I/O failure was reported as success");
        }
        require(atomic_file_bytes(path) == original, "failed save destroyed the last complete config");
        no_temporaries();
    }
    {
        AtomicFile first{path}, second{path};
        require(first.write("first\n") && second.write("second\n"), "overlapping writers failed");
        require(atomic_file_bytes(path) == original, "overlapping writers published before commit");
        require(first.commit() && atomic_file_bytes(path) == "first\n", "first commit was not complete");
        require(second.commit() && atomic_file_bytes(path) == "second\n", "second writer reused another's temporary");
    }
    no_temporaries();
    {
        AtomicFileIo io;
        io.open = collision_open;
        AtomicFile file{path, io};
        require(file.write("after collision\n") && file.commit(), "exclusive-create collision was not retried");
        require(atomic_file_bytes(atomic_collision_path) == "other writer",
            "exclusive-create collision overwrote or removed another writer's file");
    }
    require(std::filesystem::remove(atomic_collision_path), "collision fixture cleanup failed");
    atomic_collision_path.clear();
    no_temporaries();
    {
        AtomicFile empty{path};
        require(empty.write({}) && empty.commit() && atomic_file_bytes(path).empty(),
            "empty atomic payload was not committed");
    }
    const auto blocked = root / "blocked";
    require(std::filesystem::create_directory(blocked), "blocked directory fixture failed");
    {
        AtomicFile file{blocked};
        require(!file.write("not a directory") && !file.commit()
            && std::filesystem::is_directory(blocked), "atomic save replaced a directory");
        AtomicFile bad_parent{path / "child.cfg"};
        require(!bad_parent.write("not a parent") && !bad_parent.commit(),
            "atomic save accepted a regular file as its parent");
    }
    require(std::filesystem::remove(blocked), "blocked fixture cleanup failed");
    const auto linked = root / "linked.cfg";
    std::error_code link_error;
    std::filesystem::create_symlink(path, linked, link_error);
    if (!link_error) {
        AtomicFile file{linked};
        require(!file.write("replace symlink") && !file.commit()
            && std::filesystem::is_symlink(std::filesystem::symlink_status(linked))
            && atomic_file_bytes(path).empty(), "atomic save replaced an unrelated symlink");
        require(std::filesystem::remove(linked), "symlink fixture cleanup failed");
    }
    no_temporaries();
    require(std::filesystem::remove(path) && std::filesystem::remove(root),
        "atomic fixture cleanup failed");
    std::cout << "Atomic config: abandonment, 5 I/O faults, overlapping writers, Unicode and collision checks passed\n";
}

} // namespace
