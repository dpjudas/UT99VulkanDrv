
#include "Precomp.h"
#include "TextureManager.h"
#include "UVulkanRenderDevice.h"
#include "VulkanTexture.h"
#include "VulkanBuilders.h"

TextureManager::TextureManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
	CreateNullTexture();
}

TextureManager::~TextureManager()
{
	delete NullTextureView; NullTextureView = nullptr;
	delete NullTexture; NullTexture = nullptr;
}

void TextureManager::UpdateTextureRect(FTextureInfo* texture, int x, int y, int w, int h)
{
	VulkanTexture*& tex = TextureCache[0][texture->CacheID];
	if (tex)
		tex->UpdateRect(renderer, *texture, x, y, w, h);
}

VulkanTexture* TextureManager::GetTexture(FTextureInfo* texture, bool masked)
{
	if (!texture)
		return nullptr;

	VulkanTexture*& tex = TextureCache[(int)masked][texture->CacheID];
	if (!tex)
	{
		tex = new VulkanTexture(renderer, *texture, masked);
	}
	else if (texture->bRealtimeChanged)
	{
		texture->bRealtimeChanged = 0;
		tex->Update(renderer, *texture, masked);
	}
	return tex;
}

void TextureManager::ClearCache()
{
	for (auto& cache : TextureCache)
	{
		for (auto it : cache)
			delete it.second;
		cache.clear();
	}
}

void TextureManager::CreateNullTexture()
{
	auto cmdbuffer = renderer->Commands->GetTransferCommands();

	ImageBuilder imgbuilder;
	imgbuilder.setFormat(VK_FORMAT_R8G8B8A8_UNORM);
	imgbuilder.setSize(1, 1);
	imgbuilder.setUsage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	NullTexture = imgbuilder.create(renderer->Device).release();

	ImageViewBuilder viewbuilder;
	viewbuilder.setImage(NullTexture, VK_FORMAT_R8G8B8A8_UNORM);
	NullTextureView = viewbuilder.create(renderer->Device).release();

	BufferBuilder builder;
	builder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	builder.setSize(4);
	auto stagingbuffer = builder.create(renderer->Device);
	auto data = (uint32_t*)stagingbuffer->Map(0, 4);
	data[0] = 0xffffffff;
	stagingbuffer->Unmap();

	PipelineBarrier imageTransition0;
	imageTransition0.addImage(NullTexture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
	imageTransition0.execute(cmdbuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { 1, 1, 1 };
	cmdbuffer->copyBufferToImage(stagingbuffer->buffer, NullTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	PipelineBarrier imageTransition1;
	imageTransition1.addImage(NullTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
	imageTransition1.execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	renderer->Commands->FrameDeleteList->buffers.push_back(std::move(stagingbuffer));
}
