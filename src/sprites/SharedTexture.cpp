#include "SharedTexture.h"
#include "render.h"

SharedTexture::SharedTexture()
    : m_texture(nullptr), m_refCount(nullptr)
{
}

SharedTexture::SharedTexture(SDL_Texture *texture)
    : m_texture(texture), m_refCount(new int(1))
{
}

SharedTexture::~SharedTexture()
{
    reset();
}

SharedTexture::SharedTexture(const SharedTexture& rhs)
{
    assign(rhs);
    if (m_refCount)
        ++*m_refCount;
}

SharedTexture::SharedTexture(SharedTexture&& rhs)
{
    assign(rhs);
    clear(rhs);
}

SharedTexture& SharedTexture::operator=(const SharedTexture& rhs)
{
    if (this != &rhs) {
        release();
        assign(rhs);
        if (m_refCount)
            ++*m_refCount;
    }
    return *this;
}

SharedTexture& SharedTexture::operator=(SharedTexture&& rhs)
{
    if (this != &rhs) {
        release();
        assign(rhs);
        clear(rhs);
    }
    return *this;
}

SDL_Texture *SharedTexture::texture()
{
    return m_texture;
}

SharedTexture::operator bool() const
{
    return !!m_texture;
}

void SharedTexture::reset()
{
    release();
    clear(*this);
}

SharedTexture SharedTexture::fromSurface(SDL_Surface *surface)
{
    return SDL_CreateTextureFromSurface(getRenderer(), surface);
}

void SharedTexture::release()
{
    assert(!m_refCount == !m_texture && (!m_refCount || *m_refCount > 0));

    if (m_refCount && !--*m_refCount) {
        delete m_refCount;
        if (m_texture)
            SDL_DestroyTexture(m_texture);
    }
}

void SharedTexture::assign(const SharedTexture& rhs)
{
    m_refCount = rhs.m_refCount;
    m_texture = rhs.m_texture;
}

void SharedTexture::clear(SharedTexture& texture)
{
    texture.m_refCount = nullptr;
    texture.m_texture = nullptr;
}
