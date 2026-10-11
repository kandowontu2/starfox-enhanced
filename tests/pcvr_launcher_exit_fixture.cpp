#include <cstdio>
#include <cstdlib>
#include <limits>

// Launcher-only fixture, never installed or linked into the actual game.
int main(int argc, char** argv) {
    std::puts("PCVR launcher fixture invoked");
    std::fputs("PCVR fixture stderr retained\n", stderr);
    for (int i=1; i<argc; ++i) std::printf("arg[%d]=%s\n", i, argv[i]);
    const char* value=std::getenv("SFE_PCVR_TEST_EXIT_CODE");
    if (!value) return 0;
    char* end=nullptr;
    const long long code=std::strtoll(value, &end, 10);
    if (end==value || *end || code<std::numeric_limits<int>::min()
        || code>std::numeric_limits<int>::max()) return 99;
    return static_cast<int>(code);
}
