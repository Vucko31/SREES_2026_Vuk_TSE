#pragma once

#include "TSEPlugin.h"
#include <atomic>
#include <functional>
#include <string>

struct ConversionResult
{
    bool ok = false;
    std::string message;
    std::string preview;
    std::string outputPath;
    bool openInDTwin = true;
};

using ProgressCallback = std::function<void(double, const std::string&)>;

ConversionResult convertToComplexCoordinates(const std::string& inputPath,
                                             const std::string& outputPath,
                                             const ConversionOptions& options,
                                             std::atomic_bool& cancelRequested,
                                             const ProgressCallback& onProgress);
