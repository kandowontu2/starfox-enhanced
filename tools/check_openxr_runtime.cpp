#include "starfox/vr/application.hpp"
#include <charconv>
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
template<class T>
bool parse_positive(std::string_view text, T maximum, T& value) {
    if (text.empty()) return false;
    T parsed{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()
        || parsed == 0 || parsed > maximum) return false;
    value = parsed;
    return true;
}

void usage() {
    std::cout << "Usage: starfox_vr_runtime_check [--frames 1..1000000] [--seconds 1..3600] [diagnostic options]\n"
        "Default diagnostic duration: 120 frames or 30 seconds, whichever comes first.\n"
        "Supplying only --frames or --seconds disables the other default limit.\n";
}
}

int main(int argc, char** argv) {
    std::optional<unsigned> frames;
    std::optional<unsigned> seconds;
    std::vector<std::string> forwarded;
    if (argc > 0) forwarded.emplace_back(argv[0]);

    for (int i = 1; i < argc; ++i) {
        const std::string_view option = argv[i];
        if (option == "--help" || option == "-h") {
            usage();
            return 0;
        }
        if (option == "--frames" || option == "--seconds") {
            if (i + 1 >= argc) {
                std::cerr << option << " requires a value\n";
                return 2;
            }
            const std::string_view text = argv[++i];
            unsigned value{};
            const unsigned maximum = option == "--frames" ? 1'000'000U : 3'600U;
            if (!parse_positive(text, maximum, value)) {
                std::cerr << option << " must be between 1 and " << maximum << "\n";
                return 2;
            }
            auto& destination = option == "--frames" ? frames : seconds;
            if (destination) {
                std::cerr << option << " may be specified only once\n";
                return 2;
            }
            destination = value;
            continue;
        }
        forwarded.emplace_back(argv[i]);
    }

    starfox::vr::ApplicationHost host;
    if (frames || seconds) {
        host.frame_limit = frames.value_or(0U);
        host.time_limit = seconds
            ? std::chrono::seconds(*seconds) : std::chrono::seconds(0);
    }
    std::vector<char*> application_argv;
    application_argv.reserve(forwarded.size());
    for (auto& argument : forwarded) application_argv.push_back(argument.data());
    const auto application_argc = static_cast<int>(application_argv.size());
    application_argv.push_back(nullptr);
    return starfox::vr::run_application(
        application_argc, application_argv.data(), host);
}
