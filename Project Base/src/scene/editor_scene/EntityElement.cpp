#include "EntityElement.h"

#include "rendering/imgui/ImGuiManager.h"
#include "scene/SceneContext.h"
#include <type_traits>
#include <glm/gtc/type_ptr.hpp>

namespace {
    template<typename Material, typename = void>
    struct has_material_ambient : std::false_type {};

    template<typename Material>
    struct has_material_ambient<Material, std::void_t<decltype(std::declval<Material>().ambient)>> : std::true_type {};

    template<typename Material, typename = void>
    struct has_material_ambient_tint : std::false_type {};

    template<typename Material>
    struct has_material_ambient_tint<Material, std::void_t<decltype(std::declval<Material>().ambient_tint)>> : std::true_type {};

    template<typename Material, typename = void>
    struct has_material_diffuse : std::false_type {};

    template<typename Material>
    struct has_material_diffuse<Material, std::void_t<decltype(std::declval<Material>().diffuse)>> : std::true_type {};

    template<typename Material, typename = void>
    struct has_material_diffuse_tint : std::false_type {};

    template<typename Material>
    struct has_material_diffuse_tint<Material, std::void_t<decltype(std::declval<Material>().diffuse_tint)>> : std::true_type {};

    template<typename Material, typename = void>
    struct has_material_emissive : std::false_type {};

    template<typename Material>
    struct has_material_emissive<Material, std::void_t<decltype(std::declval<Material>().emissive)>> : std::true_type {};

    template<typename Material, typename = void>
    struct has_material_specular : std::false_type {};

    template<typename Material>
    struct has_material_specular<Material, std::void_t<decltype(std::declval<Material>().specular)>> : std::true_type {};

    template<typename Material, typename = void>
    struct has_material_specular_tint : std::false_type {};

    template<typename Material>
    struct has_material_specular_tint<Material, std::void_t<decltype(std::declval<Material>().specular_tint)>> : std::true_type {};

    template<typename Material, typename = void>
    struct has_material_shininess : std::false_type {};

    template<typename Material>
    struct has_material_shininess<Material, std::void_t<decltype(std::declval<Material>().shininess)>> : std::true_type {};
}

std::unique_ptr<EditorScene::EntityElement> EditorScene::EntityElement::new_default(const SceneContext& scene_context, ElementRef parent) {
    auto rendered_entity = EntityRenderer::Entity::create(
        scene_context.model_loader.load_from_file<EntityRenderer::VertexData>("cube.obj"),
        EntityRenderer::InstanceData{
            glm::mat4{}, // Set via update_instance_data()
            EntityRenderer::EntityMaterial{
                {1.0f, 1.0f, 1.0f, 1.0f},
                {1.0f, 1.0f, 1.0f, 1.0f},
                {1.0f, 1.0f, 1.0f, 1.0f},
                512.0f,
            }
        },
        EntityRenderer::RenderData{
            scene_context.texture_loader.default_white_texture(),
            scene_context.texture_loader.default_white_texture()
        }
    );
    auto new_entity = std::make_unique<EntityElement>(
        parent,
        "New Entity",
        glm::vec3{0.0f},
        glm::vec3{0.0f},
        glm::vec3{1.0f},
        rendered_entity
    );
    new_entity->update_instance_data();
    return new_entity;
}
std::unique_ptr<EditorScene::EntityElement> EditorScene::EntityElement::from_json(const SceneContext& scene_context, EditorScene::ElementRef parent, const json& j) {
    auto new_entity = new_default(scene_context, parent);
    new_entity->update_local_transform_from_json(j);
    new_entity->update_material_from_json(j);
    new_entity->rendered_entity->model = scene_context.model_loader.load_from_file<EntityRenderer::VertexData>(j["model"]);
    new_entity->rendered_entity->render_data.diffuse_texture = texture_from_json(scene_context, j["diffuse_texture"]);
    new_entity->rendered_entity->render_data.specular_map_texture = texture_from_json(scene_context, j["specular_map_texture"]);
    if (j.contains("texture_scale")) {
        new_entity->rendered_entity->render_data.texture_scale = j["texture_scale"];
    }
    new_entity->update_instance_data();
    return new_entity;
}
json EditorScene::EntityElement::into_json() const {
    if (!rendered_entity->model->get_filename().has_value()) {
        return {
            {"error", Formatter() << "Entity [" << name << "]'s model does not have a filename so can not be exported, and has been skipped."}
        };
    }
    return {
        local_transform_into_json(),
        material_into_json(),
        {"model", rendered_entity->model->get_filename().value()},
        {"diffuse_texture", texture_to_json(rendered_entity->render_data.diffuse_texture)},
        {"specular_map_texture", texture_to_json(rendered_entity->render_data.specular_map_texture)},
        {"texture_scale", rendered_entity->render_data.texture_scale}
    };
}
void EditorScene::EntityElement::add_imgui_edit_section(MasterRenderScene& render_scene, const SceneContext& scene_context) {
    ImGui::Text("Entity");
    SceneElement::add_imgui_edit_section(render_scene, scene_context);
    add_local_transform_imgui_edit_section(render_scene, scene_context);
    add_material_imgui_edit_section(render_scene, scene_context);
    ImGui::Text("Model & Textures");
    scene_context.model_loader.add_imgui_model_selector("Model Selection", rendered_entity->model);
    scene_context.texture_loader.add_imgui_texture_selector("Diffuse Texture", rendered_entity->render_data.diffuse_texture);
    scene_context.texture_loader.add_imgui_texture_selector("Specular Map", rendered_entity->render_data.specular_map_texture, false);
    ImGui::DragFloat2("Texture Scale", &rendered_entity->render_data.texture_scale[0], 0.01f, 0.01f, 10.0f);
    ImGui::Spacing();
}

void EditorScene::EntityElement::add_material_imgui_edit_section(MasterRenderScene& render_scene, const SceneContext& scene_context) {
    ImGui::Text("Material");
    bool material_changed = false;

    if constexpr (has_material_ambient_tint<decltype(material)>::value) {
        material_changed |= ImGui::ColorEdit4("Ambient Tint", (float*)&material.ambient_tint);
    }

    if constexpr (has_material_diffuse_tint<decltype(material)>::value) {
        material_changed |= ImGui::ColorEdit4("Diffuse Tint", (float*)&material.diffuse_tint);
    }

    if constexpr (has_material_specular_tint<decltype(material)>::value) {
        material_changed |= ImGui::ColorEdit4("Specular Tint", (float*)&material.specular_tint);
    }

    if constexpr (has_material_shininess<decltype(material)>::value) {
        material_changed |= ImGui::SliderFloat("Shininess", &material.shininess, 0.0f, 100.0f);
    }

    ImGui::Spacing();

    if (material_changed) {
        update_instance_data();
    }
}

void EditorScene::EntityElement::update_instance_data() {
    transform = calc_model_matrix();

    if (!EditorScene::is_null(parent)) {
        // Post multiply by transform so that local transformations are applied first
        transform = (*parent)->transform * transform;
    }
    rendered_entity->instance_data.model_matrix = transform;
    rendered_entity->instance_data.material = material;
}
const char* EditorScene::EntityElement::element_type_name() const {
    return ELEMENT_TYPE_NAME;
}