#ifndef spring_hpp
#define spring_hpp

#include <glm/glm.hpp>

template <glm::length_t L, typename T, glm::qualifier Q = glm::defaultp>
class spring_ {
public:
    /**
			Create a spring_. Default mass is 1, default spring_ force is 1, and default dampening is 0.
		*/

    spring_(glm::vec<L, T, Q> zero = {})
        : _target(zero)
        , _value(zero)
        , _velocity(zero)
        , _zero(zero)
    {
        setMass(1.0);
        setForce(1.0);
        setDamping(0.0);
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
        _target = rhs._target;
        _value = rhs._value;
        _velocity = rhs._velocity;
        _zero = rhs._zero;

        _mass = rhs._mass;
        _force = rhs._force;
        _damping = rhs._damping;

        return *this;
    }

    spring_(T mass, T force, T damping, glm::vec<L,T,Q> zero = {})
        : _target(zero)
        , _value(zero)
        , _velocity(zero)
        , _zero(zero)
    {
        setMass(mass);
        setForce(force);
        setDamping(damping);
    }

    static T minimumMass(void) { return T(0.001); }

    void set(T mass, T force, T damping)
    {
        setMass(mass);
        setForce(force);
        setDamping(damping);
    }

    void setTarget(T target) { _target = target; }

    T target(void) const { return _target; }

    void setValue(T v)
    {
        _value = v;
        _velocity = _zero;
    }

    T value(void) const { return _value; }

    void setMass(T m) { _mass = std::max(m, minimumMass()); }

    T mass(void) const { return _mass; }

    void setForce(T f) { _force = std::max(f, minimumMass()); }

    T force(void) const { return _force; }

    void setDamping(T d) { _damping = std::max(std::min(d, T(1)), T(0)); }

    T damping(void) const { return _damping; }

    void setVelocity(glm::vec<L,T,Q> vel) { _velocity = vel; }

    glm::vec<L,T,Q> velocity(void) const { return _velocity; }

    bool converged(T epsilon = 0.001) const
    {
        return glm::distance(_velocity, 0.0) <= epsilon && glm::distance(_value, _target) <= epsilon;
    }

    glm::vec<L,T,Q> step(T deltaT)
    {
        if (deltaT < T(1e-4))
            return _value;

        T error = _target - _value;
        T correctiveForce = error * _force;
        T correctiveAcceleration = correctiveForce / _mass;

        _velocity += correctiveAcceleration * deltaT;

        if (_damping > 0) {
            _velocity = _velocity * (T(1) - _damping);
        }

        _value = _value + _velocity / deltaT;
        return _value;
    }

private:
    T _mass, _force, _damping;
    glm::vec<L, T, Q> _target, _value, _velocity, _zero;
};

typedef spring_<3, float> spring3;

#endif