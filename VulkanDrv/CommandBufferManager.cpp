
#include "Precomp.h"
#include "CommandBufferManager.h"
#include "VulkanBuilders.h"
#include "VulkanSwapChain.h"
#include "VulkanPostprocess.h"

CommandBufferManager::CommandBufferManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
	SwapChain = new VulkanSwapChain(renderer->Device, renderer->UseVSync);
	ImageAvailableSemaphore = new VulkanSemaphore(renderer->Device);
	RenderFinishedSemaphore = new VulkanSemaphore(renderer->Device);
	RenderFinishedFence = new VulkanFence(renderer->Device);
	TransferSemaphore = new VulkanSemaphore(renderer->Device);
	CommandPool = new VulkanCommandPool(renderer->Device, renderer->Device->graphicsFamily);
	FrameDeleteList = new DeleteList();

	PostprocessModel = new ::Postprocess();
	Postprocess = new VulkanPostprocess(renderer);
}

CommandBufferManager::~CommandBufferManager()
{
	DeleteFrameObjects();

	delete Postprocess; Postprocess = nullptr;
	delete PostprocessModel; PostprocessModel = nullptr;
	delete ImageAvailableSemaphore; ImageAvailableSemaphore = nullptr;
	delete RenderFinishedSemaphore; RenderFinishedSemaphore = nullptr;
	delete RenderFinishedFence; RenderFinishedFence = nullptr;
	delete TransferSemaphore; TransferSemaphore = nullptr;
	delete CommandPool; CommandPool = nullptr;
	delete SwapChain; SwapChain = nullptr;
}

void CommandBufferManager::BeginFrame()
{
	Postprocess->beginFrame();
	Postprocess->imageTransitionScene(true);
}

void CommandBufferManager::EndFrame()
{
	Postprocess->blitSceneToPostprocess();

	int sceneWidth = renderer->Textures->Scene->colorBuffer->width;
	int sceneHeight = renderer->Textures->Scene->colorBuffer->height;

	PostprocessModel->present.gamma = 1.5f * renderer->Viewport->GetOuterUClient()->Brightness * 2.0f;
}

void CommandBufferManager::SubmitCommands(bool present, int presentWidth, int presentHeight)
{
	if (present)
	{
		//RECT clientbox = {};
		//GetClientRect(WindowHandle, &clientbox);
		//int presentWidth = clientbox.right;
		//int presentHeight = clientbox.bottom;

		PresentImageIndex = SwapChain->acquireImage(presentWidth, presentHeight, ImageAvailableSemaphore);
		if (PresentImageIndex != 0xffffffff)
		{
			PPViewport box;
			box.x = 0;
			box.y = 0;
			box.width = presentWidth;
			box.height = presentHeight;
			Postprocess->drawPresentTexture(box);
		}
	}

	if (TransferCommands)
	{
		TransferCommands->end();

		QueueSubmit submit;
		submit.addCommandBuffer(TransferCommands);
		submit.addSignal(TransferSemaphore);
		submit.execute(renderer->Device, renderer->Device->graphicsQueue);
	}

	if (DrawCommands)
		DrawCommands->end();

	QueueSubmit submit;
	if (DrawCommands)
	{
		submit.addCommandBuffer(DrawCommands);
	}
	if (TransferCommands)
	{
		submit.addWait(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, TransferSemaphore);
	}
	if (present && PresentImageIndex != 0xffffffff)
	{
		submit.addWait(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, ImageAvailableSemaphore);
		submit.addSignal(RenderFinishedSemaphore);
	}
	submit.execute(renderer->Device, renderer->Device->graphicsQueue, RenderFinishedFence);

	if (present && PresentImageIndex != 0xffffffff)
	{
		SwapChain->queuePresent(PresentImageIndex, RenderFinishedSemaphore);
	}

	vkWaitForFences(renderer->Device->device, 1, &RenderFinishedFence->fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(renderer->Device->device, 1, &RenderFinishedFence->fence);

	delete DrawCommands; DrawCommands = nullptr;
	delete TransferCommands; TransferCommands = nullptr;
	DeleteFrameObjects();
}

VulkanCommandBuffer* CommandBufferManager::GetTransferCommands()
{
	if (!TransferCommands)
	{
		TransferCommands = CommandPool->createBuffer().release();
		TransferCommands->begin();
	}
	return TransferCommands;
}

VulkanCommandBuffer* CommandBufferManager::GetDrawCommands()
{
	if (!DrawCommands)
	{
		DrawCommands = CommandPool->createBuffer().release();
		DrawCommands->begin();
	}
	return DrawCommands;
}

void CommandBufferManager::DeleteFrameObjects()
{
	delete FrameDeleteList; FrameDeleteList = nullptr;
	FrameDeleteList = new DeleteList();
}

void CommandBufferManager::CopyScreenToBuffer(int w, int h, void* data, float gamma)
{
	// Convert from rgba16f to bgra8 using the GPU:
	ImageBuilder imgbuilder;
	imgbuilder.setFormat(VK_FORMAT_B8G8R8A8_UNORM);
	imgbuilder.setUsage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	imgbuilder.setSize(w, h);
	auto image = imgbuilder.create(renderer->Device);
	VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	Postprocess->blitCurrentToImage(image.get(), &imageLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	// Staging buffer for download
	BufferBuilder bufbuilder;
	bufbuilder.setSize(w * h * 4);
	bufbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU);
	auto staging = bufbuilder.create(renderer->Device);

	// Copy from image to buffer
	VkBufferImageCopy region = {};
	region.imageExtent.width = w;
	region.imageExtent.height = h;
	region.imageExtent.depth = 1;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	GetDrawCommands()->copyImageToBuffer(image->image, imageLayout, staging->buffer, 1, &region);

	// Submit command buffers and wait for device to finish the work
	SubmitCommands(false, 0, 0);

	uint8_t* pixels = (uint8_t*)staging->Map(0, w * h * 4);
	if (gamma != 1.0f)
	{
		float invGamma = 1.0f / gamma;

		uint8_t gammatable[256];
		for (int i = 0; i < 256; i++)
			gammatable[i] = (int)clamp(std::round(std::pow(i / 255.0f, invGamma) * 255.0f), 0.0f, 255.0f);

		uint8_t* dest = (uint8_t*)data;
		for (int i = 0; i < w * h * 4; i += 4)
		{
			dest[i] = gammatable[pixels[i]];
			dest[i + 1] = gammatable[pixels[i + 1]];
			dest[i + 2] = gammatable[pixels[i + 2]];
			dest[i + 3] = pixels[i + 3];
		}
	}
	else
	{
		memcpy(data, pixels, w * h * 4);
	}
	staging->Unmap();
}
