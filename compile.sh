#!/bin/bash
#Compile app
g++ -ldl -lglfw -D USE_GLFW -D ENABLED_DEBUG vulkan.cpp -o vulkan 
#Compile shaders
/home/maria/Downloads/VulkanSDK/1.0.54.0/x86_64/bin/glslangValidator -V Shaders/shader.vert 
/home/maria/Downloads/VulkanSDK/1.0.54.0/x86_64/bin/glslangValidator -V Shaders/shader.frag
