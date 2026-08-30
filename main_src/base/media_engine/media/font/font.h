#pragma once

#include <string>
#include <memory>
#include <stdint.h>
#include "colors.h"

namespace media_engine {

class Font {
    public:
        explicit Font(const std::string &file_path, int font_size);
        ~Font();

        bool RenderText(const std::string& text, float x, float y, float r, float g, float b, float a);

    private:
        struct FontImpl;
         std::unique_ptr<FontImpl> impl_{nullptr};
};

} // namespace media_engine
