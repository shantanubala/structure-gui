/*
    GL.h

    Copyright © 2017 Occipital, Inc. All rights reserved.
    This file is part of the Structure SDK.
    Unauthorized copying of this file, via any medium is strictly prohibited.
    Proprietary and confidential.

    http://structure.io
*/

#pragma once

#include <ST/Macros.h>
#include <cstdint>

#if   __linux__
#   define GL_GLEXT_PROTOTYPES 1
#   include <EGL/egl.h>
#   include <GL/gl.h>
#   include <GL/glext.h>
#   include <GL/glx.h>
    // Eliminate conflicting Xlib macros
#   ifdef None
#   undef None
#   endif
#   ifdef True
#   undef True
#   endif
#   ifdef False
#   undef False
#   endif
#   ifdef Success
#   undef Success
#   endif
#   ifdef Status
#   undef Status
#   endif
#endif

#if   __linux__
struct LinuxOpenGLContextWrapper;
#endif

namespace ST {

//------------------------------------------------------------------------------

struct ColorFrame;

//------------------------------------------------------------------------------

#if   __linux__

/** @brief Internal typedef to make context cross platform. */
typedef struct LinuxOpenGLContextWrapper* OpenGLContext;

/** @brief Creates OpenGL context wrapper from an existing context and surface.
    The context wrapper takes ownership of the context and surface.
    DestroyOpenGLContext() will destroy the context and the surface.
*/
ST_API OpenGLContext CreateOpenGLContext(EGLContext context, EGLSurface surface);

#endif

/** @brief Creates a new OpenGL context.
    @param sharedContext The new context will share OpenGL objects with sharedContext.
    @return An OpenGL context.
*/
ST_API OpenGLContext CreateOpenGLContext(OpenGLContext sharedContext = nullptr);

/** @brief Destroys the underlying OpenGL context and surface. */
ST_API void DestroyOpenGLContext(OpenGLContext* context);

/** @brief Sets the current context for the calling thread.
    Note that this must be caled if you switch controlling threads in order to use the OpenGL context.
    @param context A valid OpenGL context. (can also be nullptr to clear context)
*/
ST_API void SetCurrentOpenGLContext(OpenGLContext context);

/** @brief Returns the currently bound OpenGL context.
    This will return context wrapper only for contexts created using one of CreateOpenGLContext functions.
    @return An OpenGL context. If the current context was never passed to SetCurrentOpenGLContext, this function will return nullptr.
*/
ST_API OpenGLContext GetCurrentGLContext();

//------------------------------------------------------------------------------s

/** @brief Helper class for allocating and rendering textures in OpenGL. */
struct ST_API OpenGLTexture
{
public:
     OpenGLTexture();
    ~OpenGLTexture();

    /** @brief Returns true if this texture has data. */
    bool isInitialized() const;

    /** @brief Width of the texture. */
    int width() const;

    /** @brief Height of the texture. */
    int height() const;

    /** @brief Aspect ratio of the texture. */
    float aspectRatio() const;

    /** @brief Upload monochrome or grayscale image data.
        @param data Contiguous single-channel image data.
        @param width Width of the image.
        @param height Height of the image.
    */
    void uploadGrayScale(const uint8_t* data, int width, int height);

    /** @brief Upload a ColorFrame. */
    void upload(const ColorFrame& colorFrame);

    /** @brief Upload image data from a PNG file.
        @param pngImagePath The path to a PNG image file.
        @return Returns true if the image was loaded successfully. Otherwise, false.
    */
    bool upload(const char* pngImagePath);

    /** @brief The internal GL_TEXTURE_2D id to be used in OpenGL calls. */
    GLuint glId() const;

    /** @brief Returns true if a single-channel image was uploaded. */
    bool isGrayscale() const;

    ST_DECLARE_OPAQUE_INTERNALS(OpenGLTexture);
};

//------------------------------------------------------------------------------

} // ST namespace


