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

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = mRenderpass;
    renderPassInfo.framebuffer       = mFrameBuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = mContext->Swapchain->extent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{0.0f, 1.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = clearValues.size();
    renderPassInfo.pClearValues    = clearValues.data();

    vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);

    VkDescriptorSet descriptorSet = mDescriptorSet.GetDescriptorSet();
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

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

void EmissiveTriangleMeshStage::Destroy()
{
	RasterizedRenderStage::Destroy();

	if(mRenderpass != nullptr)
    {
		vkDestroyRenderPass(mContext->Device(), mRenderpass, nullptr);
		mRenderpass = nullptr;
	}

	if(mFrameBuffer != nullptr)
    {
        vkDestroyFramebuffer(mContext->Device(), mFrameBuffer, nullptr);
        mFrameBuffer = nullptr;
    }

    if(mVertexBuffer.Exists())
        mVertexBuffer.Destroy();

    if(mShaderModuleFrag.Exists())
        mShaderModuleFrag.Destroy();

    if(mShaderModuleVert.Exists())
        mShaderModuleVert.Destroy();

    if(mPipeline != nullptr)
    {
        vkDestroyPipeline(mContext->Device(), mPipeline, nullptr);
		mPipeline = nullptr;
	}

    if(mPipelineLayout)
        mPipelineLayout.Destroy();

	if(mDescriptorSet.Exists())
        mDescriptorSet.Destroy();
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
    builder.SetPolygonMode(VK_POLYGON_MODE_LINE);
    builder.SetPipelineLayout(mPipelineLayout.GetPipelineLayout());
    mPipeline = builder.Build();
}

void EmissiveTriangleMeshStage::CreateShaders()
{
    foray::core::ShaderCompilerConfig options{.IncludeDirs = {FORAY_SHADER_DIR}};

    mShaderKeys.push_back(mShaderModuleVert.CompileFromSource(mContext, VERT_FILE, options));
    mShaderKeys.push_back(mShaderModuleFrag.CompileFromSource(mContext, FRAG_FILE, options));
    mShaderModuleVert.SetName("EmissiveTris_ShaderVert");
    mShaderModuleFrag.SetName("EmissiveTris_ShaderFrag");
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

    VkSubpassDependency subPassDependencies[2] = {};
    subPassDependencies[0].srcSubpass          = VK_SUBPASS_EXTERNAL;
    subPassDependencies[0].dstSubpass          = 0;
    subPassDependencies[0].srcStageMask        = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subPassDependencies[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    subPassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    subPassDependencies[0].dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    subPassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    subPassDependencies[1].srcSubpass   = 0;
    subPassDependencies[1].dstSubpass   = VK_SUBPASS_EXTERNAL;
    subPassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    subPassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subPassDependencies[1].srcAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    subPassDependencies[1].dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
    subPassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;


    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.pAttachments           = attachmentDescriptions.data();
    renderPassInfo.attachmentCount        = static_cast<uint32_t>(attachmentDescriptions.size());
    renderPassInfo.subpassCount           = 1;
    renderPassInfo.pSubpasses             = &subpass;
    renderPassInfo.dependencyCount        = 2;
    renderPassInfo.pDependencies          = subPassDependencies;
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
    mPipelineLayout.SetName("EmissiveTris_PipelineLayout");
}