
#include <zvulkan/vulkaninstance.h>
#include <zvulkan/vulkandevice.h>
#include <zvulkan/vulkansurface.h>
#include <zvulkan/vulkancompatibledevice.h>
#include <zvulkan/vulkanobjects.h>
#include <zvulkan/vulkanswapchain.h>
#include <zvulkan/vulkanbuilders.h>

void VulkanPrintLog(const char* typestr, const std::string& msg)
{
	// Log messages from the validation layer here
}

void VulkanError(const char* message)
{
	throw std::runtime_error(message);
}

#ifdef WIN32

bool exitFlag;

LRESULT CALLBACK ZVulkanWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_PAINT || msg == WM_ERASEBKGND)
	{
		return 0;
	}
	else if (msg == WM_CLOSE)
	{
		exitFlag = true;
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
	try
	{
		// Create a window class for the win32 window
		WNDCLASSEX classdesc = {};
		classdesc.cbSize = sizeof(WNDCLASSEX);
		classdesc.hInstance = GetModuleHandle(nullptr);
		classdesc.lpszClassName = L"ZVulkanWindow";
		classdesc.lpfnWndProc = &ZVulkanWindowProc;
		classdesc.style = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;
		classdesc.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
		BOOL result = RegisterClassEx(&classdesc);
		if (!result)
			throw std::runtime_error("RegisterClassEx failed");

		// Create and show the window
		HWND hwnd = CreateWindowEx(WS_EX_OVERLAPPEDWINDOW, L"ZVulkanWindow", L"ZVulkan Example", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 1280, 1024, 0, 0, GetModuleHandle(nullptr), nullptr);
		if (!hwnd)
			throw std::runtime_error("CreateWindowEx failed");

		// Create vulkan instance
		auto instance = VulkanInstanceBuilder()
			.RequireSurfaceExtensions()
			.DebugLayer(false)
			.Create();

		// Create a surface for our window
		auto surface = VulkanSurfaceBuilder()
			.Win32Window(hwnd)
			.Create(instance);

		// Create the vulkan device
		auto device = VulkanDeviceBuilder()
			.Surface(surface)
			.Create(instance);

		// Create a swap chain for our window
		auto swapchain = VulkanSwapChainBuilder()
			.VSync(true)
			.Create(device.get());

		// GLSL for a vertex shader
		std::string vertexCode = R"(
			#version 450

			vec2 positions[3] = vec2[](
				vec2(0.0, -0.5),
				vec2(0.5, 0.5),
				vec2(-0.5, 0.5)
			);

			vec4 colors[3] = vec4[](
				vec4(1.0, 0.0, 0.0, 1.0),
				vec4(0.0, 1.0, 0.0, 1.0),
				vec4(0.0, 0.0, 1.0, 1.0)
			);

			layout(location = 0) out vec4 color;

			void main() {
				color = colors[gl_VertexIndex];
				gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
			}
		)";

		// GLSL for a fragment shader
		std::string fragmentCode = R"(
			#version 450

			layout(location = 0) in vec4 color;
			layout(location = 0) out vec4 outColor;

			void main() {
				outColor = color;
			}
		)";

		// Create a vertex shader
		auto vertexShader = ShaderBuilder()
			.VertexShader(vertexCode)
			.DebugName("vertexShader")
			.Create("vertex", device.get());

		// Create a fragment shader
		auto fragmentShader = ShaderBuilder()
			.FragmentShader(fragmentCode)
			.DebugName("fragmentShader")
			.Create("frag", device.get());

		// Create a render pass where we clear the frame buffer when it begins and ends the pass by transitioning the image to what the swap chain needs to present it
		auto renderPass = RenderPassBuilder()
			.AddAttachment(VK_FORMAT_B8G8R8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
			.AddSubpass()
			.AddSubpassColorAttachmentRef(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
			.Create(device.get());

		// Create a pipeline layout (descriptor sets used by the pipeline)
		auto pipelineLayout = PipelineLayoutBuilder()
			.DebugName("pipelineLayout")
			.Create(device.get());

		// Create a pipeline (which shaders to use, blending rules, etc.)
		auto pipeline = GraphicsPipelineBuilder()
			.RenderPass(renderPass.get())
			.Layout(pipelineLayout.get())
			.AddVertexShader(vertexShader.get())
			.AddFragmentShader(fragmentShader.get())
			.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
			.AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
			.DebugName("pipeline")
			.Create(device.get());

		// Create a command buffer pool
		auto commandPool = CommandPoolBuilder()
			.DebugName("commandPool")
			.Create(device.get());

		// Create semaphore that is set when the swap chain has acquired the image
		auto imageAvailableSemaphore = SemaphoreBuilder()
			.DebugName("imageAvailableSemaphore")
			.Create(device.get());

		// Create semaphore that is set when drawing has completed
		auto renderFinishedSemaphore = SemaphoreBuilder()
			.DebugName("renderFinishedSemaphore")
			.Create(device.get());

		// Create a fence that is set when the command buffer finished executing
		auto presentFinishedFence = FenceBuilder()
			.DebugName("presentFinishedFence")
			.Create(device.get());

		// Draw a scene and pump window messages until the window is closed
		while (!exitFlag)
		{
			// Pump the win32 message queue
			MSG msg = {};
			BOOL result = PeekMessage(&msg, 0, 0, 0, PM_REMOVE);
			if (result < 0)
				break;
			if (result == TRUE)
			{
				if (msg.message == WM_QUIT)
					break;
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			// How big is the window client area in this frame?
			RECT clientRect = {};
			GetClientRect(hwnd, &clientRect);
			int width = clientRect.right;
			int height = clientRect.bottom;

			// Try acquire a frame buffer image from the swap chain
			uint32_t imageIndex = swapchain->acquireImage(width, height, false, imageAvailableSemaphore.get());
			if (imageIndex != 0xffffffff)
			{
				// Create a frame buffer object
				auto framebuffer = FramebufferBuilder()
					.RenderPass(renderPass.get())
					.AddAttachment(swapchain->swapChainImageViews[imageIndex])
					.Size(width, height)
					.DebugName("framebuffer")
					.Create(device.get());

				// Create a command buffer and begin adding commands to it
				auto commands = commandPool->createBuffer();
				commands->begin();

				// Begin a render pass
				RenderPassBegin()
					.RenderPass(renderPass.get())
					.Framebuffer(framebuffer.get())
					.AddClearColor(0.0f, 0.0f, 0.1f, 1.0f)
					.AddClearDepthStencil(1.0f, 0)
					.RenderArea(0, 0, width, height)
					.Execute(commands.get());

				// Set the viewport to cover the window
				VkViewport viewport = {};
				viewport.width = (float)width;
				viewport.height = (float)height;
				viewport.maxDepth = 1.0f;
				commands->setViewport(0, 1, &viewport);

				// Set the scissor box
				VkRect2D scissor = {};
				scissor.extent.width = width;
				scissor.extent.height = height;
				commands->setScissor(0, 1, &scissor);

				// Bind the pipeline
				commands->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());

				// Draw a triangle
				commands->draw(3, 1, 0, 0);

				// End rendering
				commands->endRenderPass();
				commands->end();

				// Submit command buffer to the graphics queue
				QueueSubmit()
					.AddCommandBuffer(commands.get())
					.AddWait(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, imageAvailableSemaphore.get())
					.AddSignal(renderFinishedSemaphore.get())
					.Execute(device.get(), device->GraphicsQueue, presentFinishedFence.get());

				// Present the frame buffer image
				swapchain->queuePresent(imageIndex, renderFinishedSemaphore.get());

				// Wait for the swapchain present to finish
				vkWaitForFences(device->device, 1, &presentFinishedFence->fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
				vkResetFences(device->device, 1, &presentFinishedFence->fence);
			}
		}

		return 0;
	}
	catch (const std::exception& e)
	{
		MessageBoxA(0, e.what(), "Unhandled Exception", MB_OK);
		return 0;
	}
}

#else

int main()
{
	return 0;
}

#endif
