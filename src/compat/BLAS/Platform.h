#pragma once

namespace BLAS
{
enum class Platform : unsigned char
{
    Default = 0,
    Reference,
    OpenBLAS,
    MKL
};
}
