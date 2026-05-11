#include "DirectionalLightElement.h"

#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/component_wise.hpp>
#include <limits>

#include "rendering/imgui/ImGuiManager.h"
#include "scene/SceneContext.h"

// Creates a new directional light with default settings
std::unique_ptr<EditorScene::DirectionalLightElement> EditorScene::DirectionalLightElement::new_default(const SceneContext& scene_context, EditorScene::ElementRef parent) {
    // Default: pointing down and slightly back
    glm::vec3 rotation = glm::vec3{-45.0f, -15.0f, 0.0f}; // pitch, yaw, roll in degrees
    
    glm::vec3 direction = glm::vec3(
        std::sin(glm::radians(rotation.y)) * std::cos(glm::radians(rotation.x)),
        -std::sin(glm::radians(rotation.x)),
        -std::cos(glm::radians(rotation.y)) * std::cos(glm::radians(rotation.x))
    );
    
    // Create the directional light element with cone visual representation
    auto light_element = std::make_unique<DirectionalLightElement>(
        parent,
        "New Directional Light",
        glm::vec3{0.0f, 5.0f, 0.0f},
        rotation,
        DirectionalLight::create(
            direction,
            glm::vec4{0.75f, 0.75f, 0.75f, 1.0f}
        ),
        EmissiveEntityRenderer::Entity::create(
            scene_context.model_loader.load_from_file<EmissiveEntityRenderer::VertexData>("cone.obj"),
            EmissiveEntityRenderer::InstanceData{
                glm::mat4{}, // Set via update_instance_data()
                EmissiveEntityRenderer::EmissiveEntityMaterial{
                    glm::vec4{1.0f}
                }
            },
            EmissiveEntityRenderer::RenderData{
                scene_context.texture_loader.default_white_texture()
            }
        )
    );

    light_element->update_instance_data();
    return light_element;
}

// Loads a directional light from serialized JSON data
std::unique_ptr<EditorScene::DirectionalLightElement> EditorScene::DirectionalLightElement::from_json(const SceneContext& scene_context, EditorScene::ElementRef parent, const json& j) {
    auto light_element = new_default(scene_context, parent);

    // Restore all properties from saved JSON
    light_element->position = j.value("position", glm::vec3{0.0f, 5.0f, 0.0f});
    light_element->rotation = j.value("rotation", glm::vec3{-45.0f, -15.0f, 0.0f});
    light_element->light->colour = j["colour"].get<glm::vec4>();
    light_element->visible = j.value("visible", true);
    light_element->visual_scale = j.value("visual_scale", 1.0f);

    light_element->update_instance_data();
    return light_element;
}

// Serializes directional light properties to JSON for saving
json EditorScene::DirectionalLightElement::into_json() const {
    return {
        {"position", position},
        {"rotation", rotation},
        {"colour", light->colour},
        {"visible", visible},
        {"visual_scale", visual_scale},
    };
}

// Renders ImGui UI for editing directional light properties
void EditorScene::DirectionalLightElement::add_imgui_edit_section(MasterRenderScene& render_scene, const SceneContext& scene_context) {
    ImGui::Text("Directional Light");
    SceneElement::add_imgui_edit_section(render_scene, scene_context);

    // Position controls
    ImGui::Text("Local Transformation");
    bool light_changed = false;
    light_changed |= ImGui::DragFloat3("Translation", &position[0], 0.01f);
    ImGui::DragDisableCursor(scene_context.window);
    ImGui::Spacing();

    // Rotation controls (pitch, yaw, roll)
    ImGui::Text("Direction");
    
    bool rotation_changed = ImGui::DragFloat3("Rotation", &rotation[0], 0.5f, -180.0f, 180.0f);
    ImGui::DragDisableCursor(scene_context.window);
    
    if (rotation_changed) {
        light_changed = true;
    }
    ImGui::Spacing();
    // Color and intensity controls    ImGui::Text("Light Properties");
    light_changed |= ImGui::ColorEdit3("Colour", &light->colour[0]);
    ImGui::Spacing();
    ImGui::DragFloat("Intensity", &light->colour.a, 0.01f, 0.0f, FLT_MAX);
    ImGui::DragDisableCursor(scene_context.window);

    ImGui::Spacing();
    
    // Visual representation controls
    ImGui::Text("Visuals");

    light_changed |= ImGui::Checkbox("Show Visuals", &visible);
    light_changed |= ImGui::DragFloat("Visual Scale", &visual_scale, 0.01f, 0.0f, FLT_MAX);
    ImGui::DragDisableCursor(scene_context.window);

    if (light_changed) {
        update_instance_data();
    }
}

// Updates light direction and visual representation based on current transformation
void EditorScene::DirectionalLightElement::update_instance_data() {
    // Build transformation matrix from position and Euler angles
    glm::mat4 rotation_matrix = glm::rotate(glm::radians(rotation.z), glm::vec3{0.0f, 0.0f, 1.0f}) *
                               glm::rotate(glm::radians(rotation.y), glm::vec3{0.0f, 1.0f, 0.0f}) *
                               glm::rotate(glm::radians(rotation.x), glm::vec3{1.0f, 0.0f, 0.0f});
    transform = glm::translate(position) * rotation_matrix;
    
    // Extract light direction from transformation (negative Z is forward in OpenGL)
    glm::vec3 forward_direction = glm::normalize(-glm::vec3(transform[2]));
    light->direction = forward_direction;

    if (visible) {
        // Calculate world-space position including parent transformations
        glm::vec3 visual_position = glm::vec3(transform[3]);
        if (!EditorScene::is_null(parent)) {
            glm::mat4 world_transform = (*parent)->transform * transform;
            visual_position = glm::vec3(world_transform[3]);
        }
        
        // Orient cone visual to point in light direction
        glm::vec3 target_direction = glm::normalize(light->direction);
        glm::quat rotation_quat = glm::rotation(glm::vec3{0.0f, 1.0f, 0.0f}, target_direction);
        
        // Build final model matrix for visual representation
        visual_entity->instance_data.model_matrix = 
            glm::translate(visual_position) * 
            glm::mat4_cast(rotation_quat) * 
            glm::scale(glm::vec3{0.5f * visual_scale});
    } else {
        // Hide by moving to infinity
        visual_entity->instance_data.model_matrix = 
            glm::scale(glm::vec3{std::numeric_limits<float>::infinity()}) * 
            glm::translate(glm::vec3{std::numeric_limits<float>::infinity()});
    }

    // Normalize and apply light color to visual cone for visual feedback
    glm::vec3 normalised_colour = glm::vec3(light->colour) / glm::compMax(glm::vec3(light->colour));
    visual_entity->instance_data.material.emission_tint = glm::vec4(normalised_colour, 1.0f);
}

// Returns the type identifier for this scene element
const char* EditorScene::DirectionalLightElement::element_type_name() const {
    return ELEMENT_TYPE_NAME;
}
