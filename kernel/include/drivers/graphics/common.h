/* A Common interface for all graphics drivers (Not that I plan to write any... but you never know) */
#pragma once

#include <stdint.h>
#include <stddef.h>

namespace drivers{
    class GraphicsDriver{
        public:
        
        // @brief Sets a pixel to a specific color
        virtual void PlotPixel(uint16_t x, uint16_t y, uint32_t color) = 0;

        // @brief Returns the color of a pixel
        virtual uint32_t GetPixel(uint16_t x, uint16_t y) = 0;

        // @brief Draws a rectangle
        virtual void DrawRectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t color) = 0;

        // @brief Clears the screen
        virtual void ClearScreen(uint32_t Color = 0) = 0;

        // @brief Updates the screen (In case of double buffering)
        virtual void Update() = 0;

        // @brief Updates a specific region in the screen (In case of double buffering)
        virtual void Update(int x, int y, int width, int height) = 0;

        // @brief Scrolls up by the amount of pixels provided
        virtual void Scroll(int y) = 0;
        
        // @brief The vertical resolution
        uint32_t height;

        // @brief The horizontal resolution
        uint32_t width;

        // @brief Bit Depth
        uint32_t bits_per_pixel;

        // @brief The screen's pitch
        uint32_t pitch;
    };
}