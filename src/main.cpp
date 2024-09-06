#include "../lum-al/src/al.hpp"
#include "../lum-al/src/defines/macros.hpp"

#include "vktf/VulkanglTFModel.hpp"

#include <tiny_gltf.h>

//example fo simple triangle rendering with 2 subpasses - main shading and posteffect

Renderer render = {};
vkglTF::Model character = {};
RasterPipe raster_pipe = {};
RasterPipe depth_pipe = {};
VkDescriptorSetLayout raster_pipe_el = {};
VkDescriptorSetLayout depth_pipe_el = {};
RenderPass depth_rpass;
RenderPass shading_rpass;
VkSampler linear_sampler = {};
VkSampler nearest_sampler = {};
// ring<Image> simple_inter_image; 
ring<Image> depth_buffer; 
ring<Image> norm_buffer; 
ring<Buffer> ubo;

bool is_depth_prepass = true;

double delt_time = 0;
double prev_time = 0;
double curr_time = 0;
// dvec2 originViewSize = dvec2(1920, 1080);
// dvec3 cameraRayDirPlane = normalize (dvec3 (cameraDir.x, cameraDir.y, 0));
// dvec3 horizline = normalize (cross (cameraRayDirPlane, dvec3 (0, 0, 1)));
// dvec3 vertiline = normalize (cross (cameraDir, horizline));

dmat4 cameraTransform;
// dvec3 cameraPos = dvec3(1,1,1);
// dvec3 cameraDir = normalize(dvec3(-1));
dvec3 cameraPos = dvec3(0.730976, -1.366005, 1.308277);
dvec3 cameraDir = normalize(dvec3(-0.507972, 0.209617, -0.835479));

dvec3 lightDir = normalize (vec3 (0.5, 0.5, -0.9));
#include <glm/gtx/string_cast.hpp>

void drawNode(vkglTF::Node *node, VkCommandBuffer commandBuffer)
{   
    if (node->mesh) {
        VkDescriptorBufferInfo
            uboInfo = {};
            uboInfo.buffer = node->mesh->uniformBuffer.buffer;
            uboInfo.range = VK_WHOLE_SIZE;
            uboInfo.offset = 0;
        VkDescriptorImageInfo
            nInfo = {};
            nInfo.imageView = character.textures[0].image.view;
            nInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            nInfo.sampler = character.textures[0].sampler;
        VkDescriptorImageInfo
            gInfo = {};
            gInfo.imageView = character.textures[1].image.view;
            gInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            gInfo.sampler = character.textures[1].sampler;
        VkDescriptorImageInfo
            dInfo = {};
            dInfo.imageView = character.textures[2].image.view;
            dInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            dInfo.sampler = character.textures[2].sampler;
        
        VkWriteDescriptorSet
            uboWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            uboWrite.dstSet = NULL;
            uboWrite.dstBinding = 0;
            uboWrite.dstArrayElement = 0;
            uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uboWrite.descriptorCount = 1;
            uboWrite.pBufferInfo = &uboInfo;
        vector<VkWriteDescriptorSet> descriptorWrites = {};
        descriptorWrites.push_back(uboWrite);
        if(!is_depth_prepass){
            VkWriteDescriptorSet
                write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                write.dstSet = NULL;
                write.dstBinding = 1;
                write.dstArrayElement = 0;
                write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                write.descriptorCount = 1;
                write.pImageInfo = &nInfo;
            descriptorWrites.push_back(write);
                write.dstBinding++;
                write.pImageInfo = &gInfo;
            descriptorWrites.push_back(write);
                write.dstBinding++;
                write.pImageInfo = &dInfo;
            descriptorWrites.push_back(write);
            const int TARGET_DSET = 1;
            vkCmdPushDescriptorSetKHR (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raster_pipe.lineLayout, TARGET_DSET, descriptorWrites.size(), descriptorWrites.data());
            struct {mat4 m;} pco = {node->getMatrix()};
            vkCmdPushConstants(commandBuffer, raster_pipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pco), &pco);
        } else {
            const int TARGET_DSET = 1;
            vkCmdPushDescriptorSetKHR (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, depth_pipe.lineLayout, TARGET_DSET, descriptorWrites.size(), descriptorWrites.data());
            struct {mat4 m;} pco = {node->getMatrix()};
            vkCmdPushConstants(commandBuffer, depth_pipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pco), &pco);
        }
        
        // cout << to_string(node->getMatrix()) << "\n";
        // cout << to_string(node->mesh->uniformBlock.matrix) << "\n";
        
        assert(node->mesh->primitives.size() > 0);
        for (auto *primitive : node->mesh->primitives) {
            // printl(primitive->indexCount);
            // printl(primitive->firstIndex);
            // printl(primitive->vertexCount);
            if (primitive->hasIndices) {
                vkCmdDrawIndexed(commandBuffer, primitive->indexCount, 1, primitive->firstIndex, 0, 0);
            } else {
                vkCmdDraw(commandBuffer, primitive->vertexCount, 1, 0, 0);
            }
            // vkCmdDrawIndexed(commandBuffer, primitive->indexCount, 1, 0, 0, 0);
            // vkCmdDrawIndexed(commandBuffer, primitive->vertexCount, 1, 0, 0, 0);
            // node.
        }
    }
// println
    for (auto& child : node->children) {
        drawNode(child, commandBuffer);
    }
}

void drawModel(vkglTF::Model* model, VkCommandBuffer commandBuffer)
{
    const VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &model->vertices.buffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
    // printl(model->nodes.size());
    // printl(model->linearNodes.size());
    if(is_depth_prepass){
        for (auto& node : model->nodes) {
            node->update();
        }
    }
    for (auto& node : model->nodes) {
        drawNode(node, commandBuffer);
    }
    // abort();
}

void updateCamera() {
    dvec3 up = glm::dvec3 (0, 1, 0); // Up vector
    dmat4 view = glm::lookAt (cameraPos, cameraPos + cameraDir, up);
    dmat4 projection = glm::perspectiveFov(glm::radians(45.0f),1920.0f,1080.0f,0.01f,25.60f);
    dmat4 worldToScreen = projection * view;
    cameraTransform = worldToScreen;

    // cameraRayDirPlane = normalize (dvec3 (cameraDir.x, cameraDir.y, 0));
    // horizline = normalize (cross (cameraRayDirPlane, dvec3 (0, 0, 1)));
    // vertiline = normalize (cross (cameraDir, horizline));
}

float yaw = -90.0f;  // Yaw is initialized to point along the Z-axis (left = -90 degrees)
float pitch = 0.0f;  // No pitch at start
float lastX = 400, lastY = 300;  // Mouse position
float sensitivity = 0.1f;  // Mouse sensitivity
bool firstMove = true;

void mouse_callback(GLFWwindow* window, double xpos, double ypos){
    if (firstMove){
        lastX = xpos;
        lastY = ypos;
        firstMove = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = ypos - lastY; // GLTF is +Y
    lastX = xpos;
    lastY = ypos;

    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    glm::vec3 
        direction;
        direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        direction.y = sin(glm::radians(pitch));
        direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraDir = glm::normalize(direction);
}

void update_controls(){
    prev_time = curr_time;
    curr_time = glfwGetTime();
    delt_time = curr_time-prev_time;
    render.deltaTime = delt_time;
    
    delt_time *= 0.0010;
    #define set_key(key, action) if(glfwGetKey(render.window.pointer, key) == GLFW_PRESS) {action;}
    set_key(GLFW_KEY_W, cameraPos += delt_time* dvec3(cameraDir) * 600. / 2.0 );
    set_key(GLFW_KEY_S, cameraPos -= delt_time* dvec3(cameraDir) * 600. / 2.0 );

    dvec3 camera_direction_to_right = glm::dquat(dvec3(0.0, glm::pi<double>()/2.0, 0.0)) * cameraDir;

    set_key(GLFW_KEY_A, cameraPos += delt_time* dvec3(camera_direction_to_right.x,camera_direction_to_right.y*0,camera_direction_to_right.z) * 400.5 / 2.0);
    set_key(GLFW_KEY_D, cameraPos -= delt_time* dvec3(camera_direction_to_right.x,camera_direction_to_right.y*0,camera_direction_to_right.z) * 400.5 / 2.0);
    set_key(GLFW_KEY_SPACE     , cameraPos -= delt_time * dvec3(0,600,0));
    set_key(GLFW_KEY_LEFT_SHIFT, cameraPos += delt_time * dvec3(0,600,0));
    set_key(GLFW_KEY_COMMA  , cameraDir = rotate(glm::identity<dmat4>(), +600. * delt_time, dvec3(0,1,0)) * dvec4(cameraDir,0));
    set_key(GLFW_KEY_PERIOD , cameraDir = rotate(glm::identity<dmat4>(), -600. * delt_time, dvec3(0,1,0)) * dvec4(cameraDir,0));
    set_key(GLFW_KEY_PAGE_DOWN, cameraDir = glm::dquat(dvec3(+600.*delt_time, 0, 0)) * cameraDir);
    set_key(GLFW_KEY_PAGE_UP  , cameraDir = glm::dquat(dvec3(-600.*delt_time, 0, 0)) * cameraDir);
    cameraDir = normalize(cameraDir); 
}

#define so(x) sizeof(x)

int main(){
    Settings settings = {};
        settings.vsync = false; //every time deciding to which image to render, wait until monitor draws current. Icreases perfomance, but limits fps
        settings.fullscreen = false;
        settings.debug = true; //Validation Layers. Use them while developing or be tricked into thinking that your code is correct
        settings.timestampCount = 128;
        settings.profile = false; //monitors perfomance via timestamps. You can place one with PLACE_TIMESTAMP() macro
        settings.fif = 2; // Frames In Flight. If 1, then record cmdbuff and submit it. If multiple, cpu will (might) be ahead of gpu by FIF-1, which makes GPU wait less
        settings.deviceFeatures.independentBlend = VK_TRUE;
        settings.deviceFeatures.samplerAnisotropy = VK_TRUE;
    render.init(settings);
    printl(render.VMAllocator);

    render.createSampler(&linear_sampler, VK_FILTER_LINEAR, VK_FILTER_LINEAR, 
        VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    render.createSampler(&nearest_sampler, VK_FILTER_NEAREST, VK_FILTER_NEAREST, 
        VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    
    // character.loadFromFile("C:/prog/mangaka/assets/chess/glTF/ABeautifulGame.gltf", &render, 1.0);
    character.loadFromFile("C:/prog/mangaka/assets/Mutant Punch_out/Mutant Punch.gltf", &render, 1.0);
    // character.loadFromFile("C:/prog/mangaka/assets/char/untitled.glb", &render, 1.0);
    // character.loadFromFile("C:/prog/mangaka/assets/t.gltf", &render, 1.0);

    render.createImageStorages(&depth_buffer,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
    {render.swapChainExtent.width, render.swapChainExtent.height, 1});
    render.createImageStorages(&norm_buffer,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R8G8B8_SNORM,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {render.swapChainExtent.width, render.swapChainExtent.height, 1});
    for(auto img : depth_buffer){
        render.transitionImageLayoutSingletime(&img, VK_IMAGE_LAYOUT_GENERAL);
    }
    for(auto img : norm_buffer){
        render.transitionImageLayoutSingletime(&img, VK_IMAGE_LAYOUT_GENERAL);
    }
    for(auto img : render.swapchainImages){
        render.transitionImageLayoutSingletime(&img, VK_IMAGE_LAYOUT_GENERAL);
    }
    render.createBufferStorages(&ubo, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, so(mat4)+so(vec4)*2+so(vec3), false);


    render.descriptorBuilder
        .setLayout(&raster_pipe.setLayout)
        .setDescriptions({  
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {/*empty*/}, {depth_buffer}, nearest_sampler, VK_IMAGE_LAYOUT_GENERAL, VK_SHADER_STAGE_FRAGMENT_BIT},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {/*empty*/}, {norm_buffer}, nearest_sampler, VK_IMAGE_LAYOUT_GENERAL, VK_SHADER_STAGE_FRAGMENT_BIT},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER  , RD_CURRENT, {ubo},       {/*empty*/},    NO_SAMPLER, NO_LAYOUT, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT},
        }).setDescriptorSets(&raster_pipe.sets)
        .defer();
    render.createDescriptorSetLayout({
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}, //in uniform buffer with joints
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
    }, &raster_pipe_el,
    VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
// println
    render.descriptorBuilder
        .setLayout(&depth_pipe.setLayout)
        .setDescriptions({  
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, {ubo}, {/*empty*/}, NO_SAMPLER, NO_LAYOUT, VK_SHADER_STAGE_VERTEX_BIT},
        }).setDescriptorSets(&depth_pipe.sets)
        .defer();
    render.createDescriptorSetLayout({
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}, //in uniform buffer with joints
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
    }, &depth_pipe_el,
    VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
// println
    render.flushDescriptorSetup();
// println

    render.renderPassBuilder.setAttachments({
            {&render.swapchainImages, Clear, Store, DontCare, DontCare, (VkClearValue){.color={.5,.5,.5}}, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
            {&depth_buffer,           Load,  DontCare, DontCare, DontCare, (VkClearValue){}, VK_IMAGE_LAYOUT_GENERAL},
        }).setSubpasses({
            {{&raster_pipe},  {}, {&render.swapchainImages}, &depth_buffer},
        }).build(&shading_rpass);

    render.renderPassBuilder.setAttachments({
            {&depth_buffer, Clear, Store, DontCare, DontCare, (VkClearValue){.depthStencil = {.depth=1.0}}, VK_IMAGE_LAYOUT_GENERAL},
            {&norm_buffer, DontCare, Store, DontCare, DontCare, (VkClearValue){}, VK_IMAGE_LAYOUT_GENERAL},
        }).setSubpasses({
            {{&depth_pipe},  {}, {&norm_buffer}, &depth_buffer},
        }).build(&depth_rpass);
// println

    struct Vertex {
			vec3 pos;
			vec3 normal;
			vec2 uv0;
			vec2 uv1;
			uvec4 joint0;
			vec4 weight0;
			vec4 color;
		};
// println
    render.pipeBuilder.setStages({
            {"shaders/compiled/charVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
            {"shaders/compiled/charFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}
        }).setAttributes({
            {VK_FORMAT_R32G32B32_SFLOAT, offsetof (Vertex, pos)},
            {VK_FORMAT_R32G32B32_SFLOAT, offsetof (Vertex, normal)},
            {VK_FORMAT_R32G32_SFLOAT, offsetof (Vertex, uv0)},
            {VK_FORMAT_R32G32_SFLOAT, offsetof (Vertex, uv1)},
            {VK_FORMAT_R32G32B32A32_UINT, offsetof (Vertex, joint0)},
            {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof (Vertex, weight0)},
            {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof (Vertex, color)},
        }).setStride(sizeof(Vertex)).setPushConstantSize(sizeof(mat4))
        .setExtent(render.swapChainExtent).setBlends({NO_BLEND})
        .setExtraDynamicLayout(raster_pipe_el)
        .setCulling(VK_CULL_MODE_NONE).setInputRate(VK_VERTEX_INPUT_RATE_VERTEX).setTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .setDepthTesting(DEPTH_TEST_READ_BIT).setDepthCompareOp(VK_COMPARE_OP_EQUAL) //depth prepass works this way
        // .setDepthTesting(DEPTH_TEST_READ_BIT | DEPTH_TEST_WRITE_BIT).setDepthCompareOp(VK_COMPARE_OP_ALWAYS)
        // .setDepthTesting(DEPTH_TEST_NONE_BIT).setDepthCompareOp(VK_COMPARE_OP_ALWAYS)
        .buildRaster(&raster_pipe);
// println
    
    render.pipeBuilder.setStages({
            {"shaders/compiled/prepassVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
            {"shaders/compiled/prepassFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
            // {"shaders/compiled/charVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        }).setDepthTesting(DEPTH_TEST_READ_BIT | DEPTH_TEST_WRITE_BIT).setDepthCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL)
        .setExtraDynamicLayout(depth_pipe_el)
        .buildRaster(&depth_pipe);


    ring<VkCommandBuffer> mainCommandBuffers;
    ring<VkCommandBuffer> extraCommandBuffers; //runtime copies. Also does first frame resources
    //you typically want to have FIF'count command buffers in their ring
    //but if you only need like 1 "baked" command buffer, just use 1
    render.createCommandBuffers ( &mainCommandBuffers, render.settings.fif);
    render.createCommandBuffers (&extraCommandBuffers, render.settings.fif);

    //you have to set this if you want to use builtin profiler
    render.mainCommandBuffers = &mainCommandBuffers;
// println

    // glfwSetCursorPosCallback(render.window.pointer, mouse_callback);
    // glfwSetInputMode(render.window.pointer, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    while(!glfwWindowShouldClose(render.window.pointer) && (glfwGetKey(render.window.pointer, GLFW_KEY_ESCAPE) != GLFW_PRESS)){
        glfwPollEvents();
        // update_controls();
        updateCamera();
// println
        // render.deviceWaitIdle();
        render.start_frame({mainCommandBuffers.current()});                
// println
                struct unicopy {mat4 trans; vec4 ldir; vec4 cdir; vec3 campos;} unicopy = {cameraTransform, vec4(lightDir,0), vec4(cameraDir,0), cameraPos};
                render.cmdPipelineBarrier (mainCommandBuffers.current(),
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
                    ubo.current());
                vkCmdUpdateBuffer (mainCommandBuffers.current(), ubo.current().buffer, 0, sizeof(unicopy), &unicopy);
                render.cmdPipelineBarrier (mainCommandBuffers.current(),
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
                    ubo.current());
            is_depth_prepass = true;
// println
            render.cmdBeginRenderPass(mainCommandBuffers.current(), &depth_rpass);
// println
                render.cmdBindPipe(mainCommandBuffers.current(), depth_pipe);
// println
                    // double anim_length = character.animations[0].end - character.animations[0].start;
                    // character.updateAnimation(0, character.animations[0].start + fmod(curr_time/1.0,anim_length));
                    drawModel(&character, mainCommandBuffers.current());
            render.cmdEndRenderPass(mainCommandBuffers.current(), &depth_rpass);
// println
            render.cmdPipelineBarrier (mainCommandBuffers.current(),
                VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT|VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT|VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
                depth_buffer.current());
            render.cmdPipelineBarrier (mainCommandBuffers.current(),
                VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT|VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT|VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
                norm_buffer.current());
            is_depth_prepass = false;
            // render.deviceWaitIdle();
// println
            render.cmdBeginRenderPass(mainCommandBuffers.current(), &shading_rpass);
                render.cmdBindPipe(mainCommandBuffers.current(), raster_pipe);
                    // double anim_length = character.animations[0].end - character.animations[0].start;
                    // character.updateAnimation(0, character.animations[0].start + fmod(curr_time/1.0,anim_length));
                    drawModel(&character, mainCommandBuffers.current());
            render.cmdEndRenderPass(mainCommandBuffers.current(), &shading_rpass);

        // render.deviceWaitIdle();
// println
        render.end_frame({mainCommandBuffers.current()});
// println
        // render.deviceWaitIdle();
        //you are the one responsible for this, because using "previous" command buffer is quite common
        mainCommandBuffers.move();
        // static int ctr=0; ctr++; if(ctr==2) abort();
    }
// println
    // cout << glm::to_string(cameraDir);
    // cout << glm::to_string(cameraPos);
    // render.deviceWaitIdle();
    render.destroyRenderPass(&shading_rpass);
    render.destroyRenderPass(&depth_rpass);
    render.destroyRasterPipeline(&raster_pipe);
    render.destroyRasterPipeline(&depth_pipe);
    render.deleteImages(&depth_buffer);
    render.deleteBuffers(&ubo);
// println

    character.destroy();
// println
    render.cleanup();
}