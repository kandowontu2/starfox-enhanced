#include "starfox/assets/runtime_import.hpp"
#include "starfox/assets/bps.hpp"
#include "starfox/assets/runtime_bundle.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

namespace starfox::assets {
namespace {
struct RetailVariant {
    const char* name;
    std::uint32_t crc;
    int patch;
};
constexpr std::size_t retail_size = 1U << 20U;
constexpr std::uint32_t retail_v12_crc = 0x8fc4e6d0U;
constexpr std::array variants{
    RetailVariant{"Star Fox (USA) (Rev 2)", retail_v12_crc, 0},
    RetailVariant{"Star Fox (Japan)", 0x41a60b3fU, 120},
    RetailVariant{"Star Fox (Japan) (Rev 1)", 0xad668a41U, 121},
    RetailVariant{"Star Fox (USA)", 0x0bae0941U, 122},
    RetailVariant{"Star Fox (USA) (Rev 1)", 0xb18676b2U, 123},
    RetailVariant{"Starwing (Europe)", 0x865f1a71U, 124},
    RetailVariant{"Starwing (Europe) (Rev 1)", 0xba64da2bU, 125},
    RetailVariant{"Starwing (Germany)", 0xb48ca238U, 126},
};
}

RuntimeImport prepare_runtime_input(std::span<const std::uint8_t> input,
        std::span<const std::uint8_t> (*resource)(int)) {
    if (input.empty() || input.size() > 64U * 1024U * 1024U)
        throw std::runtime_error("Select a supported Star Fox/Starwing ROM or Starfox-Assets.BIN (maximum 64 MiB)");
    const auto manifest = runtime_companion_manifest(resource);
    constexpr std::string_view magic = "SFOXAS01";
    if (input.size() >= magic.size() && std::equal(magic.begin(), magic.end(), input.begin())) {
        (void)decode_runtime_bundle(input, manifest);
        return {{input.begin(), input.end()}, "Starfox-Assets.BIN"};
    }
    if (input.size() == retail_size + 512U) input = input.subspan(512U);
    if (input.size() != retail_size)
        throw std::runtime_error("Select an extracted .sfc/.smc ROM or Starfox-Assets.BIN, not a ZIP archive");
    const auto checksum = crc32(input);
    const auto variant = std::find_if(variants.begin(), variants.end(),
        [checksum](const auto& value) { return value.crc == checksum; });
    if (variant == variants.end())
        throw std::runtime_error("Input is not a supported unmodified Star Fox/Starwing ROM");
    auto retail = variant->patch ? apply_bps_patch(input, resource(variant->patch))
                                : std::vector<std::uint8_t>(input.begin(), input.end());
    if (retail.size() != retail_size || crc32(retail) != retail_v12_crc)
        throw std::runtime_error("Regional ROM canonicalization failed");
    const auto text = [resource](int id) {
        const auto bytes = resource(id);
        return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    };
    RuntimeBundlePayload payload{apply_bps_patch(retail, resource(101)), text(102),
        apply_bps_patch(retail, resource(108)), text(109)};
    auto bundle = encode_runtime_bundle(payload, manifest);
    (void)decode_runtime_bundle(bundle, manifest);
    return {std::move(bundle), variant->name};
}
} // namespace starfox::assets
