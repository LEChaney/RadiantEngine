
#pragma once 

namespace vkutil {

    void transition_image(
        VkCommandBuffer cmd,
        VkImage image,
        VkImageLayout currentLayout,
        VkImageLayout newLayout
    );

    void copy_image_to_image(
        VkCommandBuffer cmd,
        VkImage src,
        VkImage dst,
        VkExtent2D srcExtent,
        VkExtent2D dstExtent
    );

    void generate_mipmaps(
        VkCommandBuffer cmd,
        VkImage image,
        VkExtent2D imageSize
    );

    std::string vk_format_to_string(VkFormat format);
    std::string vk_color_space_to_string(VkColorSpaceKHR colorSpace);
};