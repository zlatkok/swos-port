#pragma once

// Used for sprite textures that are shared with team textures cache.
class SharedTexture
{
public:
    SharedTexture();
    SharedTexture(SDL_Texture *texture);
    ~SharedTexture();
    SharedTexture(const SharedTexture& rhs);
    SharedTexture(SharedTexture&& rhs);
    SharedTexture& operator=(const SharedTexture& rhs);
    SharedTexture& operator=(SharedTexture&& rhs);

    static SharedTexture fromSurface(SDL_Surface *surface);
    SDL_Texture *texture();
    explicit operator bool() const;
    void reset();

private:
    void release();
    void assign(const SharedTexture& rhs);
    static void clear(SharedTexture& texture);

    SDL_Texture *m_texture;
    int *m_refCount;
};
