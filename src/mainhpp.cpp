#include <vulkan/vulkan.hpp>
#include <iostream>

int main()
{
    
    vk::ApplicationInfo AppInfo{
        "VulkanCompute",
        1,
        nullptr,
        0,
        VK_API_VERSION_1_3
    };

    //启用了 VK_LAYER_KHRONOS_validation层
    const std::vector<const char*> Layers = { "VK_LAYER_KHRONOS_validation" };

    vk::InstanceCreateInfo InstanceCreateInfo{vk::InstanceCreateFlags(),
                                                &AppInfo,
                                                Layers,
                                                {}};//Extensions
    vk::Instance Instance = vk::createInstance(InstanceCreateInfo);
    
    //Pick a sutable PhysicalDevice
    vk::PhysicalDevice PhysicalDevice = Instance.enumeratePhysicalDevices().front();
    vk::PhysicalDeviceProperties DeviceProps = PhysicalDevice.getProperties();

    std::cout << "Device Name : " << DeviceProps.deviceName << std::endl;
    
    const uint32_t ApiVersion = DeviceProps.apiVersion;
    std::cout << "Vulkan Version : " << VK_VERSION_MAJOR(ApiVersion) << "." << VK_VERSION_MINOR(ApiVersion)<<"."<<VK_VERSION_PATCH(ApiVersion);

    vk::PhysicalDeviceLimits DeviceLimits = DeviceProps.limits;
    std::cout << "Max Compute Shared Memory Size : " << DeviceLimits.maxComputeSharedMemorySize / 1024 << " KB" << std::endl;

    //到这里我们只是打印第一个可用的物理设备：这里到底适不适合我们的需求，我们不清楚


    //query which queue family we need to create a queue suitable for compute work

    std::vector<vk::QueueFamilyProperties> QueueFamilyProps = PhysicalDevice.getQueueFamilyProperties();
    auto PropIt = std::find_if(QueueFamilyProps.begin(),QueueFamilyProps.end(),[](const vk::QueueFamilyProperties& Prop)
    {

        return Prop.queueFlags & vk::QueueFlagBits::eCompute;
    });


    const uint32_t ComputeQueueFamilyIndex = std::distance(QueueFamilyProps.begin(),PropIt);
    std::cout << "Compute Queue Family Index : " << ComputeQueueFamilyIndex << std::endl;

    //create Vulkan Device

    vk::DeviceQueueCreateInfo DeviceQueueCreateInfo(vk::DeviceQueueCreateFlags(),
                                                    ComputeQueueFamilyIndex,
                                                    1);

    vk::DeviceCreateInfo DeviceCreateInfo(vk::DeviceCreateFlags(),
                                            DeviceQueueCreateInfo);                                                    

    vk::Device Device = PhysicalDevice.createDevice(DeviceCreateInfo);                                       


    //Allocating Memory

    const uint32_t NumElements = 10;
    const uint32_t BufferSize = NumElements * sizeof(int32_t);

    vk::BufferCreateInfo BufferCreateInfo{

        vk::BufferCreateFlags(),                //Flags
        BufferSize,                             //Size
        vk::BufferUsageFlagBits::eStorageBuffer,//Usage
        vk::SharingMode::eExclusive,            //Sharing mode
        1,                                      //Number of queue family indices
        &ComputeQueueFamilyIndex                //List of queue family indices
    };
    //01 Creating the buffers
    vk::Buffer InBuffer = Device.createBuffer(BufferCreateInfo);
    vk::Buffer OutBuffer = Device.createBuffer(BufferCreateInfo);

    //02 Allocating memory 

    //find the type of memory we actually require to back the buffers we have
    vk::MemoryRequirements InBufferMemoryRequirements = Device.getBufferMemoryRequirements(InBuffer);
    vk::MemoryRequirements OutBufferMemoryRequirements = Device.getBufferMemoryRequirements(OutBuffer);

    //allocate memory that is visible form the host
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

    //get a mapped pointer to this memory that can be used to copy data from the host to the device.

    int32_t* InBufferPtr = static_cast<int32_t*>(Device.mapMemory(InBufferMemory, 0, BufferSize));
    for (int32_t I = 0; I < NumElements; ++I)
    {
        InBufferPtr[I] = I;//Init the data
    }
    Device.unmapMemory(InBufferMemory);

    Device.bindBufferMemory(InBuffer, InBufferMemory, 0);
    Device.bindBufferMemory(OutBuffer, OutBufferMemory, 0);


    //Creating the Compute Pipeline

    // //Shader Module
     std::vector<char> ShaderContents;
    // if (std::ifstream ShaderFile{ "Square.spv", std::ios::binary | std::ios::ate })
    // {
    //     const size_t FileSize = ShaderFile.tellg();
    //     ShaderFile.seekg(0);
    //     ShaderContents.resize(FileSize, '\0');
    //     ShaderFile.read(ShaderContents.data(), FileSize);
    // }

    vk::ShaderModuleCreateInfo ShaderModuleCreateInfo(
        vk::ShaderModuleCreateFlags(),                                // Flags
        ShaderContents.size(),                                        // Code size
        reinterpret_cast<const uint32_t*>(ShaderContents.data()));    // Code
    vk::ShaderModule ShaderModule = Device.createShaderModule(ShaderModuleCreateInfo);

    //Descriptor Set Layout:
    const std::vector<vk::DescriptorSetLayoutBinding> DescriptorSetLayoutBinding = {
        {0,vk::DescriptorType::eStorageBuffer,1,vk::ShaderStageFlagBits::eCompute},
        {1,vk::DescriptorType::eStorageBuffer,1,vk::ShaderStageFlagBits::eCompute}
    };

    vk::DescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(
        vk::DescriptorSetLayoutCreateFlags(),
        DescriptorSetLayoutBinding);//创建Descriptor Set Layout之前需要指定Binding信息
    //DescriptorSetLayout 使用一系列 DescriptorSetLayoutBinding对象来指定。
    vk::DescriptorSetLayout DescriptorSetLayout = Device.createDescriptorSetLayout(DescriptorSetLayoutCreateInfo);
    

    //PipelineLayout
    vk::PipelineLayoutCreateInfo PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), DescriptorSetLayout);
    vk::PipelineLayout PipelineLayout = Device.createPipelineLayout(PipelineLayoutCreateInfo);
    vk::PipelineCache PipelineCache = Device.createPipelineCache(vk::PipelineCacheCreateInfo());

    //Pipeline
    vk::PipelineShaderStageCreateInfo PipelineShaderCreateInfo(
    vk::PipelineShaderStageCreateFlags(),  // Flags
    vk::ShaderStageFlagBits::eCompute,     // Stage
    ShaderModule,                          // Shader Module
    "Main");                               // Shader Entry Point

    vk::ComputePipelineCreateInfo ComputePipelineCreateInfo(
        vk::PipelineCreateFlags(),    // Flags
        PipelineShaderCreateInfo,     // Shader Create Info struct
        PipelineLayout);              // Pipeline Layout
    //vk::Pipeline ComputePipeline = Device.createComputePipeline(PipelineCache, ComputePipelineCreateInfo);
    vk::Pipeline ComputePipeline = Device.createComputePipeline(PipelineCache,ComputePipelineCreateInfo);

    //Creating the DescriptorSet 
    vk::DescriptorPoolSize DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 2);
    vk::DescriptorPoolCreateInfo DescriptorPoolCreateInfo(vk::DescriptorPoolCreateFlags(), 1, DescriptorPoolSize);
    vk::DescriptorPool DescriptorPool = Device.createDescriptorPool(DescriptorPoolCreateInfo);

    vk::DescriptorSetAllocateInfo DescriptorSetAllocInfo(DescriptorPool, 1, &DescriptorSetLayout);
    const std::vector<vk::DescriptorSet> DescriptorSets = Device.allocateDescriptorSets(DescriptorSetAllocInfo);
    vk::DescriptorSet DescriptorSet = DescriptorSets.front();
    vk::DescriptorBufferInfo InBufferInfo(InBuffer, 0, NumElements * sizeof(int32_t));
    vk::DescriptorBufferInfo OutBufferInfo(OutBuffer, 0, NumElements * sizeof(int32_t));

    const std::vector<vk::WriteDescriptorSet> WriteDescriptorSets = {
        {DescriptorSet, 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &InBufferInfo},
        {DescriptorSet, 1, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &OutBufferInfo},
    };

    Device.updateDescriptorSets(WriteDescriptorSets, {});

    //Submitting the work to the GPU
    //Command Pool
    vk::CommandPoolCreateInfo CommandPoolCreateInfo(vk::CommandPoolCreateFlags(), ComputeQueueFamilyIndex);
    vk::CommandPool CommandPool = Device.createCommandPool(CommandPoolCreateInfo);

    vk::CommandBufferAllocateInfo CommandBufferAllocInfo(
    CommandPool,                         // Command Pool
    vk::CommandBufferLevel::ePrimary,    // Level
    1);                                  // Num Command Buffers
    const std::vector<vk::CommandBuffer> CmdBuffers = Device.allocateCommandBuffers(CommandBufferAllocInfo);
    vk::CommandBuffer CmdBuffer = CmdBuffers.front();


    //Recording Commands
    vk::CommandBufferBeginInfo CmdBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    CmdBuffer.begin(CmdBufferBeginInfo);
    CmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, ComputePipeline);
    CmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,    // Bind point
                                    PipelineLayout,                  // Pipeline Layout
                                    0,                               // First descriptor set
                                    { DescriptorSet },               // List of descriptor sets
                                    {});                             // Dynamic offsets
    CmdBuffer.dispatch(NumElements, 1, 1);
    CmdBuffer.end();


    vk::Queue Queue = Device.getQueue(ComputeQueueFamilyIndex, 0);
    vk::Fence Fence = Device.createFence(vk::FenceCreateInfo());

    vk::SubmitInfo SubmitInfo(0,                // Num Wait Semaphores
                                nullptr,        // Wait Semaphores
                                nullptr,        // Pipeline Stage Flags
                                1,              // Num Command Buffers
                                &CmdBuffer);    // List of command buffers
    Queue.submit({ SubmitInfo }, Fence);
    Device.waitForFences({ Fence },             // List of fences
                            true,               // Wait All
                            uint64_t(-1));      // Timeout


    //map the output buffer and read the results
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


    Device.resetCommandPool(CommandPool, vk::CommandPoolResetFlags());
    Device.destroyFence(Fence);
    Device.destroyDescriptorSetLayout(DescriptorSetLayout);
    Device.destroyPipelineLayout(PipelineLayout);
    Device.destroyPipelineCache(PipelineCache);
    Device.destroyShaderModule(ShaderModule);
    Device.destroyPipeline(ComputePipeline);
    Device.destroyDescriptorPool(DescriptorPool);
    Device.destroyCommandPool(CommandPool);
    Device.freeMemory(InBufferMemory);
    Device.freeMemory(OutBufferMemory);
    Device.destroyBuffer(InBuffer);
    Device.destroyBuffer(OutBuffer);
    Device.destroy();
    Instance.destroy();

    return 0;
}