/* simple Casio Loopy tile map, VRAM, palette and tile data viewer */

#include <fstream>
#include <iterator>
#include <vector>

#include "SDL2/SDL.h"

#undef main

const char *const tile_path = R"(D:\loo_pal\vram.bin)"; // Visual Studio doesn't let me use command line arguments,
const char *const pal_path  = R"(D:\loo_pal\pal.bin)";  // sad.

std::vector<uint8_t> load_file(const char *const path)
{
    std::ifstream file(path, std::ios::binary);
    
    file.unsetf(std::ios::skipws);
    file.seekg (std::ios::end);

    const auto size = file.tellg();

    file.seekg(std::ios::beg);

    std::vector<uint8_t> data;

    data.resize(size);
    data.insert(data.begin(), std::istream_iterator<uint8_t>(file), std::istream_iterator<uint8_t>());

    return data;
}

void swap_bytes(std::vector<uint8_t> &data, const int size)
{
    for (auto i = 0; i < size; i += 2)
    {
        const auto temp = data[i];

        data[i]     = data[i + 1];
        data[i + 1] = temp;
    }
}

int main(const int argc, const char *const *const argv)
{
    SDL_Window   *tile_win = nullptr, *pal_win = nullptr, *map_win = nullptr, *data_win = nullptr;
    SDL_Renderer *tile_ren = nullptr, *pal_ren = nullptr, *map_ren = nullptr, *data_ren = nullptr;
    SDL_Texture  *tile_tex = nullptr, *pal_tex = nullptr, *map_tex = nullptr, *data_tex = nullptr;
    SDL_Event e;

    SDL_Init   (SDL_INIT_VIDEO);
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

    SDL_CreateWindowAndRenderer(32 * 8 * 2, 32 * 8 * 2, 0, &tile_win, &tile_ren);
    SDL_SetWindowSize          (tile_win, 32 * 8 * 2, 32 * 8 * 2);
    SDL_RenderSetLogicalSize   (tile_ren, 32 * 8 * 2, 32 * 8 * 2);
    SDL_SetWindowResizable     (tile_win, SDL_FALSE);
    SDL_SetWindowTitle         (tile_win, "VRAM viewer");

    tile_tex = SDL_CreateTexture(tile_ren, SDL_PIXELFORMAT_RGB555, SDL_TEXTUREACCESS_STREAMING, 32 * 8, 32 * 8);

    SDL_CreateWindowAndRenderer(64 * 8, 4 * 8, 0, &pal_win, &pal_ren);
    SDL_SetWindowSize          (pal_win, 64 * 8, 4 * 8);
    SDL_RenderSetLogicalSize   (pal_ren, 64 * 8, 4 * 8);
    SDL_SetWindowResizable     (pal_win, SDL_FALSE);
    SDL_SetWindowTitle         (pal_win, "Palette viewer");

    pal_tex = SDL_CreateTexture(pal_ren, SDL_PIXELFORMAT_RGB555, SDL_TEXTUREACCESS_STREAMING, 64, 4);

    SDL_CreateWindowAndRenderer((32 * 8 + 31) * 2, (28 * 8 + 27) * 2, 0, &map_win, &map_ren);
    SDL_SetWindowSize          (map_win, (32 * 8 + 31) * 2, (28 * 8 + 27) * 2);
    SDL_RenderSetLogicalSize   (map_ren, (32 * 8 + 31) * 2, (28 * 8 + 27) * 2);
    SDL_SetWindowResizable     (map_win, SDL_FALSE);
    SDL_SetWindowTitle         (map_win, "Tile map viewer");
    
    map_tex = SDL_CreateTexture(map_ren, SDL_PIXELFORMAT_RGB555, SDL_TEXTUREACCESS_STREAMING, 32 * 8 + 31, 28 * 8 + 27);

    SDL_CreateWindowAndRenderer((32 * 8 + 31) * 2, (8 * 8 + 7) * 2, 0, &data_win, &data_ren);
    SDL_SetWindowSize          (data_win, (32 * 8 + 31) * 2, (8 * 8 + 7) * 2);
    SDL_RenderSetLogicalSize   (data_ren, (32 * 8 + 31) * 2, (8 * 8 + 7) * 2);
    SDL_SetWindowResizable     (data_win, SDL_FALSE);
    SDL_SetWindowTitle         (data_win, "Tile data viewer");
    
    data_tex = SDL_CreateTexture(data_ren, SDL_PIXELFORMAT_RGB555, SDL_TEXTUREACCESS_STREAMING, 32 * 8 + 31, 8 * 8 + 7);

    auto tile_data = load_file(tile_path), pal_data = load_file(pal_path);

    std::vector<uint8_t> tile_fb, tile_map, map_fb, data_fb;

    // smol side note: palette data is supposed to be byte swapped. However, tiles look like shit using "correct" palette data.
    // that's because I don't know how to get the correct palette data index (yet).

    swap_bytes(pal_data, 512);

    tile_fb.resize (0x10000 * 2);
    tile_map.resize(0x800);
    map_fb.resize  ((32 * 8 + 31) * (28 * 8 + 27) * 2);
    data_fb.resize ((32 * 8 + 31) * (8 * 8 + 7) * 2);

    for (auto i = 0; i < 0x10000; i++)
    {
        const auto t_col = tile_data[i];
        const auto color = *reinterpret_cast<uint16_t *>(pal_data.data() + t_col * 2);

        *reinterpret_cast<uint16_t *>(tile_fb.data() + i * 2) = color;
    }

    for (auto i = 0; i < 0x700; i++)
    {
        tile_map[i] = tile_data[i];
    }

    /* DRAW PALETTE DATA */

    SDL_UpdateTexture(pal_tex, nullptr, pal_data.data(), 64 * 2);
    SDL_RenderCopy   (pal_ren, pal_tex, nullptr, nullptr);
    SDL_RenderPresent(pal_ren);

    /* DRAW VRAM */

    SDL_UpdateTexture(tile_tex, nullptr,  tile_fb.data(), 32 * 8 * 2);
    SDL_RenderCopy   (tile_ren, tile_tex, nullptr, nullptr);
    SDL_RenderPresent(tile_ren);

    swap_bytes(pal_data, 512);

    const auto map_addr = 0x2400; // ??????

    auto map_x  = 0;
    auto map_y  = 0;
    auto grid_x = 0;
    auto grid_y = 0;

    /* DRAW THE TILE MAP */

    for (auto i = 0; i < 0x700; i += 2) // not sure about the tile map being 700h???
    {
        auto tile_x = 0;
        auto tile_y = 0;

        const auto tile = tile_map[i]; // fetch tile from tile map

        while (tile_y < 8)
        {
            while (tile_x < 8)
            {
                /* yay, 4-bit tiles */
                const auto pixel_addr = map_addr + tile * 32 + tile_y * 4 + tile_x / 2;

                auto tile_d = tile_data[pixel_addr];

                if ((tile_x % 2) != 0)
                {
                    tile_d &= 0xF;
                }
                else
                {
                    tile_d >>= 4;
                }

                // TODO: find out how to get the right color
                const auto color = *reinterpret_cast<uint16_t *>(pal_data.data() + tile_d * 4);
               
                const auto index = ((map_y * 8 + tile_y + grid_y) * (32 * 8 + 31) + map_x * 8 + grid_x + tile_x) * 2;

                *reinterpret_cast<uint16_t *>(map_fb.data() + index) = color;

                ++tile_x;
            }

            if (map_x < 31)
            {
                const auto index = ((map_y * 8 + tile_y + grid_y) * (32 * 8 + 31) + map_x * 8 + grid_x + tile_x) * 2;

                *reinterpret_cast<uint16_t *>(map_fb.data() + index) = 0x7FFF;
            }

            tile_x = 0;
            ++tile_y;
        }

        ++map_x;
        ++grid_x;

        if (map_x == 32)
        {
            map_x = 0;
            grid_x = 0;

            if (map_y < 27)
            {
                const auto g_y = 32 * 8 + 31;

                for (auto j = 0; j < g_y; j++)
                {
                    const auto index = ((map_y * 8 + tile_y + grid_y) * (32 * 8 + 31) + j) * 2;
    
                    *reinterpret_cast<uint16_t *>(map_fb.data() + index) = 0x7FFF;
                }

                ++grid_y;
            }

            ++map_y;
        }
    } 

    SDL_UpdateTexture(map_tex, nullptr, map_fb.data(), (32 * 8 + 31) * 2);
    SDL_RenderCopy   (map_ren, map_tex, nullptr, nullptr);
    SDL_RenderPresent(map_ren);

    map_x  = 0;
    map_y  = 0;
    grid_x = 0;
    grid_y = 0;

    /* DRAW TILE DATA */

    for (auto i = 0; i < 256; i++)
    {
        auto tile_x = 0;
        auto tile_y = 0;

        const auto tile = i;

        while (tile_y < 8)
        {
            while (tile_x < 8)
            {
                const auto pixel_addr = map_addr + tile * 32 + tile_y * 4 + tile_x / 2;

                auto tile_d = tile_data[pixel_addr];

                if ((tile_x % 2) != 0)
                {
                    tile_d &= 0xF;
                }
                else
                {
                    tile_d >>= 4;
                }

                // TODO: find out how to get the right color
                const auto color = *reinterpret_cast<uint16_t *>(pal_data.data() + tile_d * 4);
               
                const auto index = ((map_y * 8 + tile_y + grid_y) * (32 * 8 + 31) + map_x * 8 + grid_x + tile_x) * 2;

                *reinterpret_cast<uint16_t *>(data_fb.data() + index) = color;

                ++tile_x;
            }

            if (map_x < 31)
            {
                const auto index = ((map_y * 8 + tile_y + grid_y) * (32 * 8 + 31) + map_x * 8 + grid_x + tile_x) * 2;

                *reinterpret_cast<uint16_t *>(data_fb.data() + index) = 0x7FFF;
            }

            tile_x = 0;
            ++tile_y;
        }

        ++map_x;
        ++grid_x;

        if (map_x == 32)
        {
            map_x = 0;
            grid_x = 0;

            if (map_y < 7)
            {
                const auto g_y = 32 * 8 + 31;

                for (auto j = 0;j < g_y; j++)
                {
                    const auto index = ((map_y * 8 + tile_y + grid_y) * (32 * 8 + 31) + j) * 2;
    
                    *reinterpret_cast<uint16_t *>(data_fb.data() + index) = 0x7FFF;
                }

                ++grid_y;
            }

            ++map_y;
        }
    } 

    SDL_UpdateTexture(data_tex, nullptr, data_fb.data(), (32 * 8 + 31) * 2);
    SDL_RenderCopy   (data_ren, data_tex, nullptr, nullptr);
    SDL_RenderPresent(data_ren);

    while (true)
    {
        while (SDL_PollEvent(&e))
        {

        }
    }
}
