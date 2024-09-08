#include "../lum-al/src/al.hpp"
#include "../lum-al/src/defines/macros.hpp"

#include "vktf/VulkanglTFModel.hpp"

#include <tiny_gltf.h>

//example fo simple triangle rendering with 2 subpasses - main shading and posteffect

Renderer render = {};
vkglTF::Model character = {};
vkglTF::Model chess = {};

RenderPass shadowmap_rpass;
VkDescriptorSetLayout shadowmap_pipe_el = {};
RasterPipe shadowmap_pipe = {};

RenderPass shading_rpass;
VkDescriptorSetLayout raster_pipe_el = {};
RasterPipe raster_pipe = {};

RenderPass depth_rpass;
RasterPipe depth_pipe = {};
VkDescriptorSetLayout depth_pipe_el = {};

VkSampler linear_sampler = {};
VkSampler nearest_sampler = {};
VkSampler shadow_sampler = {};

ring<Image> shadowmap; 
ring<Image> depth_buffer; 
ring<Image> norm_buffer; 
ring<Buffer> ubo;
ring<Buffer> lubo;

enum STAGE{
    SHADOWMAP_PASS,
    DEPTH_PREPASS,
    SHADING_PASS,
};
STAGE stage;

double delt_time = 0;
double prev_time = 0;
double curr_time = 0;

dmat4 cameraTransform;
dmat4 lightTransform;
// dvec3 cameraPos = dvec3(1,1,1);
// dvec3 cameraDir = normalize(dvec3(-1));
dvec3 cameraPos = dvec3(0.730976, -0.8, 1.308277);
dvec3 cameraDir = normalize(dvec3(-0.507972, 0.209617, -0.835479));

dvec3 lightDir = normalize (vec3 (+0.507972, 0.309617, +0.735479));
// dvec3 lightDir = normalize (vec3 (0.5, 0.5, -0.9));
#include <glm/gtx/string_cast.hpp>

void checkNpushDset(vkglTF::Node *node, VkCommandBuffer commandBuffer, vkglTF::Primitive* primitive){
    VkDescriptorBufferInfo
        uboInfo = {};
        uboInfo.buffer = node->mesh->uniformBuffer[node->mesh->currentFIF].buffer;
        uboInfo.range = VK_WHOLE_SIZE;
        uboInfo.offset = 0;
    VkDescriptorImageInfo
        nInfo = {};
        nInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkDescriptorImageInfo
        gInfo = {};
        gInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkDescriptorImageInfo
        dInfo = {};
        dInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    
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
    VkWriteDescriptorSet
        write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = NULL;
        write.dstBinding = 1;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        nInfo.imageView = primitive->material.normalTexture->image.view;
        nInfo.sampler = primitive->material.normalTexture->sampler;
    if(stage == SHADING_PASS){
        if(primitive->material.normalTexture){
            nInfo.imageView = primitive->material.normalTexture->image.view;
            nInfo.sampler = primitive->material.normalTexture->sampler;
            write.pImageInfo = &nInfo;
            descriptorWrites.push_back(write);
        }
        write.dstBinding++;
        if(primitive->material.metallicRoughnessTexture){
            nInfo.imageView = primitive->material.metallicRoughnessTexture->image.view;
            nInfo.sampler = primitive->material.metallicRoughnessTexture->sampler;
            write.pImageInfo = &nInfo;
            descriptorWrites.push_back(write);
        }
        write.dstBinding++;
        if(primitive->material.baseColorTexture){
            nInfo.imageView = primitive->material.baseColorTexture->image.view;
            nInfo.sampler = primitive->material.baseColorTexture->sampler;
            write.pImageInfo = &nInfo;
            descriptorWrites.push_back(write);
        }
        const int TARGET_DSET = 1;
        vkCmdPushDescriptorSetKHR (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raster_pipe.lineLayout, TARGET_DSET, descriptorWrites.size(), descriptorWrites.data());
        struct {mat4 m;} pco = {node->getMatrix()};
        vkCmdPushConstants(commandBuffer, raster_pipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pco), &pco);
    } else if(stage == DEPTH_PREPASS) {
        if(primitive->material.normalTexture){
            nInfo.imageView = primitive->material.normalTexture->image.view;
            nInfo.sampler = primitive->material.normalTexture->sampler;
            write.pImageInfo = &nInfo;
            descriptorWrites.push_back(write);
        }
        const int TARGET_DSET = 1;
        vkCmdPushDescriptorSetKHR (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, depth_pipe.lineLayout, TARGET_DSET, descriptorWrites.size(), descriptorWrites.data());
        struct {mat4 m;} pco = {node->getMatrix()};
        vkCmdPushConstants(commandBuffer, depth_pipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pco), &pco);
    } else if(stage == SHADOWMAP_PASS){
        const int TARGET_DSET = 1;
        vkCmdPushDescriptorSetKHR (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowmap_pipe.lineLayout, TARGET_DSET, descriptorWrites.size(), descriptorWrites.data());
        struct {mat4 m;} pco = {node->getMatrix()};
        vkCmdPushConstants(commandBuffer, shadowmap_pipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pco), &pco);
    }

}

void drawNode(vkglTF::Node *node, VkCommandBuffer commandBuffer)
{   
    if (node->mesh) {
        // cout << to_string(node->getMatrix()) << "\n";
        // cout << to_string(node->mesh->uniformBlock.matrix) << "\n";
        
        assert(node->mesh->primitives.size() > 0);
        for (auto *primitive : node->mesh->primitives) {
            // printl(primitive->indexCount);
            checkNpushDset(node, commandBuffer, primitive);        
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
    // if(stage == SHADOWMAP_PASS){ //first stage out of all 3
    //     for (auto& node : model->nodes) {
    //         node->update();
    //     }
    // }
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
}
void updateLight(){
    dvec3 horizon = normalize (cross (dvec3 (1, 0, 0), lightDir));
    dvec3 up = normalize (cross (horizon, lightDir)); // Up vector
    // dvec3 light_pos = dvec3(dvec2(settings.world_size*16),0)/2.0 - 5*16.0*lightDir;
    dvec3 light_pos = dvec3(0)/2. - 2.5 * lightDir;
    dmat4 view = glm::lookAt (light_pos, light_pos + lightDir, up);
    dmat4 projection = glm::ortho (-2. ,2., 2.,-2., -10.0, +10.0);
    dmat4 worldToScreen = projection * view;
    lightTransform = worldToScreen;
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
void update_timer(){
    prev_time = curr_time;
    curr_time = glfwGetTime();
    delt_time = curr_time-prev_time;
    render.deltaTime = delt_time;
}
void update_controls(){
    double dt = delt_time * 0.0010 * 600.0;
    #define set_key(key, action) if(glfwGetKey(render.window.pointer, key) == GLFW_PRESS) {action;}
    set_key(GLFW_KEY_W, cameraPos += dt* dvec3(cameraDir) * 1.5 );
    set_key(GLFW_KEY_S, cameraPos -= dt* dvec3(cameraDir) * 1.5 );

    dvec3 camera_direction_to_right = glm::dquat(dvec3(0.0, glm::pi<double>()/2.0, 0.0)) * cameraDir;

    set_key(GLFW_KEY_A, cameraPos += dt* dvec3(camera_direction_to_right.x,camera_direction_to_right.y*0,camera_direction_to_right.z));
    set_key(GLFW_KEY_D, cameraPos -= dt* dvec3(camera_direction_to_right.x,camera_direction_to_right.y*0,camera_direction_to_right.z));
    set_key(GLFW_KEY_SPACE     , cameraPos -= dt * dvec3(0,1,0));
    set_key(GLFW_KEY_LEFT_SHIFT, cameraPos += dt * dvec3(0,1,0));
    set_key(GLFW_KEY_COMMA  , cameraDir = rotate(glm::identity<dmat4>(), dt, dvec3(0,1,0)) * dvec4(cameraDir,0));
    set_key(GLFW_KEY_PERIOD , cameraDir = rotate(glm::identity<dmat4>(), dt, dvec3(0,1,0)) * dvec4(cameraDir,0));
    set_key(GLFW_KEY_PAGE_DOWN, cameraDir = glm::dquat(dvec3(dt, 0, 0)) * cameraDir);
    set_key(GLFW_KEY_PAGE_UP  , cameraDir = glm::dquat(dvec3(dt, 0, 0)) * cameraDir);
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
        // settings.deviceFeatures.independentBlend = VK_TRUE;
        settings.deviceFeatures.samplerAnisotropy = VK_TRUE;
    render.init(settings);
    printl(render.VMAllocator);

    VkSamplerCreateInfo 
        samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.compareEnable = VK_TRUE;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        samplerInfo.compareOp = VK_COMPARE_OP_LESS;
    VK_CHECK(vkCreateSampler (render.device, &samplerInfo, NULL, &shadow_sampler));

    render.createSampler(&linear_sampler, VK_FILTER_LINEAR, VK_FILTER_LINEAR, 
        VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    render.createSampler(&nearest_sampler, VK_FILTER_NEAREST, VK_FILTER_NEAREST, 
        VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    
    // character.loadFromFile("C:/prog/mangaka/assets/chess/glTF/ABeautifulGame.gltf", &render, 1.0);
    character.loadFromFile("assets/Mutant Punch_out/Mutant Punch.gltf", &render, 1.0);
    chess.loadFromFile("assets/chess/glTF/ABeautifulGame.gltf", &render, 2.0);
    // character.loadFromFile("C:/prog/mangaka/assets/Bistro_v5_2/untitled.gltf", &render, 1.0);
    // character.loadFromFile("C:/prog/mangaka/assets/char/untitled.glb", &render, 1.0);
    // character.loadFromFile("C:/prog/mangaka/assets/t.gltf", &render, 1.0);

    render.createImageStorages(&shadowmap,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
    {1024*2, 1024*2, 1});
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
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {render.swapChainExtent.width, render.swapChainExtent.height, 1});
    for(auto img : shadowmap){
        render.transitionImageLayoutSingletime(&img, VK_IMAGE_LAYOUT_GENERAL);
    }
    for(auto img : depth_buffer){
        render.transitionImageLayoutSingletime(&img, VK_IMAGE_LAYOUT_GENERAL);
    }
    for(auto img : norm_buffer){
        render.transitionImageLayoutSingletime(&img, VK_IMAGE_LAYOUT_GENERAL);
    }
    for(auto img : render.swapchainImages){
        render.transitionImageLayoutSingletime(&img, VK_IMAGE_LAYOUT_GENERAL);
    }
    render.createBufferStorages(&ubo, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, so(mat4)*2+so(vec4)*2+so(vec3), false);
    render.createBufferStorages(&lubo, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, so(mat4)+so(vec4), false);


    render.descriptorBuilder
        .setLayout(&raster_pipe.setLayout)
        .setDescriptions({  
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER  , RD_CURRENT, {ubo},       {/*empty*/},    NO_SAMPLER, NO_LAYOUT, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {/*empty*/}, {depth_buffer}, nearest_sampler, VK_IMAGE_LAYOUT_GENERAL, VK_SHADER_STAGE_FRAGMENT_BIT},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {/*empty*/}, {norm_buffer}, nearest_sampler, VK_IMAGE_LAYOUT_GENERAL, VK_SHADER_STAGE_FRAGMENT_BIT},
            // {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {/*empty*/}, {shadowmap}, shadow_sampler, VK_IMAGE_LAYOUT_GENERAL, VK_SHADER_STAGE_FRAGMENT_BIT},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {/*empty*/}, {shadowmap}, linear_sampler, VK_IMAGE_LAYOUT_GENERAL, VK_SHADER_STAGE_FRAGMENT_BIT},
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
    render.descriptorBuilder
        .setLayout(&shadowmap_pipe.setLayout)
        .setDescriptions({  
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, {lubo}, {/*empty*/}, NO_SAMPLER, NO_LAYOUT, VK_SHADER_STAGE_VERTEX_BIT},
        }).setDescriptorSets(&shadowmap_pipe.sets)
        .defer();
    render.createDescriptorSetLayout({
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}, //in uniform buffer with joints
    }, &shadowmap_pipe_el,
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
    
    render.renderPassBuilder.setAttachments({
            {&shadowmap,   Clear, Store, DontCare, DontCare, (VkClearValue){.depthStencil = {.depth=1.0}}, VK_IMAGE_LAYOUT_GENERAL},
        }).setSubpasses({
            {{&shadowmap_pipe},  {}, {}, &shadowmap},
        }).build(&shadowmap_rpass);
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
    render.pipeBuilder.setStages({
            {"shaders/compiled/shadowmapVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        }).setDepthTesting(DEPTH_TEST_READ_BIT | DEPTH_TEST_WRITE_BIT).setDepthCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL)
        .setExtraDynamicLayout(shadowmap_pipe_el)
        .setCulling(VK_CULL_MODE_FRONT_BIT)
        .buildRaster(&shadowmap_pipe);

    ring<VkCommandBuffer> mainCommandBuffers;
    ring<VkCommandBuffer> extraCommandBuffers; //runtime copies. Also does first frame resources
    //you typically want to have FIF'count command buffers in their ring
    //but if you only need like 1 "baked" command buffer, just use 1
    render.createCommandBuffers ( &mainCommandBuffers, render.settings.fif);
    render.createCommandBuffers (&extraCommandBuffers, render.settings.fif);

    //you have to set this if you want to use builtin profiler
    render.mainCommandBuffers = &mainCommandBuffers;
// println

    glfwSetCursorPosCallback(render.window.pointer, mouse_callback);
    glfwSetInputMode(render.window.pointer, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
// println
    for(int i=0; i<render.settings.fif; i++){
        for (auto &node : chess.nodes) {
            node->update();
        }
    }

    while(!glfwWindowShouldClose(render.window.pointer) && (glfwGetKey(render.window.pointer, GLFW_KEY_ESCAPE) != GLFW_PRESS)){
        glfwPollEvents();
        update_timer();
        update_controls();
        updateCamera();
        updateLight();
        double anim_length = character.animations[0].end - character.animations[0].start;
        character.updateAnimation(0, character.animations[0].start + fmod(curr_time/2.0,anim_length));
        // render.deviceWaitIdle(); //wait untill all resources not in use

// println
        render.start_frame({mainCommandBuffers.current()});                
// println
                struct unicopy {mat4 trans; mat4 ltrans; vec4 ldir; vec4 cdir; vec3 campos;} unicopy = {cameraTransform, lightTransform, vec4(lightDir,0), vec4(cameraDir,0), cameraPos};
                render.cmdPipelineBarrier (mainCommandBuffers.current(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
                    ubo.current());
                vkCmdUpdateBuffer (mainCommandBuffers.current(), ubo.current().buffer, 0, sizeof(unicopy), &unicopy);
                render.cmdPipelineBarrier (mainCommandBuffers.current(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
                    ubo.current());
// println
                struct lunicopy {mat4 ltrans; vec4 ldir;} lunicopy = {lightTransform, vec4(lightDir,0)};
                render.cmdPipelineBarrier (mainCommandBuffers.current(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
                    lubo.current());
                vkCmdUpdateBuffer (mainCommandBuffers.current(), lubo.current().buffer, 0, sizeof(lunicopy), &lunicopy);
                render.cmdPipelineBarrier (mainCommandBuffers.current(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
                    lubo.current());
// println
            render.cmdPipelineBarrier (mainCommandBuffers.current(),
                VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT|VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT|VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
                shadowmap.current());
            stage = SHADOWMAP_PASS;
            render.cmdBeginRenderPass(mainCommandBuffers.current(), &shadowmap_rpass);
                render.cmdBindPipe(mainCommandBuffers.current(), shadowmap_pipe);
                    drawModel(&character, mainCommandBuffers.current());
                    drawModel(&chess, mainCommandBuffers.current());
            render.cmdEndRenderPass(mainCommandBuffers.current(), &shadowmap_rpass);
// println
                render.cmdPipelineBarrier (mainCommandBuffers.current(),
                    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT|VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT|VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
                    shadowmap.current());
            stage = DEPTH_PREPASS;
            render.cmdBeginRenderPass(mainCommandBuffers.current(), &depth_rpass);
                render.cmdBindPipe(mainCommandBuffers.current(), depth_pipe);
                    drawModel(&character, mainCommandBuffers.current());
                    drawModel(&chess, mainCommandBuffers.current());
            render.cmdEndRenderPass(mainCommandBuffers.current(), &depth_rpass);
                render.cmdPipelineBarrier (mainCommandBuffers.current(),
                    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT|VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT|VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
                    depth_buffer.current());
                render.cmdPipelineBarrier (mainCommandBuffers.current(),
                    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT|VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT|VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
                    norm_buffer.current());

            stage = SHADING_PASS;
            render.cmdBeginRenderPass(mainCommandBuffers.current(), &shading_rpass);
                render.cmdBindPipe(mainCommandBuffers.current(), raster_pipe);
                    // double anim_length = character.animations[0].end - character.animations[0].start;
                    // character.updateAnimation(0, character.animations[0].start + fmod(curr_time/1.0,anim_length));
                    drawModel(&character, mainCommandBuffers.current());
                    drawModel(&chess, mainCommandBuffers.current());
            render.cmdEndRenderPass(mainCommandBuffers.current(), &shading_rpass);
            render.cmdPipelineBarrier (mainCommandBuffers.current(),
                VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT|VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT|VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
                norm_buffer.current());

// println
        render.end_frame({mainCommandBuffers.current()});
// println
        //you are the one responsible for this, because using "previous" command buffer is quite common
        mainCommandBuffers.move();
        lubo.move();
        ubo.move();
        shadowmap.move(); 
        depth_buffer.move(); 
        norm_buffer.move(); 
        // static int ctr=0; ctr++; if(ctr==2) abort();
    }
// println
    render.deviceWaitIdle(); //wait untill all resources not in use
    // cout << glm::to_string(cameraDir);
    // cout << glm::to_string(cameraPos);
    render.destroyRenderPass(&shadowmap_rpass);
    render.destroyRenderPass(&depth_rpass);
    render.destroyRenderPass(&shading_rpass);
    render.destroyRasterPipeline(&shadowmap_pipe);
    render.destroyRasterPipeline(&depth_pipe);
    render.destroyRasterPipeline(&raster_pipe);
    render.deleteImages(&shadowmap);
    render.deleteImages(&depth_buffer);
    render.deleteImages(&norm_buffer);
    render.deleteBuffers(&ubo);
    render.deleteBuffers(&lubo);
// println
    render.destroySampler(linear_sampler);
    render.destroySampler(nearest_sampler);
    render.destroySampler(shadow_sampler);

    vkDestroyDescriptorSetLayout(render.device, shadowmap_pipe_el, NULL);
    vkDestroyDescriptorSetLayout(render.device, depth_pipe_el, NULL);
    vkDestroyDescriptorSetLayout(render.device, raster_pipe_el, NULL);
    character.destroy();
    chess.destroy();
// println
    render.cleanup();
}