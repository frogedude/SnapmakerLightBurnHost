// image_processing.cpp - Resize, adjustment, test pattern generation
#include "host.h"

ImageBuffer ResizeImage(const unsigned char* src, int srcW, int srcH, int dstW, int dstH) {
    if (!src || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0)
        return ImageBuffer();

    constexpr int CHANNELS = 3;
    size_t u_srcW = static_cast<size_t>(srcW);
    size_t u_srcH = static_cast<size_t>(srcH);
    size_t u_dstW = static_cast<size_t>(dstW);
    size_t u_dstH = static_cast<size_t>(dstH);

    if (u_srcW > SIZE_MAX / u_srcH) {
        std::cerr << GetTimeStamp() << "[ERR] ResizeImage: source width * height overflow." << std::endl;
        return ImageBuffer();
    }
    if (u_dstW > SIZE_MAX / u_dstH) {
        std::cerr << GetTimeStamp() << "[ERR] ResizeImage: destination width * height overflow." << std::endl;
        return ImageBuffer();
    }
    size_t dstPixels = u_dstW * u_dstH;
    if (dstPixels > SIZE_MAX / CHANNELS) {
        std::cerr << GetTimeStamp() << "[ERR] ResizeImage: destination buffer size overflow." << std::endl;
        return ImageBuffer();
    }
    size_t dstSize = dstPixels * CHANNELS;

    ImageBuffer dstBuf;
    try {
        dstBuf = ImageBuffer(dstSize);
    }
    catch (const std::bad_alloc&) {
        std::cerr << GetTimeStamp() << "[ERR] ResizeImage: memory allocation failed (bad_alloc)." << std::endl;
        return ImageBuffer();
    }

    if (!dstBuf) {
        std::cerr << GetTimeStamp() << "[ERR] ResizeImage: failed to allocate buffer." << std::endl;
        return ImageBuffer();
    }

    int result = stbir_resize_uint8(src, srcW, srcH, 0,
        dstBuf.get(), dstW, dstH, 0,
        CHANNELS);

    if (result != 0) {
        std::cerr << GetTimeStamp() << "[ERR] ResizeImage: stbir_resize_uint8 failed (error code " << result << ")." << std::endl;
        return ImageBuffer();
    }

    return dstBuf;
}

ImageBuffer CreateEnhancedTestPattern(int width, int height) {
    if (width <= 0 || height <= 0) return ImageBuffer();
    int channels = 3;
    size_t bufferSize = static_cast<size_t>(width) * height * channels;
    ImageBuffer imgBuf(bufferSize);
    if (!imgBuf) return ImageBuffer();
    unsigned char* img = imgBuf.get();

    for (size_t i = 0; i < bufferSize; i += 3) {
        img[i] = 200; img[i + 1] = 200; img[i + 2] = 200;
    }
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = (static_cast<size_t>(y) * width + x) * channels;
            if (x % 50 == 0 || y % 50 == 0) {
                img[idx] = 100; img[idx + 1] = 100; img[idx + 2] = 100;
            }
            if (x % 100 == 0 || y % 100 == 0) {
                img[idx] = 50; img[idx + 1] = 50; img[idx + 2] = 50;
            }
        }
    }
    int cx = width / 2, cy = height / 2;
    for (int x = std::max(0, cx - 50); x <= std::min(width - 1, cx + 50); ++x) {
        size_t idx = (static_cast<size_t>(cy) * width + x) * channels;
        img[idx] = 255; img[idx + 1] = 0; img[idx + 2] = 0;
    }
    for (int y = std::max(0, cy - 50); y <= std::min(height - 1, cy + 50); ++y) {
        size_t idx = (static_cast<size_t>(y) * width + cx) * channels;
        img[idx] = 255; img[idx + 1] = 0; img[idx + 2] = 0;
    }
    for (int y = 10; y < std::min(30, height); ++y) {
        for (int x = 10; x < std::min(30, width); ++x) {
            size_t idx = (static_cast<size_t>(y) * width + x) * channels;
            img[idx] = 0; img[idx + 1] = 0; img[idx + 2] = 255;
        }
    }
    for (int y = 10; y < std::min(30, height); ++y) {
        for (int x = std::max(10, width - 30); x < width - 10; ++x) {
            size_t idx = (static_cast<size_t>(y) * width + x) * channels;
            img[idx] = 0; img[idx + 1] = 255; img[idx + 2] = 0;
        }
    }
    for (int y = std::max(10, height - 30); y < height - 10; ++y) {
        for (int x = 10; x < std::min(30, width); ++x) {
            size_t idx = (static_cast<size_t>(y) * width + x) * channels;
            img[idx] = 255; img[idx + 1] = 255; img[idx + 2] = 0;
        }
    }
    for (int y = std::max(10, height - 30); y < height - 10; ++y) {
        for (int x = std::max(10, width - 30); x < width - 10; ++x) {
            size_t idx = (static_cast<size_t>(y) * width + x) * channels;
            img[idx] = 255; img[idx + 1] = 0; img[idx + 2] = 255;
        }
    }
    return imgBuf;
}

ImageBuffer ApplyImageAdjustment(const unsigned char* src, int width, int height, int channels, int offsetX, int offsetY) {
    if (!src || width <= 0 || height <= 0 || channels <= 0)
        return ImageBuffer();

    constexpr int EXPECTED_CHANNELS = 3;
    if (channels != EXPECTED_CHANNELS) {
        std::cerr << GetTimeStamp() << "[ERR] ApplyImageAdjustment: unexpected channel count " << channels << std::endl;
        return ImageBuffer();
    }

    int validOffsetX = offsetX;
    int validOffsetY = offsetY;
    bool clamped = false;

    int maxOffsetX = width - 1;
    int minOffsetX = -width + 1;
    int maxOffsetY = height - 1;
    int minOffsetY = -height + 1;

    if (offsetX < minOffsetX) {
        validOffsetX = minOffsetX;
        clamped = true;
    }
    else if (offsetX > maxOffsetX) {
        validOffsetX = maxOffsetX;
        clamped = true;
    }

    if (offsetY < minOffsetY) {
        validOffsetY = minOffsetY;
        clamped = true;
    }
    else if (offsetY > maxOffsetY) {
        validOffsetY = maxOffsetY;
        clamped = true;
    }

    if (clamped) {
        std::cout << GetTimeStamp() << "[WARN] Adjustment offsets clamped to valid range. "
            << "Original: X=" << offsetX << ", Y=" << offsetY << " -> "
            << "Clamped: X=" << validOffsetX << ", Y=" << validOffsetY << std::endl;
    }

    if (validOffsetX <= -width || validOffsetX >= width ||
        validOffsetY <= -height || validOffsetY >= height) {
        std::cerr << GetTimeStamp() << "[ERR] ApplyImageAdjustment: No valid pixels after clamping" << std::endl;
        return ImageBuffer();
    }

    size_t w = static_cast<size_t>(width);
    size_t h = static_cast<size_t>(height);
    if (w > SIZE_MAX / h) {
        std::cerr << GetTimeStamp() << "[ERR] ApplyImageAdjustment: width * height overflow." << std::endl;
        return ImageBuffer();
    }
    size_t pixels = w * h;
    if (pixels > SIZE_MAX / EXPECTED_CHANNELS) {
        std::cerr << GetTimeStamp() << "[ERR] ApplyImageAdjustment: buffer size overflow." << std::endl;
        return ImageBuffer();
    }
    size_t bufferSize = pixels * EXPECTED_CHANNELS;

    ImageBuffer dstBuf;
    try {
        dstBuf = ImageBuffer(bufferSize);
    }
    catch (const std::bad_alloc&) {
        std::cerr << GetTimeStamp() << "[ERR] ApplyImageAdjustment: memory allocation failed." << std::endl;
        return ImageBuffer();
    }
    if (!dstBuf) {
        std::cerr << GetTimeStamp() << "[ERR] ApplyImageAdjustment: failed to allocate buffer." << std::endl;
        return ImageBuffer();
    }

    unsigned char* dst = dstBuf.get();
    std::fill(dst, dst + bufferSize, 0);

    for (int y = 0; y < height; ++y) {
        int srcY = y + validOffsetY;
        if (srcY < 0 || srcY >= height)
            continue;

        for (int x = 0; x < width; ++x) {
            int srcX = x + validOffsetX;
            if (srcX < 0 || srcX >= width)
                continue;

            size_t dstIdx = (static_cast<size_t>(y) * width + x) * EXPECTED_CHANNELS;
            size_t srcIdx = (static_cast<size_t>(srcY) * width + srcX) * EXPECTED_CHANNELS;

            memcpy(dst + dstIdx, src + srcIdx, EXPECTED_CHANNELS);
        }
    }
    return dstBuf;
}

// ---------- Image Adjustment Functions ----------
void SaveConfigDebounced() {
    fs::path configPath = HostContext::instance().getConfigPath();

    if (configPath.empty()) {
        std::cerr << GetTimeStamp() << "[ERR] Cannot save config: config path is empty" << std::endl;
        return;
    }

    if (HostContext::instance().shouldDebounceSave()) {
        try {
            SaveConfig(configPath.string());
            HostContext::instance().updateLastSaveTime();
        }
        catch (const std::exception& e) {
            std::cerr << GetTimeStamp() << "[ERR] Failed to save config: " << e.what() << std::endl;
        }
    }
}

void DisplayAdjustmentStatus() {
    UserConfig cfg = HostContext::instance().snapshotConfig();
    std::cout << "Image Adjustment: X=" << cfg.adjustment.pixelsX << " pixels, Y=" << cfg.adjustment.pixelsY << " pixels";
    if (cfg.adjustment.useAdjustment) std::cout << " [Active]";
    else std::cout << " [Disabled]";
    std::cout << std::endl;
}

void AdjustImageLeft() {
    HostContext::instance().updateConfig([](UserConfig& cfg) {
        int newValue = cfg.adjustment.pixelsX - ADJUST_STEP_PIXELS;
        if (newValue >= -MAX_ADJUST_PIXELS) {
            cfg.adjustment.pixelsX = newValue;
            cfg.adjustment.useAdjustment = true;
            std::cout << GetTimeStamp() << "[OK] Image adjustment: X = " << cfg.adjustment.pixelsX
                << " pixels, Y = " << cfg.adjustment.pixelsY << " pixels" << std::endl;
            return true;
        }
        else {
            std::cout << GetTimeStamp() << "[WARN] Maximum left adjustment reached. Limit: "
                << -MAX_ADJUST_PIXELS << " pixels" << std::endl;
            return false;
        }
        });
    ClearAndDrawHeader(true);
    SaveConfigDebounced();
}

void AdjustImageRight() {
    HostContext::instance().updateConfig([](UserConfig& cfg) {
        int newValue = cfg.adjustment.pixelsX + ADJUST_STEP_PIXELS;
        if (newValue <= MAX_ADJUST_PIXELS) {
            cfg.adjustment.pixelsX = newValue;
            cfg.adjustment.useAdjustment = true;
            std::cout << GetTimeStamp() << "[OK] Image adjustment: X = " << cfg.adjustment.pixelsX
                << " pixels, Y = " << cfg.adjustment.pixelsY << " pixels" << std::endl;
            return true;
        }
        else {
            std::cout << GetTimeStamp() << "[WARN] Maximum right adjustment reached. Limit: "
                << MAX_ADJUST_PIXELS << " pixels" << std::endl;
            return false;
        }
        });
    ClearAndDrawHeader(true);
    SaveConfigDebounced();
}

void AdjustImageUp() {
    HostContext::instance().updateConfig([](UserConfig& cfg) {
        int newValue = cfg.adjustment.pixelsY - ADJUST_STEP_PIXELS;
        if (newValue >= -MAX_ADJUST_PIXELS) {
            cfg.adjustment.pixelsY = newValue;
            cfg.adjustment.useAdjustment = true;
            std::cout << GetTimeStamp() << "[OK] Image adjustment: X = " << cfg.adjustment.pixelsX
                << " pixels, Y = " << cfg.adjustment.pixelsY << " pixels" << std::endl;
            return true;
        }
        else {
            std::cout << GetTimeStamp() << "[WARN] Maximum up adjustment reached. Limit: "
                << -MAX_ADJUST_PIXELS << " pixels" << std::endl;
            return false;
        }
        });
    ClearAndDrawHeader(true);
    SaveConfigDebounced();
}

void AdjustImageDown() {
    HostContext::instance().updateConfig([](UserConfig& cfg) {
        int newValue = cfg.adjustment.pixelsY + ADJUST_STEP_PIXELS;
        if (newValue <= MAX_ADJUST_PIXELS) {
            cfg.adjustment.pixelsY = newValue;
            cfg.adjustment.useAdjustment = true;
            std::cout << GetTimeStamp() << "[OK] Image adjustment: X = " << cfg.adjustment.pixelsX
                << " pixels, Y = " << cfg.adjustment.pixelsY << " pixels" << std::endl;
            return true;
        }
        else {
            std::cout << GetTimeStamp() << "[WARN] Maximum down adjustment reached. Limit: "
                << MAX_ADJUST_PIXELS << " pixels" << std::endl;
            return false;
        }
        });
    ClearAndDrawHeader(true);
    SaveConfigDebounced();
}

void ResetImageAdjustment() {
    HostContext::instance().updateConfig([](UserConfig& cfg) {
        cfg.adjustment.pixelsX = 0;
        cfg.adjustment.pixelsY = 0;
        cfg.adjustment.useAdjustment = false;
        std::cout << GetTimeStamp() << "[OK] Image adjustment reset to zero." << std::endl;
        return true;
        });
    ClearAndDrawHeader(true);
    SaveConfig(HostContext::instance().getConfigPath().string());
}

void ToggleImageAdjustment() {
    HostContext::instance().updateConfig([](UserConfig& cfg) {
        cfg.adjustment.useAdjustment = !cfg.adjustment.useAdjustment;
        std::cout << GetTimeStamp() << "[OK] Image adjustment "
            << (cfg.adjustment.useAdjustment ? "enabled" : "disabled") << std::endl;
        return true;
        });
    ClearAndDrawHeader(true);
    SaveConfig(HostContext::instance().getConfigPath().string());
}