TODO

## Mocking SDL functions

In order to successfully unit test the game it would be essential to control calls to SDL, and reroute them
to a controlled implementation. It turns out that SDL keeps internal table of pointers to *every* public
API function, a perfect tool for this purpose. The real usage for this mechanism is to allow overriding the
SDL library that the game uses, for example to force usage a Valve's SDL or to allow updates even if SDL
was compiled statically.

But how to reach this table? It is internal to SDL, but there is a mechanism to override it. When the SDL
library starts up it will look up environment variable "SDL_DYNAMIC_API" and use its value as a path to
external DLL. This external DLL will be loaded, and a function will be looked up and invoked:

`SInt32 SDL_DYNAPI_entry(Uint32 version, void *table, Uint32 tablesize);`

This function is given a pointer to the internal SDL function table, and is expected to fill it in and
return 0 for success. However, we are going to be a bit sneaky here. SDL address table fetcher is a DLL that
responds to this API, and its function will store table and its size and return -1 as failure. In this case
SDL continues and fills the table with its own pointers. All that's left is to access the table pointer
later on, which is now filled with valid pointers to SDL functions, and mock to our heart's content. :)
