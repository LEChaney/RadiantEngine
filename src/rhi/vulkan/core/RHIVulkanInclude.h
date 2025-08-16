#pragma once

#include "volk.h"
#include "fmt/printf.h"

#if !defined(NDEBUG)
    #include <vulkan/vk_enum_string_helper.h>
    #define VK_CHECK(x)                                                     \
        do {                                                                \
            VkResult err = x;                                               \
            if (err) {                                                      \
                fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
                abort();                                                    \
            }                                                               \
        } while (0)
#else
    #define VK_CHECK(x) ((void)0)
#endif
