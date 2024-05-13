-- 定义项目

-- 设置编译器工具链
set_toolset("cc", "D:\\_iWorks\\_RenderPipeline\\Tools\\mingw64\\bin\\gcc.exe")
set_toolset("cxx", "D:\\_iWorks\\_RenderPipeline\\Tools\\mingw64\\bin\\g++.exe")
set_toolset("ld", "D:\\_iWorks\\_RenderPipeline\\Tools\\mingw64\\bin\\g++.exe")
set_toolset("ar", "D:\\_iWorks\\_RenderPipeline\\Tools\\mingw64\\bin\\ar.exe")
set_toolset("sh", "D:\\_iWorks\\_RenderPipeline\\Tools\\mingw64\\bin\\g++.exe")


-- 设置 Vulkan SDK、GLFW 和 GLM 的路径
local vulkan_sdk_path = "D:\\Program Files\\VulkanSDK\\1.3.275.0"
local glfw_sdk_path = "D:\\_iWorks\\_RenderPipeline\\Vulkan\\Plugins\\glfw-3.3.9.bin.WIN64"
local glm_path = "D:\\Program Files\\VulkanSDK\\1.3.275.0\\Include\\glm" 

-- 添加包含目录
add_includedirs(vulkan_sdk_path .. "\\Include")
add_includedirs(glfw_sdk_path .. "\\include")
add_includedirs(glm_path)

-- 添加链接目录
add_linkdirs(vulkan_sdk_path .. "\\Lib")
add_linkdirs(glfw_sdk_path .. "\\lib-mingw-w64")

-- 添加链接依赖项
add_links("glfw3", "vulkan-1")

-- 添加系统链接库
add_syslinks("user32", "gdi32", "shell32")


-- 添加目标
target("shaders")
    set_kind("phony")
    -- -- 对 .vert 和 .frag 文件应用 glsl_shader 规则
    -- add_files("shaders/*.vert", "shaders/*.frag", {rules = "glsl_shader"})    

target("hello")
    set_kind("binary")
    -- 添加依赖，确保在编译主程序之前先编译 shaders 目标
    -- add_deps("shaders")
    add_files("src/*.cpp") -- 添加源文件
    --add_files("include/*.hpp") -- 显式添加头文件
    add_includedirs("include")
    set_languages("c++17") -- 设置C++17标准    
    --set_rundir("D:/_iWorks/GithubBackup/VulkanTest/SocPipeline")
     set_rundir("./")  -- 直接设置为当前的根目录
    -- 设置编译选项
    if is_mode("debug") then
        set_symbols("debug")
        set_optimize("none")
    elseif is_mode("release") then
        set_symbols("hidden")
        set_optimize("fastest")
        set_strip("all")
    end

-- target("hello")

--     -- 设置语言为 C++
--     set_kind("binary")
--     set_languages("cxx11")

--     -- 添加源文件
--     add_files("main.cpp")

--     -- 指定编译器选项
--     if is_plat("windows") then
--         add_cxxflags("/std:c++latest")  -- 或者使用 "/std:c++17"
--     else
--         add_cxxflags("-std=c++11")
--     end
-- -- 定义项目
-- target("hello")

--     -- 设置语言为 C
--     set_kind("binary")
--     set_languages("c11")

--     -- 添加源文件
--     add_files("main.c")
