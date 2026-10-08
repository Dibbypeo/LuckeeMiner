#include "luckee/particle_engine.hpp"

#include <cstddef>
#include <utility>

#include "luckee/particle.hpp"

namespace luckee {

ParticleEngine::ParticleEngine(Level& level)
    : level_(level) {
    particles_.reserve(1024);
}

ParticleEngine::~ParticleEngine() = default;

void ParticleEngine::add(std::unique_ptr<Particle> particle) {
    if (particle)
        particles_.push_back(std::move(particle));
}

void ParticleEngine::tick() {
    std::size_t write = 0;

    for (std::size_t read = 0;
         read < particles_.size();
         ++read) {
        Particle& particle =
            *particles_[read];

        particle.tick();

        if (particle.removed())
            continue;

        if (write != read)
            particles_[write] =
                std::move(particles_[read]);

        ++write;
    }

    particles_.resize(write);
}

} // namespace luckee
