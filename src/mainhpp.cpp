#include <iostream>
#include <fstream>

// Comment this to disable VMA support
#define WITH_VMA

#include <vulkan/vulkan.hpp>

#ifdef WITH_VMA
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#endif

int main()
{
	try
	{
		std::cout << "Hello Vulkan Compute" << std::endl;
		vk::ApplicationInfo AppInfo{
			"VulkanCompute",	// Application Name
			1,					// Application Version
			nullptr,			// Engine Name or nullptr
			0,					// Engine Version
			VK_API_VERSION_1_1  // Vulkan API version
		};

		const std::vector<const char*> Layers = { "VK_LAYER_KHRONOS_validation" };
		vk::InstanceCreateInfo InstanceCreateInfo(vk::InstanceCreateFlags(),	// Flags
												  &AppInfo,						// Application Info
												  Layers.size(),				// Layers count
												  Layers.data());				// Layers
		//Create Instance												  
		vk::Instance Instance = vk::createInstance(InstanceCreateInfo);



		vk::PhysicalDevice PhysicalDevice = Instance.enumeratePhysicalDevices().front();
		vk::PhysicalDeviceProperties DeviceProps = PhysicalDevice.getProperties();
		std::cout << "Device Name    : " << DeviceProps.deviceName << std::endl;
		const uint32_t ApiVersion = DeviceProps.apiVersion;
		std::cout << "Vulkan Version : " << VK_VERSION_MAJOR(ApiVersion) << "." << VK_VERSION_MINOR(ApiVersion) << "." << VK_VERSION_PATCH(ApiVersion) << std::endl;
		vk::PhysicalDeviceLimits DeviceLimits = DeviceProps.limits;
		std::cout << "Max Compute Shared Memory Size: " << DeviceLimits.maxComputeSharedMemorySize / 1024 << " KB" << std::endl;

		std::vector<vk::QueueFamilyProperties> QueueFamilyProps = PhysicalDevice.getQueueFamilyProperties();
		auto PropIt = std::find_if(QueueFamilyProps.begin(), QueueFamilyProps.end(), [](const vk::QueueFamilyProperties& Prop)
		{
			return Prop.queueFlags & vk::QueueFlagBits::eCompute;
		});
		const uint32_t ComputeQueueFamilyIndex = std::distance(QueueFamilyProps.begin(), PropIt);
		std::cout << "Compute Queue Family Index: " << ComputeQueueFamilyIndex << std::endl;

		// Just to avoid a warning from the Vulkan Validation Layer
		const float QueuePriority = 1.0f;
		vk::DeviceQueueCreateInfo DeviceQueueCreateInfo(vk::DeviceQueueCreateFlags(),	// Flags
														ComputeQueueFamilyIndex,		// Queue Family Index
														1,								// Number of Queues
														&QueuePriority);
		vk::DeviceCreateInfo DeviceCreateInfo(vk::DeviceCreateFlags(), // Flags
											  DeviceQueueCreateInfo);  // Device Queue Create Info struct
		//Pick a suitable Physical Device											  
		vk::Device Device = PhysicalDevice.createDevice(DeviceCreateInfo);

		const uint32_t NumElements = 10;
		const uint32_t BufferSize = NumElements * sizeof(int32_t);

		vk::BufferCreateInfo BufferCreateInfo{
			vk::BufferCreateFlags(),					// Flags
			BufferSize,									// Size
			vk::BufferUsageFlagBits::eStorageBuffer,	// Usage
			vk::SharingMode::eExclusive,				// Sharing mode
			1,											// Number of queue family indices
			&ComputeQueueFamilyIndex					// List of queue family indices
		};
		auto vkBufferCreateInfo = static_cast<VkBufferCreateInfo>(BufferCreateInfo);

#ifdef WITH_VMA
		//1 create the Vulkan Buffer
		//2  upload the data to from the CPU to GPU

		VmaAllocatorCreateInfo AllocatorInfo = {};
		AllocatorInfo.vulkanApiVersion = DeviceProps.apiVersion;
		AllocatorInfo.physicalDevice = PhysicalDevice;
		AllocatorInfo.device = Device;
		AllocatorInfo.instance = Instance;
		//01 创建了一个VMA分配器， 并初始化了相关信息，包括Vulkan API版本，物理设备，设备和实例
		VmaAllocator Allocator;
		vmaCreateAllocator(&AllocatorInfo, &Allocator);


		VkBuffer InBufferRaw;
		VkBuffer OutBufferRaw;

		VmaAllocationCreateInfo AllocationInfo = {};
		//02 分配了两个Vulkan缓冲区，InBufferRaw用于CPU到GPU的数据传输
		//OutBufferRaw用于GPU到CPU的数据传输

		AllocationInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		VmaAllocation InBufferAllocation;		
		vmaCreateBuffer(Allocator,
						&vkBufferCreateInfo,
						&AllocationInfo,
						&InBufferRaw,
						&InBufferAllocation,
						nullptr);

		AllocationInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
		VmaAllocation OutBufferAllocation;
		vmaCreateBuffer(Allocator,
						&vkBufferCreateInfo,
						&AllocationInfo,
						&OutBufferRaw,
						&OutBufferAllocation,
						nullptr);
		//03 将Vulkan原始缓冲区（InBufferRaw和OutBufferRaw)转换为 Vulkan C++封装类
		vk::Buffer InBuffer = InBufferRaw;
		vk::Buffer OutBuffer = OutBufferRaw;

		//Upload data from CPU to GPU
		int32_t* InBufferPtr = nullptr;
		//将GPU的内存映射到CPU内存空间，并使用InBufferPtr访问映射内存
		vmaMapMemory(Allocator, InBufferAllocation, reinterpret_cast<void**>(&InBufferPtr));
		
		for (int32_t I = 0; I < NumElements; ++I)
		{
			InBufferPtr[I] = I;//将数据写入到映射内存
		}
		//取消映射
		vmaUnmapMemory(Allocator, InBufferAllocation);


#else
		vk::Buffer InBuffer = Device.createBuffer(BufferCreateInfo);
		vk::Buffer OutBuffer = Device.createBuffer(BufferCreateInfo);

		vk::MemoryRequirements InBufferMemoryRequirements = Device.getBufferMemoryRequirements(InBuffer);
		vk::MemoryRequirements OutBufferMemoryRequirements = Device.getBufferMemoryRequirements(OutBuffer);

		vk::PhysicalDeviceMemoryProperties MemoryProperties = PhysicalDevice.getMemoryProperties();

		uint32_t MemoryTypeIndex = uint32_t(~0);
		vk::DeviceSize MemoryHeapSize = uint32_t(~0);
		for (uint32_t CurrentMemoryTypeIndex = 0; CurrentMemoryTypeIndex < MemoryProperties.memoryTypeCount; ++CurrentMemoryTypeIndex)
		{
			vk::MemoryType MemoryType = MemoryProperties.memoryTypes[CurrentMemoryTypeIndex];
			if ((vk::MemoryPropertyFlagBits::eHostVisible & MemoryType.propertyFlags) &&
				(vk::MemoryPropertyFlagBits::eHostCoherent & MemoryType.propertyFlags))
			{
				MemoryHeapSize = MemoryProperties.memoryHeaps[MemoryType.heapIndex].size;
				MemoryTypeIndex = CurrentMemoryTypeIndex;
				break;
			}
		}

		std::cout << "Memory Type Index: " << MemoryTypeIndex << std::endl;
		std::cout << "Memory Heap Size : " << MemoryHeapSize / 1024 / 1024 / 1024 << " GB" << std::endl;

		vk::MemoryAllocateInfo InBufferMemoryAllocateInfo(InBufferMemoryRequirements.size, MemoryTypeIndex);
		vk::MemoryAllocateInfo OutBufferMemoryAllocateInfo(OutBufferMemoryRequirements.size, MemoryTypeIndex);
		vk::DeviceMemory InBufferMemory = Device.allocateMemory(InBufferMemoryAllocateInfo);
		vk::DeviceMemory OutBufferMemory = Device.allocateMemory(InBufferMemoryAllocateInfo);

		int32_t* InBufferPtr = static_cast<int32_t*>(Device.mapMemory(InBufferMemory, 0, BufferSize));
		for (int32_t I = 0; I < NumElements; ++I)
		{
			InBufferPtr[I] = I;
		}
		Device.unmapMemory(InBufferMemory);

		Device.bindBufferMemory(InBuffer, InBufferMemory, 0);
		Device.bindBufferMemory(OutBuffer, OutBufferMemory, 0);
#endif

	
		std::vector<char> ShaderContents;
		if (std::ifstream ShaderFile{ "shaders/Square.spv", std::ios::binary | std::ios::ate })
		{
			const size_t FileSize = ShaderFile.tellg();
			ShaderFile.seekg(0);
			ShaderContents.resize(FileSize, '\0');
			ShaderFile.read(ShaderContents.data(), FileSize);
		}

		vk::ShaderModuleCreateInfo ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(),								// Flags
														  ShaderContents.size(),										// Code size
														  reinterpret_cast<const uint32_t*>(ShaderContents.data()));	// Code
		//Create a Shader Moudle														  
		vk::ShaderModule ShaderModule = Device.createShaderModule(ShaderModuleCreateInfo);

		const std::vector<vk::DescriptorSetLayoutBinding> DescriptorSetLayoutBinding = {
			{0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
			{1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute}
		};
		vk::DescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(),
																		DescriptorSetLayoutBinding);
		//Creae a DescriptorSetLayout 																		
		vk::DescriptorSetLayout DescriptorSetLayout = Device.createDescriptorSetLayout(DescriptorSetLayoutCreateInfo);

		vk::PipelineLayoutCreateInfo PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), DescriptorSetLayout);
		vk::PipelineLayout PipelineLayout = Device.createPipelineLayout(PipelineLayoutCreateInfo);
		vk::PipelineCache PipelineCache = Device.createPipelineCache(vk::PipelineCacheCreateInfo());

		vk::PipelineShaderStageCreateInfo PipelineShaderCreateInfo(vk::PipelineShaderStageCreateFlags(),  // Flags
																   vk::ShaderStageFlagBits::eCompute,     // Stage
																   ShaderModule,					      // Shader Module
																   "main");								  // Shader Entry Point
		vk::ComputePipelineCreateInfo ComputePipelineCreateInfo(vk::PipelineCreateFlags(),	// Flags
																PipelineShaderCreateInfo,	// Shader Create Info struct
																PipelineLayout);			// Pipeline Layout

		//Create a Compute Pipeline																
		vk::Pipeline ComputePipeline = Device.createComputePipeline(PipelineCache, ComputePipelineCreateInfo).value;

		vk::DescriptorPoolSize DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 2);
		vk::DescriptorPoolCreateInfo DescriptorPoolCreateInfo(vk::DescriptorPoolCreateFlags(), 1, DescriptorPoolSize);
		vk::DescriptorPool DescriptorPool = Device.createDescriptorPool(DescriptorPoolCreateInfo);

		vk::DescriptorSetAllocateInfo DescriptorSetAllocInfo(DescriptorPool, 1, &DescriptorSetLayout);
		//Create a DescriptorSets and get it.
		const std::vector<vk::DescriptorSet> DescriptorSets = Device.allocateDescriptorSets(DescriptorSetAllocInfo);		
		vk::DescriptorSet DescriptorSet = DescriptorSets.front();

		vk::DescriptorBufferInfo InBufferInfo(InBuffer, 0, NumElements * sizeof(int32_t));
		vk::DescriptorBufferInfo OutBufferInfo(OutBuffer, 0, NumElements * sizeof(int32_t));
		//Fill the data to the descriptor
		const std::vector<vk::WriteDescriptorSet> WriteDescriptorSets = {
			{DescriptorSet, 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &InBufferInfo},
			{DescriptorSet, 1, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &OutBufferInfo},
		};
		Device.updateDescriptorSets(WriteDescriptorSets, {});

		//
		vk::CommandPoolCreateInfo CommandPoolCreateInfo(vk::CommandPoolCreateFlags(), ComputeQueueFamilyIndex);
		vk::CommandPool CommandPool = Device.createCommandPool(CommandPoolCreateInfo);

		vk::CommandBufferAllocateInfo CommandBufferAllocInfo(CommandPool,						// Command Pool
															 vk::CommandBufferLevel::ePrimary,	// Level
															 1);							    // Num Command Buffers
		//Create the CommandBuffer and get it															 
		const std::vector<vk::CommandBuffer> CmdBuffers = Device.allocateCommandBuffers(CommandBufferAllocInfo);
		vk::CommandBuffer CmdBuffer = CmdBuffers.front();

		//Setting the CommandBuffer
		vk::CommandBufferBeginInfo CmdBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
		CmdBuffer.begin(CmdBufferBeginInfo);
		CmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, ComputePipeline);
		CmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,	// Bind point
									 PipelineLayout,				    // Pipeline Layout
									 0,								    // First descriptor set
									 { DescriptorSet },					// List of descriptor sets
									 {});								// Dynamic offsets
		CmdBuffer.dispatch(NumElements, 1, 1);
		CmdBuffer.end();

		//get the Queue and Fence
		vk::Queue Queue = Device.getQueue(ComputeQueueFamilyIndex, 0);
		vk::Fence Fence = Device.createFence(vk::FenceCreateInfo());

		vk::SubmitInfo SubmitInfo(0,			// Num Wait Semaphores
								  nullptr,		// Wait Semaphores
								  nullptr,		// Pipeline Stage Flags
								  1,			// Num Command Buffers
								  &CmdBuffer);  // List of command buffers
		Queue.submit({ SubmitInfo }, Fence);
		auto result = Device.waitForFences({ Fence },			// List of fences
							 true,				// Wait All
							 uint64_t(-1));		// Timeout


#ifdef WITH_VMA
//
		//将内存映射到CPU,然后通过一个循环打印出InBufferPtr指向的内存中的数据，然后取消映射
		vmaMapMemory(Allocator, InBufferAllocation, reinterpret_cast<void**>(&InBufferPtr));
		for (uint32_t I = 0; I < NumElements; ++I)
		{
			std::cout << InBufferPtr[I] << " ";
		}
		std::cout << std::endl;
		vmaUnmapMemory(Allocator, InBufferAllocation);

		//通过vmaMapMemory将另一个缓冲区的内存映射到CPU,然后通过循环打印出OutBufferPtr指向的内存中的数据
		int32_t* OutBufferPtr = nullptr;
		vmaMapMemory(Allocator, OutBufferAllocation, reinterpret_cast<void**>(&OutBufferPtr));
		for (uint32_t I = 0; I < NumElements; ++I)
		{
			std::cout << OutBufferPtr[I] << " ";
		}
		std::cout << std::endl;
		vmaUnmapMemory(Allocator, OutBufferAllocation);

		//用于存储Vulkan缓冲区的相关信息以及其分配的内存
		struct BufferInfo
		{
			VkBuffer Buffer;
			VmaAllocation Allocation;
		};
		//定义一个lambda的函数，用于根据指定的参数分配Vulkan缓冲区，并返回其相关信息
		// Lets allocate a couple of buffers to see how they are layed out in memory
		auto AllocateBuffer = [Allocator, ComputeQueueFamilyIndex](size_t SizeInBytes, VmaMemoryUsage Usage)
		{
			vk::BufferCreateInfo BufferCreateInfo{
				vk::BufferCreateFlags(),					// Flags
				SizeInBytes,								// Size
				vk::BufferUsageFlagBits::eStorageBuffer,	// Usage
				vk::SharingMode::eExclusive,				// Sharing mode
				1,											// Number of queue family indices
				&ComputeQueueFamilyIndex					// List of queue family indices
			};

			auto vkBufferCreateInfo = static_cast<VkBufferCreateInfo>(BufferCreateInfo);

			VmaAllocationCreateInfo AllocationInfo = {};
			AllocationInfo.usage = Usage;

			BufferInfo Info;
			vmaCreateBuffer(Allocator,
							&vkBufferCreateInfo,
							&AllocationInfo,
							&Info.Buffer,
							&Info.Allocation,
							nullptr);

			return Info;
		};
		//销毁相关的缓冲区
		auto DestroyBuffer = [Allocator](BufferInfo Info)
		{
			vmaDestroyBuffer(Allocator, Info.Buffer, Info.Allocation);
		};

		//分配四个不同大小和用途的缓冲区，并存储在B1,B2,B3,B4中
		constexpr size_t MB = 1024 * 1024;
		BufferInfo B1 = AllocateBuffer(4 * MB, VMA_MEMORY_USAGE_CPU_TO_GPU);
		BufferInfo B2 = AllocateBuffer(10 * MB, VMA_MEMORY_USAGE_GPU_TO_CPU);
		BufferInfo B3 = AllocateBuffer(20 * MB, VMA_MEMORY_USAGE_GPU_ONLY);
		BufferInfo B4 = AllocateBuffer(100 * MB, VMA_MEMORY_USAGE_CPU_ONLY);

		//在不同的解决调用了类似的代码来记录VMA分配器的统计信息
		{
			//通过vmaBuildStatsString 和 vmaFreeStatsString生成并保存VMA分配器的统计信息
			VmaStats Stats;
			char* StatsString = nullptr;
			vmaBuildStatsString(Allocator, &StatsString, true);
			{
				std::ofstream OutStats{ "VmaStats_2.json" };
				OutStats << StatsString;
			}
			vmaFreeStatsString(Allocator, StatsString);
		}

		//销毁缓冲区
		DestroyBuffer(B1);
		DestroyBuffer(B2);
		DestroyBuffer(B3);
		DestroyBuffer(B4);
#else
		InBufferPtr = static_cast<int32_t*>(Device.mapMemory(InBufferMemory, 0, BufferSize));
		for (uint32_t I = 0; I < NumElements; ++I)
		{
			std::cout << InBufferPtr[I] << " ";
		}
		std::cout << std::endl;
		Device.unmapMemory(InBufferMemory);

		int32_t* OutBufferPtr = static_cast<int32_t*>(Device.mapMemory(OutBufferMemory, 0, BufferSize));
		for (uint32_t I = 0; I < NumElements; ++I)
		{
			std::cout << OutBufferPtr[I] << " ";
		}
		std::cout << std::endl;
		Device.unmapMemory(OutBufferMemory);

#endif

#ifdef WITH_VMA
		//使用Vulkan Memory Allocator（VMA)进行资源清理和统计信息记录
		char* StatsString = nullptr;//该指针用于存储VMA库生成的统计信息字符串
		vmaBuildStatsString(Allocator, &StatsString, true);
		{
			std::ofstream OutStats{ "VmaStats.json" };
			OutStats << StatsString;
		}
		vmaFreeStatsString(Allocator, StatsString);

		vmaDestroyBuffer(Allocator, InBuffer, InBufferAllocation);
		vmaDestroyBuffer(Allocator, OutBuffer, OutBufferAllocation);
		vmaDestroyAllocator(Allocator);
#else
		Device.freeMemory(InBufferMemory);
		Device.freeMemory(OutBufferMemory);
		Device.destroyBuffer(InBuffer);
		Device.destroyBuffer(OutBuffer);
#endif

		Device.resetCommandPool(CommandPool, vk::CommandPoolResetFlags());
		Device.destroyFence(Fence);
		Device.destroyDescriptorSetLayout(DescriptorSetLayout);
		Device.destroyPipelineLayout(PipelineLayout);
		Device.destroyPipelineCache(PipelineCache);
		Device.destroyShaderModule(ShaderModule);
		Device.destroyPipeline(ComputePipeline);
		Device.destroyDescriptorPool(DescriptorPool);
		Device.destroyCommandPool(CommandPool);
		Device.destroy();
		Instance.destroy();
	}
	catch (const std::exception& Exception)
	{
		std::cout << "Error: " << Exception.what() << std::endl;
	}

	return 0;
}
