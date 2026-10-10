#include "luckee/entity.hpp"

#include <utility>

#include <cmath>
#include "luckee/java_random.hpp"

#include "luckee/level.hpp"

namespace luckee {
namespace {
float random01() {
    return static_cast<float>(mathRandom().nextDouble());
}
} // namespace

Entity::Entity(Level& level, std::size_t collisionCubeReserve)
    : level_(level) {
    collisionCubes_.reserve(collisionCubeReserve);
    resetPos();
}

Entity::Entity(Entity&& other) noexcept
    : level_(other.level_),
      xo_(other.xo_),
      yo_(other.yo_),
      zo_(other.zo_),
      x_(other.x_),
      y_(other.y_),
      z_(other.z_),
      xd_(other.xd_),
      yd_(other.yd_),
      zd_(other.zd_),
      yRot_(other.yRot_),
      xRot_(other.xRot_),
      bb_(other.bb_),
      onGround_(other.onGround_),
      removed_(other.removed_),
      heightOffset_(other.heightOffset_),
      bbWidth_(other.bbWidth_),
      bbHeight_(other.bbHeight_),
      collisionCubes_(std::move(other.collisionCubes_)) {
}

Entity& Entity::operator=(Entity&& other) noexcept {
    if (this == &other)
        return *this;

    // level_ is a reference and therefore cannot be rebound. The vector only
    // contains entities belonging to the same Level in this program.
    xo_ = other.xo_;
    yo_ = other.yo_;
    zo_ = other.zo_;
    x_ = other.x_;
    y_ = other.y_;
    z_ = other.z_;
    xd_ = other.xd_;
    yd_ = other.yd_;
    zd_ = other.zd_;
    yRot_ = other.yRot_;
    xRot_ = other.xRot_;
    bb_ = other.bb_;
    onGround_ = other.onGround_;
    removed_ = other.removed_;
    heightOffset_ = other.heightOffset_;
    bbWidth_ = other.bbWidth_;
    bbHeight_ = other.bbHeight_;
    collisionCubes_ = std::move(other.collisionCubes_);

    return *this;
}

void Entity::resetPos() {
    const float x =
        random01() * static_cast<float>(level_.width());
    const float y =
        static_cast<float>(level_.depth()) + 10.0f;
    const float z =
        random01() * static_cast<float>(level_.height());

    setPos(x, y, z);
}

void Entity::resetPosition() {
    resetPos();
}

void Entity::remove() {
    removed_ = true;
}

void Entity::setSize(float width, float height) {
    bbWidth_ = width;
    bbHeight_ = height;
}

void Entity::setPos(float x, float y, float z) {
    x_ = x;
    y_ = y;
    z_ = z;

    const float halfWidth = bbWidth_ / 2.0f;
    const float bottom = y - heightOffset_;

    // Entity y_ is the reference position used by movement/rendering:
    // the feet are y_ - heightOffset_, not y_ - height/2. Keeping setPos
    // consistent with move() prevents the initial/reset AABB from being
    // vertically displaced (notably for the player's 1.62-unit eye offset).
    bb_ = AABB(
        x - halfWidth,
        bottom,
        z - halfWidth,
        x + halfWidth,
        bottom + bbHeight_,
        z + halfWidth);
}

void Entity::turn(float deltaX, float deltaY) {
    yRot_ =
        static_cast<float>(
            static_cast<double>(yRot_) +
            static_cast<double>(deltaX) * 0.15);
    xRot_ =
        static_cast<float>(
            static_cast<double>(xRot_) -
            static_cast<double>(deltaY) * 0.15);

    if (xRot_ < -90.0f)
        xRot_ = -90.0f;

    if (xRot_ > 90.0f)
        xRot_ = 90.0f;
}

void Entity::tick() {
    xo_ = x_;
    yo_ = y_;
    zo_ = z_;
}

void Entity::move(float xa, float ya, float za) {
    const float xaOrg = xa;
    const float yaOrg = ya;
    const float zaOrg = za;

    level_.getCubes(
        bb_.expand(xa, ya, za),
        collisionCubes_);

    for (const AABB& cube : collisionCubes_)
        ya = cube.clipYCollide(bb_, ya);

    bb_.move(0.0f, ya, 0.0f);

    for (const AABB& cube : collisionCubes_)
        xa = cube.clipXCollide(bb_, xa);

    bb_.move(xa, 0.0f, 0.0f);

    for (const AABB& cube : collisionCubes_)
        za = cube.clipZCollide(bb_, za);

    bb_.move(0.0f, 0.0f, za);

    onGround_ = yaOrg != ya && yaOrg < 0.0f;

    if (xaOrg != xa)
        xd_ = 0.0f;

    if (yaOrg != ya)
        yd_ = 0.0f;

    if (zaOrg != za)
        zd_ = 0.0f;

    x_ = (bb_.x0 + bb_.x1) / 2.0f;
    y_ = bb_.y0 + heightOffset_;
    z_ = (bb_.z0 + bb_.z1) / 2.0f;
}

void Entity::moveRelative(float xa, float za, float speed) {
    float dist = xa * xa + za * za;
    if (dist < 0.01f)
        return;

    dist =
        speed / static_cast<float>(
            std::sqrt(
                static_cast<double>(dist)));

    const double radians =
        static_cast<double>(yRot_) *
        3.14159265358979323846 / 180.0;

    const float sin =
        static_cast<float>(std::sin(radians));
    const float cos =
        static_cast<float>(std::cos(radians));

    xa *= dist;
    za *= dist;

    xd_ += xa * cos - za * sin;
    zd_ += za * cos + xa * sin;
}

bool Entity::isLit() const {
    return level_.isLit(
        static_cast<int>(x_),
        static_cast<int>(y_),
        static_cast<int>(z_));
}

} // namespace luckee
