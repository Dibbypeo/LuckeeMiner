#include "luckee/java_random.hpp"

namespace luckee {

JavaRandom& mathRandom() {
    static JavaRandom random;
    return random;
}

} // namespace luckee
