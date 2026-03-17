#pragma once
#include <drivers/graphics/common.h>
#include <limine.h>

namespace drivers{
    class GOP : public GraphicsDriver {
        public:
        /* @brief A basic framebuffer driver
         * @param limine_framebuffer* framebuffer
        */
        GOP(limine_framebuffer* fb);

        // @brief Sets a pixel to a specific color
        void PlotPixel(uint16_t x, uint16_t y, uint32_t color);

        // @brief Returns the color of a pixel
        uint32_t GetPixel(uint16_t x, uint16_t y);

        // @brief Draws a rectangle
        void DrawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color);

        // @brief Clears the screen
        void ClearScreen(uint32_t Color = 0);

        // @brief Updates the screen (In case of double buffering)
        void Update();

        // @brief Updates a specific region in the screen (In case of double buffering)
        void Update(int x, int y, int width, int height);

        // @brief Scrolls up by the amount of pixels provided
        void Scroll(int y);

        private:
        // @brief A backbuffer (To speed up read operations)
        uint32_t* backbuffer;

        // @brief The framebuffer;
        uint32_t* framebuffer;
    };
}