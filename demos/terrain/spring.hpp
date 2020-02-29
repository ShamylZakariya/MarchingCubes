#ifndef spring_hpp
#define spring_hpp

#include <glm/glm.hpp>

template <glm::length_t L, typename T, glm::qualifier Q = glm::defaultp>
class spring_ {
public:
    spring_(T mass, T force, T damping, glm::vec<L, T, Q> zero = {})
        : _mass(mass)
        , _force(force)
        , _damping(damping)
        , _target(zero)
        , _value(zero)
        , _velocity(zero)
        , _zero(zero)
    {
    }

    spring_(const spring_& c)
        : _target(c._target)
        , _value(c._value)
        , _velocity(c._velocity)
        , _zero(c._zero)
        , _mass(c._mass)
        , _force(c._force)
        , _damping(c._damping)
    {
    }

    spring_& operator=(const spring_& rhs)
    {
        _mass = rhs._mass;
        _force = rhs._force;
        _damping = rhs._damping;
        _target = rhs._target;
        _value = rhs._value;
        _velocity = rhs._velocity;
        _zero = rhs._zero;

        return *this;
    }

    void setTarget(glm::vec<L, T, Q> target) { _target = target; }

    glm::vec<L, T, Q> target(void) const { return _target; }

    void setValue(glm::vec<L, T, Q> v)
    {
        _value = v;
        _velocity = _zero;
    }

    glm::vec<L, T, Q> value(void) const { return _value; }

    void setVelocity(glm::vec<L, T, Q> vel) { _velocity = vel; }

    glm::vec<L, T, Q> velocity(void) const { return _velocity; }

    bool converged(T epsilon = 0.001) const
    {
        return glm::distance(_velocity, 0.0) <= epsilon && glm::distance(_value, _target) <= epsilon;
    }

    glm::vec<L, T, Q> step(T deltaT)
    {
        if (deltaT < T(1e-4))
            return _value;

        auto error = _target - _value;
        auto dampingForce = _damping * _velocity;
        auto force = (error * _force) - dampingForce;
        auto acceleration = force / _mass;

        _velocity += acceleration * deltaT;
        _value += _velocity * deltaT;

        return _value;
    }

private:
    T _mass, _force, _damping;
    glm::vec<L, T, Q> _target, _value, _velocity, _zero;
};

typedef spring_<3, float> spring3;

#endif