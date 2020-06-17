#include "io.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace mc {
namespace util {

    std::string ReadFile(const std::string& filename)
    {
        std::ifstream in(filename.c_str());
        std::stringstream buffer;
        buffer << in.rdbuf();
        auto contents = buffer.str();
        if (contents.empty()) {
            throw std::runtime_error("[ReadFile] - Unable to read contents of file \"" + filename + "\"");
        }
        return contents;
    }

    Image::Image(const std::string& filename)
    {
        _bytes = stbi_load(filename.c_str(), &_width, &_height, &_channels, STBI_rgb_alpha);
        if (!_bytes) {
            throw std::runtime_error("[Image::ctor] - Failed to load image \"" + filename + "\"");
        }
    }

    Image::~Image()
    {
        stbi_image_free(_bytes);
    }

    TextureHandleRef LoadTexture2D(const std::string& filename, std::function<void()> setup)
    {
        int texWidth, texHeight, texChannels;
        stbi_uc* data = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

        if (!data) {
            throw std::runtime_error("[LoadTexture2d] - Failed to load image \"" + filename + "\"");
        }

        GLuint textureId;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
        if (setup) {
            setup();
        } else {
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        glBindTexture(GL_TEXTURE_2D, 0);

        return std::make_shared<TextureHandle>(textureId, GL_TEXTURE_2D, texWidth, texHeight);
    }

    TextureHandleRef LoadTextureCube(const std::array<std::string, 6>& faces)
    {
        unsigned int textureId;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textureId);

        int width, height, nrChannels;
        for (unsigned int i = 0; i < faces.size(); i++) {
            stbi_uc* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
            if (data) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                    0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
                stbi_image_free(data);
            } else {
                std::cout << "[LoadTextureCube] - Unable to load cubemap face[" << i << "] path(" << faces[i] << ")" << std::endl;
                stbi_image_free(data);
            }
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        // note, sampling mipmap lods requires glEnable(GL_TEXTURE_CUBEMAP_SEAMLESS)
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        return std::make_shared<TextureHandle>(textureId, GL_TEXTURE_CUBE_MAP, width, height);
    }

    TextureHandleRef LoadTextureCube(const std::string& folder, const std::string& ext)
    {
        return LoadTextureCube({
            folder + "/right" + ext,
            folder + "/left" + ext,
            folder + "/top" + ext,
            folder + "/bottom" + ext,
            folder + "/front" + ext,
            folder + "/back" + ext,
        });
    }

    void CheckGlError(const char* ctx)
    {
        GLint err = glGetError();
        if (err != GL_NO_ERROR) {
            std::cerr << "GL error at " << ctx << " err: " << err << std::endl;
            throw std::runtime_error("CheckGLError");
        }
    }

    GLuint CreateShader(GLenum shader_type, const char* src,
        std::function<void(const std::string&)> onError)
    {
        GLuint shader = glCreateShader(shader_type);
        if (!shader) {
            CHECK_GL_ERROR("glCreateShader");
            return 0;
        }
        glShaderSource(shader, 1, &src, nullptr);

        GLint compiled = GL_FALSE;
        glCompileShader(shader);
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLogLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);
            if (infoLogLen > 0) {
                auto* infoLog = (GLchar*)malloc(static_cast<size_t>(infoLogLen));
                if (infoLog) {
                    glGetShaderInfoLog(shader, infoLogLen, nullptr, infoLog);
                    onError(std::string(infoLog));
                    throw std::runtime_error("Could not compile shader");
                }
            }
            glDeleteShader(shader);
            return 0;
        }

        return shader;
    }

    GLuint CreateProgram(const char* vtxSrc, const char* fragSrc,
        std::function<void(const std::string&)> onVertexError,
        std::function<void(const std::string&)> onFragmentError)
    {
        GLuint vtxShader = 0;
        GLuint fragShader = 0;
        GLuint program = 0;
        GLint linked = GL_FALSE;

        vtxShader = CreateShader(GL_VERTEX_SHADER, vtxSrc, onVertexError);
        if (!vtxShader)
            goto exit;

        fragShader = CreateShader(GL_FRAGMENT_SHADER, fragSrc, onFragmentError);
        if (!fragShader)
            goto exit;

        program = glCreateProgram();
        if (!program) {
            CHECK_GL_ERROR("glCreateProgram");
            goto exit;
        }
        glAttachShader(program, vtxShader);
        glAttachShader(program, fragShader);

        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) {
            GLint infoLogLen = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLen);
            if (infoLogLen) {
                auto* infoLog = (GLchar*)malloc(static_cast<size_t>(infoLogLen));
                if (infoLog) {
                    glGetProgramInfoLog(program, infoLogLen, nullptr, infoLog);
                    std::cerr << "Could not link program: " << infoLog << std::endl;
                    throw std::runtime_error("Could not link program");
                }
            }
            glDeleteProgram(program);
            program = 0;
            std::cerr << "Could not link program" << std::endl;
            throw std::runtime_error("Could not link program");
        }

    exit:
        glDeleteShader(vtxShader);
        glDeleteShader(fragShader);
        return program;
    }

    namespace {

        std::vector<std::string> split(const std::string& str, const std::string& delimeter)
        {
            using namespace std;

            std::vector<std::string> tokens;

            size_t start = 0, next = string::npos;
            while (true) {
                bool done = false;
                string ss; //substring
                next = str.find(delimeter, start);
                if (next != string::npos) {
                    ss = str.substr(start, next - start);
                } else {
                    ss = str.substr(start, (str.length() - start));
                    done = true;
                }

                tokens.push_back(ss);
                if (done)
                    break;

                start = next + delimeter.length();
            }

            return tokens;
        }

        void replace(std::string& str, const std::string& token, const std::string& with)
        {
            std::string::size_type pos = str.find(token);
            while (pos != std::string::npos) {
                str.replace(pos, token.size(), with);
                pos = str.find(token, pos);
            }
        }

        void apply_substitutions(std::string& src, const std::map<std::string, std::string>& substitutions)
        {
            for (const auto& s : substitutions) {
                replace(src, s.first, s.second);
            }
        }

        std::string offset_error_line(const std::string& line, int offset)
        {
            // error format looks something like:
            // 0:16(19): error: no matching function for call to `mix(vec4, vec3, float)'; candidates are:
            static const std::regex kErrorPositionMatcher = std::regex(R"(^(\d+):(\d+)\((\d+)\))");

            if (offset == 0)
                return line;

            std::smatch pieces;
            if (std::regex_search(line, pieces, kErrorPositionMatcher) && pieces.size() == 4) {
                auto a = pieces[1].str();
                int b = std::stoi(pieces[2].str());
                auto c = pieces[3].str();

                b += offset;

                std::string updatedErrorPosition = a + ":" + std::to_string(b) + "(" + c + ")";
                std::string result = line;
                replace(result, pieces[0].str(), updatedErrorPosition);

                return result;
            }
            // no match found, leave line alone
            return line;
        }

        std::string offset_error_lines(const std::string& errorMessage, int lineOffset)
        {
            std::string result;
            for (const auto& line : split(errorMessage, "\n")) {
                auto updatedLine = offset_error_line(line, lineOffset);
                result += updatedLine + "\n";
            }
            return result;
        }

        void process_includes(std::string &src) {
            std::size_t includePos = src.find("#include");
            while(includePos != std::string::npos) {
                // advance to find the include path
                std::size_t openQuotePos = includePos;
                while (src[openQuotePos] != '\"' && src[openQuotePos] != '\n')  openQuotePos++;
                if (src[openQuotePos] == '\n') {
                    throw std::runtime_error("Expect \" after #include directive");
                }
                openQuotePos++;
                std::size_t closeQuotePos = openQuotePos;
                while (src[closeQuotePos] != '\"' && src[closeQuotePos] != '\n')  closeQuotePos++;
                if (src[closeQuotePos] == '\n') {
                    throw std::runtime_error("Expect cloding \" after #include directive");
                }
                std::string includePath = src.substr(openQuotePos, closeQuotePos - openQuotePos);
                std::string includeDirective = src.substr(includePos, closeQuotePos + 1 - includePos);
                replace(src, includeDirective, "");
                src.insert(includePos, ReadFile(includePath));

                // now that substitution has been performed, run again.
                includePos = src.find("#include");
            }
        }
    }

    GLuint CreateProgramFromFile(const char* glslFile, const std::map<std::string, std::string>& substitutions)
    {
        auto glslSrc = ReadFile(glslFile);
        std::vector<std::string> bufferLines = split(glslSrc, "\n");

        std::string vertex, fragment;
        std::string* current = nullptr;
        int firstVertexLine = 0;
        int firstFragmentLine = 0;
        int currentLine = 0;
        for (auto line : bufferLines) {
            if (line.find("vertex:") != std::string::npos) {
                current = &vertex;
                firstVertexLine = currentLine;
            } else if (line.find("fragment:") != std::string::npos) {
                current = &fragment;
                firstFragmentLine = currentLine;
            } else if (current != nullptr) {
                *current += line + "\n";
            }
            currentLine++;
        }

        if (vertex.empty()) {
            throw std::runtime_error("GLSL file \"" + std::string(glslFile) + "\" missing \"vertex:\" shader section");
        }
        if (fragment.empty()) {
            throw std::runtime_error("GLSL file \"" + std::string(glslFile) + "\" missing \"fragment:\" shader section");
        }

        process_includes(vertex);
        apply_substitutions(vertex, substitutions);

        process_includes(fragment);
        apply_substitutions(fragment, substitutions);

        return CreateProgram(
            vertex.c_str(), fragment.c_str(),
            [glslFile, firstVertexLine](const std::string& error) {
                std::cerr << "Could not compile vertex shader. File: " << glslFile << "\nError:\n"
                          << offset_error_lines(error, firstVertexLine + 1) << std::endl;
            },
            [glslFile, firstFragmentLine](const std::string& error) {
                std::cerr << "Could not compile fragment shader. File: " << glslFile << "\nError:\n"
                          << offset_error_lines(error, firstFragmentLine + 1) << std::endl;
            });
    }

}
} // namespace mc::util