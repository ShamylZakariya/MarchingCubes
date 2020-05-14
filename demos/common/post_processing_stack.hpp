#ifndef post_processing_stack_hpp
#define post_processing_stack_hpp

#include <algorithm>
#include <epoxy/gl.h>

#include <mc/triangle_consumer.hpp>
#include <mc/util/io.hpp>
#include <mc/util/unowned_ptr.hpp>

namespace post_processing {
using mc::util::unowned_ptr;

namespace detail {
    struct VertexP2T2 {
        glm::vec2 pos;
        glm::vec2 texCoord;

        enum class AttributeLayout : GLuint {
            Pos = 0,
            TexCoord = 1
        };

        bool operator==(const VertexP2T2& other) const
        {
            return pos == other.pos && texCoord == other.texCoord;
        }

        static void bindVertexAttributes();
    };
}

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
    virtual void _resize(const glm::ivec2& newSize) { }

    /// Called via setAlpha() to respond to changes to alpha
    virtual void _alphaChanged(double oldAlpha, double newAlpha) { }

    /// Perform time-based updates
    virtual void _update(double time) { }

    /// Called by FilterStack to prepare render state; you should most-likely implement _render() and leave this alone
    virtual void _execute(const glm::ivec2& size, GLuint colorTex, GLuint depthTex, const mc::TriangleConsumer<detail::VertexP2T2>& clipspaceQuad);

    /// Perform your filtered render using input as your source texture data.
    virtual void _render(const glm::ivec2& size, GLuint colorTex, GLuint depthTex, const mc::TriangleConsumer<detail::VertexP2T2>& clipspaceQuad) = 0;

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
    FilterStack();
    ~FilterStack();

    template <class T>
    mc::util::unowned_ptr<T> push(std::unique_ptr<T>&& filter)
    {
        auto ptr = filter.get();
        _filters.push_back(std::move(filter));
        return ptr;
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

    /// Capture the render output of renderFunc, execute the installed filters,
    /// and then draw the result to the active framebuffer
    void execute(glm::ivec2 captureSize, std::function<void()> renderFunc)
    {
        execute(captureSize, captureSize, renderFunc);
    }

    /// Capture the render output of renderFunc, execute the installed filters,
    /// and then draw the result to the active framebuffer
    void execute(glm::ivec2 captureSize, glm::ivec2 displaySize, std::function<void()> renderFunc);

protected:
    /// blits colorTex and depthTex to default framebuffer (e.g., display)
    void blit(glm::ivec2 captureSize, glm::ivec2 displaySize, GLuint colorTex, GLuint depthTex);
    void destroyAttachments();
    void createAttachments(glm::ivec2 size);

private:
    std::vector<std::unique_ptr<Filter>> _filters;

    GLuint _fbo = 0;
    GLuint _colorTexSrc = 0;
    GLuint _colorTexDst = 0;
    GLuint _depthTex = 0;
    glm::ivec2 _size { 0, 0 };
    mc::TriangleConsumer<detail::VertexP2T2> _clipspaceQuad;
};

}

#endif