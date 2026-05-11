#include "SceneElement.h"
#include "scene/SceneContext.h"
#include "rendering/imgui/ImGuiManager.h"

void EditorScene::SceneElement::add_imgui_edit_section(MasterRenderScene& /*render_scene*/, const SceneContext& /*scene_context*/) {
    ImGui::InputText("Name", &name, 0);
    ImGui::Spacing();
}

void EditorScene::SceneElement::visit_children_recursive(const std::function<void(SceneElement&)>& fn) const {
    auto children = get_children();
    if (children == nullptr) return;
    for (auto& child: *children) {
        fn(*child);
        child->visit_children_recursive(fn);
    }
}

json EditorScene::SceneElement::texture_to_json(const std::shared_ptr<TextureHandle>& texture) {
    if (!texture->get_filename().has_value()) {
        return {
            {"error", Formatter() << "Texture does not have a filename so can not be exported, and has been skipped."}
        };
    }

    return {
        {"filename",   texture->get_filename().value()},
        {"is_srgb",    texture->is_srgb()},
        {"is_flipped", texture->is_flipped()},
    };
}

std::shared_ptr<TextureHandle> EditorScene::SceneElement::texture_from_json(const SceneContext& scene_context, const json& json) {
    if (json.contains("error")) {
        return scene_context.texture_loader.default_white_texture();
    }

    return scene_context.texture_loader.load_from_file(json["filename"], json["is_srgb"], json["is_flipped"]);
}

void EditorScene::LocalTransformComponent::add_local_transform_imgui_edit_section(MasterRenderScene& /*render_scene*/, const SceneContext& scene_context) {
    ImGui::Text("Local Transformation");
    bool transformUpdated = false;
    transformUpdated |= ImGui::DragFloat3("Translation", &position[0], 0.01f);
    ImGui::DragDisableCursor(scene_context.window);

    glm::vec3 euler_rotation_degrees = glm::degrees(euler_rotation);
    transformUpdated |= ImGui::DragFloat3("Rotation", &euler_rotation_degrees[0]);
    ImGui::DragDisableCursor(scene_context.window);
    euler_rotation = glm::radians(glm::mod(euler_rotation_degrees, 360.0f));

    {
        // Static also means that all [EntityElement] will share the value
        static bool lock_scale = true;

        glm::vec3 temp_scale = scale;
        if (ImGui::DragFloat3("Scale", &temp_scale[0], 0.01f)) {
            transformUpdated = true;

            if (lock_scale) {
                // Assume only channel can change at a time (I think this is true based on how ImGui works?)
                if (temp_scale.x != scale.x) {
                    if (scale.x == 0.0f) {
                        scale = glm::vec3(temp_scale.x);
                    } else {
                        scale *= temp_scale.x / scale.x;
                    }
                } else if (temp_scale.y != scale.y) {
                    if (scale.y == 0.0f) {
                        scale = glm::vec3(temp_scale.y);
                    } else {
                        scale *= temp_scale.y / scale.y;
                    }
                } else if (temp_scale.z != scale.z) {
                    if (scale.z == 0.0f) {
                        scale = glm::vec3(temp_scale.z);
                    } else {
                        scale *= temp_scale.z / scale.z;
                    }
                }
            } else {
                scale = temp_scale;
            }
        }

        ImGui::SameLine();
        ImGui::Checkbox("[Lock]", &lock_scale);
    }

    // UI to control keyframe animation
    ImGui::Text("Keyframe Animation");
    if (ImGui::Button("Add Keyframe")) {
        add_keyframe_at_current_time();
        transformUpdated = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Keyframes")) {
        clear_keyframes();
        transformUpdated = true;
    }

    // Show keyframe controls if there are any keyframes
    if (!keyframes.empty()) {
        const float duration = keyframe_duration();
        if (duration > 0.0f) {
            if (ImGui::SliderFloat("Animation Time", &keyframe_time, 0.0f, duration, "%.3f")) {
                transformUpdated = true;
            }
        }
        
        // Only show play options if more than 2 keyframes
        if (keyframes.size() >= 2) {
            if (ImGui::Checkbox("Play", &keyframe_playing)) {
                if (keyframe_time > duration) {
                    keyframe_time = duration;
                }
            }
            ImGui::SameLine();
            ImGui::Checkbox("Loop", &keyframe_loop);

            float speed = keyframe_speed;
            if (ImGui::DragFloat("Speed", &speed, 0.1f, 0.0f, 10.0f)) {
                keyframe_speed = speed;
            }

            ImGui::Separator();
        }

        // Show controls for each keyframe
        for (size_t i = 0; i < keyframes.size(); ++i) {
            auto& keyframe = keyframes[i];
            std::string node_label = std::string("Keyframe ") + std::to_string(i);
            if (ImGui::TreeNode(node_label.c_str())) {
                if (ImGui::InputFloat(("Time##" + std::to_string(i)).c_str(), &keyframe.time_seconds, 0.0f, 0.0f, "%.3f")) {
                    transformUpdated = true;
                    sort_keyframes();
                }
                if (ImGui::DragFloat3(("Translation##" + std::to_string(i)).c_str(), &keyframe.position[0], 0.01f)) {
                    transformUpdated = true;
                }
                glm::vec3 keyframe_rotation_degrees = glm::degrees(keyframe.euler_rotation);
                if (ImGui::DragFloat3(("Rotation##" + std::to_string(i)).c_str(), &keyframe_rotation_degrees[0])) {
                    keyframe.euler_rotation = glm::radians(glm::mod(keyframe_rotation_degrees, 360.0f));
                    transformUpdated = true;
                }
                if (ImGui::DragFloat3(("Scale##" + std::to_string(i)).c_str(), &keyframe.scale[0], 0.01f)) {
                    transformUpdated = true;
                }
                if (ImGui::Button(("Delete Keyframe##" + std::to_string(i)).c_str())) {
                    remove_keyframe_at(i);
                    transformUpdated = true;
                    ImGui::TreePop();
                    break;
                }
                ImGui::TreePop();
            }
        }
    }

    ImGui::Spacing();

    if (transformUpdated) {
        update_instance_data();
    }
}

glm::mat4 EditorScene::LocalTransformComponent::calc_model_matrix() const {
    glm::vec3 current_position = position;
    // Changed to quaternion to avoid gimbal lock issues when interpolating rotation between keyframes
    glm::quat current_rotation = quaternion_from_euler(euler_rotation);
    glm::vec3 current_scale = scale;

    // If there are keyframes, sample the animation at the current time and use that instead of the base transform
    if (!keyframes.empty()) {
        const auto [sampled_position, sampled_rotation, sampled_scale] = sample_keyframe_transform(keyframe_time);
        current_position = sampled_position;
        current_rotation = sampled_rotation;
        current_scale = sampled_scale;
    }

    return glm::translate(current_position) * glm::toMat4(current_rotation) * glm::scale(current_scale);
}

void EditorScene::LocalTransformComponent::update_local_transform_from_json(const json& json) {
    auto t = json["local_transform"];
    position = t["position"];
    euler_rotation = t["euler_rotation"];
    scale = t["scale"];

    // Load keyframes if they exist, and sort them to ensure they're in the correct order
    keyframes.clear();
    if (t.contains("keyframes")) {
        for (const auto& keyframe_json : t["keyframes"]) {
            LocalTransformKeyFrame keyframe;
            keyframe.time_seconds = keyframe_json["time_seconds"];
            keyframe.position = keyframe_json["position"];
            keyframe.euler_rotation = keyframe_json["euler_rotation"];
            keyframe.scale = keyframe_json["scale"];
            keyframes.push_back(keyframe);
        }
        sort_keyframes();
    }

    if (t.contains("keyframe_time")) {
        keyframe_time = t["keyframe_time"];
    }
    if (t.contains("keyframe_speed")) {
        keyframe_speed = t["keyframe_speed"];
    }
    if (t.contains("keyframe_playing")) {
        keyframe_playing = t["keyframe_playing"];
    }
    if (t.contains("keyframe_loop")) {
        keyframe_loop = t["keyframe_loop"];
    }
}

 // Convert keyframes into json array
json EditorScene::LocalTransformComponent::local_transform_into_json() const {
    json transform = json::object();

    transform["position"] = position;
    transform["euler_rotation"] = euler_rotation;
    transform["scale"] = scale;

    // Only save keyframe data if there are any
    if (!keyframes.empty()) {
        json keyframes_json = json::array();

        for (const auto& keyframe : keyframes) {
            keyframes_json.push_back({
                {"time_seconds", keyframe.time_seconds},
                {"position", keyframe.position},
                {"euler_rotation", keyframe.euler_rotation},
                {"scale", keyframe.scale}
            });
        }

        transform["keyframes"] = keyframes_json;
        transform["keyframe_time"] = keyframe_time;
        transform["keyframe_speed"] = keyframe_speed;
        transform["keyframe_playing"] = keyframe_playing;
        transform["keyframe_loop"] = keyframe_loop;
    }

    return { "local_transform", transform };
}

void EditorScene::LocalTransformComponent::update_keyframe_animation(float dt) {
    if (keyframes.size() < 2 || !keyframe_playing) {
        return;
    }

    const float duration = keyframe_duration();
    if (duration <= 0.0f) {
        return;
    }

    // Advance the keyframe time by the delta time multiplied by the speed
    keyframe_time += dt * keyframe_speed;
    if (keyframe_time > duration) {
        if (keyframe_loop) {
            keyframe_time = std::fmod(keyframe_time, duration);
            if (keyframe_time < 0.0f) {
                keyframe_time += duration;
            }
        } else {
            keyframe_time = duration;
            keyframe_playing = false;
        }
    } else if (keyframe_time < 0.0f) {
        keyframe_time = 0.0f;
        keyframe_playing = false;
    }
}

bool EditorScene::LocalTransformComponent::has_keyframe_animation() const {
    return keyframes.size() >= 2;
}

float EditorScene::LocalTransformComponent::keyframe_duration() const {
    if (keyframes.empty()) {
        return 0.0f;
    }
    return keyframes.back().time_seconds;
}

// Helper function to convert euler angles to quaternion, using the same convention as glm::eulerAngles (yaw around y, pitch around x, roll around z)
glm::quat EditorScene::LocalTransformComponent::quaternion_from_euler(const glm::vec3& euler) {
    return glm::normalize(
        glm::angleAxis(euler.z, glm::vec3(0.0f, 0.0f, 1.0f)) *
        glm::angleAxis(euler.y, glm::vec3(0.0f, 1.0f, 0.0f)) *
        glm::angleAxis(euler.x, glm::vec3(1.0f, 0.0f, 0.0f))
    );
}

// Sample the keyframe animation at the given time, returning the interpolated position, rotation (as a quaternion), and scale
std::tuple<glm::vec3, glm::quat, glm::vec3> EditorScene::LocalTransformComponent::sample_keyframe_transform(float time_seconds) const {
    if (keyframes.empty()) {
        return {position, quaternion_from_euler(euler_rotation), scale};
    }
    if (keyframes.size() == 1) {
        const auto& frame = keyframes[0];
        return {frame.position, quaternion_from_euler(frame.euler_rotation), frame.scale};
    }

    if (time_seconds <= keyframes.front().time_seconds) {
        const auto& frame = keyframes.front();
        return {frame.position, quaternion_from_euler(frame.euler_rotation), frame.scale};
    }

    if (time_seconds >= keyframes.back().time_seconds) {
        const auto& frame = keyframes.back();
        return {frame.position, quaternion_from_euler(frame.euler_rotation), frame.scale};
    }
    
    // Find the two keyframes that the current time is between and interpolate between them using slerp (spherical linear interpolation)
    for (size_t i = 1; i < keyframes.size(); ++i) {
        const auto& earlier = keyframes[i - 1];
        const auto& later = keyframes[i];
        if (time_seconds <= later.time_seconds) {
            float segment_duration = later.time_seconds - earlier.time_seconds;
            float blend = segment_duration > 0.0f ? (time_seconds - earlier.time_seconds) / segment_duration : 0.0f;
            glm::vec3 position = glm::mix(earlier.position, later.position, blend);
            glm::vec3 scale = glm::mix(earlier.scale, later.scale, blend);
            glm::quat rotation = glm::slerp(
                quaternion_from_euler(earlier.euler_rotation),
                quaternion_from_euler(later.euler_rotation),
                blend
            );
            return {position, rotation, scale};
        }
    }

    const auto& last_frame = keyframes.back();
    return {last_frame.position, quaternion_from_euler(last_frame.euler_rotation), last_frame.scale};
}

// Sort the keyframes in ascending order based on their time_seconds value
void EditorScene::LocalTransformComponent::sort_keyframes() {
    std::sort(keyframes.begin(), keyframes.end(), [](const LocalTransformKeyFrame& a, const LocalTransformKeyFrame& b) {
        return a.time_seconds < b.time_seconds;
    });
}

void EditorScene::LocalTransformComponent::add_keyframe_at_current_time() {
    LocalTransformKeyFrame keyframe;
    keyframe.time_seconds = keyframe_time;

    if (keyframes.empty()) {
        keyframe.position = position;
        keyframe.euler_rotation = euler_rotation;
        keyframe.scale = scale;
    } else {
        glm::vec3 sampled_position;
        glm::quat sampled_rotation;
        glm::vec3 sampled_scale;
        std::tie(sampled_position, sampled_rotation, sampled_scale) = sample_keyframe_transform(keyframe_time);
        keyframe.position = sampled_position;
        keyframe.euler_rotation = glm::eulerAngles(sampled_rotation);
        keyframe.scale = sampled_scale;
    }

    // If the current time already has a keyframe, add a new keyframe with + 0.1s to avoid conflict
    auto existing = std::find_if(keyframes.begin(), keyframes.end(), [this](const LocalTransformKeyFrame& candidate) {
        return candidate.time_seconds == keyframe_time;
    });
    if (existing != keyframes.end()) {
        keyframe.time_seconds = std::max(keyframe_time, keyframe_duration()) + 0.1f;
        keyframe_time = keyframe.time_seconds;
        keyframes.push_back(keyframe);
    } else {
        keyframes.push_back(keyframe);
    }
    sort_keyframes();
}

// Remove the keyframe at the given index
void EditorScene::LocalTransformComponent::remove_keyframe_at(size_t index) {
    if (index < keyframes.size()) {
        keyframes.erase(keyframes.begin() + index);
    }
}

// Remove all keyframes and reset animation state
void EditorScene::LocalTransformComponent::clear_keyframes() {
    keyframes.clear();
    keyframe_time = 0.0f;
    keyframe_playing = false;
}

void EditorScene::LitMaterialComponent::add_material_imgui_edit_section(MasterRenderScene& /*render_scene*/, const SceneContext& /*scene_context*/) {
    // Set this to true if the user has changed any of the material values, otherwise the changes won't be propagated
    bool material_changed = false;
    ImGui::Text("Material");

    // Add UI controls here

    ImGui::Spacing();
    if (material_changed) {
        update_instance_data();
    }
}

void EditorScene::LitMaterialComponent::update_material_from_json(const json& json) {
    auto m = json["material"];
    material.diffuse_tint = m["diffuse_tint"];
    material.specular_tint = m["specular_tint"];
    material.ambient_tint = m["ambient_tint"];
    material.shininess = m["shininess"];
}

json EditorScene::LitMaterialComponent::material_into_json() const {
    return {"material", {
        {"diffuse_tint", material.diffuse_tint},
        {"specular_tint", material.specular_tint},
        {"ambient_tint", material.ambient_tint},
        {"shininess", material.shininess},
    }};
}

void EditorScene::EmissiveMaterialComponent::add_emissive_material_imgui_edit_section(MasterRenderScene& /*render_scene*/, const SceneContext& /*scene_context*/) {
    // Set this to true if the user has changed any of the material values, otherwise the changes won't be propagated
    bool material_changed = false;
    ImGui::Text("Emissive Material");

    // Add UI controls here

    ImGui::Spacing();
    if (material_changed) {
        update_instance_data();
    }
}

void EditorScene::EmissiveMaterialComponent::update_emissive_material_from_json(const json& json) {
    auto m = json["material"];
    material.emission_tint = m["emission_tint"];
}

json EditorScene::EmissiveMaterialComponent::emissive_material_into_json() const {
    return {"material", {
        {"emission_tint", material.emission_tint},
    }};
}

void EditorScene::AnimationComponent::add_animation_imgui_edit_section(MasterRenderScene& render_scene, const SceneContext& /*scene_context*/) {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    auto entity = get_entity();
    const auto& animations = entity->get_animations();

    ImGui::Text("Animation");
    std::string selected_animation = "[NONE]";
    double ticks_per_second = 1.0;
    double duration_ticks = 0.0;
    if (get_animation_parameters().animation_id != NONE_ANIMATION) {
        std::tie(selected_animation, ticks_per_second, duration_ticks) = animations[get_animation_parameters().animation_id];
    }
    if (ImGui::BeginCombo("Animation Selection", selected_animation.c_str(), 0)) {
        for (auto i = 0u; i < animations.size(); ++i) {
            const auto& animation = animations[i];
            const bool is_selected = i == get_animation_parameters().animation_id;
            if (ImGui::Selectable(std::get<0>(animation).c_str(), is_selected)) {
                render_scene.animator.stop(entity);
                get_animation_parameters().animation_id = i;
                entity->get_animation_time_seconds() = 0.0;
            }

            // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        if (ImGui::Selectable("[NONE]", get_animation_parameters().animation_id == NONE_ANIMATION)) {
            render_scene.animator.stop(entity);
            get_animation_parameters().animation_id = NONE_ANIMATION;
            entity->get_animation_time_seconds() = 0.0;
        }
        ImGui::EndCombo();

        entity->get_animation_id() = get_animation_parameters().animation_id;
    }
    if (get_animation_parameters().animation_id != NONE_ANIMATION) {
        std::tie(selected_animation, ticks_per_second, duration_ticks) = animations[get_animation_parameters().animation_id];

        auto float_time = (float) entity->get_animation_time_seconds();
        auto float_duration = (float) (duration_ticks / ticks_per_second);
        if (ImGui::SliderFloat("Animation Time (sec)", &float_time, 0.0f, float_duration, "%.3f", ImGuiSliderFlags_NoRoundToFormat)) {
            entity->get_animation_time_seconds() = float_time;
        }

        bool is_playing = render_scene.animator.is_animating(entity).has_value();

        if (ImGui::Button("Start")) {
            render_scene.animator.start(entity, get_animation_parameters());
        }

        ImGui::SameLine();

        if (!is_playing) ImGui::BeginDisabled();
        if (ImGui::Button("Pause")) {
            render_scene.animator.pause(entity);
        }
        if (!is_playing) ImGui::EndDisabled();

        ImGui::SameLine();

        if (ImGui::Button("Resume")) {
            render_scene.animator.resume(entity, get_animation_parameters());
        }

        ImGui::SameLine();

        if (ImGui::Button("Stop")) {
            render_scene.animator.stop(entity);
        }

        ImGui::SameLine();

        if (ImGui::Checkbox("Loop", &get_animation_parameters().loop) && is_playing) {
            render_scene.animator.update_param(entity, get_animation_parameters());
        }

        auto float_speed = (float) get_animation_parameters().speed;
        if (ImGui::SliderFloat("Speed", &float_speed, 0.0, 10.0)) {
            get_animation_parameters().speed = float_speed;
            if (is_playing) {
                render_scene.animator.update_param(entity, get_animation_parameters());
            }
        }
    }
}

bool EditorScene::is_null(const ElementRef& ref) {
    return *((const void**) &ref) == nullptr;
}

bool EditorScene::eq(const ElementRef& e1, const ElementRef& e2) {
    return *((const void**) &e1) == *((const void**) &e2);
}
