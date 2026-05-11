#ifndef DIRECTIONAL_LIGHT_ELEMENT_H
#define DIRECTIONAL_LIGHT_ELEMENT_H

#include "SceneElement.h"
#include "rendering/scene/Lights.h"
#include "scene/SceneContext.h"

namespace EditorScene {
    class DirectionalLightElement : public SceneElement {
    public:
        static constexpr const char* ELEMENT_TYPE_NAME = "Directional Light";

        glm::vec3 position;  // Local position for the light visual
        glm::vec3 rotation;  // Euler angles (pitch, yaw, roll) in degrees for orientation
        bool visible = true;
        float visual_scale = 1.0f;
        std::shared_ptr<DirectionalLight> light;
        std::shared_ptr<EmissiveEntityRenderer::Entity> visual_entity;
        glm::mat4 transform = glm::mat4(1.0f);  // Local transformation matrix

        DirectionalLightElement(const ElementRef& parent, std::string name, glm::vec3 position, glm::vec3 rotation, std::shared_ptr<DirectionalLight> light, std::shared_ptr<EmissiveEntityRenderer::Entity> visual_entity) :
            SceneElement(parent, std::move(name)), position(position), rotation(rotation), light(std::move(light)), visual_entity(std::move(visual_entity)) {}

        static std::unique_ptr<DirectionalLightElement> new_default(const SceneContext& scene_context, ElementRef parent);
        static std::unique_ptr<DirectionalLightElement> from_json(const SceneContext& scene_context, ElementRef parent, const json& j);

        [[nodiscard]] json into_json() const override;

        void add_imgui_edit_section(MasterRenderScene& render_scene, const SceneContext& scene_context) override;

        void update_instance_data() override;

        void add_to_render_scene(MasterRenderScene& target_render_scene) override {
            target_render_scene.insert_entity(visual_entity);
            target_render_scene.insert_light(light);
        }

        void remove_from_render_scene(MasterRenderScene& target_render_scene) override {
            target_render_scene.remove_entity(visual_entity);
            target_render_scene.remove_light(light);
        }

        [[nodiscard]] const char* element_type_name() const override;
    };
}

#endif // DIRECTIONAL_LIGHT_ELEMENT_H
