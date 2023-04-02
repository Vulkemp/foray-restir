#include "emissive_triangle_mesh_stage.hpp"
#include <scene/globalcomponents/foray_cameramanager.hpp>
#include <scene/globalcomponents/foray_drawmanager.hpp>
#include <scene/globalcomponents/foray_materialmanager.hpp>
#include <scene/globalcomponents/foray_texturemanager.hpp>
#include <util/foray_pipelinebuilder.hpp>

using namespace foray;

struct Vertex
{
    glm::vec3 pos;

    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding   = 0;
        bindingDescription.stride    = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 1> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 1> attributeDescriptions{};

        attributeDescriptions[0].binding  = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset   = offsetof(Vertex, pos);

        return attributeDescriptions;
    }
};

void EmissiveTriangleMeshStage::RecordFrame(VkCommandBuffer cmdBuffer, foray::base::FrameRenderInfo& renderInfo)
{
    {
        std::vector<VkImageMemoryBarrier2> barriers;
        VkImageMemoryBarrier2              barrier = VkImageMemoryBarrier2{
                         .sType               = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                         .srcStageMask        = VK_PIPELINE_STAGE_2_NONE,
                         .srcAccessMask       = VK_ACCESS_2_NONE,
                         .dstStageMask        = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                         .dstAccessMask       = 0,
                         .oldLayout           = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
                         .newLayout           = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
                         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                         .image               = mOutput->GetImage(),
                         .subresourceRange =
                VkImageSubresourceRange{
                                 .aspectMask     = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
                                 .baseMipLevel   = 0,
                                 .levelCount     = VK_REMAINING_MIP_LEVELS,
                                 .baseArrayLayer = 0,
                                 .layerCount     = VK_REMAINING_ARRAY_LAYERS,
                },
        };
        barriers.push_back(barrier);

        VkDependencyInfo depInfo{
            .sType = VkStructureType::VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = (uint32_t)barriers.size(), .pImageMemoryBarriers = barriers.data()};

        vkCmdPipelineBarrier2(cmdBuffer, &depInfo);
    }

    {
        std::vector<VkImageMemoryBarrier2> barriers;
        VkImageMemoryBarrier2              barrier = VkImageMemoryBarrier2{
                         .sType               = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                         .srcStageMask        = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                         .srcAccessMask       = VK_ACCESS_2_NONE,
                         .dstStageMask        = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                         .dstAccessMask       = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                         .oldLayout           = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
                         .newLayout           = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                         .image               = mDepthImage->GetImage(),
                         .subresourceRange =
                VkImageSubresourceRange{
                                 .aspectMask     = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT,
                                 .baseMipLevel   = 0,
                                 .levelCount     = VK_REMAINING_MIP_LEVELS,
                                 .baseArrayLayer = 0,
                                 .layerCount     = VK_REMAINING_ARRAY_LAYERS,
                },
        };
        barriers.push_back(barrier);

        VkDependencyInfo depInfo{
            .sType = VkStructureType::VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = (uint32_t)barriers.size(), .pImageMemoryBarriers = barriers.data()};

        vkCmdPipelineBarrier2(cmdBuffer, &depInfo);
    }


    //VkRenderPassBeginInfo renderPassBeginInfo{};
    //renderPassBeginInfo.sType             = VkStructureType::VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    //renderPassBeginInfo.renderPass        = mRenderpass;
    //renderPassBeginInfo.framebuffer       = mFrameBuffer;
    //renderPassBeginInfo.renderArea.extent = mContext->GetSwapchainSize();

    //std::array<VkClearValue, 2> clearValues{};
    //clearValues[0].color        = {{0.0f, 1.0f, 0.0f, 1.0f}};
    //clearValues[1].depthStencil = {1.0f, 0};

    //renderPassBeginInfo.clearValueCount = clearValues.size();
    //renderPassBeginInfo.pClearValues    = clearValues.data();

    //vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    //VkViewport viewport{0.f, 0.f, (float)mContext->GetSwapchainSize().width, (float)mContext->GetSwapchainSize().height, 0.0f, 1.0f};
    //vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    //VkRect2D scissor{VkOffset2D{}, VkExtent2D{mContext->GetSwapchainSize()}};
    //vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

    //vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);

    //VkDescriptorSet descriptorSet = mDescriptorSet.GetDescriptorSet();
    //// Instanced object
    ////vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayoutV2, 0, 1, &descriptorSet, 0, nullptr);

    //VkBuffer     buffer = mVertexBuffer2.GetBuffer();
    //VkDeviceSize offset = 0;
    //vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &buffer, &offset);
    //VkBuffer iBuf = mIndices2.GetBuffer();
    //vkCmdBindIndexBuffer(cmdBuffer, iBuf, 0, VK_INDEX_TYPE_UINT32);
    ////vkCmdDraw(cmdBuffer, mTriangleVertices.size(), 0, 0, 0);
    //vkCmdDrawIndexed(cmdBuffer, 3, 0, 0, 0, 0);

    //vkCmdEndRenderPass(cmdBuffer);


    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = mRenderpassV2;
    renderPassInfo.framebuffer       = mFrameBufferV2;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = mContext->Swapchain->extent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{0.0f, 1.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount   = clearValues.size();
    renderPassInfo.pClearValues    = clearValues.data();

    vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineV2);

	/*VkBuffer buffer = mVertexBuffer2.GetBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &buffer, &offset);
    vkCmdDraw(cmdBuffer, 3, 1, 0, 0);*/

	VkDescriptorSet descriptorSet = mDescriptorSet.GetDescriptorSet();
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayoutV2, 0, 1, &descriptorSet, 0, nullptr);

	VkBuffer     buffer = mVertexBuffer.GetBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &buffer, &offset);
    vkCmdDraw(cmdBuffer, mTriangleVertices.size(), 1, 0, 0);

    vkCmdEndRenderPass(cmdBuffer);

    {
        std::vector<VkImageMemoryBarrier2> barriers;
        VkImageMemoryBarrier2              barrier = VkImageMemoryBarrier2{
                         .sType               = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                         .srcStageMask        = VK_PIPELINE_STAGE_2_NONE,
                         .srcAccessMask       = VK_ACCESS_2_NONE,
                         .dstStageMask        = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                         .dstAccessMask       = 0,
                         .oldLayout           = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
                         .newLayout           = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
                         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                         .image               = mOutput->GetImage(),
                         .subresourceRange =
                VkImageSubresourceRange{
                                 .aspectMask     = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
                                 .baseMipLevel   = 0,
                                 .levelCount     = VK_REMAINING_MIP_LEVELS,
                                 .baseArrayLayer = 0,
                                 .layerCount     = VK_REMAINING_ARRAY_LAYERS,
                },
        };
        barriers.push_back(barrier);

        VkDependencyInfo depInfo{
            .sType = VkStructureType::VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = (uint32_t)barriers.size(), .pImageMemoryBarriers = barriers.data()};

        vkCmdPipelineBarrier2(cmdBuffer, &depInfo);
    }
}

void EmissiveTriangleMeshStage::CreatePipeline()
{
    if(mPipeline != nullptr)
    {
        vkDestroyPipeline(mContext->Device(), mPipeline, nullptr);
        mPipeline = nullptr;
    }

    foray::scene::VertexInputStateBuilder vertexInputStateBuilder;
    vertexInputStateBuilder.AddVertexComponentBinding(scene::EVertexComponent::Position);
    vertexInputStateBuilder.SetStride((uint32_t)sizeof(glm::vec3));
    vertexInputStateBuilder.Build();

    foray::util::PipelineBuilder builder;
    builder.SetContext(mContext);
    builder.SetVertexInputStateBuilder(&vertexInputStateBuilder);

    std::vector<VkPipelineShaderStageCreateInfo> createInfos(2);
    createInfos[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfos[0].pName  = "main";
    createInfos[0].module = mShaderModuleVert;
    createInfos[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;

    createInfos[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfos[1].pName  = "main";
    createInfos[1].module = mShaderModuleFrag;
    createInfos[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;

    builder.SetShaderStageCreateInfos(&createInfos);
    builder.SetRenderPass(mRenderpass);
    builder.SetColorAttachmentBlendCount(1);
    builder.SetCullMode(VK_CULL_MODE_NONE);
    builder.SetPipelineLayout(mPipelineLayout.GetPipelineLayout());
    mPipeline = builder.Build();
}

void EmissiveTriangleMeshStage::CreateShaders()
{
    foray::core::ShaderCompilerConfig options{.IncludeDirs = {FORAY_SHADER_DIR}};

    mShaderKeys.push_back(mShaderModuleVert.CompileFromSource(mContext, VERT_FILE, options));
    mShaderKeys.push_back(mShaderModuleFrag.CompileFromSource(mContext, FRAG_FILE, options));
};

void EmissiveTriangleMeshStage::CreateTriangleVertexBuffer()
{

    mTriangleVertices.reserve(mTriangles->size() * 3);

    for(size_t i = 0; i < mTriangles->size(); i++)
    {
        mTriangleVertices.push_back((*mTriangles)[i].p1);
        mTriangleVertices.push_back((*mTriangles)[i].p2);
        mTriangleVertices.push_back((*mTriangles)[i].p3);
    }

    VkBufferUsageFlags       bufferUsage    = VkBufferUsageFlagBits::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkDeviceSize             bufferSize     = mTriangleVertices.size() * sizeof(glm::vec3);
    VmaMemoryUsage           bufferMemUsage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    VmaAllocationCreateFlags allocFlags     = 0;
    mVertexBuffer.Create(mContext, bufferUsage, bufferSize, bufferMemUsage, allocFlags, "TriangleLightsVertexBuffer");
    mVertexBuffer.WriteDataDeviceLocal(mTriangleVertices.data(), bufferSize);

    std::vector<glm::vec3> p            = {{0.5, 0.0, 0}, {1, 1.0, 0}, {0, 1, 0}};
 
    //std::vector<glm::vec3> p            = {{0.0, -0.5, 0}, {0.5, 0.5, 0}, {-0.5, 0.5, 0}};
    VkBufferUsageFlags     bufferUsage2 = VkBufferUsageFlagBits::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkDeviceSize           bufferSize2  = p.size() * sizeof(glm::vec3);
    mVertexBuffer2.Create(mContext, bufferUsage2, bufferSize2, bufferMemUsage, allocFlags, "TriangleXXX");
    mVertexBuffer2.WriteDataDeviceLocal(p.data(), bufferSize2);

    std::vector<uint32_t> i            = {0, 1, 2};
    VkBufferUsageFlags    bufferUsage3 = VkBufferUsageFlagBits::VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkDeviceSize          bufferSize3  = p.size() * sizeof(uint32_t);
    mIndices2.Create(mContext, bufferUsage3, bufferSize3, bufferMemUsage, allocFlags, "indices");
    mIndices2.WriteDataDeviceLocal(i.data(), bufferSize3);
}

void EmissiveTriangleMeshStage::PrepareRenderpass()
{

    // Color Output
    VkAttachmentDescription colorAttachmentDesc{};
    colorAttachmentDesc.samples        = mOutput->GetSampleCount();
    colorAttachmentDesc.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentDesc.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentDesc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentDesc.initialLayout  = VK_IMAGE_LAYOUT_GENERAL;
    colorAttachmentDesc.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentDesc.format         = mOutput->GetFormat();

    // Depth Output
    VkAttachmentDescription depthAttachmentDescription{};
    depthAttachmentDescription.samples        = mDepthImage->GetSampleCount();
    depthAttachmentDescription.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachmentDescription.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachmentDescription.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachmentDescription.initialLayout  = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachmentDescription.finalLayout    = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachmentDescription.format         = mDepthImage->GetFormat();

    std::vector<VkAttachmentDescription> attachmentDescriptions;
    attachmentDescriptions.push_back(colorAttachmentDesc);
    attachmentDescriptions.push_back(depthAttachmentDescription);

    VkAttachmentReference colorAttachmentReference{};
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout     = VK_IMAGE_LAYOUT_GENERAL;
    VkAttachmentReference depthAttachmentReference{};
    depthAttachmentReference.attachment = 1;
    depthAttachmentReference.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Subpass description
    VkSubpassDescription subpass    = {};
    subpass.pipelineBindPoint       = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorAttachmentReference;
    subpass.pDepthStencilAttachment = &depthAttachmentReference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // Subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies;

    // This makes sure that writes to the depth image are done before we try to write to it again
    dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass      = 0;
    dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencies[0].dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[1].dstSubpass      = 0;
    dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.pAttachments           = attachmentDescriptions.data();
    renderPassInfo.attachmentCount        = static_cast<uint32_t>(attachmentDescriptions.size());
    renderPassInfo.subpassCount           = 1;
    renderPassInfo.pSubpasses             = &subpass;
    renderPassInfo.dependencyCount        = 2;
    renderPassInfo.pDependencies          = dependencies.data();
    AssertVkResult(vkCreateRenderPass(mContext->Device(), &renderPassInfo, nullptr, &mRenderpass));

    std::vector<VkImageView> attachmentViews;
    attachmentViews.push_back(mOutput->GetImageView());
    attachmentViews.push_back(mDepthImage->GetImageView());

    VkFramebufferCreateInfo fbufCreateInfo = {};
    fbufCreateInfo.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbufCreateInfo.pNext                   = NULL;
    fbufCreateInfo.renderPass              = mRenderpass;
    fbufCreateInfo.pAttachments            = attachmentViews.data();
    fbufCreateInfo.attachmentCount         = static_cast<uint32_t>(attachmentViews.size());
    fbufCreateInfo.width                   = mContext->GetSwapchainSize().width;
    fbufCreateInfo.height                  = mContext->GetSwapchainSize().height;
    fbufCreateInfo.layers                  = 1;
    AssertVkResult(vkCreateFramebuffer(mContext->Device(), &fbufCreateInfo, nullptr, &mFrameBuffer));
}

void EmissiveTriangleMeshStage::SetupDescriptors()
{
    auto materialBuffer = mScene->GetComponent<scene::gcomp::MaterialManager>();
    auto textureStore   = mScene->GetComponent<scene::gcomp::TextureManager>();
    auto cameraManager  = mScene->GetComponent<scene::gcomp::CameraManager>();
    mDescriptorSet.SetDescriptorAt(0, materialBuffer->GetVkDescriptorInfo(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    mDescriptorSet.SetDescriptorAt(1, textureStore->GetDescriptorInfos(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    mDescriptorSet.SetDescriptorAt(2, cameraManager->GetVkDescriptorInfo(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
}

void EmissiveTriangleMeshStage::CreateDescriptorSets()
{
    mDescriptorSet.Create(mContext, "EmissiveTris_DescriptorSet");
}

void EmissiveTriangleMeshStage::CreatePipelineLayout()
{
    mPipelineLayout.AddDescriptorSetLayout(mDescriptorSet.GetDescriptorSetLayout());
    mPipelineLayout.AddPushConstantRange<scene::DrawPushConstant>(VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT | VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT);
    mPipelineLayout.Build(mContext);
}

void EmissiveTriangleMeshStage::CreateImages()
{
    static const VkFormat colorFormat    = VK_FORMAT_R16G16B16A16_SFLOAT;
    static const VkFormat geometryFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

    static const VkImageUsageFlags imageUsageFlags =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    VkExtent2D extent = mContext->GetSwapchainSize();

    VkClearValue defaultClearValue = {VkClearColorValue{{0, 0, 0, 0}}};


    {  // Position
        mColorOutput.Create(mContext, imageUsageFlags, geometryFormat, extent, "EmiTri_ColorOutput");
    }

    {  // Depth
        VkImageUsageFlags depthUsage =
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        core::ManagedImage::CreateInfo ci(depthUsage, VK_FORMAT_D32_SFLOAT, extent, "EmiTri_DepthOutput");
        ci.ImageViewCI.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;
        mDepthOutput.Create(mContext, ci);
    }

    //{  // Pre-transfer to correct layout
    //    core::HostSyncCommandBuffer commandBuffer;
    //    commandBuffer.Create(mContext);
    //    commandBuffer.Begin();


    //    std::vector<VkImageMemoryBarrier2> barriers;

    //    VkImageMemoryBarrier2 attachmentMemBarrier{
    //        .sType               = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    //        .srcStageMask        = VK_PIPELINE_STAGE_2_NONE,
    //        .srcAccessMask       = VK_ACCESS_2_NONE,
    //        .dstStageMask        = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    //        .dstAccessMask       = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    //        .oldLayout           = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
    //        .newLayout           = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
    //        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    //        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    //        .image               = mColorOutput.GetImage(),
    //        .subresourceRange =
    //            VkImageSubresourceRange{
    //                .aspectMask     = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
    //                .baseMipLevel   = 0,
    //                .levelCount     = VK_REMAINING_MIP_LEVELS,
    //                .baseArrayLayer = 0,
    //                .layerCount     = VK_REMAINING_ARRAY_LAYERS,
    //            },
    //    };


    //    barriers.push_back(attachmentMemBarrier);


    //    VkImageMemoryBarrier2 depthBarrier = VkImageMemoryBarrier2{
    //        .sType               = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    //        .srcStageMask        = VK_PIPELINE_STAGE_2_NONE,
    //        .srcAccessMask       = VK_ACCESS_2_NONE,
    //        .dstStageMask        = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
    //        .dstAccessMask       = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    //        .oldLayout           = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
    //        .newLayout           = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    //        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    //        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    //        .image               = mDepthOutput.GetImage(),
    //        .subresourceRange =
    //            VkImageSubresourceRange{
    //                .aspectMask     = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT,
    //                .baseMipLevel   = 0,
    //                .levelCount     = VK_REMAINING_MIP_LEVELS,
    //                .baseArrayLayer = 0,
    //                .layerCount     = VK_REMAINING_ARRAY_LAYERS,
    //            },
    //    };
    //    barriers.push_back(depthBarrier);

    //    VkDependencyInfo depInfo{
    //        .sType = VkStructureType::VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = (uint32_t)barriers.size(), .pImageMemoryBarriers = barriers.data()};

    //    vkCmdPipelineBarrier2(commandBuffer, &depInfo);

    //    commandBuffer.SubmitAndWait();
    //}
}

/////

void EmissiveTriangleMeshStage::createGraphicsPipeline()
{
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = mShaderModuleVert;
    vertShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = mShaderModuleFrag;
    fragShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto bindingDescription    = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = (float)mContext->Swapchain->extent.width;
    viewport.height   = (float)mContext->Swapchain->extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = mContext->Swapchain->extent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_LINE;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable     = VK_FALSE;
    colorBlending.logicOp           = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount   = 1;
    colorBlending.pAttachments      = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    auto descriptorSetLayout = mDescriptorSet.GetDescriptorSetLayout();
    pipelineLayoutInfo.pSetLayouts            = &descriptorSetLayout;

    if(vkCreatePipelineLayout(mContext->Device(), &pipelineLayoutInfo, nullptr, &mPipelineLayoutV2) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable       = VK_TRUE;
    depthStencil.depthWriteEnable      = VK_TRUE;
    depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds        = 0.0f;  // Optional
    depthStencil.maxDepthBounds        = 1.0f;  // Optional
    depthStencil.stencilTestEnable     = VK_FALSE;
    depthStencil.front                 = {};  // Optional
    depthStencil.back                  = {};  // Optional

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.layout              = mPipelineLayoutV2;
    pipelineInfo.renderPass          = mRenderpassV2;
    pipelineInfo.subpass             = 0;
    pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
    pipelineInfo.pDepthStencilState  = &depthStencil;


    if(vkCreateGraphicsPipelines(mContext->Device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mPipelineV2) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
}

void EmissiveTriangleMeshStage::createRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = mOutput->GetFormat();
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth Output
    VkAttachmentDescription depthAttachmentDescription{};
    depthAttachmentDescription.samples        = mDepthImage->GetSampleCount();
    depthAttachmentDescription.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachmentDescription.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachmentDescription.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachmentDescription.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachmentDescription.finalLayout    = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachmentDescription.format         = mDepthImage->GetFormat();

    std::vector<VkAttachmentDescription> attachmentDescriptions;
    attachmentDescriptions.push_back(colorAttachment);
    attachmentDescriptions.push_back(depthAttachmentDescription);

    VkAttachmentReference depthAttachmentReference{};
    depthAttachmentReference.attachment = 1;
    depthAttachmentReference.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentReference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = attachmentDescriptions.size();
    renderPassInfo.pAttachments    = attachmentDescriptions.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    if(vkCreateRenderPass(mContext->Device(), &renderPassInfo, nullptr, &mRenderpassV2) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
    }

    std::vector<VkImageView> attachmentViews;
    attachmentViews.push_back(mOutput->GetImageView());
    attachmentViews.push_back(mDepthImage->GetImageView());

    VkFramebufferCreateInfo fbufCreateInfo = {};
    fbufCreateInfo.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbufCreateInfo.pNext                   = NULL;
    fbufCreateInfo.renderPass              = mRenderpassV2;
    fbufCreateInfo.pAttachments            = attachmentViews.data();
    fbufCreateInfo.attachmentCount         = static_cast<uint32_t>(attachmentViews.size());
    fbufCreateInfo.width                   = mContext->GetSwapchainSize().width;
    fbufCreateInfo.height                  = mContext->GetSwapchainSize().height;
    fbufCreateInfo.layers                  = 1;
    AssertVkResult(vkCreateFramebuffer(mContext->Device(), &fbufCreateInfo, nullptr, &mFrameBufferV2));
}