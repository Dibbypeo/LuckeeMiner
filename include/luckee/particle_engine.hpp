#pragma once

#include <cstddef>
#include <memory>
#include <vector>

namespace luckee {

class Level;
class Particle;
class Player;

class ParticleEngine {
public:
    static constexpr std::size_t MAX_ACTIVE_PARTICLES = 512;

    explicit ParticleEngine(Level& level);
    ~ParticleEngine();

    bool hasCapacityFor(std::size_t count) const {
        return count <= MAX_ACTIVE_PARTICLES &&
               particles_.size() <= MAX_ACTIVE_PARTICLES - count;
    }

    void add(std::unique_ptr<Particle> particle);
    void tick();

    const std::vector<std::unique_ptr<Particle>>& particles() const {
        return particles_;
    }

private:
    Level& level_;
    std::vector<std::unique_ptr<Particle>> particles_;
};

} // namespace luckee
