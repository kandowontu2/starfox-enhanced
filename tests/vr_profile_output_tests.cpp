#include "starfox/vr/profile_csv.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition,const char* message) {
    if(!condition) throw std::runtime_error(message);
}
}

int main() try {
    starfox::vr::ProfileCsvOutput output;
    require(!output.enabled(),"Default profile output must be disabled when no path is configured");
    require(output.good(),"Default profile output stream should begin in good state");

    const auto unique=std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path=std::filesystem::temp_directory_path()
        /("starfox-profile-output-"+std::to_string(unique)+".csv");
    struct RemoveFile {
        std::filesystem::path path;
        ~RemoveFile() {std::error_code error;std::filesystem::remove(path,error);}
    } remove{path};
    require(output.open(path),"Explicit profile path did not open");
    require(output.enabled(),"Opened profile output did not become active");
    output.stream()<<"frame_index,gpu_left_ms\n1,0.125\n";
    require(output.flush(),"Profile output flush reported a write failure");
    std::ifstream input(path);
    const std::string contents((std::istreambuf_iterator<char>(input)),{});
    require(contents=="frame_index,gpu_left_ms\n1,0.125\n",
        "Profile output did not preserve the written CSV data");
    return 0;
} catch(...) {
    return 1;
}
