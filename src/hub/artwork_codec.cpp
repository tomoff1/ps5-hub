// SPDX-License-Identifier: GPL-3.0-or-later
// Décodage sur le travailleur, avant envoi au GPU. Entrées et dimensions bornées.
#include "hub/artwork.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <csetjmp>
#include <png.h>
#include <jpeglib.h>
#include <webp/decode.h>

namespace hub
{
namespace
{
bool bounded(int w, int h)
{
    return w > 0 && h > 0 && w <= 2048 && h <= 2048;
}
std::shared_ptr<const Artwork> resize(const std::vector<std::uint8_t> &rgba, int w, int h)
{
    if (!bounded(w, h) || rgba.size() != static_cast<std::size_t>(w * h * 4))
        return {};
    auto out = std::make_shared<Artwork>();
    const float scale = std::min({1.0f, 256.0f / w, 384.0f / h});
    out->width = std::max(1, static_cast<int>(w * scale));
    out->height = std::max(1, static_cast<int>(h * scale));
    out->pixels.resize(static_cast<std::size_t>(out->width * out->height * 4));
    for (int y = 0; y < out->height; ++y)
        for (int x = 0; x < out->width; ++x)
        {
            const auto source =
                static_cast<std::size_t>(((y * h / out->height) * w + x * w / out->width) * 4);
            const auto target = static_cast<std::size_t>((y * out->width + x) * 4);
            std::copy_n(rgba.data() + source, 4, out->pixels.data() + target);
        }
    return out;
}
struct JpegError
{
    jpeg_error_mgr manager;
    std::jmp_buf jump;
};
void jpeg_error(j_common_ptr common)
{
    auto *error = reinterpret_cast<JpegError *>(common->err);
    std::longjmp(error->jump, 1);
}
std::shared_ptr<const Artwork> jpeg(const std::string &data)
{
    // Les objets C++ restent vivants de part et d'autre du point de reprise C.
    std::vector<std::uint8_t> rgb, rgba;
    auto context = std::make_unique<jpeg_decompress_struct>();
    JpegError error{};
    context->err = jpeg_std_error(&error.manager);
    error.manager.error_exit = jpeg_error;
    error.manager.output_message = [](j_common_ptr) {};
    if (setjmp(error.jump))
    {
        jpeg_destroy_decompress(context.get());
        return {};
    }
    jpeg_create_decompress(context.get());
    jpeg_mem_src(context.get(), reinterpret_cast<const unsigned char *>(data.data()), data.size());
    if (jpeg_read_header(context.get(), TRUE) != JPEG_HEADER_OK ||
        !bounded(static_cast<int>(context->image_width), static_cast<int>(context->image_height)))
    {
        jpeg_destroy_decompress(context.get());
        return {};
    }
    context->out_color_space = JCS_RGB;
    context->scale_num = 1;
    context->scale_denom = 1;
    while (context->scale_denom < 8 && context->image_width / context->scale_denom > 512)
        context->scale_denom *= 2;
    jpeg_start_decompress(context.get());
    const int w = static_cast<int>(context->output_width),
              h = static_cast<int>(context->output_height);
    if (!bounded(w, h) || context->output_components != 3)
    {
        jpeg_destroy_decompress(context.get());
        return {};
    }
    rgb.resize(static_cast<std::size_t>(w * h * 3));
    while (context->output_scanline < context->output_height)
    {
        JSAMPROW row = rgb.data() + static_cast<std::size_t>(context->output_scanline) * w * 3;
        if (jpeg_read_scanlines(context.get(), &row, 1) != 1)
        {
            jpeg_destroy_decompress(context.get());
            return {};
        }
    }
    jpeg_finish_decompress(context.get());
    jpeg_destroy_decompress(context.get());
    rgba.resize(static_cast<std::size_t>(w * h * 4));
    for (int i = 0; i < w * h; ++i)
    {
        for (int c = 0; c < 3; ++c)
            rgba[static_cast<std::size_t>(i) * 4 + c] = rgb[static_cast<std::size_t>(i) * 3 + c];
        rgba[static_cast<std::size_t>(i) * 4 + 3] = 255;
    }
    return resize(rgba, w, h);
}
} // namespace
std::shared_ptr<const Artwork> decode_artwork(const std::string &data)
{
    if (data.size() < 12 || data.size() > 8 * 1024 * 1024)
        return {};
    const auto *bytes = reinterpret_cast<const std::uint8_t *>(data.data());
    if (data.compare(0, 4, "RIFF") == 0 && data.compare(8, 4, "WEBP") == 0)
    {
        WebPDecoderConfig config{};
        if (!WebPInitDecoderConfig(&config) ||
            WebPGetFeatures(bytes, data.size(), &config.input) != VP8_STATUS_OK ||
            !bounded(config.input.width, config.input.height))
            return {};
        const float scale =
            std::min({1.0f, 256.0f / config.input.width, 384.0f / config.input.height});
        const int w = std::max(1, static_cast<int>(config.input.width * scale));
        const int h = std::max(1, static_cast<int>(config.input.height * scale));
        std::vector<std::uint8_t> rgba(static_cast<std::size_t>(w * h * 4));
        config.options.use_scaling = 1;
        config.options.scaled_width = w;
        config.options.scaled_height = h;
        config.output.colorspace = MODE_RGBA;
        config.output.is_external_memory = 1;
        config.output.u.RGBA.rgba = rgba.data();
        config.output.u.RGBA.stride = w * 4;
        config.output.u.RGBA.size = rgba.size();
        const auto status = WebPDecode(bytes, data.size(), &config);
        WebPFreeDecBuffer(&config.output);
        return status == VP8_STATUS_OK ? resize(rgba, w, h) : nullptr;
    }
    if (png_sig_cmp(bytes, 0, 8) == 0)
    {
        png_image image{};
        image.version = PNG_IMAGE_VERSION;
        if (!png_image_begin_read_from_memory(&image, bytes, data.size()))
        {
            png_image_free(&image);
            return {};
        }
        if (!bounded(static_cast<int>(image.width), static_cast<int>(image.height)))
        {
            png_image_free(&image);
            return {};
        }
        image.format = PNG_FORMAT_RGBA;
        const int w = static_cast<int>(image.width), h = static_cast<int>(image.height);
        std::vector<std::uint8_t> rgba(PNG_IMAGE_SIZE(image));
        const bool ok = png_image_finish_read(&image, nullptr, rgba.data(), 0, nullptr) != 0;
        png_image_free(&image);
        return ok ? resize(rgba, w, h) : nullptr;
    }
    if (bytes[0] == 0xff && bytes[1] == 0xd8)
        return jpeg(data);
    return {};
}
} // namespace hub
