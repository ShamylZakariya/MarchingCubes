#ifndef post_processing_stack_hpp
#define post_processing_stack_hpp

#include <algorithm>
#include <epoxy/gl.h>

#include <mc/util/io.hpp>
#include <mc/util/unowned_ptr.hpp>

namespace post_processing {
using mc::util::unowned_ptr;

class Fbo {
private:
    GLuint _fbo = 0;
    GLuint _colorTex = 0;
    GLuint _depthTex = 0;
    glm::ivec2 _size { 0, 0 };

public:
    Fbo() = default;

    // non-copyable
    Fbo(const Fbo& other) = delete;
    Fbo(Fbo&& other) = delete;
    Fbo& operator=(const Fbo& other) = delete;

    ~Fbo()
    {
        destroy();
    }

    void create(const glm::ivec2& size, bool includeDepth);
    void destroy();

    GLuint getFramebuffer() const { return _fbo; }
    GLuint getColorTex() const { return _colorTex; }
    GLuint getDepthTex() const { return _depthTex; }
    glm::ivec2 getSize() const { return _size; }
};

class FboRelay {
public:
    FboRelay(unowned_ptr<Fbo> src, unowned_ptr<Fbo> dst)
        : _a(src)
        , _b(dst)
    {
    }

    unowned_ptr<Fbo> getSrc() const { return _a; }
    unowned_ptr<Fbo> getDst() const { return _b; }

    void next()
    {
        std::swap(_a, _b);
    }

private:
    unowned_ptr<Fbo> _a, _b;
};

class Filter {
public:
    Filter(const std::string& name)
        : _name(name)
    {
    }
    virtual ~Filter() = default;

    std::string getName() const { return _name; }

    void setAlpha(float alpha)
    {
        float oldAlpha = _alpha;
        _alpha = glm::clamp<float>(alpha, 0, 1);
        if (_alpha != oldAlpha) {
            _alphaChanged(oldAlpha, _alpha);
        }
    }
    float getAlpha() const { return _alpha; }

    void setClearsColorBuffer(bool clears) { _clearsColorBuffer = clears; }
    bool clearsColorBuffer() const { return _clearsColorBuffer; }

    void setClearColor(glm::vec4 clearColor) { _clearColor = clearColor; }
    glm::vec4 getClearColor() const { return _clearColor; }

protected:
    /// Called by FilterStack when it's resized; Note: FilterStack directly sets _size before calling this.
    virtual void _resize(const glm::ivec2& newSize) {}

    /// Called via setAlpha() to respond to changes to alpha
    virtual void _alphaChanged(double oldAlpha, double newAlpha) {}

    /// Perform time-based updates
    virtual void _update(double time) {}

    /// Called by FilterStack to prepare render state; you should most-likely implement _render() and leave this alone
    virtual void _execute(FboRelay& relay, GLuint depthTexture);

    /// Perform your filtered render using input as your source texture data.
    virtual void _render(GLuint colorTex, GLuint depthTex) = 0;

private:
    friend class FilterStack;

    std::string _name;
    glm::ivec2 _size { 0, 0 };
    float _alpha { 0 };
    bool _clearsColorBuffer { false };
    glm::vec4 _clearColor;
};

class FilterStack {
public:
    FilterStack() = default;
    virtual ~FilterStack();

    void push(std::unique_ptr<Filter>&& filter)
    {
        _filters.push_back(std::move(filter));
    }
    void pop()
    {
        _filters.pop_back();
    }
    void remove(const std::string& name)
    {
        _filters.erase(std::remove_if(
                           _filters.begin(), _filters.end(),
                           [&name](const std::unique_ptr<Filter>& f) { return f->getName() == name; }),
            _filters.end());
    }
    void remove(unowned_ptr<Filter> filter)
    {
        _filters.erase(std::remove_if(
                           _filters.begin(), _filters.end(),
                           [&filter](const std::unique_ptr<Filter>& f) { return f.get() == filter.get(); }),
            _filters.end());
    }
    void clear()
    {
        _filters.clear();
    }
    bool isEmpty() const
    {
        return _filters.empty();
    }

    unowned_ptr<Filter> getFilter(const std::string& name) const
    {
        for (const auto& f : _filters) {
            if (f->getName() == name) {
                return f.get();
            }
        }
        return nullptr;
    }

    template <typename T>
    unowned_ptr<T> getFilter()
    {
        for (const auto& f : _filters) {
            T* fPtr = dynamic_cast<T*>(f.get());
            if (fPtr) {
                return fPtr;
            }
        }
        return nullptr;
    }

    void update(double time)
    {
        for (const auto& f : _filters) {
            f->_update(time);
        }
    }

    /// Capture the render output of renderFunc into an RGBA+Depth Fbo.
    /// Returns the capture Fbo
    unowned_ptr<Fbo> capture(glm::ivec2 size, std::function<void()> renderFunc);

    /// Execute the filter stack against the input Fbo
    unowned_ptr<Fbo> execute(unowned_ptr<Fbo> source);

private:
    std::vector<std::unique_ptr<Filter>> _filters;
    Fbo _captureFbo;
    Fbo _buffer;
};

}

#endif