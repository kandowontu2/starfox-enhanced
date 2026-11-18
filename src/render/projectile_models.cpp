#include "starfox/render/projectile_models.hpp"
#include "starfox/render/asteroid_models.hpp"
#include "starfox/render/software_renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <utility>
#include <memory>
#include <mutex>
#include <numbers>
#include <vector>

namespace starfox::render {
namespace {

// Synthetic header addresses sit outside the cartridge window (the asteroid
// models use $ff0000 up).
constexpr std::uint32_t kModelAddressBase = 0xfe0000U;
// Model units per sprite texel.
constexpr double kTexelUnits = 16.0;
constexpr double kPi = std::numbers::pi;

enum class Kind : std::uint8_t {orb, bomb, ring, oval};
struct Known {std::uint32_t hash; Kind kind;};
// render::asteroid_texture_hash of each 32x32 frame; Original and EX share
// the texels (starfox_texture_dump's textures.csv lists them).
constexpr std::array<Known, 11> kKnown{{
    {0x8949ee9aU, Kind::orb},  // PLAYERBEAM: the top laser upgrade's pulse.
    {0xf8efc30bU, Kind::orb},  // Plasma ball, blue frame (plasma1).
    {0x20168bcaU, Kind::orb},  // Plasma ball, red frame (plasma2).
    {0xf47bd953U, Kind::bomb}, // Nova Bomb frames (specialweapon1-3).
    {0x9fc9b2b1U, Kind::bomb},
    {0xf8ad755cU, Kind::bomb},
    {0x002491ddU, Kind::ring}, // RINGLASER, blue frame (ring1).
    {0xe0252575U, Kind::ring}, // RINGLASER, orange frame (ring2).
    {0xdf1fda83U, Kind::oval}, // OVALBEAM, blue frame (oval1).
    {0x571ba6edU, Kind::oval}, // OVALBEAM, orange frame (oval2).
    {0x91db379dU, Kind::oval}, // OVALBEAM, the round flash between them (oval3).
}};

// The sprite frame's texels about the texture centre, where the cartridge
// centres the sprite. Positions are in texels, +y down.
class Sprite {
public:
    explicit Sprite(const assets::TextureImage& texture)
        : width_(static_cast<int>(texture.u_mask) + 1), height_(static_cast<int>(texture.v_mask) + 1),
          texels_(texture.texels) {}
    [[nodiscard]] std::uint8_t at(double x, double y) const noexcept {
        const auto column = static_cast<int>(std::floor(x + width_ / 2.0));
        const auto row = static_cast<int>(std::floor(y + height_ / 2.0));
        if (column < 0 || row < 0 || column >= width_ || row >= height_) return 0U;
        const auto index = static_cast<std::size_t>(row * width_ + column);
        return index < texels_.size() ? static_cast<std::uint8_t>(texels_[index] & 0x0fU) : 0U;
    }
    // Distance from the centre to the outline along an angle: the last opaque
    // texel before a gap of over a texel, so stray sparks off the ball (the
    // red plasma frame's arcs) stay out of the outline.
    [[nodiscard]] double edge(double angle) const noexcept {
        const auto c = std::cos(angle), s = std::sin(angle);
        double last = 0.0, gap = 0.0;
        bool seen = false;
        for (double r = 0.0; r < std::max(width_, height_) * 0.75; r += 0.25) {
            if (at(r * c, r * s) != 0U) {last = r; gap = 0.0; seen = true;}
            else if (seen && (gap += 0.25) > 1.25) break;
        }
        return seen ? last + 0.5 : 0.0;
    }
    // Distance from the centre to a ring's hole edge along an angle: the
    // first opaque texel. Zero when the middle is filled.
    [[nodiscard]] double inner(double angle) const noexcept {
        const auto c = std::cos(angle), s = std::sin(angle);
        for (double r = 0.0; r < std::max(width_, height_) * 0.75; r += 0.25) {
            if (at(r * c, r * s) != 0U) return r;
        }
        return 0.0;
    }
    // A ring's glow as bands across its tube: the most common slot between
    // each pair of `bounds`, fractions of the way from the outer edge to the
    // hole along the texel's own angle.
    [[nodiscard]] std::vector<std::uint8_t> tube_bands(const std::vector<double>& bounds) const {
        const auto count = bounds.size() - 1U;
        std::vector<std::array<std::size_t, 16>> votes(count);
        for (int row = 0; row < height_; ++row) {
            for (int column = 0; column < width_; ++column) {
                const auto x = column + 0.5 - width_ / 2.0, y = row + 0.5 - height_ / 2.0;
                const auto slot = at(x, y);
                const auto angle = std::atan2(y, x);
                const auto outer = edge(angle), hole = inner(angle);
                if (slot == 0U || outer - hole <= 0.0) continue;
                const auto t = std::clamp((outer - std::hypot(x, y)) / (outer - hole), 0.0, 1.0);
                std::size_t band = 0;
                while (band + 1U < count && t >= bounds[band + 1U]) ++band;
                ++votes[band][slot];
            }
        }
        std::vector<std::uint8_t> result(count, 0U);
        for (std::size_t band = 0; band < count; ++band) {
            const auto best = std::max_element(votes[band].begin() + 1, votes[band].end());
            if (*best != 0U) result[band] = static_cast<std::uint8_t>(best - votes[band].begin());
        }
        // An empty band takes its neighbour's colour.
        for (std::size_t band = 1; band < count; ++band) if (result[band] == 0U) result[band] = result[band - 1U];
        for (std::size_t band = count - 1U; band-- > 0U;) if (result[band] == 0U) result[band] = result[band + 1U];
        return result;
    }
    // Palette slot at a point, stepping in toward the centre past a hole.
    [[nodiscard]] std::uint8_t colour(double x, double y) const noexcept {
        const auto r = std::hypot(x, y);
        for (double t = 1.0; t > 0.0; t -= 0.5 / std::max(r, 0.5)) {
            if (const auto slot = at(x * t, y * t); slot != 0U) return slot;
        }
        return at(0.0, 0.0);
    }
    // The sprite's glow as concentric bands: the most common slot in each of
    // `count` rings, measured as a fraction of the outline's distance along
    // the texel's own angle, so a ragged or octagonal edge still maps to the
    // last band. Sparkles inside a band give way to its main colour.
    [[nodiscard]] std::vector<std::uint8_t> bands(std::size_t count) const {
        std::vector<std::array<std::size_t, 16>> votes(count);
        for (int row = 0; row < height_; ++row) {
            for (int column = 0; column < width_; ++column) {
                const auto x = column + 0.5 - width_ / 2.0, y = row + 0.5 - height_ / 2.0;
                const auto slot = at(x, y);
                const auto outline = edge(std::atan2(y, x));
                if (slot == 0U || outline <= 0.0) continue;
                const auto band = std::min(count - 1U, static_cast<std::size_t>(std::hypot(x, y) / outline * count));
                ++votes[band][slot];
            }
        }
        std::vector<std::uint8_t> result(count, at(0.0, 0.0));
        for (std::size_t band = 0; band < count; ++band) {
            const auto best = std::max_element(votes[band].begin() + 1, votes[band].end());
            if (*best != 0U) result[band] = static_cast<std::uint8_t>(best - votes[band].begin());
            else if (band != 0U) result[band] = result[band - 1U];
        }
        return result;
    }
    [[nodiscard]] int width() const noexcept {return width_;}
private:
    int width_, height_;
    const std::vector<std::uint8_t>& texels_;
};

struct Point {double x{}, y{}, z{};};

class Builder {
public:
    explicit Builder(const Sprite& sprite) : sprite_(sprite) {}
    std::uint8_t vertex(double x, double y, double z) {
        points_.push_back({x, y, z});
        shape_.vertices.push_back({static_cast<std::int32_t>(std::lround(x * kTexelUnits)),
            static_cast<std::int32_t>(std::lround(y * kTexelUnits)),
            static_cast<std::int32_t>(std::lround(z * kTexelUnits))});
        return static_cast<std::uint8_t>(shape_.vertices.size() - 1U);
    }
    // A triangle coloured by the sprite at (sample_x, sample_y).
    void face(std::uint8_t a, std::uint8_t b, std::uint8_t c, const Point& inside, double sample_x, double sample_y) {
        slot_face(a, b, c, inside, sprite_.colour(sample_x, sample_y));
    }
    // A triangle in palette slot `slot`, wound so its (b-a)x(c-a) points away
    // from `inside`, with the retail inward normal.
    void slot_face(std::uint8_t a, std::uint8_t b, std::uint8_t c, const Point& inside, std::uint8_t slot) {
        const auto& p = points_;
        const Point u{p[b].x - p[a].x, p[b].y - p[a].y, p[b].z - p[a].z};
        const Point v{p[c].x - p[a].x, p[c].y - p[a].y, p[c].z - p[a].z};
        Point n{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
        const Point centre{(p[a].x + p[b].x + p[c].x) / 3.0, (p[a].y + p[b].y + p[c].y) / 3.0, (p[a].z + p[b].z + p[c].z) / 3.0};
        const Point out{centre.x - inside.x, centre.y - inside.y, centre.z - inside.z};
        if (n.x * out.x + n.y * out.y + n.z * out.z < 0.0) {std::swap(b, c); n = {-n.x, -n.y, -n.z};}
        const auto length = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (length < 1e-9) return;
        shape_.visibilities.push_back({a, b, c});
        assets::Face result;
        result.visibility_index = static_cast<std::int16_t>(shape_.visibilities.size() - 1U);
        result.colour_id = colour_id(slot);
        result.normal = {static_cast<std::int32_t>(std::lround(-n.x / length * 127.0)),
            static_cast<std::int32_t>(std::lround(-n.y / length * 127.0)),
            static_cast<std::int32_t>(std::lround(-n.z / length * 127.0))};
        result.vertex_indices = {a, b, c};
        shape_.faces.push_back(std::move(result));
    }
    // A quad a-b-c-d (in order round it) in one colour, sampled at its middle.
    void quad(std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d, const Point& inside) {
        const auto& p = points_;
        const auto x = (p[a].x + p[b].x + p[c].x + p[d].x) / 4.0, y = (p[a].y + p[b].y + p[c].y + p[d].y) / 4.0;
        face(a, b, c, inside, x, y);
        face(a, c, d, inside, x, y);
    }
    [[nodiscard]] const Point& point(std::uint8_t index) const {return points_[index];}
    assets::Shape finish(std::uint32_t address) {
        std::int32_t extent = 1;
        for (const auto& v : shape_.vertices) extent = std::max({extent, std::abs(v.x), std::abs(v.y), std::abs(v.z)});
        shape_.name = "PROJECTILE_MODEL_" + std::to_string(address - kModelAddressBase);
        shape_.header.address = address;
        shape_.header.size = static_cast<std::int16_t>(extent);
        shape_.header.radius = static_cast<std::uint16_t>(extent);
        shape_.header.x_max = shape_.header.y_max = shape_.header.z_max = static_cast<std::int16_t>(extent);
        return std::move(shape_);
    }
private:
    std::uint8_t colour_id(std::uint8_t slot) {
        // COLSMOOTH: an unlit solid palette slot, as the glowing sprites were.
        const auto word = static_cast<std::uint16_t>(0xc000U | slot);
        const auto found = std::find(shape_.colour_words.begin(), shape_.colour_words.end(), word);
        if (found != shape_.colour_words.end()) return static_cast<std::uint8_t>(found - shape_.colour_words.begin());
        shape_.colour_words.push_back(word);
        return static_cast<std::uint8_t>(shape_.colour_words.size() - 1U);
    }
    const Sprite& sprite_;
    assets::Shape shape_;
    std::vector<Point> points_;
};

// A flatter cap behind the outline row so the model stays closed when a
// headset eye looks past its edge; coloured like the sprite's rim.
void back_cap(Builder& builder, const std::vector<std::uint8_t>& rim, double depth) {
    const auto count = rim.size();
    std::vector<std::uint8_t> ring;
    for (const auto index : rim) {
        const auto& p = builder.point(index);
        // The rim's own bow carries over, so a straight side's faces fan
        // instead of sharing the apex's plane.
        ring.push_back(builder.vertex(p.x * 0.6, p.y * 0.6, depth * 0.8 + p.z * 0.5));
    }
    const auto apex = builder.vertex(0.0, 0.0, depth);
    const Point inside{0.0, 0.0, 0.0};
    for (std::size_t j = 0; j < count; ++j) {
        const auto next = (j + 1U) % count;
        const auto& p = builder.point(rim[j]);
        const auto r = std::max(std::hypot(p.x, p.y), 1e-6), sample = std::max(r - 1.0, 0.0) / r;
        builder.face(rim[j], rim[next], ring[next], inside, p.x * sample, p.y * sample);
        builder.face(rim[j], ring[next], ring[j], inside, p.x * sample, p.y * sample);
        builder.face(ring[j], ring[next], apex, inside, p.x * sample, p.y * sample);
    }
}

// Orbs (the pulse and the plasma ball): a sphere for the bright core inside
// a flat halo ring for the glow, both in the sprite's concentric colour bands
// so the ball shows no spokes. The sphere is a geodesic ball (an icosahedron
// split once: 80 even triangles, no pole where slivers meet). The halo traces
// the sprite's outline and starts behind the sphere's edge so the two never
// part.
// The oval beam is the same model with `traced` set: the ball is stretched
// to the sprite's own outline (0.62 of it along every angle), so the tilted
// ellipse becomes an ellipsoid as deep as it is narrow, in the same halo.
assets::Shape build_orb(const Sprite& sprite, std::uint32_t address, bool traced = false) {
    constexpr std::size_t angles = 24, halo_rows = 5, band_count = 8;
    constexpr double core = 0.62; // The sphere's radius, as a fraction of the outline.
    Builder builder{sprite};
    const auto bands = sprite.bands(band_count);
    const auto band_slot = [&](double t) {
        return bands[std::min(band_count - 1U, static_cast<std::size_t>(std::max(t, 0.0) * band_count))];
    };
    std::array<double, angles> edge{};
    double mean = 0.0;
    for (std::size_t j = 0; j < angles; ++j) {
        edge[j] = sprite.edge(2.0 * kPi * static_cast<double>(j) / angles);
        mean += edge[j] / angles;
    }
    const auto radius = core * mean;
    // The outline between the traced angles, and the narrowest of them.
    const auto outline = [&edge](double x, double y) {
        const auto turn = std::fmod(std::atan2(y, x) / (2.0 * kPi) + 1.0, 1.0) * angles;
        const auto low = static_cast<std::size_t>(turn) % angles;
        const auto part = turn - std::floor(turn);
        return edge[low] * (1.0 - part) + edge[(low + 1U) % angles] * part;
    };
    const auto narrow = *std::min_element(edge.begin(), edge.end());
    // The icosahedron's corners, then each edge's midpoint pushed out to the sphere.
    const auto t = (1.0 + std::sqrt(5.0)) / 2.0;
    const std::array<Point, 12> corners{{{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t},
        {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}}};
    constexpr std::array<std::array<int, 3>, 20> triangles{{{0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
        {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8}, {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8},
        {3, 8, 9}, {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}}};
    std::vector<Point> unit;
    const auto add = [&](Point p) {
        const auto length = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
        unit.push_back({p.x / length, p.y / length, p.z / length});
        const auto u = unit.back();
        if (!traced) return builder.vertex(u.x * radius, u.y * radius, u.z * radius);
        const auto reach = core * outline(u.x, u.y);
        return builder.vertex(u.x * reach, u.y * reach, u.z * core * narrow);
    };
    std::vector<std::uint8_t> sphere;
    for (const auto& corner : corners) sphere.push_back(add(corner));
    std::map<std::pair<int, int>, std::uint8_t> middles;
    const auto middle = [&](int a, int b) {
        const auto key = std::minmax(a, b);
        if (const auto found = middles.find(key); found != middles.end()) return found->second;
        const auto& p = unit[static_cast<std::size_t>(a)];
        const auto& q = unit[static_cast<std::size_t>(b)];
        const auto index = add({p.x + q.x, p.y + q.y, p.z + q.z});
        middles.emplace(key, index);
        return index;
    };
    const Point inside{0.0, 0.0, 0.0};
    const auto sphere_face = [&](std::uint8_t a, std::uint8_t b, std::uint8_t c) {
        // Its band by where its middle shows from the front.
        const auto& p = builder.point(a); const auto& q = builder.point(b); const auto& r = builder.point(c);
        const auto x = (p.x + q.x + r.x) / 3.0, y = (p.y + q.y + r.y) / 3.0;
        const auto spread = std::hypot(x, y) / (traced ? core * outline(x, y) : radius);
        builder.slot_face(a, b, c, inside, band_slot(spread * core));
    };
    for (const auto& [a, b, c] : triangles) {
        const auto ab = middle(a, b), bc = middle(b, c), ca = middle(c, a);
        const auto ia = sphere[static_cast<std::size_t>(a)], ib = sphere[static_cast<std::size_t>(b)];
        const auto ic = sphere[static_cast<std::size_t>(c)];
        sphere_face(ia, ab, ca);
        sphere_face(ib, bc, ab);
        sphere_face(ic, ca, bc);
        sphere_face(ab, bc, ca);
    }
    // The halo, facing the camera from the sphere's middle plane, its outer
    // edge swept back into a shallow cone. A flat ring would put every halo
    // face in one plane, and the solid-decal pass (source_span_model) tests
    // coplanar faces in pairs: about 18 ms a frame for a stream of pulses.
    std::vector<std::vector<std::uint8_t>> rows(halo_rows);
    for (std::size_t j = 0; j < angles; ++j) {
        const auto a = 2.0 * kPi * static_cast<double>(j) / angles;
        for (std::size_t k = 0; k < halo_rows; ++k) {
            const auto step = static_cast<double>(k) / (halo_rows - 1U);
            const auto fraction = core * 0.85 + (1.0 - core * 0.85) * step;
            rows[k].push_back(builder.vertex(fraction * edge[j] * std::cos(a), fraction * edge[j] * std::sin(a),
                0.3 * step * edge[j]));
        }
    }
    const Point behind{0.0, 0.0, 1e4};
    for (std::size_t j = 0; j < angles; ++j) {
        const auto next = (j + 1U) % angles;
        for (std::size_t k = 0; k + 1U < halo_rows; ++k) {
            const auto fraction = core * 0.85 + (1.0 - core * 0.85) * (static_cast<double>(k) + 0.5) / (halo_rows - 1U);
            const auto slot = band_slot(std::max(fraction, core));
            builder.slot_face(rows[k][j], rows[k + 1U][j], rows[k + 1U][next], behind, slot);
            builder.slot_face(rows[k][j], rows[k + 1U][next], rows[k][next], behind, slot);
        }
    }
    return builder.finish(address);
}

// The Nova Bomb: a low-poly faceted gem (24 facets about an apex) standing
// out of a halo ring with a rounded cross-section. The gem fills the inner
// three quarters of the octagon, as the sprite's painted gem does.
assets::Shape build_bomb(const Sprite& sprite, std::uint32_t address) {
    constexpr std::size_t sides = 8, angles = 32, rows = 4;
    constexpr double gem = 0.74;
    Builder builder{sprite};
    double mean = 0.0;
    for (std::size_t j = 0; j < angles; ++j) mean += sprite.edge(2.0 * kPi * static_cast<double>(j) / angles) / angles;
    const auto height = gem * mean;
    // Rim corners on the octagon's corners (22.5 degrees off the axes).
    std::array<Point, sides> corner{};
    std::array<std::uint8_t, sides> rim{}, inner{};
    for (std::size_t i = 0; i < sides; ++i) {
        const auto a = 2.0 * kPi * (static_cast<double>(i) + 0.5) / sides;
        const auto r = gem * sprite.edge(a);
        corner[i] = {r * std::cos(a), r * std::sin(a), 0.0};
        rim[i] = builder.vertex(corner[i].x, corner[i].y, 0.0);
    }
    for (std::size_t i = 0; i < sides; ++i) {
        const auto a = 2.0 * kPi * (static_cast<double>(i) + 1.0) / sides;
        const auto r = 0.5 * gem * sprite.edge(a);
        inner[i] = builder.vertex(r * std::cos(a), r * std::sin(a), -height * std::sqrt(0.75));
    }
    const auto apex = builder.vertex(0.0, 0.0, -height * 1.05);
    const Point inside{0.0, 0.0, 0.0};
    // Each facet takes the sprite's painted facet under its centre.
    const auto facet = [&](std::uint8_t a, std::uint8_t b, std::uint8_t c) {
        const auto& p = builder.point(a); const auto& q = builder.point(b); const auto& s = builder.point(c);
        builder.face(a, b, c, inside, (p.x + q.x + s.x) / 3.0, (p.y + q.y + s.y) / 3.0);
    };
    for (std::size_t i = 0; i < sides; ++i) {
        const auto next = (i + 1U) % sides, previous = (i + sides - 1U) % sides;
        facet(apex, inner[i], inner[next]);
        facet(rim[i], rim[next], inner[i]);
        facet(inner[previous], rim[i], inner[i]);
    }
    // The halo: its inner row on the gem's rim edges (so the two meet
    // without a gap), its outer row on the sprite's outline.
    std::vector<std::vector<std::uint8_t>> halo(rows);
    for (std::size_t j = 0; j < angles; ++j) {
        const auto a = 2.0 * kPi * static_cast<double>(j) / angles;
        const auto c = std::cos(a), s = std::sin(a);
        // Where the ray meets the rim edge between its two corners.
        const auto turn = std::fmod(a / (2.0 * kPi) * sides - 0.5 + sides, static_cast<double>(sides));
        const auto& p = corner[static_cast<std::size_t>(turn) % sides];
        const auto& q = corner[(static_cast<std::size_t>(turn) + 1U) % sides];
        const auto ex = q.x - p.x, ey = q.y - p.y;
        const auto denominator = c * ey - s * ex;
        const auto inner_r = std::abs(denominator) > 1e-9 ? (p.x * ey - p.y * ex) / denominator : gem * mean;
        const auto outer_r = std::max(sprite.edge(a), inner_r + 0.5);
        // The octagon's corners sweep back a texel, more toward the outline:
        // along a straight side the rows would otherwise put up to eight
        // faces in one plane (see build_orb's halo).
        const auto bow = mean * (inner_r / std::max(gem * mean * std::cos(kPi / sides), 1e-6) - 1.0);
        for (std::size_t k = 0; k < rows; ++k) {
            const auto t = static_cast<double>(k) / (rows - 1U);
            const auto r = inner_r + (outer_r - inner_r) * t;
            halo[k].push_back(builder.vertex(r * c, r * s, -0.5 * (outer_r - inner_r) * std::sin(kPi * t) + bow * t));
        }
    }
    for (std::size_t j = 0; j < angles; ++j) {
        const auto next = (j + 1U) % angles;
        const auto a = 2.0 * kPi * (static_cast<double>(j) + 0.5) / angles;
        for (std::size_t k = 0; k + 1U < rows; ++k) {
            // The ring's core is inside each band's faces.
            const auto core = gem * mean + (1.0 - gem) * mean * 0.5;
            builder.quad(halo[k][j], halo[k + 1U][j], halo[k + 1U][next], halo[k][next],
                {core * std::cos(a), core * std::sin(a), 0.0});
        }
    }
    back_cap(builder, halo.back(), mean * 0.5);
    return builder.finish(address);
}

// The ring laser: a torus on the sprite's ring, its tube as thick as the
// ring is wide. Toward the camera the tube has six facets, which carry the
// sprite's bands from the outer edge to the hole and meet in a ridge at the
// front (an odd count would leave a flat annulus: 48 faces in one plane).
// Two more close it behind in a shallow ridge.
assets::Shape build_ring(const Sprite& sprite, std::uint32_t address) {
    constexpr std::size_t segments = 24, front = 6, sides = front + 2U;
    Builder builder{sprite};
    double outer = 0.0, hole = 0.0;
    for (std::size_t j = 0; j < segments; ++j) {
        const auto a = 2.0 * kPi * static_cast<double>(j) / segments;
        outer += sprite.edge(a) / segments;
        hole += sprite.inner(a) / segments;
    }
    const auto tube = std::max((outer - hole) / 2.0, 1.0), major = outer - tube;
    // Each front facet shows between these fractions of the tube's width.
    std::vector<double> bounds;
    for (std::size_t k = 0; k <= front; ++k) bounds.push_back((1.0 - std::cos(kPi * static_cast<double>(k) / front)) / 2.0);
    const auto bands = sprite.tube_bands(bounds);
    std::vector<std::array<std::uint8_t, sides>> rows(segments);
    for (std::size_t j = 0; j < segments; ++j) {
        const auto a = 2.0 * kPi * static_cast<double>(j) / segments;
        const auto c = std::cos(a), s = std::sin(a);
        for (std::size_t k = 0; k <= front; ++k) {
            const auto turn = kPi * static_cast<double>(k) / front;
            const auto r = major + tube * std::cos(turn);
            rows[j][k] = builder.vertex(r * c, r * s, -tube * std::sin(turn));
        }
        rows[j][sides - 1U] = builder.vertex(major * c, major * s, tube * 0.6);
    }
    for (std::size_t j = 0; j < segments; ++j) {
        const auto next = (j + 1U) % segments;
        const auto a = 2.0 * kPi * (static_cast<double>(j) + 0.5) / segments;
        const Point inside{major * std::cos(a), major * std::sin(a), 0.0};
        for (std::size_t k = 0; k < sides; ++k) {
            const auto after = (k + 1U) % sides;
            const auto slot = bands[k < front ? k : k == front ? front - 1U : 0U];
            builder.slot_face(rows[j][k], rows[j][after], rows[next][after], inside, slot);
            builder.slot_face(rows[j][k], rows[next][after], rows[next][k], inside, slot);
        }
    }
    return builder.finish(address);
}

const assets::Shape* model_for(const assets::TextureImage& texture) {
    if (texture.texels.size() != 32U * 32U) return nullptr;
    const auto hash = asteroid_texture_hash(texture);
    const auto known = std::find_if(kKnown.begin(), kKnown.end(), [hash](const Known& k) {return k.hash == hash;});
    if (known == kKnown.end()) return nullptr;
    static std::mutex mutex;
    static std::map<std::uint32_t, std::unique_ptr<assets::Shape>> models;
    const std::lock_guard lock{mutex};
    auto& model = models[hash];
    if (!model) {
        const Sprite sprite{texture};
        const auto address = kModelAddressBase + static_cast<std::uint32_t>(known - kKnown.begin());
        model = std::make_unique<assets::Shape>(known->kind == Kind::bomb ? build_bomb(sprite, address)
            : known->kind == Kind::ring ? build_ring(sprite, address)
            : build_orb(sprite, address, known->kind == Kind::oval));
    }
    return model.get();
}

// Texture recognition per draw, remembered by texture address with a cheap
// fingerprint, as for the asteroids.
struct TextureMatch {
    const assets::TextureImage* texture{};
    std::uint16_t descriptor{};
    std::array<std::uint8_t, 16> sample{};
    const assets::Shape* model{};
};

std::array<std::uint8_t, 16> fingerprint(const assets::TextureImage& texture) {
    std::array<std::uint8_t, 16> sample{};
    for (std::size_t index = 0; index < sample.size() && !texture.texels.empty(); ++index) {
        sample[index] = texture.texels[index * texture.texels.size() / sample.size()];
    }
    return sample;
}

const assets::Shape* match_texture(const assets::TextureImage& texture) {
    thread_local std::vector<TextureMatch> matches;
    const auto sample = fingerprint(texture);
    for (const auto& match : matches) {
        if (match.texture == &texture && match.descriptor == texture.descriptor && match.sample == sample) return match.model;
    }
    if (matches.size() >= 64U) matches.clear();
    matches.push_back({&texture, texture.descriptor, sample, model_for(texture)});
    return matches.back().model;
}

} // namespace

const assets::Shape* projectile_model_for_texture(const assets::TextureImage& texture) {
    return model_for(texture);
}

const assets::Shape* substitute_projectile_model(const assets::Shape& source, RenderPose& pose, bool enabled) {
    if (!enabled || pose.explosion_progress != 0U) return nullptr;
    const auto [texture, half_size] = whole_object_sprite(source, pose);
    if (texture == nullptr) return nullptr;
    const auto* model = match_texture(*texture);
    if (model == nullptr) return nullptr;
    pose.simple_scaled_sprite = false;
    // The sprite's world square spans the whole texture.
    pose.scale = half_size / (static_cast<double>(texture->u_mask + 1U) / 2.0 * kTexelUnits);
    // Face the camera, as the sprite did.
    pose.pitch = pose.yaw = pose.roll = 0.0;
    pose.rotation_matrix = {32767, 0, 0, 0, 32767, 0, 0, 0, 32767};
    pose.use_rotation_matrix = true;
    return model;
}

}
