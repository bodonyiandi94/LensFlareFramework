#pragma once

////////////////////////////////////////////////////////////////////////////////
//  Headers
////////////////////////////////////////////////////////////////////////////////

#include "PCH.h"
#include "Common.h"
#include "ShadowMap.h"

namespace PointLight
{
	////////////////////////////////////////////////////////////////////////////////
	/** Component and display name. */
	static constexpr const char* COMPONENT_NAME = "PointLightGrid";
	static constexpr const char* DISPLAY_NAME = "Point Light Grid";
	static constexpr const char* CATEGORY = "Lighting";

	////////////////////////////////////////////////////////////////////////////////
	// Number of light sources to render per batch
	static const size_t LIGHT_SOURCES_PER_BATCH = 64;

	////////////////////////////////////////////////////////////////////////////////
	/** Represents a single point light in the grid. */
	struct PointLightSource
	{
		// Location of the light source in the light grid
		glm::ivec3 m_gridLocation;

		// World-space location of the source
		glm::vec3 m_lightLocation;

		// Color of the light source
		glm::vec3 m_color;

		// Diffuse intensity.
		float m_diffuseIntensity = 1.0f;

		// Specular intensity.
		float m_specularItensity = 1.0f;

		// Light radius.
		float m_radius = 1.0f;
	};

	////////////////////////////////////////////////////////////////////////////////
	/** A point light grid component. */
	struct PointLightGridComponent
	{
		// Grid layout method
		meta_enum(GridLayoutMethod, int, Regular, Custom);

		// Backface culling mode
		meta_enum(AttenuationMethod, int, Quadratic, Realistic);

		// Which grid layout method to use
		GridLayoutMethod m_gridLayout = Regular;

		// Number of light sources along the individual axes.
		glm::ivec3 m_numLightSources{ 1, 1, 1 };

		// Size of the grid
		glm::vec3 m_gridSize;

		// Positions of the light sources on the custom grid
		std::vector<glm::vec3> m_customLightPositions;

		// Light color.
		glm::vec3 m_color{ 1.0f };

		// Diffuse intensity.
		float m_diffuseIntensity = 1.0f;

		// Specular intensity.
		float m_specularItensity = 1.0f;

		// Light radius.
		float m_radius = 1.0f;

		// Which attenuation method to use
		AttenuationMethod m_attenuationMethod = Realistic;

		// Attenuation factor (constant, linear, quadratic)
		//
		// SOURCE: http://wiki.ogre3d.org/tiki-index.php?page=-Point+Light+Attenuation
		//  Range   Constant     Linear     Quadratic
		//  3250,        1.0,    0.0014,     0.000007
		//   600,        1.0,    0.007,      0.0002
		//   325,        1.0,    0.014,      0.0007
		//   200,        1.0,    0.022,      0.0019
		//   160,        1.0,    0.027,      0.0028
		//   100,        1.0,    0.045,      0.0075
		//    65,        1.0,    0.07,       0.017
		//    50,        1.0,    0.09,       0.032
		//    32,        1.0,    0.14,       0.07
		//    20,        1.0,    0.22,       0.20
		//    13,        1.0,    0.35,       0.44
		//     7,        1.0,    0.7,        1.8     
		glm::vec3 m_attenuationFactor = glm::vec3(1.0f);

		// ---- Private members

		// A list of all the light sources in the grid
		std::vector<PointLightSource> m_lightSources;
	};

	////////////////////////////////////////////////////////////////////////////////
	/** Uniform buffer for the point light data. */
	struct alignas(sizeof(glm::vec4)) UniformDataLightSource
	{
		struct alignas(sizeof(glm::vec4)) aligned_vec2 { glm::vec2 v; };
		glm::vec3 m_position;
		alignas(sizeof(glm::vec4)) glm::vec3 m_color;
		alignas(sizeof(glm::vec4)) GLfloat m_diffuseIntensity;
		GLfloat m_specularIntensity;
		GLfloat m_radius;
		alignas(sizeof(glm::vec4)) glm::mat4 m_lightTransforms[6];
		aligned_vec2 m_shadowMapOffsets[6];
	};

	////////////////////////////////////////////////////////////////////////////////
	/** Uniform buffer for the point light data. */
	struct UniformData
	{
		GLint m_numSources;
		GLuint m_attenuationMethod;
		alignas(sizeof(glm::vec4)) glm::vec4 m_attenuation;
		GLfloat m_castsShadow;
		GLuint m_shadowAlgorithm;
		GLuint m_shadowPrecision;
		GLfloat m_shadowMapNear;
		GLfloat m_shadowMapFar;
		GLfloat m_shadowDepthBias;
		GLfloat m_shadowMinVariance;
		GLfloat m_shadowLightBleedBias;
		GLfloat m_shadowMomentsBias;
		alignas(sizeof(glm::vec2)) glm::vec2 m_shadowExponentialConstants;
		alignas(sizeof(glm::vec2)) glm::vec2 m_shadowMapScale;
		UniformDataLightSource m_lightSources[LIGHT_SOURCES_PER_BATCH];
	};

	////////////////////////////////////////////////////////////////////////////////
	void initObject(Scene::Scene& scene, Scene::Object& object);

	////////////////////////////////////////////////////////////////////////////////
	void releaseObject(Scene::Scene& scene, Scene::Object& object);

	////////////////////////////////////////////////////////////////////////////////
	void updateObject(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* object);

	////////////////////////////////////////////////////////////////////////////////
	void generateGui(Scene::Scene& scene, Scene::Object* guiSettings, Scene::Object* object);

	////////////////////////////////////////////////////////////////////////////////
	void updateLightSources(Scene::Scene& scene, Scene::Object* object);

	////////////////////////////////////////////////////////////////////////////////
	void updateShadowMapSlices(Scene::Scene& scene, Scene::Object* object);

	////////////////////////////////////////////////////////////////////////////////
	void updateShadowMapTexture(Scene::Scene& scene, Scene::Object* object);

	////////////////////////////////////////////////////////////////////////////////
	bool voxelLightingTypePreConditionOpenGL(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* renderSettings, Scene::Object* camera, std::string const& functionName);

	////////////////////////////////////////////////////////////////////////////////
	void voxelLightingBeginOpenGL(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* renderSettings, Scene::Object* camera, std::string const& functionName);

	////////////////////////////////////////////////////////////////////////////////
	void voxelLightingEndOpenGL(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* renderSettings, Scene::Object* camera, std::string const& functionName);

	////////////////////////////////////////////////////////////////////////////////
	void voxelLightingOpengl(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* renderSettings, Scene::Object* camera, std::string const& functionName, Scene::Object* object);

	////////////////////////////////////////////////////////////////////////////////
	bool lightingTypePreConditionOpenGL(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* renderSettings, Scene::Object* camera, std::string const& functionName);

	////////////////////////////////////////////////////////////////////////////////
	void lightingBeginOpenGL(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* renderSettings, Scene::Object* camera, std::string const& functionName);

	////////////////////////////////////////////////////////////////////////////////
	void lightingEndOpenGL(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* renderSettings, Scene::Object* camera, std::string const& functionName);

	////////////////////////////////////////////////////////////////////////////////
	void lightingOpenGL(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* renderSettings, Scene::Object* camera, std::string const& functionName, Scene::Object* object);
}

////////////////////////////////////////////////////////////////////////////////
// Component declaration
DECLARE_COMPONENT(POINT_LIGHT_GRID, PointLightGridComponent, PointLight::PointLightGridComponent)
DECLARE_OBJECT(POINT_LIGHT_GRID, COMPONENT_ID_TRANSFORM, COMPONENT_ID_POINT_LIGHT_GRID, COMPONENT_ID_SHADOW_MAP, COMPONENT_ID_EDITOR_SETTINGS)