#pragma once

#include <memory>
#include <vector>

namespace luckee {

class Level;
class Particle;
class Player;

class ParticleEngine {
public:
    explicit ParticleEngine(Level& level);

    void add(std::unique_ptr<Particle> particle);
    void tick();

    const std::vector<std::unique_ptr<Particle>>& particles() const {
        return particles_;
    }

    std::vector<std::unique_ptr<Particle>>& particles() {
        return particles_;
    }

private:
    Level& level_;
    std::vector<std::unique_ptr<Particle>> particles_;
};

} // namespace luckee
