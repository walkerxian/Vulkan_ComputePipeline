#include <limits>
#include <fstream>
#include <iostream>
#include <vector>
#include <vulkan/vulkan.h>

uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice,
                          VkMemoryRequirements requirements,
                          VkMemoryPropertyFlags memoryProp) {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index) {
        auto propertyFlags = memoryProperties.memoryTypes[index].propertyFlags;
        bool match = (propertyFlags & memoryProp) == memoryProp;
        if (requirements.memoryTypeBits & (1 << index) && match) {
            return index;
        }
    }

    exit(EXIT_FAILURE);
}

/// @brief 找出指定的队列家族
/// @param physicalDevice 
/// @param queueBit 
/// @return 
uint32_t findQueueFamily(VkPhysicalDevice physicalDevice, VkQueueFlags queueBit) {

    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

    //找出支持队列 queueBit 的 FamilyProperties
    for (uint32_t index = 0; index < queueFamilyCount; index++) {
        if (queueFamilyProperties[index].queueFlags & queueBit) {
            return index;
        }
    }

    exit(EXIT_FAILURE);
}

//读取SPV文件
std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

template <typename T>
struct StorageBuffer {
    StorageBuffer(VkDevice device, VkPhysicalDevice physicalDevice, const std::vector<T>& data) {
        // Create buffer
        memorySize = sizeof(T) * data.size();

        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = memorySize;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        // Allocate memory
        VkMemoryRequirements requirements;
        vkGetBufferMemoryRequirements(device, buffer, &requirements);

        uint32_t memoryTypeIndex = findMemoryTypeIndex(physicalDevice, requirements,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VkMemoryAllocateInfo memoryInfo = {};
        memoryInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryInfo.allocationSize = requirements.size;
        memoryInfo.memoryTypeIndex = memoryTypeIndex;

        if (vkAllocateMemory(device, &memoryInfo, nullptr, &memory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        // Bind buffer
        vkBindBufferMemory(device, buffer, memory, 0);

        // Copy :这里只是CPU将数据传输到了GPU设备内存，并没进一步将数据Copy到其他的数据上面
        vkMapMemory(device, memory, 0, memorySize, 0, (void**)&mapped);
        memcpy(mapped, data.data(), memorySize);
        vkUnmapMemory(device, memory);
    }

    ~StorageBuffer() {
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, memory, nullptr);
    }

    void print() {
        for (int i = 0; i < 10; i++) {
            std::cout << mapped[i] << " ";//这里的mapped实际上主机上存储的设备内存的地址（这个地址通过内存映射获取MapMemory)
        }
        std::cout << std::endl;
    }

    T* mapped;
    uint64_t memorySize;
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDevice device;
};

int main() {

    // Create Instance
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Compute";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;//需要指定版本号

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;

    //1.1 创建Vulkan  Instance
    VkInstance instance;
    if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }

    // Pick first physical device
    VkPhysicalDevice physicalDevice;//这里直接返回一个显卡
    uint32_t deviceCount = 1;
    vkEnumeratePhysicalDevices(instance, &deviceCount, &physicalDevice);

    //1.2 找到设备上面符合要求的队列家族
    uint32_t queueFamily = findQueueFamily(physicalDevice, VK_QUEUE_COMPUTE_BIT);

    // Create device
    float queuePriority = 0.0f;
    VkDeviceQueueCreateInfo queueInfo = {};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = queueFamily;//指定计算队列家族
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;//指定队列信息

    //1.3 创建指定相关的Queue的逻辑设备LogicDevice 并检索出相关的Queue

    VkDevice device;
    if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create device!");
    }

    // Get Queue : 从device里面检索处相关的队列
    VkQueue queue;
    vkGetDeviceQueue(device, queueFamily, 0, &queue);

    //到这里第一阶段完成
    
    //2.1 准备数据并且上传数据到设备内存
    // Create data
    constexpr uint32_t dataSize = 1'000'000;
    std::vector<uint32_t> dataA(dataSize, 3);
    std::vector<uint32_t> dataB(dataSize, 5);
    std::vector<uint32_t> dataC(dataSize, 0);

    // Create Buffer
    StorageBuffer<uint32_t> bufferA{ device, physicalDevice, dataA };
    StorageBuffer<uint32_t> bufferB{ device, physicalDevice, dataB };
    StorageBuffer<uint32_t> bufferC{ device, physicalDevice, dataC };

    //2.2 创建DescriptorPool 和 DescriptorSetLayout  
    // Create descriptor Pool
    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 10;

    VkDescriptorPoolCreateInfo descPoolInfo = {};
    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descPoolInfo.maxSets = 1;
    descPoolInfo.poolSizeCount = 1;
    descPoolInfo.pPoolSizes = &poolSize;

    VkDescriptorPool descPool;
    //DescriptorPool
    if (vkCreateDescriptorPool(device, &descPoolInfo, nullptr, &descPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    // Create a descriptorSetLayoutBinding 
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;//指定描述符类型
    binding.descriptorCount = 3;//指定描述符数量
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;//指定哪个stage使用

    VkDescriptorSetLayoutCreateInfo descSetLayoutInfo = {};
    descSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descSetLayoutInfo.bindingCount = 1;
    descSetLayoutInfo.pBindings = &binding;//添加binding信息

    //Create descriptor set layout
    VkDescriptorSetLayout descSetLayout;
    if (vkCreateDescriptorSetLayout(device, &descSetLayoutInfo, nullptr, &descSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
   
    // 2.3 Allocate desc set ：DescriptorPool 在DescriptorSetLayout的指导下，组装 DescriptorSet。
    VkDescriptorSetAllocateInfo descSetInfo = {};
    descSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descSetInfo.descriptorPool = descPool;//指定DescriptorPool
    descSetInfo.descriptorSetCount = 1;//指定只有一个DescriptorSetLayout
    descSetInfo.pSetLayouts = &descSetLayout;
    //Create descriptorSets
    VkDescriptorSet descSet;
    if (vkAllocateDescriptorSets(device, &descSetInfo, &descSet) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    //2.4 开始填充这些Descriptor 和 DescriptorSet数据    
    // Update desc set：更新Descriptor信息
    std::vector<VkDescriptorBufferInfo> descBufferInfos(3);
    descBufferInfos[0].buffer = bufferA.buffer;//实际的数据
    descBufferInfos[0].offset = 0;
    descBufferInfos[0].range = bufferA.memorySize;

    descBufferInfos[1].buffer = bufferB.buffer;
    descBufferInfos[1].offset = 0;
    descBufferInfos[1].range = bufferB.memorySize;

    descBufferInfos[2].buffer = bufferC.buffer;
    descBufferInfos[2].offset = 0;
    descBufferInfos[2].range = bufferC.memorySize;

    //只有一个DescriptorSet，有3个Descriptor,每一个Descriptor都是Storage Unifrom类型    
    VkWriteDescriptorSet writeDescSet = {};
    writeDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescSet.dstSet = descSet;
    writeDescSet.dstBinding = 0;
    writeDescSet.dstArrayElement = 0;
    writeDescSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescSet.descriptorCount = 3;
    writeDescSet.pBufferInfo = descBufferInfos.data();
    //更新DescriptorSet以及里面的Descriptor里面的信息
    vkUpdateDescriptorSets(device, 1, &writeDescSet, 0, nullptr);
    //第二阶段目标就是：创建并初始化DescriptorSets

    //3.1 读取Shader 代码以及 创建 ShaderModule模块
    // Create shader module
    std::vector<char> shaderCode = readFile("shaders/compute.spv");

    VkShaderModuleCreateInfo shaderModuleInfo = {};
    shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleInfo.codeSize = shaderCode.size();
    shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());//这里需要注意： 转换为uint32_t*类型

    
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    // Create compute pipeline : Stage
    VkPipelineShaderStageCreateInfo shaderStageInfo = {};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;//指定计算管线
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main";

    //3.2 创建PipelineLayout资源    
    // Create pipeline layout  ：
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descSetLayout;

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkComputePipelineCreateInfo computePipelineInfo = {};
    computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineInfo.stage = shaderStageInfo;//1：stage
    computePipelineInfo.layout = pipelineLayout;//2 layout

    //3.3 创建ComputePipeline 计算管线   
    VkPipeline pipeline;
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipelines!");
    }

    //4.1 创建命令缓冲区池
    //1  Create command pool
    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = queueFamily;    

    VkCommandPool commandPool;
    if (vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }


    //4.2 分配具体的命令缓冲区
    // 2 Create command buffer
    VkCommandBufferAllocateInfo commandBufferInfo = {};
    commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferInfo.commandPool = commandPool;//指定CommandPool
    commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;//指定当前CommandBuffer的level类型
    commandBufferInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(device, &commandBufferInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
    //4.3 记录命令[启动命令，具体的执行逻辑（绑定管线，绑定DescriptorSet,分发调用线程组）， 结束命令]
    // Record command buffer
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    //告诉GPU 队列启动命令缓冲区
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }
    //1:绑定管线
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    //2:绑定DescriptorSet
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descSet, 0, nullptr);
    //3: 分发调用线程组
    vkCmdDispatch(commandBuffer, dataSize, 1, 1);
    //告诉GPU队列 结束命令缓冲区
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
    //4.4 将命令缓冲区 提及到 指定的GPU 队列
    // Submit and wait
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    vkQueueWaitIdle(queue);//等待GPU队列执行完成

    //读取数据
    std::cout << "result : " << std::endl;
    bufferA.print();
    bufferB.print();
    bufferC.print();

    //程序结束-销毁资源
    vkDestroyShaderModule(device, shaderModule, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
    vkDestroyDescriptorPool(device, descPool, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    return 0;
}