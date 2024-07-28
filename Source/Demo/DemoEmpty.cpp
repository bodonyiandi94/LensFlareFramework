#include "PCH.h"
#include "Demo.h"

namespace DemoEmpty
{
	////////////////////////////////////////////////////////////////////////////////
	// @CONSOLE_VAR(Object Groups, -object_group, Scene_Empty)
	static const std::string SCENE_NAME = "Empty";
	static const std::string OBJECT_GROUP_NAME = "Scene_" + SCENE_NAME;

	////////////////////////////////////////////////////////////////////////////////
	void setupAnimationObject(Scene::Object& object, Scene::Object& camera, std::string const& cameraName)
	{
		object.component<KeyframedAnim::KeyFramedAnimComponent>().m_animFileName = "CameraAnim_" + cameraName;
		object.component<KeyframedAnim::KeyFramedAnimComponent>().m_length = 5.0f;
		object.component<KeyframedAnim::KeyFramedAnimComponent>().m_playbackStart = 0.5f;
		object.component<KeyframedAnim::KeyFramedAnimComponent>().m_playbackSpeed = 0.5f;
		object.component<KeyframedAnim::KeyFramedAnimComponent>().m_playDirection = KeyframedAnim::KeyFramedAnimComponent::Forward;
		object.component<KeyframedAnim::KeyFramedAnimComponent>().m_playbackType = KeyframedAnim::KeyFramedAnimComponent::Synced;
		object.component<KeyframedAnim::KeyFramedAnimComponent>().m_recordPlayback = true;
		//object.component<KeyframedAnim::KeyFramedAnimComponent>().m_playbackType = KeyframedAnim::KeyFramedAnimComponent::PlayType::RealTime;
		//object.component<KeyframedAnim::KeyFramedAnimComponent>().m_recordPlayback = false;
		object.component<KeyframedAnim::KeyFramedAnimComponent>().m_looping = false;
		object.component<KeyframedAnim::KeyFramedAnimComponent>().m_generateWhileStopped = false;
		object.component<KeyframedAnim::KeyFramedAnimComponent>().m_record.m_timePerFrame = 1.0f / 60.0f;
		object.component<KeyframedAnim::KeyFramedAnimComponent>().m_record.m_linearThreshold = 0.0f;
		object.component<KeyframedAnim::KeyFramedAnimComponent>().m_tracks =
		{
			{
				"Orientation (X)",
				KeyframedAnim::KeyFramedAnimTrack
				{
					// Interpolation method						
					KeyframedAnim::KeyFramedAnimTrack::Linear,

					// Variables
					{ { "Coordinate", &camera.component<Transform::TransformComponent>().m_orientation.x }, },

					// Frames
					{
						KeyframedAnim::KeyFramedAnimNode{ 0.0f, 19.0f },
						KeyframedAnim::KeyFramedAnimNode{ 1.0f, 11.0f },
					}
				}
			},
			{
				"Orientation (Y)",
				KeyframedAnim::KeyFramedAnimTrack
				{
					// Interpolation method						
					KeyframedAnim::KeyFramedAnimTrack::Linear,

					// Variables
					{ { "Coordinate", &camera.component<Transform::TransformComponent>().m_orientation.y }, },

					// Frames
					{
						KeyframedAnim::KeyFramedAnimNode{ 0.0f, 19.0f },
						KeyframedAnim::KeyFramedAnimNode{ 1.0f, 11.0f },
					}
				}
			}
		};
	}

	////////////////////////////////////////////////////////////////////////////////
	void initDemoScene(Scene::Scene& scene)
	{
		// Extract some of the used objects
		Scene::Object* renderSettings = Scene::findFirstObject(scene, Scene::OBJECT_TYPE_RENDER_SETTINGS);

		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Cameras
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		// Add the camera.
		// @CONSOLE_VAR(Camera, -camera, EmptyCamera)
		auto& camera = createObject(scene, "EmptyCamera", Scene::OBJECT_TYPE_CAMERA,
			Scene::extendDefaultObjectInitializerBefore([](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = true;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);
			object.component<EditorSettings::EditorSettingsComponent>().m_singular = true;

			object.component<Transform::TransformComponent>().m_position = glm::vec3(0.0f, 0.0f, 0.0f);
			object.component<Transform::TransformComponent>().m_orientation = glm::radians(glm::vec3(30.057f, 267.082f, 0.0f));
			Demo::commonCameraProperties(scene, object, false);
		}));

		/*
		auto& cameraOrthogonal = createObject(scene, "Camera (Orthogonal)", Scene::OBJECT_TYPE_CAMERA,
			Scene::extendDefaultObjectInitializerBefore([](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = true;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);
			object.component<EditorSettings::EditorSettingsComponent>().m_singular = true;

			object.component<Transform::TransformComponent>().m_position = glm::vec3(0.0f, 0.0f, 0.0f);
			object.component<Transform::TransformComponent>().m_orientation = glm::radians(glm::vec3(25.0f, 275.0f, 0.0f));
			Demo::commonCameraProperties(scene, object, true);
		}));

		auto& cameraTiledHeliar = createObject(scene, "Camera (Tiled - Heliar)", Scene::OBJECT_TYPE_CAMERA,
			Scene::extendDefaultObjectInitializerBefore([](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = true;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);
			object.component<EditorSettings::EditorSettingsComponent>().m_singular = true;

			object.component<Transform::TransformComponent>().m_position = glm::vec3(0.0f, 0.0f, 0.0f);
			object.component<Transform::TransformComponent>().m_orientation = glm::radians(glm::vec3(30.057f, 267.082f, 0.0f));
			Demo::commonCameraProperties(scene, object, true);
		}));

		auto& cameraTiledItohMid = createObject(scene, "Camera (Tiled - Itoh Mid)", Scene::OBJECT_TYPE_CAMERA,
			Scene::extendDefaultObjectInitializerBefore([](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = true;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);
			object.component<EditorSettings::EditorSettingsComponent>().m_singular = true;

			object.component<Transform::TransformComponent>().m_position = glm::vec3(0.0f, 0.0f, 0.0f);
			object.component<Transform::TransformComponent>().m_orientation = glm::radians(glm::vec3(22.628f, 280.710f, 0.0f));
			Demo::commonCameraProperties(scene, object, true);
		}));

		auto& cameraTiledNikonMid = createObject(scene, "Camera (Tiled - Nikon Mid)", Scene::OBJECT_TYPE_CAMERA,
			Scene::extendDefaultObjectInitializerBefore([](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = true;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);
			object.component<EditorSettings::EditorSettingsComponent>().m_singular = true;

			object.component<Transform::TransformComponent>().m_position = glm::vec3(0.0f, 0.0f, 0.0f);
			object.component<Transform::TransformComponent>().m_orientation = glm::radians(glm::vec3(20.808f, 283.682f, 0.0f));
			Demo::commonCameraProperties(scene, object, true);
		}));

		auto& cameraTiledCanonMid = createObject(scene, "Camera (Tiled - Canon Mid)", Scene::OBJECT_TYPE_CAMERA,
			Scene::extendDefaultObjectInitializerBefore([](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = true;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);
			object.component<EditorSettings::EditorSettingsComponent>().m_singular = true;

			object.component<Transform::TransformComponent>().m_position = glm::vec3(0.0f, 0.0f, 0.0f);
			object.component<Transform::TransformComponent>().m_orientation = glm::radians(glm::vec3(20.993f, 285.840f, 0.0f));
			Demo::commonCameraProperties(scene, object, true);
		}));

		auto& cameraTiledCanonLong = createObject(scene, "Camera (Tiled - Canon Long)", Scene::OBJECT_TYPE_CAMERA,
			Scene::extendDefaultObjectInitializerBefore([](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = true;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);
			object.component<EditorSettings::EditorSettingsComponent>().m_singular = true;

			object.component<Transform::TransformComponent>().m_position = glm::vec3(0.0f, 0.0f, 0.0f);
			object.component<Transform::TransformComponent>().m_orientation = glm::radians(glm::vec3(29.594f, 282.128f, 0.0f));
			Demo::commonCameraProperties(scene, object, true);
		}));
		*/

		auto& cameraPolynomialHeliar = createObject(scene, "Camera (Polynomial - Heliar)", Scene::OBJECT_TYPE_CAMERA,
			Scene::extendDefaultObjectInitializerBefore([](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = true;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);
			object.component<EditorSettings::EditorSettingsComponent>().m_singular = true;

			object.component<Transform::TransformComponent>().m_position = glm::vec3(0.0f, 0.0f, 0.0f);
			object.component<Transform::TransformComponent>().m_orientation = glm::radians(glm::vec3(-381.000f, -272.000f, 0.0f));
			Demo::commonCameraProperties(scene, object, true);
		}));

		auto& cameraPolynomialGaussian = createObject(scene, "Camera (Polynomial - Double Gaussian)", Scene::OBJECT_TYPE_CAMERA,
			Scene::extendDefaultObjectInitializerBefore([](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = true;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);
			object.component<EditorSettings::EditorSettingsComponent>().m_singular = true;

			object.component<Transform::TransformComponent>().m_position = glm::vec3(0.0f, 0.0f, 0.0f);
			object.component<Transform::TransformComponent>().m_orientation = glm::radians(glm::vec3(-387.000f, -270.500f, 0.0f));
			Demo::commonCameraProperties(scene, object, true);
		}));

		auto& cameraPolynomialItohLong = createObject(scene, "Camera (Polynomial - Itoh Long)", Scene::OBJECT_TYPE_CAMERA,
			Scene::extendDefaultObjectInitializerBefore([](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = true;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);
			object.component<EditorSettings::EditorSettingsComponent>().m_singular = true;

			object.component<Transform::TransformComponent>().m_position = glm::vec3(0.0f, 0.0f, 0.0f);
			object.component<Transform::TransformComponent>().m_orientation = glm::radians(glm::vec3(-386.000f, -268.000f, 0.0f));
			Demo::commonCameraProperties(scene, object, true);
		}));

		auto& cameraPolynomialNikonLong = createObject(scene, "Camera (Polynomial - Nikon Long)", Scene::OBJECT_TYPE_CAMERA,
			Scene::extendDefaultObjectInitializerBefore([](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = true;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);
			object.component<EditorSettings::EditorSettingsComponent>().m_singular = true;

			object.component<Transform::TransformComponent>().m_position = glm::vec3(0.0f, 0.0f, 0.0f);
			object.component<Transform::TransformComponent>().m_orientation = glm::radians(glm::vec3(-153.000f, -95.000f, 0.0f));
			Demo::commonCameraProperties(scene, object, true);
		}));

		auto& cameraPolynomialCanonMid = createObject(scene, "Camera (Polynomial - Canon Mid)", Scene::OBJECT_TYPE_CAMERA,
			Scene::extendDefaultObjectInitializerBefore([](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = true;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);
			object.component<EditorSettings::EditorSettingsComponent>().m_singular = true;

			object.component<Transform::TransformComponent>().m_position = glm::vec3(0.0f, 0.0f, 0.0f);
			object.component<Transform::TransformComponent>().m_orientation = glm::radians(glm::vec3(-154.000f, -79.500f, 0.0f));
			Demo::commonCameraProperties(scene, object, true);
		}));

		auto& cameraPolynomialAnimation = createObject(scene, "Camera (Polynomial - Animation)", Scene::OBJECT_TYPE_CAMERA,
			Scene::extendDefaultObjectInitializerBefore([](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = true;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);
			object.component<EditorSettings::EditorSettingsComponent>().m_singular = true;

			object.component<Transform::TransformComponent>().m_position = glm::vec3(0.0f, 0.0f, 0.0f);
			object.component<Transform::TransformComponent>().m_orientation = glm::radians(glm::vec3(-155.000f, -85.000f, 0.0f));
			Demo::commonCameraProperties(scene, object, false);
		}));

		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// LIGHTING
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		// Add the main light source
		auto& directionalLight = createObject(scene, "EmptyLightSource", Scene::OBJECT_TYPE_DIRECTIONAL_LIGHT,
			Scene::extendDefaultObjectInitializerBefore([](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = true;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);

			object.component<Transform::TransformComponent>().m_orientation = glm::vec3(glm::radians(-155.0f), glm::radians(-85.0f), glm::radians(0.0f));
			object.component<DirectionalLight::DirectionalLightComponent>().m_color = glm::vec3(1.0f);
			object.component<DirectionalLight::DirectionalLightComponent>().m_ambientIntensity = 0.0f;
			object.component<DirectionalLight::DirectionalLightComponent>().m_diffuseIntensity = 20.0f;
			object.component<DirectionalLight::DirectionalLightComponent>().m_specularItensity = 20.0f;
			object.component<ShadowMap::ShadowMapComponent>().m_castsShadow = false;
			object.component<ShadowMap::ShadowMapComponent>().m_resolution = GPU::shadowMapResolution();
			object.component<ShadowMap::ShadowMapComponent>().m_polygonOffsetConstant = 4.0f;
			object.component<ShadowMap::ShadowMapComponent>().m_polygonOffsetLinear = 1.0f;
			object.component<ShadowMap::ShadowMapComponent>().m_precision = ShadowMap::ShadowMapComponent::F32;
			object.component<ShadowMap::ShadowMapComponent>().m_algorithm = ShadowMap::ShadowMapComponent::Moments;
			object.component<ShadowMap::ShadowMapComponent>().m_depthBias = 0.5f;
			object.component<ShadowMap::ShadowMapComponent>().m_momentsBias = 0.001f;
			object.component<ShadowMap::ShadowMapComponent>().m_minVariance = 0.01f;
			object.component<ShadowMap::ShadowMapComponent>().m_lightBleedBias = 0.1f;
			object.component<ShadowMap::ShadowMapComponent>().m_exponentialConstants = glm::vec2(50.0f, 10.0f);
			object.component<ShadowMap::ShadowMapComponent>().m_blurStrength = 5.0f;
		}));

		/*
		// Add the main light source
		auto& directionalLightHeliar = createObject(scene, "EmptyLightSource (Tiled - Heliar)", Scene::OBJECT_TYPE_DIRECTIONAL_LIGHT,
			Scene::extendDefaultObjectInitializerBefore([](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = false;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);

			object.component<Transform::TransformComponent>().m_orientation = glm::vec3(glm::radians(-174.500f), glm::radians(-101.500f), glm::radians(0.0f));
			object.component<DirectionalLight::DirectionalLightComponent>().m_color = glm::vec3(1.0f);
			object.component<DirectionalLight::DirectionalLightComponent>().m_ambientIntensity = 0.0f;
			object.component<DirectionalLight::DirectionalLightComponent>().m_diffuseIntensity = 20.0f;
			object.component<DirectionalLight::DirectionalLightComponent>().m_specularItensity = 20.0f;
			object.component<ShadowMap::ShadowMapComponent>().m_castsShadow = false;
			object.component<ShadowMap::ShadowMapComponent>().m_resolution = GPU::shadowMapResolution();
			object.component<ShadowMap::ShadowMapComponent>().m_polygonOffsetConstant = 4.0f;
			object.component<ShadowMap::ShadowMapComponent>().m_polygonOffsetLinear = 1.0f;
			object.component<ShadowMap::ShadowMapComponent>().m_precision = ShadowMap::ShadowMapComponent::F32;
			object.component<ShadowMap::ShadowMapComponent>().m_algorithm = ShadowMap::ShadowMapComponent::Moments;
			object.component<ShadowMap::ShadowMapComponent>().m_depthBias = 0.5f;
			object.component<ShadowMap::ShadowMapComponent>().m_momentsBias = 0.001f;
			object.component<ShadowMap::ShadowMapComponent>().m_minVariance = 0.01f;
			object.component<ShadowMap::ShadowMapComponent>().m_lightBleedBias = 0.1f;
			object.component<ShadowMap::ShadowMapComponent>().m_exponentialConstants = glm::vec2(50.0f, 10.0f);
			object.component<ShadowMap::ShadowMapComponent>().m_blurStrength = 5.0f;
		}));

		// Add the main light source
		auto& directionalLightItohMid = createObject(scene, "EmptyLightSource (Tiled - Itoh Mid)", Scene::OBJECT_TYPE_DIRECTIONAL_LIGHT,
			Scene::extendDefaultObjectInitializerBefore([](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = false;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);

			object.component<Transform::TransformComponent>().m_orientation = glm::vec3(glm::radians(-149.600f), glm::radians(-66.100f), glm::radians(0.0f));
			object.component<DirectionalLight::DirectionalLightComponent>().m_color = glm::vec3(1.0f);
			object.component<DirectionalLight::DirectionalLightComponent>().m_ambientIntensity = 0.0f;
			object.component<DirectionalLight::DirectionalLightComponent>().m_diffuseIntensity = 20.0f;
			object.component<DirectionalLight::DirectionalLightComponent>().m_specularItensity = 20.0f;
			object.component<ShadowMap::ShadowMapComponent>().m_castsShadow = false;
			object.component<ShadowMap::ShadowMapComponent>().m_resolution = GPU::shadowMapResolution();
			object.component<ShadowMap::ShadowMapComponent>().m_polygonOffsetConstant = 4.0f;
			object.component<ShadowMap::ShadowMapComponent>().m_polygonOffsetLinear = 1.0f;
			object.component<ShadowMap::ShadowMapComponent>().m_precision = ShadowMap::ShadowMapComponent::F32;
			object.component<ShadowMap::ShadowMapComponent>().m_algorithm = ShadowMap::ShadowMapComponent::Moments;
			object.component<ShadowMap::ShadowMapComponent>().m_depthBias = 0.5f;
			object.component<ShadowMap::ShadowMapComponent>().m_momentsBias = 0.001f;
			object.component<ShadowMap::ShadowMapComponent>().m_minVariance = 0.01f;
			object.component<ShadowMap::ShadowMapComponent>().m_lightBleedBias = 0.1f;
			object.component<ShadowMap::ShadowMapComponent>().m_exponentialConstants = glm::vec2(50.0f, 10.0f);
			object.component<ShadowMap::ShadowMapComponent>().m_blurStrength = 5.0f;
		}));

		// Add the main light source
		auto& directionalLightNikonMid = createObject(scene, "EmptyLightSource (Tiled - Nikon Mid)", Scene::OBJECT_TYPE_DIRECTIONAL_LIGHT,
			Scene::extendDefaultObjectInitializerBefore([](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = false;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);

			object.component<Transform::TransformComponent>().m_orientation = glm::vec3(glm::radians(-160.900f), glm::radians(-87.100f), glm::radians(0.0f));
			object.component<DirectionalLight::DirectionalLightComponent>().m_color = glm::vec3(1.0f);
			object.component<DirectionalLight::DirectionalLightComponent>().m_ambientIntensity = 0.0f;
			object.component<DirectionalLight::DirectionalLightComponent>().m_diffuseIntensity = 20.0f;
			object.component<DirectionalLight::DirectionalLightComponent>().m_specularItensity = 20.0f;
			object.component<ShadowMap::ShadowMapComponent>().m_castsShadow = false;
			object.component<ShadowMap::ShadowMapComponent>().m_resolution = GPU::shadowMapResolution();
			object.component<ShadowMap::ShadowMapComponent>().m_polygonOffsetConstant = 4.0f;
			object.component<ShadowMap::ShadowMapComponent>().m_polygonOffsetLinear = 1.0f;
			object.component<ShadowMap::ShadowMapComponent>().m_precision = ShadowMap::ShadowMapComponent::F32;
			object.component<ShadowMap::ShadowMapComponent>().m_algorithm = ShadowMap::ShadowMapComponent::Moments;
			object.component<ShadowMap::ShadowMapComponent>().m_depthBias = 0.5f;
			object.component<ShadowMap::ShadowMapComponent>().m_momentsBias = 0.001f;
			object.component<ShadowMap::ShadowMapComponent>().m_minVariance = 0.01f;
			object.component<ShadowMap::ShadowMapComponent>().m_lightBleedBias = 0.1f;
			object.component<ShadowMap::ShadowMapComponent>().m_exponentialConstants = glm::vec2(50.0f, 10.0f);
			object.component<ShadowMap::ShadowMapComponent>().m_blurStrength = 5.0f;
		}));

		// Add the main light source
		auto& directionalLightCanonMid = createObject(scene, "EmptyLightSource (Tiled - Canon Mid)", Scene::OBJECT_TYPE_DIRECTIONAL_LIGHT,
			Scene::extendDefaultObjectInitializerBefore([](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = false;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);

			object.component<Transform::TransformComponent>().m_orientation = glm::vec3(glm::radians(-169.300f), glm::radians(-89.900f), glm::radians(0.0f));
			object.component<DirectionalLight::DirectionalLightComponent>().m_color = glm::vec3(1.0f);
			object.component<DirectionalLight::DirectionalLightComponent>().m_ambientIntensity = 0.0f;
			object.component<DirectionalLight::DirectionalLightComponent>().m_diffuseIntensity = 20.0f;
			object.component<DirectionalLight::DirectionalLightComponent>().m_specularItensity = 20.0f;
			object.component<ShadowMap::ShadowMapComponent>().m_castsShadow = false;
			object.component<ShadowMap::ShadowMapComponent>().m_resolution = GPU::shadowMapResolution();
			object.component<ShadowMap::ShadowMapComponent>().m_polygonOffsetConstant = 4.0f;
			object.component<ShadowMap::ShadowMapComponent>().m_polygonOffsetLinear = 1.0f;
			object.component<ShadowMap::ShadowMapComponent>().m_precision = ShadowMap::ShadowMapComponent::F32;
			object.component<ShadowMap::ShadowMapComponent>().m_algorithm = ShadowMap::ShadowMapComponent::Moments;
			object.component<ShadowMap::ShadowMapComponent>().m_depthBias = 0.5f;
			object.component<ShadowMap::ShadowMapComponent>().m_momentsBias = 0.001f;
			object.component<ShadowMap::ShadowMapComponent>().m_minVariance = 0.01f;
			object.component<ShadowMap::ShadowMapComponent>().m_lightBleedBias = 0.1f;
			object.component<ShadowMap::ShadowMapComponent>().m_exponentialConstants = glm::vec2(50.0f, 10.0f);
			object.component<ShadowMap::ShadowMapComponent>().m_blurStrength = 5.0f;
		}));

		// Add the main light source
		auto& directionalLightCanonLong = createObject(scene, "EmptyLightSource (Tiled - Canon Long)", Scene::OBJECT_TYPE_DIRECTIONAL_LIGHT,
			Scene::extendDefaultObjectInitializerBefore([](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = false;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);

			object.component<Transform::TransformComponent>().m_orientation = glm::vec3(glm::radians(-143.800f), glm::radians(-68.200f), glm::radians(0.0f));
			object.component<DirectionalLight::DirectionalLightComponent>().m_color = glm::vec3(1.0f);
			object.component<DirectionalLight::DirectionalLightComponent>().m_ambientIntensity = 0.0f;
			object.component<DirectionalLight::DirectionalLightComponent>().m_diffuseIntensity = 20.0f;
			object.component<DirectionalLight::DirectionalLightComponent>().m_specularItensity = 20.0f;
			object.component<ShadowMap::ShadowMapComponent>().m_castsShadow = false;
			object.component<ShadowMap::ShadowMapComponent>().m_resolution = GPU::shadowMapResolution();
			object.component<ShadowMap::ShadowMapComponent>().m_polygonOffsetConstant = 4.0f;
			object.component<ShadowMap::ShadowMapComponent>().m_polygonOffsetLinear = 1.0f;
			object.component<ShadowMap::ShadowMapComponent>().m_precision = ShadowMap::ShadowMapComponent::F32;
			object.component<ShadowMap::ShadowMapComponent>().m_algorithm = ShadowMap::ShadowMapComponent::Moments;
			object.component<ShadowMap::ShadowMapComponent>().m_depthBias = 0.5f;
			object.component<ShadowMap::ShadowMapComponent>().m_momentsBias = 0.001f;
			object.component<ShadowMap::ShadowMapComponent>().m_minVariance = 0.01f;
			object.component<ShadowMap::ShadowMapComponent>().m_lightBleedBias = 0.1f;
			object.component<ShadowMap::ShadowMapComponent>().m_exponentialConstants = glm::vec2(50.0f, 10.0f);
			object.component<ShadowMap::ShadowMapComponent>().m_blurStrength = 5.0f;
		}));
		*/

		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// ANIMATION
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		// Add the camera parameter animation object
		auto& cameraAnimHeliar = createObject(scene, "Camera Animation [Heliar]", Scene::OBJECT_TYPE_KEYFRAMED_ANIM,
			Scene::extendDefaultObjectInitializerBefore([&](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = true;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);

			setupAnimationObject(object, cameraPolynomialAnimation, "heliar");
		}));

		// Add the camera parameter animation object
		auto& cameraAnimDoubleGauss = createObject(scene, "Camera Animation [Double Gauss]", Scene::OBJECT_TYPE_KEYFRAMED_ANIM,
			Scene::extendDefaultObjectInitializerBefore([&](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = true;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);

			setupAnimationObject(object, cameraPolynomialAnimation, "double_gauss");
		}));

		// Add the camera parameter animation object
		auto& cameraAnimItoh = createObject(scene, "Camera Animation [Itoh]", Scene::OBJECT_TYPE_KEYFRAMED_ANIM,
			Scene::extendDefaultObjectInitializerBefore([&](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = true;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);

			setupAnimationObject(object, cameraPolynomialAnimation, "itoh");
		}));

		// Add the camera parameter animation object
		auto& cameraAnimNikon = createObject(scene, "Camera Animation [Nikon]", Scene::OBJECT_TYPE_KEYFRAMED_ANIM,
			Scene::extendDefaultObjectInitializerBefore([&](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = true;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);

			setupAnimationObject(object, cameraPolynomialAnimation, "nikon");
		}));

		// Add the camera parameter animation object
		auto& cameraAnimCanon = createObject(scene, "Camera Animation [Canon]", Scene::OBJECT_TYPE_KEYFRAMED_ANIM,
			Scene::extendDefaultObjectInitializerBefore([&](Scene::Scene& scene, Scene::Object& object)
		{
			object.m_enabled = true;
			object.m_groups = SimulationSettings::makeGroupFlags(scene, OBJECT_GROUP_NAME);

			setupAnimationObject(object, cameraPolynomialAnimation, "canon");
		}));

		// Disable the voxel GI for this scene
		auto voxelGI = Scene::findFirstObject(scene, OBJECT_GROUP_NAME, Scene::OBJECT_TYPE_VOXEL_GLOBAL_ILLUMINATION);
		if (voxelGI) voxelGI->m_enabled = false;
	}

	////////////////////////////////////////////////////////////////////////////////
	STATIC_INITIALIZER()
	{
		REGISTER_DEMO_SCENE();
	};
}