#include "../lum-al/src/al.hpp"
#include "../lum-al/src/defines/macros.hpp"

#include "vktf/VulkanglTFModel.hpp"

#include <tiny_gltf.h>

//example fo simple triangle rendering with 2 subpasses - main shading and posteffect

Renderer render = {};
vkglTF::Model character = {};

// void draw_model(vkglTF::Model& model, Renderer& render){

// }
#define so(x) sizeof(x)

int main(){
    Settings settings = {};
        settings.vsync = false; //every time deciding to which image to render, wait until monitor draws current. Icreases perfomance, but limits fps
        settings.fullscreen = false;
        settings.debug = true; //Validation Layers. Use them while developing or be tricked into thinking that your code is correct
        settings.timestampCount = 128;
        settings.profile = false; //monitors perfomance via timestamps. You can place one with PLACE_TIMESTAMP() macro
        settings.fif = 2; // Frames In Flight. If 1, then record cmdbuff and submit it. If multiple, cpu will (might) be ahead of gpu by FIF-1, which makes GPU wait less
    render.init(settings);
    printl(render.VMAllocator);

    character.loadFromFile("assets/Mutant Punch_out/Mutant Punch.gltf", &render, 1.0);

    RasterPipe raster_pipe = {};
    RasterPipe shading_pipe = {};

    ring<Image> simple_inter_image; 
    ring<Buffer> ubo;

    render.createImageStorages(&simple_inter_image,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {render.swapChainExtent.width, render.swapChainExtent.height, 1});
    for(auto img : simple_inter_image){
        render.transitionImageLayoutSingletime(&img, VK_IMAGE_LAYOUT_GENERAL);
    }
    render.createBufferStorages(&ubo, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, so(mat4)+so(vec4)+so(vec3), false);

    render.descriptorBuilder
        .setLayout(&raster_pipe.setLayout)
        .setDescriptions({
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, {simple_inter_image}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL, VK_SHADER_STAGE_FRAGMENT_BIT},
        }).setDescriptorSets(&raster_pipe.sets)
        .defer();
println
    render.descriptorBuilder
        .setLayout(&shading_pipe.setLayout)
        .setDescriptorSets(&shading_pipe.sets)
        .setDescriptions({
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, {simple_inter_image}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL, VK_SHADER_STAGE_FRAGMENT_BIT}
        })
        .defer();
    render.flushDescriptorSetup();
println

    RenderPass simple_rpass = {};
    render.renderPassBuilder.setAttachments({
            {&simple_inter_image,   DontCare, DontCare, DontCare, DontCare, {}, VK_IMAGE_LAYOUT_GENERAL},
            {&render.swapchainImages, DontCare, Store,    DontCare, DontCare, {}, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
        }).setSubpasses({
            {{&raster_pipe},     {},                     {&simple_inter_image},   {}},
            {{&shading_pipe}, {&simple_inter_image}, {&render.swapchainImages}, {}}
        }).build(&simple_rpass);

println
    render.pipeBuilder.setStages({
            {"shaders/compiled/charVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
            {"shaders/compiled/charFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
        }).setExtent(render.swapChainExtent).setBlends({NO_BLEND})
        .buildRaster(&raster_pipe);
println
    render.pipeBuilder.setStages({
            {"shaders/compiled/triagVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
            {"shaders/compiled/posteffectFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
        }).setExtent(render.swapChainExtent).setBlends({NO_BLEND})
        .buildRaster(&shading_pipe);

    ring<VkCommandBuffer> mainCommandBuffers;
    ring<VkCommandBuffer> extraCommandBuffers; //runtime copies. Also does first frame resources
    //you typically want to have FIF'count command buffers in their ring
    //but if you only need like 1 "baked" command buffer, just use 1
    render.createCommandBuffers ( &mainCommandBuffers, render.settings.fif);
    render.createCommandBuffers (&extraCommandBuffers, render.settings.fif);

    //you have to set this if you want to use builtin profiler
    render.mainCommandBuffers = &mainCommandBuffers;
println

    while(!glfwWindowShouldClose(render.window.pointer) && (glfwGetKey(render.window.pointer, GLFW_KEY_ESCAPE) != GLFW_PRESS)){
        glfwPollEvents();
println
        render.start_frame({mainCommandBuffers.current()});                
println
            render.cmdBeginRenderPass(mainCommandBuffers.current(), &simple_rpass);
println
                render.cmdBindPipe(mainCommandBuffers.current(), raster_pipe);
println
                    character.draw(mainCommandBuffers.current());
                    // render.cmdDraw(mainCommandBuffers.current(), 3, 1, 0, 0);
println
            render.cmdNextSubpass(mainCommandBuffers.current(), &simple_rpass);
println
                render.cmdBindPipe(mainCommandBuffers.current(), shading_pipe);
println
                   render.cmdDraw(mainCommandBuffers.current(), 3, 1, 0, 0);
println
            render.cmdEndRenderPass(mainCommandBuffers.current(), &simple_rpass);
println
        render.end_frame({mainCommandBuffers.current()});
        //you are the one responsible for this, because using "previous" command buffer is quite common
        mainCommandBuffers.move();
        // static int ctr=0; ctr++; if(ctr==2) abort();
    }
println
    render.deviceWaitIdle();
    render.destroyRenderPass(&simple_rpass);
    render.destroyRasterPipeline(&raster_pipe);
    render.destroyRasterPipeline(&shading_pipe);
    render.deleteImages(&simple_inter_image);

    character.destroy();

    render.cleanup();
}