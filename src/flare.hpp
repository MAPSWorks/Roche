#pragma once

#include <vector>
#include <cstdint>

/**
 * Generates the 1D flare intensity image (white dot)
 * @param size size of the image to generate
 * @return pixel data of image
 */
std::vector<uint16_t> generateFlareIntensityTex(int size);
/**
 * Generates the 2D flare line image (radial lines to simulate the eye)
 * @param size size of the image to generate
 * @return pixel data of image
 */
std::vector<uint8_t> generateFlareLinesTex(int size);

/**
 * Generates the 1D flare halo image (rainbow-y halo)
 * @param size size of the image to generate
 * @return pixel data of image
 */
std::vector<uint16_t> generateFlareHaloTex(int size);