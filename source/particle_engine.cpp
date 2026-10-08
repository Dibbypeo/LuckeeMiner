#include "luckee/particle_engine.hpp"

#include <algorithm>
#include <cstddef>

#include "luckee/particle.hpp"

namespace luckee {

ParticleEngine::ParticleEngine(Level& level)
    : level_(level) {
    particles_.reserve(1024);
}

void ParticleEngine::add(std::unique_ptr<Particle> particle) {
    if (particle)
        particles_.push_back(std::move(particle));
}

void ParticleEngine::tick() {
    for (std::size_t i = 0; i < particles_.size();) {
        Particle& particle = *particles_[i];
        particle.tick();

        if (particle.removed()) {
            particles_.erase(
                particles_.begin() +
                static_cast<std::ptrdiff_t>(i));
            continue;
        }

        ++i;
    }
}

} // namespace luckee
