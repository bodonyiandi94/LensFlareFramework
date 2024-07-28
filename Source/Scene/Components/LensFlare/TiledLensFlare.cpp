#include "PCH.h"
#include "TiledLensFlare.h"
#include "../Lighting/DirectionalLight.h"
#include "../Rendering/Camera.h"

namespace TiledLensFlare
{
	////////////////////////////////////////////////////////////////////////////////
	// Define the component
	DEFINE_COMPONENT(TILED_LENS_FLARE);
	DEFINE_OBJECT(TILED_LENS_FLARE);
	REGISTER_OBJECT_UPDATE_CALLBACK(TILED_LENS_FLARE, AFTER, GUI_SETTINGS);
	REGISTER_OBJECT_RENDER_CALLBACK(TILED_LENS_FLARE, "Tiled Lens Flare", OpenGL, AFTER, "Effects (HDR) [Begin]", 2, &TiledLensFlare::renderObjectOpenGL, &RenderSettings::firstCallTypeCondition, &RenderSettings::firstCallObjectCondition, nullptr, nullptr);

	////////////////////////////////////////////////////////////////////////////////
	template<typename T>
	std::string to_string(T const& val)
	{
		std::stringstream ss;
		ss << val;
		return ss.str();
	}

	////////////////////////////////////////////////////////////////////////////////
	std::ostream& operator<<(std::ostream& ostream, PrecomputeGhostAttribs const& ghostAttribs)
	{
		ostream << "attribs{ " << std::endl;
		ostream << " - ghost: " << ghostAttribs.m_ghostID << std::endl;
		ostream << " - channel: " << ghostAttribs.m_channelID << std::endl;
		ostream << " - theta: " << glm::degrees(ghostAttribs.m_theta) << std::endl;
		ostream << " - pupilMin: " << ghostAttribs.m_pupilMin << std::endl;
		ostream << " - pupilMax: " << ghostAttribs.m_pupilMax << std::endl;
		ostream << " - sensorMin: " << ghostAttribs.m_sensorMin << std::endl;
		ostream << " - sensorMax: " << ghostAttribs.m_sensorMax << std::endl;
		ostream << " - avgIntensity: " << ghostAttribs.m_avgIntensity << " }";
		return ostream;
	}

	////////////////////////////////////////////////////////////////////////////////
	namespace Common
	{
		////////////////////////////////////////////////////////////////////////////////
		PhysicalCamera::PhysicalCameraAttributes& getCamera(Scene::Scene& scene, Scene::Object* object)
		{
			return object->component<TiledLensFlareComponent>().m_camera;
		}

		////////////////////////////////////////////////////////////////////////////////
		glm::ivec2 computeFinalResolution(Scene::Scene& scene, Scene::Object* object)
		{
			return Scene::findFirstObject(scene, Scene::OBJECT_TYPE_RENDER_SETTINGS)->component<RenderSettings::RenderSettingsComponent>().m_resolution;
		}

		////////////////////////////////////////////////////////////////////////////////
		glm::ivec2 computeRenderResolution(const glm::ivec2 finalResolution, const float resolutionScaling)
		{
			return glm::ivec2(glm::vec2(finalResolution) * resolutionScaling);
		}

		////////////////////////////////////////////////////////////////////////////////
		glm::ivec2 computeRenderResolution(Scene::Scene& scene, Scene::Object* object)
		{
			return computeRenderResolution(computeFinalResolution(scene, object), object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_resolutionScaling);
		}

		////////////////////////////////////////////////////////////////////////////////
		glm::ivec2 computeMaxResolution(Scene::Scene& scene, Scene::Object* object)
		{
			return glm::ivec2(scene.m_textures["TiledLensFlare_Accumulation_0"].m_dimensions);
		}

		////////////////////////////////////////////////////////////////////////////////
		bool needsBlurring(Scene::Scene& scene, Scene::Object* object)
		{
			return
				object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_blurSigma > 1e-2f &&
				object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_blurKernelLinear.size() > 1;
		}

		////////////////////////////////////////////////////////////////////////////////
		bool needsTempTexture(Scene::Scene& scene, Scene::Object* object)
		{
			return needsBlurring(scene, object) || computeFinalResolution(scene, object) != computeRenderResolution(scene, object);
		}

		////////////////////////////////////////////////////////////////////////////////
		glm::ivec2 computeNumTiles(Scene::Scene& scene, Scene::Object* object)
		{
			const glm::ivec2 resolution = computeRenderResolution(scene, object);
			const int tileSize = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_tileSize;

			return (resolution + tileSize - 1) / tileSize;
		}

		////////////////////////////////////////////////////////////////////////////////
		int computeNumTiles1D(Scene::Scene & scene, Scene::Object * object)
		{
			const glm::ivec2 numTiles = computeNumTiles(scene, object);
			return numTiles.x * numTiles.y;
		}

		////////////////////////////////////////////////////////////////////////////////
		glm::ivec2 computeNumCoarseTiles(Scene::Scene& scene, Scene::Object* object)
		{
			glm::ivec2 resolution = computeRenderResolution(scene, object);
			int coarseTileSize = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_coarseTileSize;

			return (resolution + coarseTileSize - 1) / coarseTileSize;
		}

		////////////////////////////////////////////////////////////////////////////////
		int computeNumCoarseTiles1D(Scene::Scene& scene, Scene::Object* object)
		{
			const glm::ivec2 numTiles = computeNumCoarseTiles(scene, object);
			return numTiles.x * numTiles.y;
		}

		////////////////////////////////////////////////////////////////////////////////
		void computeBlurKernels(Scene::Scene& scene, Scene::Object* object)
		{
			RenderSettings::generateGaussianKernel(scene,
				object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_blurSigma,
				object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_blurKernelDiscrete,
				object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_blurKernelLinear);
		}

		////////////////////////////////////////////////////////////////////////////////
		glm::ivec3 getNumWorkGroups(const glm::ivec3 workGroupSize, const glm::ivec3 numWorkItems)
		{
			return (numWorkItems + workGroupSize - 1) / workGroupSize;
		}

		////////////////////////////////////////////////////////////////////////////////
		glm::vec3 calculateIncidentVector(const float angle, const float rotation)
		{
			return glm::vec3(glm::sin(angle) * glm::cos(rotation), glm::sin(angle) * glm::sin(rotation), glm::cos(angle));
		}

		////////////////////////////////////////////////////////////////////////////////
		float getPhysicalApertureHeightEntrance(Scene::Scene& scene, Scene::Object* object, PhysicalCamera::PhysicalCameraAttributes const& camera)
		{
			return PhysicalCamera::getCameraEffectiveApertureHeight(camera);
		}

		////////////////////////////////////////////////////////////////////////////////
		float getPhysicalApertureHeightParaxial(Scene::Scene& scene, Scene::Object* object, PhysicalCamera::PhysicalCameraAttributes const& camera)
		{
			return PhysicalCamera::getCameraPhysicalApertureHeight(camera, 575.0f);
		}

		////////////////////////////////////////////////////////////////////////////////
		float getPhysicalApertureHeight(Scene::Scene& scene, Scene::Object* object, PhysicalCamera::PhysicalCameraAttributes const& camera)
		{
			switch (object->component<TiledLensFlareComponent>().m_commonParameters.m_physicalPupilSizeMethod)
			{
			case CommonParameters::PhysicalPupilSizeMethod::EntrancePupil: return getPhysicalApertureHeightEntrance(scene, object, camera);
			case CommonParameters::PhysicalPupilSizeMethod::ParaxialTracedPupil: return getPhysicalApertureHeightParaxial(scene, object, camera);
			}
			return -1.0f;
		}
		
		////////////////////////////////////////////////////////////////////////////////
		float getPhysicalApertureHeight(Scene::Scene& scene, Scene::Object* object)
		{
			return getPhysicalApertureHeight(scene, object, getCamera(scene, object));
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	namespace TransferMatrices
	{
		////////////////////////////////////////////////////////////////////////////////
		void computeRaytraceLensFlareTransferMatrices(Scene::Scene& scene, Scene::Object* object)
		{
			static const size_t NUM_WAVELENGTHS = 3;

			// Camera parameters
			auto const& camera = object->component<TiledLensFlareComponent>().m_camera;

			// Aperture id
			const int apertureId = PhysicalCamera::getApertureId(camera);

			// Ghost indices
			auto const& ghostIndices = PhysicalCamera::enumerateGhosts(object->component<TiledLensFlareComponent>().m_camera);

			// List of ABCD matrices
			auto& transferMatrices = object->component<TiledLensFlareComponent>().m_transferMatrices;

			DateTime::ScopedTimer timer = DateTime::ScopedTimer(Debug::Debug, ghostIndices.size() * NUM_WAVELENGTHS, DateTime::Milliseconds, "Transfer Matrices");

			// First perform the bounding pass
			transferMatrices.resize(ghostIndices.size());
			for (size_t ghostId = 0; ghostId < ghostIndices.size(); ++ghostId)
			{
				transferMatrices[ghostId].resize(NUM_WAVELENGTHS);
			}

			Threading::threadedExecuteIndices(Threading::numThreads(),
				[&](Threading::ThreadedExecuteEnvironment const& environment, const size_t ghostId, const size_t channelId)
				{
					// Ghost indices
					const std::array<int, 2> ghost = { ghostIndices[ghostId][1], ghostIndices[ghostId][0] };
					const float lambda = camera.m_wavelengths[NUM_WAVELENGTHS][channelId];

					transferMatrices[ghostId][channelId].clear();

					// The transfer matrix that is accumulated
					glm::mat2 abcd = glm::mat2(1.0f, 0.0f, 0.0f, 1.0f);

					// Follow the ray through the system
					int passId = 0, direction = 1;
					for (int i = 1; i < camera.m_lenses.size(); i += direction)
					{
						// Store the matrix
						transferMatrices[ghostId][channelId].push_back(abcd);

						// Do not refract or reflect at the aperture
						if (i != apertureId)
						{
							if (passId <= 2 && ghost[passId] == i) // Reflection
							{
								const float r = direction * camera.m_lenses[i].m_radius;
								const glm::mat2 Li = PhysicalCamera::reflectionMatrix(r);

								abcd = Li * abcd;

								direction *= -1;
								++passId;
							}
							else // refraction
							{
								const int prevI = (direction > 0) ? (i - 1) : (i);
								const int currentI = (direction > 0) ? (i) : (i - 1);

								const float n0 = PhysicalCamera::cauchyDispersion(camera, prevI, lambda);
								const float n1 = PhysicalCamera::cauchyDispersion(camera, currentI, lambda);
								const float r = direction * camera.m_lenses[i].m_radius;

								const glm::mat2 Ri = PhysicalCamera::refractionMatrix(n0, n1, r);

								abcd = Ri * abcd;
							}
						}

						// Do the translation
						const int translateI = (direction > 0) ? (i) : (i - 1);
						const float d = camera.m_lenses[translateI].m_thickness;
						const glm::mat2 Ti = PhysicalCamera::translationMatrix(d);
						//const glm::mat2 Ti = PhysicalCamera::translationMatrix(direction * d);
						abcd = Ti * abcd;
					}
				},
				ghostIndices.size(),
				NUM_WAVELENGTHS);
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	namespace LightSources
	{
		////////////////////////////////////////////////////////////////////////////////
		struct LightSourceData
		{
			Scene::Object* m_lightSource;
			glm::vec3 m_lightColor;
			glm::vec3 m_toLight;
			float m_angle;
			float m_rotation;
			float m_lambert;
		};

		////////////////////////////////////////////////////////////////////////////////
		std::vector<LightSourceData> getLightSourceData(Scene::Scene& scene, Scene::Object* renderSettings, Scene::Object* camera, Scene::Object* object)
		{
			// Resulting vector
			std::vector<LightSourceData> lightSources;

			// Process point light grids
			for (auto light : Scene::filterObjects(scene, Scene::OBJECT_TYPE_DIRECTIONAL_LIGHT))
			{
				LightSourceData lightData;
				lightData.m_lightSource = light;
				lightData.m_lightColor = light->component<DirectionalLight::DirectionalLightComponent>().m_color * light->component<DirectionalLight::DirectionalLightComponent>().m_diffuseIntensity;

				// Vector pointing towards the light source
				lightData.m_toLight = glm::normalize(glm::vec3(Camera::getViewMatrix(camera) * glm::vec4(-Transform::getForwardVector(light), 0.0f)));

				// Convert it to polar coords
				lightData.m_rotation = glm::atan(lightData.m_toLight.y, lightData.m_toLight.x);
				lightData.m_angle = glm::acos(glm::dot(lightData.m_toLight, glm::vec3(0.0f, 0.0f, 1.0f)));

				// Handle fixed light angles
				if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_lockAngle)
					lightData.m_angle = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_fixedAngle;
				if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_lockRotation)
					lightData.m_rotation = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_fixedRotation;
				
				if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_lockAngle ||
					object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_lockRotation ||
					lightData.m_angle > object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_maxAngle)
				{
					lightData.m_angle = glm::min(lightData.m_angle, object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_maxAngle);
					lightData.m_toLight = Common::calculateIncidentVector(lightData.m_angle, lightData.m_rotation);
				}

				// Lambertian shading term
				lightData.m_lambert = glm::max(glm::dot(lightData.m_toLight, glm::vec3(0.0f, 0.0f, 1.0f)), 0.0f);

				// Generate a unique entry for each light source in the grid
				lightSources.push_back(lightData);
			}

			// Return the result
			return lightSources;
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	namespace Uniforms
	{
		////////////////////////////////////////////////////////////////////////////////
		const size_t NUM_GPU_POLYNOMIAL_INPUTS_FULL = 6;
		const size_t NUM_GPU_POLYNOMIAL_INPUTS_BAKED = 4;
		const size_t NUM_GPU_POLYNOMIAL_OUTPUTS = 6;

		////////////////////////////////////////////////////////////////////////////////
		/** Uniform buffer for the raytraced lens flare data. */
		struct alignas(sizeof(glm::vec4)) GhostParams
		{
			alignas(sizeof(glm::vec2)) glm::ivec2 m_ghostIndices;
			alignas(sizeof(glm::vec2)) glm::vec2 m_minPupil;
			alignas(sizeof(glm::vec2)) glm::vec2 m_maxPupil;
			alignas(sizeof(glm::vec2)) glm::vec2 m_centerPupil;
			alignas(sizeof(glm::vec2)) glm::vec2 m_radiusPupil;
			alignas(sizeof(glm::vec2)) glm::vec2 m_minSensor;
			alignas(sizeof(glm::vec2)) glm::vec2 m_maxSensor;

			GLint m_rayCount;
			GLint m_ghostId;
			GLint m_channelId;
			GLfloat m_lambda;
			GLfloat m_intensityScale;

			GLint m_gridStartId;
			GLint m_numPolynomialTerms;
		};

		////////////////////////////////////////////////////////////////////////////////
		// Lens parameters for a single lens
		struct alignas(sizeof(glm::vec4)) Lens
		{
			glm::vec3 m_center;
			GLfloat m_curvature;
			glm::vec4 m_refraction[2];
			GLfloat m_height;
			GLfloat m_aperture;
		};

		////////////////////////////////////////////////////////////////////////////////
		/** Uniform buffer for the common lens flare data. */
		struct RenderGhostsLensUniforms
		{
			alignas(sizeof(glm::vec4)) glm::vec3 m_color;
			alignas(sizeof(glm::vec4)) glm::vec3 m_rayDir;
			GLfloat m_rayDist;

			GLfloat m_lightRotation;
			GLfloat m_lightAngle;

			GLfloat m_radiusClip;
			GLfloat m_irisClip;
			GLfloat m_intensityClip;
			GLfloat m_refractionClip;

			GLfloat m_outerPupilHeight;
			GLfloat m_apertureHeight;
			alignas(sizeof(glm::vec2)) glm::vec2 m_filmSize;

			GLint m_numLenses;

			alignas(sizeof(glm::vec4)) glm::vec4 m_wavelengths[MAX_CHANNELS];
			Lens m_lenses[MAX_CHANNELS][MAX_LENSES];
		};

		////////////////////////////////////////////////////////////////////////////////
		/** Uniform buffer for the common lens flare data. */
		struct RenderGhostsCommonUniforms
		{
			alignas(sizeof(glm::ivec2)) glm::ivec2 m_renderResolution;

			GLint m_gridMethod;
			GLint m_raytraceMethod;
			GLint m_projectionMethod;
			GLint m_shadingMethod;
			GLint m_overlayMethod;
			GLint m_isWireFrame;
			GLint m_clipRays;
			GLint m_clipPixels;
			GLfloat m_filmStretch;

			GLint m_numGhosts;
			GLint m_numChannels;
			GLint m_maxNumGhosts;
			GLint m_maxNumChannels;
			GLint m_maxRayGridSize;

			GLint m_numPolynomialAngles;
			GLint m_numPolynomialRotations;
			GLfloat m_maxPolynomialAngles;
			GLfloat m_maxPolynomialRotations;
			GLfloat m_polynomialAnglesStep;
			GLfloat m_polynomialRotationsStep;
		};

		////////////////////////////////////////////////////////////////////////////////
		/** Uniform buffer for the raytraced lens flare data. */
		struct RenderGhostsCommonUniformsDirect
		{
			GLint _;
		};

		////////////////////////////////////////////////////////////////////////////////
		/** Uniform buffer for the raytraced lens flare data. */
		struct RenderGhostsCommonUniformsTiled
		{
			alignas(sizeof(glm::ivec2)) glm::ivec2 m_numCoarseTiles;
			GLint m_coarseTileSize;
			GLint m_maxCoarseTileEntries;

			alignas(sizeof(glm::ivec2)) glm::ivec2 m_numTiles;
			GLint m_tileSize;
			GLint m_maxTileEntries;

			GLint m_numQuadMergeSteps;
			GLfloat m_quadMergeEdgeThreshold;

			GLint m_sampleBaseScene;
		};

		////////////////////////////////////////////////////////////////////////////////
		/** Data describing a single ray traced through the optical system during rendering. */
		struct alignas(sizeof(glm::vec4)) GhostEntryDirectTracedRay
		{
			alignas(sizeof(glm::vec2)) glm::vec2 m_pupilPos;
			alignas(sizeof(glm::vec2)) glm::vec2 m_sensorPos;
			alignas(sizeof(glm::vec2)) glm::vec2 m_apertureUv;
			GLfloat m_relativeRadius;
			GLfloat m_intensity;
		};

		////////////////////////////////////////////////////////////////////////////////
		/** Data describing a single ray traced through the optical system. */
		struct alignas(sizeof(glm::vec4)) GhostEntryTiledTracedPrimitive
		{
			alignas(sizeof(glm::vec2)) glm::vec2 m_sensorPos[4];
			alignas(sizeof(glm::vec2)) glm::vec2 m_sensorValues[4];
			alignas(sizeof(glm::vec4)) glm::vec4 m_color;
		};

		////////////////////////////////////////////////////////////////////////////////
		/** Data describing a single ray traced through the optical system. */
		struct alignas(sizeof(glm::vec4)) GhostGlobalProperties
		{
			GLint m_numPrimitives;
		};

		////////////////////////////////////////////////////////////////////////////////
		/** Data describing a single ray traced through the optical system. */
		struct GhostEntryTile
		{
			GLint m_primitiveId;
		};

		////////////////////////////////////////////////////////////////////////////////
		/** Data describing a single ray traced through the optical system. */
		struct alignas(sizeof(glm::vec4)) GhostEntryTileProperties
		{
			GLint m_numPrimitives;
		};

		////////////////////////////////////////////////////////////////////////////////
		/** Data describing a single monomial coefficient for a polynomial. */
		struct alignas(sizeof(glm::vec4)) GhostPolynomialMonomialFull
		{
			glm::vec4 m_coefficient[2] = { glm::vec4(0.0f) };
			glm::ivec4 m_degrees[2][NUM_GPU_POLYNOMIAL_INPUTS_FULL] = { glm::ivec4(0.0f) };
		};

		struct alignas(sizeof(glm::vec4)) GhostPolynomialMonomialBaked
		{
			glm::vec4 m_coefficient[2] = { glm::vec4(0.0f) };
			glm::ivec4 m_degrees[2][NUM_GPU_POLYNOMIAL_INPUTS_BAKED] = { glm::ivec4(0) };
		};

		////////////////////////////////////////////////////////////////////////////////
		std::vector<Uniforms::GhostParams> uploadGhostParametersPrecompute(Scene::Scene& scene, Scene::Object* object,
			LightSources::LightSourceData const& lightData, const size_t ghostID, const size_t channelID, const size_t numRays, 
			const glm::vec2 pupilMin, const glm::vec2 pupilMax, const glm::vec2 pupilCenter, const glm::vec2 pupilRadius)
		{
			// Extract the ghost parameters
			auto const& camera = object->component<TiledLensFlareComponent>().m_camera;
			auto const& ghostIndices = object->component<TiledLensFlareComponent>().m_ghostIndices[ghostID];
			PrecomputeGhostsParameters const& precomputeParams = object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters;

			// Store the ghost parameters
			GhostParams uniformDataGhostParams;
			uniformDataGhostParams.m_ghostIndices = glm::ivec2(ghostIndices[1], ghostIndices[0]);
			uniformDataGhostParams.m_minPupil = pupilMin;
			uniformDataGhostParams.m_maxPupil = pupilMax;
			uniformDataGhostParams.m_centerPupil = pupilCenter;
			uniformDataGhostParams.m_radiusPupil = pupilRadius;
			uniformDataGhostParams.m_minSensor = glm::vec2(-FLT_MAX);
			uniformDataGhostParams.m_maxSensor = glm::vec2(FLT_MAX);
			uniformDataGhostParams.m_rayCount = numRays;
			uniformDataGhostParams.m_ghostId = ghostID;
			uniformDataGhostParams.m_channelId = channelID;
			uniformDataGhostParams.m_lambda = camera.m_wavelengths[precomputeParams.m_numChannels][channelID];
			uniformDataGhostParams.m_intensityScale = 1.0f;
			uniformDataGhostParams.m_gridStartId = 0;

			return { uniformDataGhostParams };
		}

		////////////////////////////////////////////////////////////////////////////////
		void uploadLensUniforms(Scene::Scene& scene, Scene::Object* object, Lens* lensData, float& lambda, const int numWavelengths, const int ch)
		{
			// Extract the camera
			auto const& camera = object->component<TiledLensFlareComponent>().m_camera;

			// Lens parameters
			const int apertureId = PhysicalCamera::getApertureId(camera);
			const float physicalApertureHeight = Common::getPhysicalApertureHeight(scene, object, camera);
			float lensDistance = PhysicalCamera::getSensorDistance(camera);

			// Extract the wavelength
			lambda = camera.m_wavelengths[numWavelengths][ch];

			// Upload each surface
			for (int i = 0; i < camera.m_lenses.size(); ++i)
			{
				const auto& lens = camera.m_lenses[i];

				lensData[i].m_curvature = (i != apertureId) ? lens.m_radius : 0.0f;
				lensData[i].m_height = (i != apertureId) ? lens.m_height : physicalApertureHeight;
				lensData[i].m_aperture = (i != apertureId) ? -1.0f : physicalApertureHeight;
				lensData[i].m_center = glm::vec3(0.0f, 0.0f, lensDistance + lensData[i].m_curvature);
				lensData[i].m_refraction[0].x = PhysicalCamera::cauchyDispersion(camera, glm::max(i - 1, 0), lambda);
				lensData[i].m_refraction[0].y = lens.m_coatingRefraction;
				lensData[i].m_refraction[0].z = PhysicalCamera::cauchyDispersion(camera, i, lambda);
				lensData[i].m_refraction[0].w = PhysicalCamera::getCoatingThickness(camera, i, lambda);
				lensData[i].m_refraction[1].x = lensData[i].m_refraction[0].z;
				lensData[i].m_refraction[1].y = lensData[i].m_refraction[0].y;
				lensData[i].m_refraction[1].z = lensData[i].m_refraction[0].x;
				lensData[i].m_refraction[1].w = lensData[i].m_refraction[0].w;
				//Debug::log_debug() << "lens[" << i << "]: " << lensData[i].m_center << ", " << lensData[i].m_curvature << Debug::end;

				lensDistance -= lens.m_thickness;
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		RenderGhostsLensUniforms uploadLensUniformsCommon(Scene::Scene& scene, Scene::Object* object, LightSources::LightSourceData const& lightData,
			const int numWavelengths, const float radiusClip, const float irisClip, const float intensityClip, const float refractionClip)
		{
			// Extract the camera
			auto const& camera = object->component<TiledLensFlareComponent>().m_camera;

			RenderGhostsLensUniforms lensFlareDataCommon;

			// Ray parameters
			lensFlareDataCommon.m_color = lightData.m_lightColor;
			lensFlareDataCommon.m_rayDir = lightData.m_toLight;
			lensFlareDataCommon.m_rayDist = PhysicalCamera::getSensorDistance(camera);

			//Debug::log_debug() << "ray dir: " << lensFlareDataCommon.m_rayDir << Debug::end;

			// Light parameters
			lensFlareDataCommon.m_lightRotation = lightData.m_rotation;
			lensFlareDataCommon.m_lightAngle = lightData.m_angle;

			// Clip parameters
			lensFlareDataCommon.m_radiusClip = radiusClip;
			lensFlareDataCommon.m_irisClip = irisClip;
			lensFlareDataCommon.m_intensityClip = intensityClip;
			lensFlareDataCommon.m_refractionClip = glm::cos(refractionClip);

			// Camera parameters
			lensFlareDataCommon.m_outerPupilHeight = camera.m_lenses[0].m_height;
			lensFlareDataCommon.m_apertureHeight = Common::getPhysicalApertureHeight(scene, object, camera);
			lensFlareDataCommon.m_filmSize = camera.m_filmSize;

			lensFlareDataCommon.m_numLenses = camera.m_lenses.size();

			// Upload the lens parameters for each channel
			for (int channelID = 0; channelID < numWavelengths; ++channelID)
				uploadLensUniforms(scene, object, lensFlareDataCommon.m_lenses[channelID], lensFlareDataCommon.m_wavelengths[channelID].x, numWavelengths, channelID);

			return lensFlareDataCommon;
		}

		////////////////////////////////////////////////////////////////////////////////
		RenderGhostsLensUniforms uploadLensUniformsPrecompute(Scene::Scene& scene, Scene::Object* object, LightSources::LightSourceData const& lightData, 
			const int numWavelengths, const float radiusClip, const float irisClip, const float intensityClip, const float refractionClip)
		{
			return uploadLensUniformsCommon(scene, object, lightData, numWavelengths, radiusClip, irisClip, intensityClip, refractionClip);
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	namespace GhostFilePaths
	{
		////////////////////////////////////////////////////////////////////////////////
		struct FileNameBuilder
		{
			std::string m_separator = "_";
			bool m_empty = true;
			std::stringstream m_stringStream;

			template<typename T>
			void addFolder(T const& val)
			{
				if (!m_empty) m_stringStream << m_separator;
				m_stringStream << val;
				m_empty = false;
				m_separator = "/";
			}

			template<typename T>
			void add(T const& val)
			{
				if (!m_empty) m_stringStream << m_separator;
				m_stringStream << val;
				m_empty = false;
				m_separator = "_";
			}

			template<typename T>
			void addExtension(T const& val)
			{
				m_stringStream << "." << val;
				m_empty = false;
			}

			operator std::string() const
			{
				return m_stringStream.str();
			}
		};

		////////////////////////////////////////////////////////////////////////////////
		std::string getFilenamePrefix(Scene::Scene& scene, Scene::Object* object,
			PrecomputeGhostsParameters const& precomputeParams, PhysicalCamera::PhysicalCameraAttributes const& camera)
		{
			FileNameBuilder builder;

			builder.add(camera.m_name);
			builder.add(camera.m_fNumber);
			builder.add(precomputeParams.m_rayCount);
			builder.add(precomputeParams.m_refinementSteps);
			builder.add(precomputeParams.m_numChannels);
			builder.add(glm::degrees(precomputeParams.m_maxAngle));
			builder.add(glm::degrees(precomputeParams.m_angleStep));
			builder.add(precomputeParams.m_pupilExpansion);
			builder.add(precomputeParams.m_irisClipping);
			builder.add(precomputeParams.m_radiusClipping);
			builder.add(precomputeParams.m_terminateOnFirstInvalid);
			builder.add(precomputeParams.m_clipSensor);
			builder.add(precomputeParams.m_slackAbsolute);
			builder.add(precomputeParams.m_slackPercentage);
			builder.add(precomputeParams.m_slackRays);

			return builder;
		}

		////////////////////////////////////////////////////////////////////////////////
		std::string getFileName(Scene::Scene& scene, Scene::Object* object,
			PrecomputeGhostsParameters const& precomputeParams, PhysicalCamera::PhysicalCameraAttributes const& camera,
			const size_t ghostID)
		{
			FileNameBuilder builder;

			builder.add(getFilenamePrefix(scene, object, precomputeParams, camera));
			builder.add("ghost");
			builder.add(ghostID);
			builder.addExtension("dat");

			return builder;
		}

		////////////////////////////////////////////////////////////////////////////////
		std::string getFilenamePrefix(Scene::Scene& scene, Scene::Object* object,
			PrecomputeGhostsParameters const& precomputeParams, PolynomialFitParameters const& fitParameters,
			PhysicalCamera::PhysicalCameraAttributes const& camera)
		{
			FileNameBuilder builder;

			builder.add(camera.m_name);
			builder.add(camera.m_fNumber);
			builder.add(precomputeParams.m_rayCount);
			builder.add(precomputeParams.m_refinementSteps);
			builder.add(precomputeParams.m_numChannels);
			builder.add(glm::degrees(precomputeParams.m_maxAngle));
			builder.add(glm::degrees(precomputeParams.m_angleStep));
			builder.add(precomputeParams.m_pupilExpansion);
			builder.add(fitParameters.m_ghostGeometryParameters.m_boundsClipping.m_irisClipping);
			builder.add(fitParameters.m_ghostGeometryParameters.m_boundsClipping.m_radiusClipping);
			builder.add(precomputeParams.m_terminateOnFirstInvalid);
			builder.add(fitParameters.m_ghostGeometryParameters.m_boundsClipping.m_clipSensor);
			builder.add(precomputeParams.m_slackAbsolute);
			builder.add(precomputeParams.m_slackPercentage);
			builder.add(precomputeParams.m_slackRays);

			return builder;
		}

		////////////////////////////////////////////////////////////////////////////////
		std::string getFileName(Scene::Scene& scene, Scene::Object* object,
			PrecomputeGhostsParameters const& precomputeParams, PolynomialFitParameters const& fitParameters,
			PhysicalCamera::PhysicalCameraAttributes const& camera, const size_t ghostID)
		{
			FileNameBuilder builder;

			builder.add(getFilenamePrefix(scene, object, precomputeParams, fitParameters, camera));
			builder.add("ghost");
			builder.add(ghostID);
			builder.addExtension("dat");

			return builder;
		}

		////////////////////////////////////////////////////////////////////////////////
		std::string getFilePath(Scene::Scene& scene, Scene::Object* object, std::string const& fileName)
		{
			return (EnginePaths::generatedFilesFolder() / "TiledLensFlare" / fileName).string();
		}

		////////////////////////////////////////////////////////////////////////////////
		void openFile(Scene::Scene& scene, Scene::Object* object, std::string const& fileName, std::ifstream& outFile)
		{
			const std::string fullFilePath = getFilePath(scene, object, fileName);
			outFile.open(fullFilePath, std::ifstream::binary);
		}

		////////////////////////////////////////////////////////////////////////////////
		void openFile(Scene::Scene& scene, Scene::Object* object, std::string const& fileName, std::ofstream& outFile)
		{
			const std::string fullFilePath = getFilePath(scene, object, fileName);
			EnginePaths::makeDirectoryStructure(fullFilePath, true);
			outFile.open(fullFilePath, std::ofstream::binary);
		}

		////////////////////////////////////////////////////////////////////////////////
		std::string getFullGhostGeometryFileName(Scene::Scene& scene, Scene::Object* object,
			PrecomputeGhostsParameters const& precomputeParams, PhysicalCamera::PhysicalCameraAttributes const& camera,
			const size_t ghostID)
		{
			FileNameBuilder builder;

			builder.addFolder("GhostGeometry");
			builder.addFolder("Full");
			builder.add(getFileName(scene, object, precomputeParams, camera, ghostID));

			return builder;
		}

		////////////////////////////////////////////////////////////////////////////////
		std::string getFullGhostGeometryFileName(Scene::Scene& scene, Scene::Object* object, const size_t ghostID)
		{
			return getFullGhostGeometryFileName(scene, object, object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters,
				object->component<TiledLensFlareComponent>().m_camera, ghostID);
		}

		////////////////////////////////////////////////////////////////////////////////
		void openFullGhostGeometryFile(Scene::Scene& scene, Scene::Object* object, const size_t ghostID, std::ofstream& outFile)
		{
			openFile(scene, object, getFullGhostGeometryFileName(scene, object, ghostID), outFile);
		}

		////////////////////////////////////////////////////////////////////////////////
		std::string getValidGhostGeometryFileName(Scene::Scene& scene, Scene::Object* object,
			PrecomputeGhostsParameters const& precomputeParams, PhysicalCamera::PhysicalCameraAttributes const& camera,
			const size_t ghostID)
		{
			FileNameBuilder builder;

			builder.addFolder("GhostGeometry");
			builder.addFolder("Valid");
			builder.add(getFileName(scene, object, precomputeParams, camera, ghostID));

			return builder;
		}

		////////////////////////////////////////////////////////////////////////////////
		std::string getValidGhostGeometryFileName(Scene::Scene& scene, Scene::Object* object,
			const size_t ghostID)
		{
			return getValidGhostGeometryFileName(scene, object, object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters,
				object->component<TiledLensFlareComponent>().m_camera, ghostID);
		}

		////////////////////////////////////////////////////////////////////////////////
		void openValidGhostGeometryFile(Scene::Scene& scene, Scene::Object* object, const size_t ghostID, std::ofstream& outFile)
		{
			openFile(scene, object, getValidGhostGeometryFileName(scene, object, ghostID), outFile);
		}

		////////////////////////////////////////////////////////////////////////////////
		std::string getBoundedGhostGeometryFileName(Scene::Scene& scene, Scene::Object* object, PrecomputeGhostsParameters const& precomputeParams,
			PhysicalCamera::PhysicalCameraAttributes const& camera, const size_t ghostID)
		{
			FileNameBuilder builder;

			builder.addFolder("GhostGeometry");
			builder.addFolder("Bounded");
			builder.add(getFileName(scene, object, precomputeParams, camera, ghostID));

			return builder;
		}

		////////////////////////////////////////////////////////////////////////////////
		std::string getBoundedGhostGeometryFileName(Scene::Scene& scene, Scene::Object* object, const size_t ghostID)
		{
			return getBoundedGhostGeometryFileName(scene, object, object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters,
				object->component<TiledLensFlareComponent>().m_camera, ghostID);
		}

		////////////////////////////////////////////////////////////////////////////////
		void openBoundedGhostGeometryFile(Scene::Scene& scene, Scene::Object* object, const size_t ghostID, std::ofstream& outFile)
		{
			openFile(scene, object, getBoundedGhostGeometryFileName(scene, object, ghostID), outFile);
		}

		////////////////////////////////////////////////////////////////////////////////
		std::string getGhostAttribFileName(Scene::Scene& scene, Scene::Object* object, PrecomputeGhostsParameters const& precomputeParams,
			PhysicalCamera::PhysicalCameraAttributes const& camera)
		{
			FileNameBuilder builder;

			builder.addFolder("GhostAttribs");
			builder.add(getFilenamePrefix(scene, object, precomputeParams, camera));
			builder.addExtension("dat");

			return builder;
		}

		////////////////////////////////////////////////////////////////////////////////
		std::string getGhostAttribFileName(Scene::Scene& scene, Scene::Object* object)
		{
			return getGhostAttribFileName(scene, object,
				object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters,
				object->component<TiledLensFlareComponent>().m_camera);
		}

		////////////////////////////////////////////////////////////////////////////////
		void openGhostAttribFile(Scene::Scene& scene, Scene::Object* object, std::ofstream& outFile)
		{
			openFile(scene, object, getGhostAttribFileName(scene, object), outFile);
		}

		////////////////////////////////////////////////////////////////////////////////
		std::string getPolynomialWeightsFileName(Scene::Scene& scene, Scene::Object* object,
			PolynomialFitParameters const& fitParameters,
			PhysicalCamera::PhysicalCameraAttributes const& camera,
			std::string const& fileName)
		{
			FileNameBuilder builder;

			builder.add(camera.m_name);
			builder.add(camera.m_fNumber);
			builder.add(fitParameters.m_ghostGeometryParameters.m_rayCount);
			builder.add(fitParameters.m_ghostGeometryParameters.m_firstGhost);
			builder.add(fitParameters.m_ghostGeometryParameters.m_numGhost);
			builder.add(fitParameters.m_ghostGeometryParameters.m_numChannels);
			builder.add(fitParameters.m_ghostGeometryParameters.m_numAngles);
			builder.add(glm::degrees(fitParameters.m_ghostGeometryParameters.m_maxAngle));
			//builder.add(fitParameters.m_ghostGeometryParameters.m_numRotations);
			//builder.add(glm::degrees(fitParameters.m_ghostGeometryParameters.m_maxRotation));
			builder.add(fitParameters.m_maxTermDegree);
			builder.add(fitParameters.m_numSparseTerms);
			//builder.add(PolynomialFitParameters::DenseFitLinearMethod_value_to_string(object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_denseFitLinearMethod));
			//builder.add(PolynomialFitParameters::DenseFitNonlinearMethod_value_to_string(object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_denseFitNonlinearMethod));
			//builder.add(PolynomialFitParameters::NonlinearFitFrequency_value_to_string(object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_nonlinearFitFrequency));
			builder.add(PolynomialFitParameters::PartialErrorCollapseMethod_value_to_string(object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_partialErrorCollapseMethod));
			builder.add(fitParameters.m_ghostGeometryParameters.m_boundsClipping.m_irisClipping);
			builder.add(fitParameters.m_ghostGeometryParameters.m_geometryClipping.m_irisClipping);
			builder.add(fitParameters.m_ghostGeometryParameters.m_ghostClipping.m_clipSensor);
			//builder.add(fitParameters.m_ghostGeometryParameters.m_ghostClipping.m_intensityClipping);
			//builder.add(fitParameters.m_ghostGeometryParameters.m_validSampleRatio);
			//builder.add(fitParameters.m_ghostGeometryParameters.m_smoothenData);
			builder.add(fitParameters.m_ghostGeometryParameters.m_includeZeroWeightedDataset);
			builder.add(fitParameters.m_ghostGeometryParameters.m_includeZeroWeightedFit);
			//builder.add(fitParameters.m_ghostGeometryParameters.m_shareNeighboringGeometries);
			if (fitParameters.m_fitMethod == PolynomialFitParameters::FitMethod::SimulatedAnnealing)
			{
				builder.add(fitParameters.m_annealingParams.m_numSamples);
				builder.add(fitParameters.m_annealingParams.m_numIterations);
			}
			if (fitParameters.m_fitMethod == PolynomialFitParameters::FitMethod::PolynomialRegression)
			{
				builder.add(fitParameters.m_polynomialRegressionParams.m_numSamples);
				builder.add(fitParameters.m_polynomialRegressionParams.m_recursionDepth);
				//builder.add(fitParameters.m_polynomialRegressionParams.m_fixedTermNumber);
			}
			builder.add(fileName);

			return builder;
		}

		////////////////////////////////////////////////////////////////////////////////
		std::string getPolynomialWeightsFullFitFilePath(Scene::Scene& scene, Scene::Object* object,
			PolynomialFitParameters const& fitParameters,
			PhysicalCamera::PhysicalCameraAttributes const& camera,
			std::string const& fileName = "weights.dat")
		{
			FileNameBuilder builder;

			builder.addFolder(EnginePaths::generatedFilesFolder().string());
			builder.addFolder("TiledLensFlare");
			builder.addFolder("PolyWeightsFull");
			builder.addFolder(PolynomialFitParameters::FitMethod_value_to_string(fitParameters.m_fitMethod));
			builder.add(getPolynomialWeightsFileName(scene, object, fitParameters, camera, fileName));

			return builder;
		}

		////////////////////////////////////////////////////////////////////////////////
		std::string getPolynomialWeightsFullFitFilePath(Scene::Scene& scene, Scene::Object* object,
			std::string const& fileName = "weights.dat")
		{
			return getPolynomialWeightsFullFitFilePath(scene, object,
				object->component<TiledLensFlareComponent>().m_polynomialFitParameters,
				object->component<TiledLensFlareComponent>().m_camera,
				fileName);
		}

		////////////////////////////////////////////////////////////////////////////////
		std::string getPolynomialWeightsPartialFitFilePath(Scene::Scene& scene, Scene::Object* object,
			PolynomialFitParameters const& fitParameters,
			PhysicalCamera::PhysicalCameraAttributes const& camera,
			std::string const& fileName = "weights.dat")
		{
			FileNameBuilder builder;

			builder.addFolder(EnginePaths::generatedFilesFolder().string());
			builder.addFolder("TiledLensFlare");
			builder.addFolder("PolyWeightsPartial");
			builder.addFolder(PolynomialFitParameters::FitMethod_value_to_string(fitParameters.m_fitMethod));
			builder.add(getPolynomialWeightsFileName(scene, object, fitParameters, camera, fileName));

			return builder;
		}

		////////////////////////////////////////////////////////////////////////////////
		std::string getPolynomialWeightsPartialFitFilePath(Scene::Scene& scene, Scene::Object* object,
			std::string const& fileName = "weights.dat")
		{
			return getPolynomialWeightsPartialFitFilePath(scene, object,
				object->component<TiledLensFlareComponent>().m_polynomialFitParameters,
				object->component<TiledLensFlareComponent>().m_camera,
				fileName);
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	namespace GhostGeometry
	{
		////////////////////////////////////////////////////////////////////////////////
		/** Data describing a single ray traced through the optical system. */
		struct alignas(sizeof(glm::vec4)) GeometryEntryGPU
		{
			glm::vec2 m_pupilPosCartesian;
			glm::vec2 m_pupilPosCartesianNormalized;
			glm::vec2 m_pupilPosPolar;
			glm::vec2 m_pupilPosPolarNormalized;
			glm::vec2 m_centeredPupilPosCartesian;
			glm::vec2 m_centeredPupilPosCartesianNormalized;
			glm::vec2 m_centeredPupilPosPolar;
			glm::vec2 m_centeredPupilPosPolarNormalized;
			glm::vec2 m_entrancePupilPosCartesian;
			glm::vec2 m_entrancePupilPosCartesianNormalized;
			glm::vec2 m_entrancePupilPosPolar;
			glm::vec2 m_entrancePupilPosPolarNormalized;
			glm::vec2 m_aperturePos;
			glm::vec2 m_aperturePosNormalized;
			glm::vec2 m_sensorPos;
			glm::vec2 m_sensorPosNormalized;
			GLfloat m_apertureDistAnalytical;
			GLfloat m_apertureDistTexture;
			GLfloat m_pupilApertureDistAbsolute;
			GLfloat m_pupilApertureDistBounded;
			GLfloat m_intensity;
			GLfloat m_relativeRadius;
			GLfloat m_clipFactor;
		};

		////////////////////////////////////////////////////////////////////////////////
		/** Data describing the validity of a single entry */
		struct ValidityFlags
		{
			bool m_isTractable{ false };
			bool m_isValid{ false };
			bool m_isInternal{ false };
			bool m_isOnSensor{ false };
			bool m_isBoundary{ false };
		};

		////////////////////////////////////////////////////////////////////////////////
		/** Data describing a single ray traced through the optical system during precomputation. */
		struct GeometryEntry
		{
			glm::vec2 m_pupilPosCartesian{ 0.0f, 0.0f };
			glm::vec2 m_pupilPosCartesianNormalized{ 0.0f, 0.0f };
			glm::vec2 m_pupilPosPolar{ 0.0f, 0.0f };
			glm::vec2 m_pupilPosPolarNormalized{ 0.0f, 0.0f };
			glm::vec2 m_centeredPupilPosCartesian{ 0.0f, 0.0f };
			glm::vec2 m_centeredPupilPosCartesianNormalized{ 0.0f, 0.0f };
			glm::vec2 m_centeredPupilPosPolar{ 0.0f, 0.0f };
			glm::vec2 m_centeredPupilPosPolarNormalized{ 0.0f, 0.0f };
			glm::vec2 m_entrancePupilPosCartesian{ 0.0f, 0.0f };
			glm::vec2 m_entrancePupilPosCartesianNormalized{ 0.0f, 0.0f };
			glm::vec2 m_entrancePupilPosPolar{ 0.0f, 0.0f };
			glm::vec2 m_entrancePupilPosPolarNormalized{ 0.0f, 0.0f };
			glm::vec2 m_aperturePos{ 0.0f, 0.0f };
			glm::vec2 m_aperturePosNormalized{ 0.0f, 0.0f };
			glm::vec2 m_sensorPos{ 0.0f, 0.0f };
			glm::vec2 m_sensorPosNormalized{ 0.0f, 0.0f };
			float m_apertureDistAnalytical{ 2.0f };
			float m_apertureDistTexture{ 2.0f };
			float m_pupilApertureDistAbsolute{ 2.0f };
			float m_pupilApertureDistBounded{ 2.0f };
			float m_intensity{ 0.0f };
			float m_relativeRadius{ 2.0f };
			float m_clipFactor{ 2.0f };

			int m_ghostID{ 0 };
			float m_wavelength{ 0.0f };
			float m_angle{ 0.0f };
			float m_rotation{ 0.0f };
			ValidityFlags m_validityFlags;
		};
		using GhostGeometry = std::vector<GeometryEntry>;

		////////////////////////////////////////////////////////////////////////////////
		std::ostream& operator<<(std::ostream& ostream, GeometryEntry const& entry)
		{
			ostream << "entry" << "{ " << std::endl;
			ostream << " - pupilPosCartesian: " << entry.m_pupilPosCartesian << ", " << std::endl;
			ostream << " - pupilPosCartesianNormalized: " << entry.m_pupilPosCartesianNormalized << ", " << std::endl;
			ostream << " - pupilPosPolar: " << entry.m_pupilPosPolar << ", " << std::endl;
			ostream << " - pupilPosPolarNormalized: " << entry.m_pupilPosPolarNormalized << ", " << std::endl;
			ostream << " - centeredPupilPosCartesian: " << entry.m_centeredPupilPosCartesian << ", " << std::endl;
			ostream << " - centeredPupilPosCartesianNormalized: " << entry.m_centeredPupilPosCartesianNormalized << ", " << std::endl;
			ostream << " - centeredPupilPosPolar: " << entry.m_centeredPupilPosPolar << ", " << std::endl;
			ostream << " - centeredPupilPosPolarNormalized: " << entry.m_centeredPupilPosPolarNormalized << ", " << std::endl;
			ostream << " - entrancePupilPosCartesian: " << entry.m_entrancePupilPosCartesian << ", " << std::endl;
			ostream << " - entrancePupilPosCartesianNormalized: " << entry.m_entrancePupilPosCartesianNormalized << ", " << std::endl;
			ostream << " - entrancePupilPosPolar: " << entry.m_entrancePupilPosPolar << ", " << std::endl;
			ostream << " - entrancePupilPosPolarNormalized: " << entry.m_entrancePupilPosPolarNormalized << ", " << std::endl;
			ostream << " - aperturePos: " << entry.m_aperturePos << ", " << std::endl;
			ostream << " - aperturePosNormalized: " << entry.m_aperturePosNormalized << ", " << std::endl;
			ostream << " - sensorPos: " << entry.m_sensorPos << ", " << std::endl;
			ostream << " - sensorPosNormalized: " << entry.m_sensorPosNormalized << ", " << std::endl;
			ostream << " - apertureDistAnalytical: " << entry.m_apertureDistAnalytical << ", " << std::endl;
			ostream << " - apertureDistTexture: " << entry.m_apertureDistTexture << ", " << std::endl;
			ostream << " - pupilApertureDistAbsolute: " << entry.m_pupilApertureDistAbsolute << ", " << std::endl;
			ostream << " - pupilApertureDistBounded: " << entry.m_pupilApertureDistBounded << ", " << std::endl;
			ostream << " - intensity: " << entry.m_intensity << ", " << std::endl;
			ostream << " - relativeRadius: " << entry.m_relativeRadius << ", " << std::endl;
			ostream << " - clipFactor: " << entry.m_clipFactor << ", " << std::endl;
			ostream << " - ghostID: " << entry.m_ghostID << ", " << std::endl;
			ostream << " - wavelength: " << entry.m_wavelength << ", " << std::endl;
			ostream << " - angle: " << entry.m_angle << ", " << std::endl;
			ostream << " - rotation: " << entry.m_rotation << ", " << std::endl;
			ostream << " - isValid: " << entry.m_validityFlags.m_isValid << ", " << std::endl;
			ostream << " - isInternal: " << entry.m_validityFlags.m_isInternal << ", " << std::endl;
			ostream << " - isOnSensor: " << entry.m_validityFlags.m_isOnSensor << ", " << std::endl;
			ostream << " - isBoundary: " << entry.m_validityFlags.m_isBoundary << " }" << std::endl;
			return ostream;
		}

		////////////////////////////////////////////////////////////////////////////////
		GeometryEntry convertGpuEntry(const size_t ghostID, const float angle, const float rotation, const float lambda, 
			GeometryEntryGPU const& entryGpu)
		{
			GeometryEntry result;

			result.m_pupilPosCartesian = entryGpu.m_pupilPosCartesian;
			result.m_pupilPosCartesianNormalized = entryGpu.m_pupilPosCartesianNormalized;
			result.m_pupilPosPolar = entryGpu.m_pupilPosPolar;
			result.m_pupilPosPolarNormalized = entryGpu.m_pupilPosPolarNormalized;
			result.m_centeredPupilPosCartesian = entryGpu.m_centeredPupilPosCartesian;
			result.m_centeredPupilPosCartesianNormalized = entryGpu.m_centeredPupilPosCartesianNormalized;
			result.m_centeredPupilPosPolar = entryGpu.m_centeredPupilPosPolar;
			result.m_centeredPupilPosPolarNormalized = entryGpu.m_centeredPupilPosPolarNormalized;
			result.m_entrancePupilPosCartesian = entryGpu.m_entrancePupilPosCartesian;
			result.m_entrancePupilPosCartesianNormalized = entryGpu.m_entrancePupilPosCartesianNormalized;
			result.m_entrancePupilPosPolar = entryGpu.m_entrancePupilPosPolar;
			result.m_entrancePupilPosPolarNormalized = entryGpu.m_entrancePupilPosPolarNormalized;
			result.m_aperturePos = entryGpu.m_aperturePos;
			result.m_aperturePosNormalized = entryGpu.m_aperturePosNormalized;
			result.m_sensorPos = entryGpu.m_sensorPos;
			result.m_sensorPosNormalized = entryGpu.m_sensorPosNormalized;
			result.m_apertureDistAnalytical = entryGpu.m_apertureDistAnalytical;
			result.m_apertureDistTexture = entryGpu.m_apertureDistTexture;
			result.m_pupilApertureDistAbsolute = entryGpu.m_pupilApertureDistAbsolute;
			result.m_pupilApertureDistBounded = entryGpu.m_pupilApertureDistBounded;
			result.m_intensity = entryGpu.m_intensity;
			result.m_relativeRadius = entryGpu.m_relativeRadius;
			result.m_clipFactor = entryGpu.m_clipFactor;

			result.m_ghostID = ghostID;
			result.m_wavelength = lambda;
			result.m_angle = angle;
			result.m_rotation = rotation;

			return result;
		}

		////////////////////////////////////////////////////////////////////////////////
		GeometryEntry& extractRay(GhostGeometry& geometry, const size_t numRays, const size_t rowID, const size_t colID)
		{
			return geometry[rowID * numRays + colID];
		}

		////////////////////////////////////////////////////////////////////////////////
		GeometryEntry const& extractRay(GhostGeometry const& geometry, const size_t numRays, const size_t rowID, const size_t colID)
		{
			return geometry[rowID * numRays + colID];
		}

		////////////////////////////////////////////////////////////////////////////////
		namespace Filtering
		{
			////////////////////////////////////////////////////////////////////////////////
			bool isEntryNonTractable(GeometryEntry const& entry)
			{
				return
					isinf(entry.m_relativeRadius) || isnan(entry.m_relativeRadius) ||
					isinf(entry.m_apertureDistTexture) || isnan(entry.m_apertureDistTexture) ||
					isinf(entry.m_intensity) || isnan(entry.m_intensity) || entry.m_intensity <= 0.0f;
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isEntryTractable(GeometryEntry const& entry)
			{
				return !isEntryNonTractable(entry);
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isEntryTractable(GhostGeometry const& geometry, const size_t entryId)
			{
				return isEntryTractable(geometry[entryId]);
			}

			////////////////////////////////////////////////////////////////////////////////
			struct ClipParameters
			{
				glm::vec2 m_filmSize;
				float m_filmLength;
				float m_radiusClip;
				float m_irisClip;
				float m_intensityClip;

				bool m_clipNonTractable = true;
				bool m_clipRadius = true;
				bool m_clipIris = true;
				bool m_clipIntensity = true;
				bool m_clipSensor = true;

				ClipParameters():
					m_filmSize(FLT_MAX, FLT_MAX),
					m_filmLength(FLT_MAX),
					m_radiusClip(1.0f),
					m_irisClip(1.0f),
					m_intensityClip(0.0f)
				{}

				ClipParameters(PhysicalCamera::PhysicalCameraAttributes const& camera, PrecomputeGhostsParameters const& precomputeParams) :
					m_filmSize(camera.m_filmSize),
					m_filmLength(glm::length(camera.m_filmSize)),
					m_radiusClip(precomputeParams.m_radiusClipping),
					m_irisClip(precomputeParams.m_irisClipping),
					m_intensityClip(precomputeParams.m_intensityClipping),
					m_clipNonTractable(true),
					m_clipRadius(true),
					m_clipIris(true),
					m_clipIntensity(true),
					m_clipSensor(precomputeParams.m_clipSensor)
				{}

				ClipParameters(PhysicalCamera::PhysicalCameraAttributes const& camera, PolynomialFitParameters const& fitParameters) :
					m_filmSize(camera.m_filmSize),
					m_filmLength(glm::length(camera.m_filmSize)),
					m_radiusClip(fitParameters.m_ghostGeometryParameters.m_geometryClipping.m_radiusClipping),
					m_irisClip(fitParameters.m_ghostGeometryParameters.m_geometryClipping.m_irisClipping),
					m_intensityClip(fitParameters.m_ghostGeometryParameters.m_geometryClipping.m_intensityClipping),
					m_clipNonTractable(true),
					m_clipRadius(true),
					m_clipIris(true),
					m_clipIntensity(true),
					m_clipSensor(fitParameters.m_ghostGeometryParameters.m_geometryClipping.m_clipSensor)
				{}

				ClipParameters(Scene::Scene& scene, Scene::Object* object, PrecomputeGhostsParameters const& precomputeParams) :
					ClipParameters(object->component<TiledLensFlareComponent>().m_camera, precomputeParams)
				{}

				ClipParameters(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters) :
					ClipParameters(object->component<TiledLensFlareComponent>().m_camera, fitParameters)
				{}

				ClipParameters(Scene::Scene& scene, Scene::Object* object) :
					ClipParameters(scene, object, object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters)
				{}

				ClipParameters(ClipParameters const& other, const bool clipNonTractable, const bool clipRadius, 
					const bool clipIris, const bool clipIntensity, const bool clipSensor):
					m_filmSize(other.m_filmSize),
					m_filmLength(other.m_filmLength),
					m_radiusClip(other.m_radiusClip),
					m_irisClip(other.m_irisClip),
					m_intensityClip(other.m_intensityClip),
					m_clipNonTractable(clipNonTractable),
					m_clipRadius(clipRadius),
					m_clipIris(clipIris),
					m_clipIntensity(clipIntensity),
					m_clipSensor(clipSensor)
				{}
			};

			////////////////////////////////////////////////////////////////////////////////
			bool isEntryValid(GeometryEntry const& entry, ClipParameters const& clipParameters)
			{
				return
					(!clipParameters.m_clipNonTractable || (clipParameters.m_clipNonTractable && isEntryTractable(entry))) &&
					(!clipParameters.m_clipRadius || (clipParameters.m_clipRadius && entry.m_relativeRadius <= clipParameters.m_radiusClip)) &&
					(!clipParameters.m_clipIris || (clipParameters.m_clipIris && entry.m_apertureDistTexture <= clipParameters.m_irisClip)) &&
					(!clipParameters.m_clipIntensity || (clipParameters.m_clipIntensity && entry.m_intensity >= clipParameters.m_intensityClip)) &&
					(!clipParameters.m_clipSensor || (clipParameters.m_clipSensor && glm::abs(entry.m_sensorPos.x) < clipParameters.m_filmLength * 0.5f)) &&
					(!clipParameters.m_clipSensor || (clipParameters.m_clipSensor && glm::abs(entry.m_sensorPos.y) < clipParameters.m_filmLength * 0.5f));
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isEntryValid(GhostGeometry const& geometry, const size_t entryId, ClipParameters const& clipParameters)
			{
				return isEntryValid(geometry[entryId], clipParameters);
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isEntryValid(GhostGeometry const& geometry, const int numRays, const int rowId, const int colId, ClipParameters const& clipParameters)
			{
				if (rowId < 0 || rowId >= numRays || colId < 0 || colId >= numRays) return false;
				return isEntryValid(extractRay(geometry, numRays, rowId, colId), clipParameters);
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isEntryValid(GeometryEntry const& entry, const glm::vec2 pupilMin, const glm::vec2 pupilMax)
			{
				return
					isEntryTractable(entry) &&
					entry.m_pupilPosCartesian.x >= pupilMin.x &&
					entry.m_pupilPosCartesian.y >= pupilMin.y &&
					entry.m_pupilPosCartesian.x <= pupilMax.x &&
					entry.m_pupilPosCartesian.y <= pupilMax.y;
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isEntryValid(GhostGeometry const& geometry, const size_t entryId, const glm::vec2 pupilMin, const glm::vec2 pupilMax)
			{
				return isEntryValid(geometry[entryId], pupilMin, pupilMax);
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isNeighborValid(GhostGeometry const& geometry, const int rowId, const int colId, const int rowOffset, const int colOffset,
				ClipParameters const& clipParameters)
			{
				return isEntryValid(geometry, int(std::sqrt(geometry.size())), rowId + rowOffset, colId + colOffset, clipParameters);
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isNeighborValid(GhostGeometry const& geometry, const int numRays, const size_t entryId, const int rowOffset, const int colOffset,
				ClipParameters const& clipParameters)
			{
				return isNeighborValid(geometry, int(entryId) / numRays, int(entryId) % numRays, rowOffset, colOffset, clipParameters);
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isNeighborValid(GhostGeometry const& geometry, const size_t entryId, const int rowOffset, const int colOffset,
				ClipParameters const& clipParameters)
			{
				return isNeighborValid(geometry, int(std::sqrt(float(geometry.size()))), entryId, rowOffset, colOffset, clipParameters);
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isAnyNeighborValid(GhostGeometry const& geometry, const int rowId, const int colId,
				ClipParameters const& clipParameters)
			{
				return
					isNeighborValid(geometry, rowId, colId, -1, -1, clipParameters) ||
					isNeighborValid(geometry, rowId, colId, -1,  0, clipParameters) ||
					isNeighborValid(geometry, rowId, colId, -1,  1, clipParameters) ||
					isNeighborValid(geometry, rowId, colId,  0, -1, clipParameters) ||
					isNeighborValid(geometry, rowId, colId,  0,  1, clipParameters) ||
					isNeighborValid(geometry, rowId, colId,  1, -1, clipParameters) ||
					isNeighborValid(geometry, rowId, colId,  1,  0, clipParameters) ||
					isNeighborValid(geometry, rowId, colId,  1,  1, clipParameters);
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isAnyNeighborValid(GhostGeometry const& geometry, const size_t entryId, const int numRays, ClipParameters const& clipParameters)
			{
				return isAnyNeighborValid(geometry, int(entryId) / numRays, int(entryId) % numRays, clipParameters);
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isAnyNeighborValid(GhostGeometry const& geometry, const size_t entryId, ClipParameters const& clipParameters)
			{
				return isAnyNeighborValid(geometry, entryId, int(std::sqrt(float(geometry.size()))), clipParameters);
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isAnyNeighborInvalid(GhostGeometry const& geometry, const int rowId, const int colId,
				ClipParameters const& clipParameters)
			{
				return
					!isNeighborValid(geometry, rowId, colId, -1, -1, clipParameters) ||
					!isNeighborValid(geometry, rowId, colId, -1,  0, clipParameters) ||
					!isNeighborValid(geometry, rowId, colId, -1,  1, clipParameters) ||
					!isNeighborValid(geometry, rowId, colId,  0, -1, clipParameters) ||
					!isNeighborValid(geometry, rowId, colId,  0,  1, clipParameters) ||
					!isNeighborValid(geometry, rowId, colId,  1, -1, clipParameters) ||
					!isNeighborValid(geometry, rowId, colId,  1,  0, clipParameters) ||
					!isNeighborValid(geometry, rowId, colId,  1,  1, clipParameters);
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isAnyNeighborInvalid(GhostGeometry const& geometry, const size_t entryId, const int numRays,
				ClipParameters const& clipParameters)
			{
				return isAnyNeighborInvalid(geometry, int(entryId) / numRays, int(entryId) % numRays, clipParameters);
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isAnyNeighborInvalid(GhostGeometry const& geometry, const size_t entryId,
				ClipParameters const& clipParameters)
			{
				return isAnyNeighborInvalid(geometry, entryId, int(std::sqrt(float(geometry.size()))), clipParameters);
			}
			
			////////////////////////////////////////////////////////////////////////////////
			bool isEntryInternal(GhostGeometry const& geometry, const size_t entryId, ClipParameters const& clipParameters)
			{
				return isEntryValid(geometry[entryId], clipParameters) || isAnyNeighborValid(geometry, entryId, clipParameters);
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isEntryBoundary(GhostGeometry const& geometry, const size_t entryId, ClipParameters const& clipParameters)
			{
				return
					(isEntryValid(geometry[entryId], clipParameters) && isAnyNeighborInvalid(geometry, entryId, clipParameters)) ||
					(!isEntryValid(geometry[entryId], clipParameters) || isAnyNeighborValid(geometry, entryId, clipParameters));
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isEntryInternalOrBoundary(GhostGeometry const& geometry, const int rowId, const int colId, ClipParameters const& clipParameters)
			{
				// Calculate the valid flags
				bool validFlags[3][3] =
				{
					{
						isNeighborValid(geometry, rowId, colId, -1, -1, clipParameters),
						isNeighborValid(geometry, rowId, colId, -1,  0, clipParameters),
						isNeighborValid(geometry, rowId, colId, -1,  1, clipParameters),
					},
					{
						isNeighborValid(geometry, rowId, colId,  0, -1, clipParameters),
						isNeighborValid(geometry, rowId, colId,  0,  0, clipParameters),
						isNeighborValid(geometry, rowId, colId,  0,  1, clipParameters),
					},
					{
						isNeighborValid(geometry, rowId, colId,  1, -1, clipParameters),
						isNeighborValid(geometry, rowId, colId,  1,  0, clipParameters),
						isNeighborValid(geometry, rowId, colId,  1,  1, clipParameters),
					},
				};

				// List of triangles to consider for validity (row and col offsets)
				static const int triangleVertexIds[8][3][2] =
				{
					// top left
					{ { 1, 1 }, { 0, 1 }, { 0, 0 }, },
					{ { 1, 1 }, { 0, 0 }, { 1, 0 }, },
					// top right
					{ { 1, 1 }, { 1, 2 }, { 0, 1 }, },
					// bottom left
					{ { 1, 1 }, { 1, 0 }, { 2, 1 }, },
					// bottom right
					{ { 1, 1 }, { 2, 2 }, { 2, 1 }, },
					{ { 1, 1 }, { 2, 2 }, { 1, 2 }, },
				};

				// Check the triangles that touch the vertex
				bool considerEntry = false;
				for (int triangleId = 0; triangleId < 6 && !considerEntry; ++triangleId)
				{
					int numValidVertices = 0;
					for (int vertexId = 0; vertexId < 3; ++vertexId)
						if (validFlags[triangleVertexIds[triangleId][vertexId][0]][triangleVertexIds[triangleId][vertexId][1]])
							++numValidVertices;
					considerEntry |= numValidVertices >= 2;
				}

				// Only consider valid rays
				return considerEntry;
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isEntryInternalOrBoundary(GhostGeometry const& geometry, const size_t entryId, const int numRays,
				ClipParameters const& clipParameters)
			{
				return isEntryInternalOrBoundary(geometry, int(entryId) / numRays, int(entryId) % numRays, clipParameters);
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isEntryInternalOrBoundary(GhostGeometry const& geometry, const size_t entryId, ClipParameters const& clipParameters)
			{
				return isEntryInternalOrBoundary(geometry, entryId, int(std::sqrt(float(geometry.size()))), clipParameters);
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isEntryOnSensor(GhostGeometry const& geometry, const size_t entryId, ClipParameters const& clipParameters)
			{
				return isEntryValid(geometry, entryId, ClipParameters(clipParameters, false, false, false, false, true));
			}

			////////////////////////////////////////////////////////////////////////////////
			template<typename Fn>
			void filterGhostGeometryByRays(GhostGeometry const& geometry, ClipParameters const& clipParameters, Fn const& fn)
			{
				for (size_t i = 0; i < geometry.size(); ++i)
					if (isEntryValid(geometry[i], clipParameters))
						fn(i, geometry[i]);
			}

			////////////////////////////////////////////////////////////////////////////////
			template<typename Fn>
			void filterGhostGeometryByPupilRegion(GhostGeometry const& geometry,
				const glm::vec2 pupilMin, const glm::vec2 pupilMax, Fn const& fn)
			{
				for (size_t i = 0; i < geometry.size(); ++i)
					if (isEntryValid(geometry[i], pupilMin, pupilMax))
						fn(i, geometry[i]);
			}

			////////////////////////////////////////////////////////////////////////////////
			GhostGeometry filterGhostGeometryByRays(GhostGeometry const& geometry, ClipParameters const& clipParameters)
			{
				GhostGeometry result;
				result.reserve(geometry.size());
				filterGhostGeometryByRays(geometry, clipParameters,
					[&](const size_t id, GeometryEntry const& entry) { result.push_back(entry); });
				return result;
			}

			////////////////////////////////////////////////////////////////////////////////
			GhostGeometry filterGhostGeometryByPupilRegion(Scene::Scene& scene, Scene::Object* object,
				GhostGeometry const& geometry, const glm::vec2 pupilMin, const glm::vec2 pupilMax)
			{
				GhostGeometry result;
				result.reserve(geometry.size());
				filterGhostGeometryByPupilRegion(geometry, pupilMin, pupilMax,
					[&](const size_t id, GeometryEntry const& entry) { result.push_back(entry); });
				return result;
			}

			////////////////////////////////////////////////////////////////////////////////
			GhostGeometry filterGhostGeometryByRays(Scene::Scene& scene, Scene::Object* object,
				GhostGeometry const& geometry, PrecomputeGhostsParameters const& precomputeParams)
			{
				return filterGhostGeometryByRays(geometry, ClipParameters(scene, object));
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		GhostGeometry computeGhostGeometry(Scene::Scene& scene, Scene::Object* object, 
			const size_t ghostID, const size_t channelID, const float angle, const float rotation,
			const size_t numWavelengths, const size_t numRays, 
			const glm::vec2 pupilMin, const glm::vec2 pupilMax, const glm::vec2 pupilCenter, const glm::vec2 pupilRadius,
			const float radiusClip, const float irisClip, const float intensityClip, const float refractionClip)
		{
			// Extract the camera
			auto const& camera = object->component<TiledLensFlareComponent>().m_camera;

			// Bind the iris textures.
			glActiveTexture(GPU::TextureEnums::TEXTURE_POST_PROCESS_1_ENUM);
			glBindTexture(GL_TEXTURE_2D, scene.m_textures[object->component<TiledLensFlareComponent>().m_camera.m_apertureTexture].m_texture);

			// Fill up a dummy light data source
			LightSources::LightSourceData lightData;
			lightData.m_lightColor = glm::vec3(1.0f);
			lightData.m_toLight = Common::calculateIncidentVector(angle, rotation);
			lightData.m_angle = angle;
			lightData.m_rotation = rotation;
			lightData.m_lambert = 1.0f;

			// Upload the common parameters
			std::vector<Uniforms::GhostParams> ghostParams = Uniforms::uploadGhostParametersPrecompute(scene, object, lightData, ghostID, channelID, numRays, pupilMin, pupilMax, pupilCenter, pupilRadius);
			uploadBufferData(scene, "TiledLensFlareTracedGhostParams", ghostParams);

			Uniforms::RenderGhostsLensUniforms lensFlareDataLens = Uniforms::uploadLensUniformsPrecompute(scene, object, lightData, numWavelengths, radiusClip, irisClip, intensityClip, refractionClip);
			uploadBufferData(scene, "TiledLensFlareLens", lensFlareDataLens);

			//Debug::log_debug() << "Outer pupil height: " << lensFlareDataLens.m_outerPupilHeight << Debug::end;
			//Debug::log_debug() << "Aperture height: " << lensFlareDataLens.m_apertureHeight << Debug::end;

			// resize the buffer
			Scene::resizeGPUBuffer(scene, "TiledLensFlarePrecomputeGeometry", numRays * numRays * sizeof(GeometryEntryGPU), true);
			Scene::bindBuffer(scene, "TiledLensFlarePrecomputeGeometry");

			// Bind the ghosts shader
			Scene::bindShader(scene, "RayTraceLensFlare/precompute_ghosts");

			// Dispatch the computation
			const glm::ivec3 groupSize(8, 8, 1);
			const glm::ivec3 numWorkItems(numRays, numRays, 1);
			const glm::ivec3 numWorkGroups = Common::getNumWorkGroups(groupSize, numWorkItems);
			glDispatchCompute(numWorkGroups.x, numWorkGroups.y, numWorkGroups.z);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

			// Dispatch the work immediately
			glFlush();

			// Map the results data buffer
			GeometryEntryGPU* tracedRays = (GeometryEntryGPU*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
			if (tracedRays == nullptr)
				Debug::log_error() << "Unable to map data buffer." << Debug::end;

			// Copy over the resulting traced rays to the output buffer
			GhostGeometry result(numRays * numRays);
			std::transform(tracedRays, tracedRays + numRays * numRays, result.begin(), [&](GeometryEntryGPU const& gpuEntry)
				{ return convertGpuEntry(ghostID, angle, rotation, camera.m_wavelengths[numWavelengths][channelID], gpuEntry); });

			// Unmap the buffer
			glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
			
			return result;
		}

		////////////////////////////////////////////////////////////////////////////////
		GhostGeometry computeGhostGeometry(Scene::Scene& scene, Scene::Object* object,
			const size_t ghostID, const size_t channelID, const float angle, const float rotation,
			const size_t numWavelengths, const size_t numRays, const glm::vec2 pupilMin, const glm::vec2 pupilMax,
			const float radiusClip, const float irisClip, const float intensityClip, const float refractionClip)
		{
			return computeGhostGeometry(scene, object, ghostID, channelID, angle, rotation, numWavelengths, numRays, 
				pupilMin, pupilMax, (pupilMax + pupilMin) / 2.0f, (pupilMax - pupilMin) / 2.0f,
				radiusClip, irisClip, intensityClip, refractionClip);
		}

		////////////////////////////////////////////////////////////////////////////////
		GhostGeometry computeGhostGeometry(Scene::Scene& scene, Scene::Object* object,
			const size_t ghostID, const size_t channelID, const float angle, const float rotation, 
			PrecomputeGhostAttribs const& ghostAttribs, PrecomputeGhostsParameters const& precomputeParams)
		{
			return computeGhostGeometry(scene, object, ghostID, channelID, angle, rotation, 
				precomputeParams.m_numChannels, precomputeParams.m_rayCount, ghostAttribs.m_pupilMin, ghostAttribs.m_pupilMax,
				precomputeParams.m_radiusClipping, precomputeParams.m_irisClipping, precomputeParams.m_intensityClipping, 
				precomputeParams.m_refractionClipping);
		}
		
		////////////////////////////////////////////////////////////////////////////////
		void saveFullGhostGeometry(Scene::Scene& scene, Scene::Object* object, GhostGeometry const& geometry, std::ofstream& outFile)
		{
			outFile.write((char*)geometry.data(), geometry.size() * sizeof(GeometryEntry));
		}

		////////////////////////////////////////////////////////////////////////////////
		void saveValidGhostGeometry(Scene::Scene& scene, Scene::Object* object, GhostGeometry const& geometry, std::ofstream& outFile)
		{
			Filtering::filterGhostGeometryByRays(geometry, Filtering::ClipParameters(scene, object),
				[&](const size_t id, GeometryEntry const& entry)
				{ outFile.write((char*)&entry, sizeof(GeometryEntry)); });
		}

		////////////////////////////////////////////////////////////////////////////////
		void saveBoundedGhostGeometry(Scene::Scene& scene, Scene::Object* object, GhostGeometry const& geometry, 
			const glm::vec2 pupilMin, const glm::vec2 pupilMax, std::ofstream& outFile)
		{
			Filtering::filterGhostGeometryByPupilRegion(geometry, pupilMin, pupilMax,
				[&](const size_t id, GeometryEntry const& entry)
				{ outFile.write((char*)&entry, sizeof(GeometryEntry)); });
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	namespace GhostAttribs
	{
		////////////////////////////////////////////////////////////////////////////////
		bool isGhostValid(Scene::Scene& scene, Scene::Object* object, PhysicalCamera::PhysicalCameraAttributes const& camera, PrecomputeGhostAttribs const& ghostAttribs)
		{
			if (isinf(ghostAttribs.m_pupilMin.x) || isnan(ghostAttribs.m_pupilMin.y) ||
				isinf(ghostAttribs.m_pupilMax.x) || isnan(ghostAttribs.m_pupilMax.x) ||
				isinf(ghostAttribs.m_sensorMin.x) || isnan(ghostAttribs.m_sensorMin.y) ||
				isinf(ghostAttribs.m_sensorMax.x) || isnan(ghostAttribs.m_sensorMax.x) ||
				isinf(ghostAttribs.m_avgIntensity) || isnan(ghostAttribs.m_avgIntensity) ||
				ghostAttribs.m_avgIntensity <= 0.0f)
				return false;
			const float filmLength = glm::length(camera.m_filmSize);
			if (object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_clipSensor && (
				ghostAttribs.m_sensorMax.x < (-0.5f * filmLength) ||
				ghostAttribs.m_sensorMax.y < (-0.5f * filmLength) ||
				ghostAttribs.m_sensorMin.x > (0.5f * filmLength) ||
				ghostAttribs.m_sensorMin.y > (0.5f * filmLength)))
				return false;

			return true;
		}

		////////////////////////////////////////////////////////////////////////////////
		bool isGhostValid(Scene::Scene& scene, Scene::Object* object, PrecomputeGhostAttribs const& ghostAttribs)
		{
			return isGhostValid(scene, object, object->component<TiledLensFlareComponent>().m_camera, ghostAttribs);
		}

		////////////////////////////////////////////////////////////////////////////////
		float roundAngle(const float maxAngle, const float angleStep, const float angle)
		{
			static const float THRESHOLD = 0.999f;
			const float angleId = glm::clamp(angle, 0.0f, maxAngle) / angleStep;
			if (glm::fract(angleId) > THRESHOLD) return glm::ceil(angleId) * angleStep + angleStep * 1e-2;
			return angle;
		}

		////////////////////////////////////////////////////////////////////////////////
		size_t angleIdPrev(const float maxAngle, const float angleStep, const float angle)
		{
			return size_t(glm::floor(glm::clamp(angle, 0.0f, maxAngle) / angleStep));
		}

		////////////////////////////////////////////////////////////////////////////////
		size_t angleIdNext(const float maxAngle, const float angleStep, const float angle)
		{
			return size_t(glm::floor(glm::clamp(angle + angleStep, 0.0f, maxAngle) / angleStep));
		}

		////////////////////////////////////////////////////////////////////////////////
		struct AngleIds
		{
			size_t m_prevID;
			size_t m_nextID;
			float m_lerpFactor;

			AngleIds():
				m_prevID(0),
				m_nextID(0),
				m_lerpFactor(0.0f)
			{}

			AngleIds(const float maxAngle, const float angleStep, const float angle) :
				m_prevID(angleIdPrev(maxAngle, angleStep, angle)),
				m_nextID(angleIdNext(maxAngle, angleStep, angle)),
				m_lerpFactor(glm::fract(angle / angleStep))
			{}
		};

		////////////////////////////////////////////////////////////////////////////////
		AngleIds getAngleIndices(const float maxAngle, const float angleStep, const float angle)
		{
			return AngleIds(maxAngle, angleStep, roundAngle(maxAngle, angleStep, angle));
		}

		////////////////////////////////////////////////////////////////////////////////
		AngleIds getAngleIndices(Scene::Scene& scene, Scene::Object* object, PrecomputeGhostsParameters const& precomputeParams, const float angle)
		{
			return getAngleIndices(precomputeParams.m_maxAngle, precomputeParams.m_angleStep, angle);
		}

		////////////////////////////////////////////////////////////////////////////////
		AngleIds getAngleIndices(Scene::Scene& scene, Scene::Object* object, const float angle)
		{
			return getAngleIndices(scene, object, object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters, angle);
		}

		////////////////////////////////////////////////////////////////////////////////
		std::string getGhostAttribName(Scene::Scene& scene, Scene::Object* object, PrecomputeGhostsParameters const& precomputeParams,
			PhysicalCamera::PhysicalCameraAttributes const& camera, const size_t ghostID, const size_t channelID, const size_t angleID)
		{
			GhostFilePaths::FileNameBuilder builder;

			builder.add(GhostFilePaths::getFilenamePrefix(scene, object, precomputeParams, camera));
			builder.add(ghostID);
			builder.add(channelID);
			builder.add(angleID);

			return builder;
		}
		
		////////////////////////////////////////////////////////////////////////////////
		std::string getGhostAttribName(Scene::Scene& scene, Scene::Object* object, PrecomputeGhostsParameters const& precomputeParams,
			PolynomialFitParameters const& fitParameters, PhysicalCamera::PhysicalCameraAttributes const& camera, 
			const size_t ghostID, const size_t channelID, const size_t angleID)
		{
			GhostFilePaths::FileNameBuilder builder;

			builder.add(GhostFilePaths::getFilenamePrefix(scene, object, precomputeParams, fitParameters, camera));
			builder.add(ghostID);
			builder.add(channelID);
			builder.add(angleID);

			return builder;
		}

		////////////////////////////////////////////////////////////////////////////////
		std::string getGhostAttribName(Scene::Scene& scene, Scene::Object* object, const size_t ghostID, const size_t channelID, const size_t angleID)
		{
			return getGhostAttribName(scene, object, object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters,
				object->component<TiledLensFlareComponent>().m_camera, ghostID, channelID, angleID);
		}

		////////////////////////////////////////////////////////////////////////////////
		std::string getGhostAttribName(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, const size_t ghostID, const size_t channelID, const size_t angleID)
		{
			return getGhostAttribName(scene, object, object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters,
				fitParameters, object->component<TiledLensFlareComponent>().m_camera, ghostID, channelID, angleID);
		}

		////////////////////////////////////////////////////////////////////////////////
		std::array<std::string, 2> getGhostAttribNames(Scene::Scene& scene, Scene::Object* object, const size_t ghostID, const size_t channelID, AngleIds const& angleIDs)
		{
			return
			{
				getGhostAttribName(scene, object, ghostID, channelID, angleIDs.m_prevID),
				getGhostAttribName(scene, object, ghostID, channelID, angleIDs.m_nextID),
			};
		}

		////////////////////////////////////////////////////////////////////////////////
		std::array<std::string, 2> getGhostAttribNames(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, const size_t ghostID, const size_t channelID, AngleIds const& angleIDs)
		{
			return
			{
				getGhostAttribName(scene, object, fitParameters, ghostID, channelID, angleIDs.m_prevID),
				getGhostAttribName(scene, object, fitParameters, ghostID, channelID, angleIDs.m_nextID),
			};
		}

		////////////////////////////////////////////////////////////////////////////////
		std::array<std::string, 2> getGhostAttribNames(Scene::Scene& scene, Scene::Object* object, PrecomputeGhostsParameters const& precomputeParams,
			PhysicalCamera::PhysicalCameraAttributes const& camera, const size_t ghostID, const size_t channelID, const float angle)
		{
			return getGhostAttribNames(scene, object, ghostID, channelID, getAngleIndices(scene, object, precomputeParams, angle));
		}

		////////////////////////////////////////////////////////////////////////////////
		std::array<std::string, 2> getGhostAttribNames(Scene::Scene& scene, Scene::Object* object, PrecomputeGhostsParameters const& precomputeParams,
			PolynomialFitParameters const& fitParameters, PhysicalCamera::PhysicalCameraAttributes const& camera,
			const size_t ghostID, const size_t channelID, const float angle)
		{
			return getGhostAttribNames(scene, object, fitParameters, ghostID, channelID, getAngleIndices(scene, object, precomputeParams, angle));
		}

		////////////////////////////////////////////////////////////////////////////////
		std::array<std::string, 2> getGhostAttribNames(Scene::Scene& scene, Scene::Object* object, size_t ghostID, size_t channelID, const float angle)
		{
			return getGhostAttribNames(scene, object, object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters,
				object->component<TiledLensFlareComponent>().m_camera, ghostID, channelID, angle);
		}

		////////////////////////////////////////////////////////////////////////////////
		std::array<std::string, 2> getGhostAttribNames(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, size_t ghostID, size_t channelID, const float angle)
		{
			return getGhostAttribNames(scene, object, object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters,
				fitParameters, object->component<TiledLensFlareComponent>().m_camera, ghostID, channelID, angle);
		}

		////////////////////////////////////////////////////////////////////////////////
		void loadSingleGhostAttribsFile(Scene::Scene& scene, Scene::Object* object, std::string fileName)
		{
			Debug::log_debug() << "Reading ghost attributes from: " << fileName << Debug::end;

			// Open the file containing all the ghost attributes for a single camera and precomputation preset
			std::ifstream inFile(fileName, std::ifstream::binary);

			// Load the number of ghost attributes stored in the file
			size_t numAttribs;
			inFile.read((char*)&numAttribs, sizeof(numAttribs));

			Debug::log_debug() << "  > Number of attributes: " << numAttribs << Debug::end;

			// Read back each ghost attrib
			for (size_t ghostAttribID = 0; ghostAttribID < numAttribs; ++ghostAttribID)
			{
				// Read the ghost attrib names length
				size_t attribNameLength;
				inFile.read((char*)&attribNameLength, sizeof(attribNameLength));

				// Read the ghost attrib name
				std::string attribName(attribNameLength, ' ');
				inFile.read(attribName.data(), attribNameLength * sizeof(char));

				//Debug::log_debug() << "  > Ghost attrib #" << ghostAttribID << " name: " << attribName << Debug::end;

				// Read back the ghost attributes
				PrecomputeGhostAttribs ghostAttribs;
				inFile.read((char*)&ghostAttribs, sizeof(ghostAttribs));

				// Store the result in the ghost attrib database
				object->component<TiledLensFlareComponent>().m_precomputedGhostAttribs[attribName] = ghostAttribs;
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		void addSlackAbsolute(float& f1, float& f2, float slack)
		{
			f1 -= slack;
			f2 += slack;
		}

		////////////////////////////////////////////////////////////////////////////////
		void addSlackAbsolute(glm::vec2& v1, glm::vec2& v2, const glm::vec2 slack)
		{
			//Debug::log_debug() << "Adding " << slack << " slack to { " << v1 << ", " << v2 << " }" << Debug::end;
			addSlackAbsolute(v1.x, v2.x, slack.x);
			addSlackAbsolute(v1.y, v2.y, slack.y);
			//Debug::log_debug() << "Result: { " << v1 << ", " << v2 << " }" << Debug::end;
		}

		////////////////////////////////////////////////////////////////////////////////
		void addSlackAbsolute(glm::vec2& v1, glm::vec2& v2, float slack)
		{
			addSlackAbsolute(v1, v2, glm::vec2(slack));
		}

		////////////////////////////////////////////////////////////////////////////////
		void addSlackPercentage(float& f1, float& f2, float slack)
		{
			if (f1 < 0.0f) f1 *= (1.0f + slack);
			else           f1 *= (1.0f - slack);
			if (f2 < 0.0f) f2 *= (1.0f - slack);
			else           f2 *= (1.0f + slack);
		}

		////////////////////////////////////////////////////////////////////////////////
		void addSlackPercentage(glm::vec2& v1, glm::vec2& v2, const glm::vec2 slack)
		{
			addSlackPercentage(v1.x, v2.x, slack.x);
			addSlackPercentage(v1.y, v2.y, slack.x);
		}

		////////////////////////////////////////////////////////////////////////////////
		void addSlackPercentage(glm::vec2& v1, glm::vec2& v2, float slack)
		{
			addSlackPercentage(v1, v2, glm::vec2(slack));
		}

		////////////////////////////////////////////////////////////////////////////////
		PrecomputeGhostAttribs createInvalidAttrib(const size_t ghostID, const size_t channelID, const size_t angleID, const float theta)
		{
			PrecomputeGhostAttribs result;
			result.m_pupilMin = glm::vec2(FLT_MAX);
			result.m_pupilMax = glm::vec2(-FLT_MAX);
			result.m_sensorMin = glm::vec2(FLT_MAX);
			result.m_sensorMax = glm::vec2(-FLT_MAX);
			result.m_avgIntensity = 0.0f;
			result.m_channelID = channelID;
			result.m_angleID = angleID;
			result.m_ghostID = ghostID;
			result.m_theta = theta;
			return result;
		}

		////////////////////////////////////////////////////////////////////////////////
		PrecomputeGhostAttribs computeSingleGhostAttribs(Scene::Scene& scene, Scene::Object* object,
			PrecomputeGhostsParameters const& precomputeParams,
			const size_t ghostID, const size_t channelID, const size_t angleID, const float theta,
			std::vector<GhostGeometry::GeometryEntry> const& geometry, const size_t refinementID)
		{
			// Clip parameters
			GhostGeometry::Filtering::ClipParameters clipParameters(object->component<TiledLensFlareComponent>().m_camera, precomputeParams);

			// Count the number of valid rays
			const int numValidRays = std::count_if(geometry.begin(), geometry.end(), [&](auto const& ray)
				{ return GhostGeometry::Filtering::isEntryValid(ray, clipParameters); });

			// Return an empty ghost attrib if not valid rays were found
			if (numValidRays <= 0)
				return createInvalidAttrib(ghostID, channelID, angleID, theta);

			// Result of the computation
			PrecomputeGhostAttribs result;
			result.m_pupilMin = glm::vec2(FLT_MAX);
			result.m_pupilMax = glm::vec2(-FLT_MAX);
			result.m_sensorMin = glm::vec2(FLT_MAX);
			result.m_sensorMax = glm::vec2(-FLT_MAX);
			result.m_avgIntensity = 0.0f;
			result.m_channelID = channelID;
			result.m_angleID = angleID;
			result.m_ghostID = ghostID;
			result.m_theta = theta;

			// Process each entry
			for (size_t entryId = 0; entryId < geometry.size(); ++entryId)
			{
				// Extract the ray
				auto const& ray = geometry[entryId];

				// Only consider valid rays
				if (!GhostGeometry::Filtering::isEntryInternalOrBoundary(geometry, entryId, clipParameters)) continue;

				// Update the bounds
				result.m_pupilMin = glm::min(result.m_pupilMin, ray.m_pupilPosCartesian);
				result.m_pupilMax = glm::max(result.m_pupilMax, ray.m_pupilPosCartesian);
				result.m_sensorMin = glm::min(result.m_sensorMin, ray.m_sensorPos);
				result.m_sensorMax = glm::max(result.m_sensorMax, ray.m_sensorPos);

				// Accumulate the intensity
				if (GhostGeometry::Filtering::isEntryValid(ray, clipParameters)) 
					result.m_avgIntensity += ray.m_intensity;
			}

			// Camera parameters
			const float pupilHeight = object->component<TiledLensFlareComponent>().m_camera.m_lenses[0].m_height;
			const float pupilExpansion = precomputeParams.m_pupilExpansion;
			const float raySizePupil = 2.0f * pupilHeight * pupilExpansion / float(precomputeParams.m_rayCount - 1);

			// Finalize the ghost attribs
			const glm::vec2 stepSize = glm::vec2(raySizePupil);
			addSlackAbsolute(result.m_pupilMin, result.m_pupilMax, precomputeParams.m_slackRays * stepSize); // expand the bounds by the size of a single ray
			addSlackPercentage(result.m_pupilMin, result.m_pupilMax, precomputeParams.m_slackPercentage);
			addSlackAbsolute(result.m_pupilMin, result.m_pupilMax, pupilHeight * precomputeParams.m_slackAbsolute);
			result.m_avgIntensity /= float(numValidRays);

			// Return the result
			return result;
		}

		////////////////////////////////////////////////////////////////////////////////
		void saveSingleGhostAttribs(Scene::Scene& scene, Scene::Object* object, PrecomputeGhostAttribs const& ghostAttribs, std::string const& attribName, std::ofstream& outFile)
		{
			//Debug::log_debug() << "Writing ghost attribues for: " << attribName << Debug::end;
			size_t attribNameLength = attribName.length();

			outFile.write((char*)&attribNameLength, sizeof(attribNameLength));
			outFile.write(attribName.c_str(), attribNameLength * sizeof(char));
			outFile.write((char*)&ghostAttribs, sizeof(ghostAttribs));
		}

		////////////////////////////////////////////////////////////////////////////////
		void computeAllGhostAttribs(Scene::Scene& scene, Scene::Object* object)
		{
			Debug::log_info() << "Initiating ghost attrib computation..." << Debug::end;

			// Precompute parameters
			PrecomputeGhostsParameters const& precomputeParams = object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters;
			PhysicalCamera::PhysicalCameraAttributes const& camera = object->component<TiledLensFlareComponent>().m_camera;
			auto& ghostAttribsMap = object->component<TiledLensFlareComponent>().m_precomputedGhostAttribs;

			// Enumerate the possible ghosts
			auto const& ghostIndices = object->component<TiledLensFlareComponent>().m_ghostIndices;
			const size_t firstGhost = precomputeParams.m_firstGhost;
			const size_t numGhosts = std::min(size_t(precomputeParams.m_numGhost), ghostIndices.size() - firstGhost);
			const size_t numAngles = size_t(precomputeParams.m_maxAngle / precomputeParams.m_angleStep) + 1;
			const size_t numChannels = precomputeParams.m_numChannels;
			const size_t numRays = precomputeParams.m_rayCount;
			const size_t numAttribs = numChannels * numAngles * numGhosts;
			const size_t refinementSteps = precomputeParams.m_refinementSteps;

			DateTime::ScopedTimer timer = DateTime::ScopedTimer(Debug::Debug, numAngles, DateTime::Milliseconds, "Ghost Bounding");

			// Ghost attrib output file
			std::ofstream ghostAttribsOutFile;
			if (precomputeParams.m_computeGhostAttribs && precomputeParams.m_saveGhostAttribs)
			{
				// Open the file
				GhostFilePaths::openGhostAttribFile(scene, object, ghostAttribsOutFile);

				// Write out the number of ghost attribs stored in the file
				ghostAttribsOutFile.write((char*)&numAttribs, sizeof(numAttribs));
			}

			// Render the ghosts themselves
			for (int ghostID = firstGhost; ghostID < firstGhost + numGhosts && ghostID < ghostIndices.size(); ++ghostID)
			{
				Debug::log_info() << "Computing ghost attributes for ghost #" << (ghostID + 1) << "/" << numGhosts << Debug::end;

				// Per-ghost full geometry out file
				std::ofstream fullGeometryOutFile;
				if (precomputeParams.m_saveFullGhostGeometry)
					GhostFilePaths::openFullGhostGeometryFile(scene, object, ghostID, fullGeometryOutFile);

				// Per-ghost valid geometry out file
				std::ofstream validGeometryOutFile;
				if (precomputeParams.m_saveValidGhostGeometry)
					GhostFilePaths::openValidGhostGeometryFile(scene, object, ghostID, validGeometryOutFile);

				// Per-ghost bounded geometry out file
				std::ofstream boundedGeometryOutFile;
				if (precomputeParams.m_saveBoundedGhostGeometry)
					GhostFilePaths::openBoundedGhostGeometryFile(scene, object, ghostID, boundedGeometryOutFile);

				// Go through each channel and incident angle
				for (int channelID = 0; channelID < numChannels; ++channelID)
				{
					bool invalidFound = false;
					for (int angleID = 0; angleID < numAngles; ++angleID)
					{
						// Name of the ghost attrib and whether it is already present
						const std::string ghostAttribName = getGhostAttribName(scene, object, ghostID, channelID, angleID);
						const bool hasGhostAttrib = ghostAttribsMap.find(ghostAttribName) != ghostAttribsMap.end();

						// Compute the incident light angle on the horizontal axis
						const float theta = precomputeParams.m_angleStep * angleID;

						// Results of the computation
						PrecomputeGhostAttribs ghostAttribs;
						GhostGeometry::GhostGeometry ghostGeometry;

						// Early out if an invalid attrib has already been encountered
						if (precomputeParams.m_terminateOnFirstInvalid && invalidFound)
						{
							ghostAttribs = createInvalidAttrib(ghostID, channelID, angleID, theta);
						}
						else
						{
							// Set the initial bounds
							ghostAttribs.m_pupilMin = -glm::vec2(precomputeParams.m_pupilExpansion * camera.m_lenses[0].m_height);
							ghostAttribs.m_pupilMax = glm::vec2(precomputeParams.m_pupilExpansion * camera.m_lenses[0].m_height);

							// Perform the various refinement steps
							for (size_t refinementID = 0; refinementID <= refinementSteps; ++refinementID)
							{
								// Compute the generated ghost geometry
								ghostGeometry = GhostGeometry::computeGhostGeometry(scene, object, ghostID, channelID, theta, 0.0f, ghostAttribs, precomputeParams);

								// Compute the ghost attributes
								if (precomputeParams.m_computeGhostAttribs || (!hasGhostAttrib && precomputeParams.m_saveBoundedGhostGeometry))
									ghostAttribs = computeSingleGhostAttribs(scene, object, precomputeParams, ghostID, channelID, angleID, theta, ghostGeometry, refinementID);
							}
						}

						// Compute the ghost attributes
						if (precomputeParams.m_computeGhostAttribs || (!hasGhostAttrib && precomputeParams.m_saveBoundedGhostGeometry))
							ghostAttribsMap[ghostAttribName] = ghostAttribs;

						// Set the invalid flag
						if (!invalidFound && !isGhostValid(scene, object, ghostAttribs))
						{
							//Debug::log_debug() << "First invalid found for ghost #" << ghostID << " at angle " << glm::degrees(theta) << Debug::end;
							invalidFound = true;
						}

						if (precomputeParams.m_saveGhostAttribs)
							saveSingleGhostAttribs(scene, object, ghostAttribs, ghostAttribName, ghostAttribsOutFile);

						// Save the computed ghost geometry to disk (combined file)
						if (precomputeParams.m_saveFullGhostGeometry)
							GhostGeometry::saveFullGhostGeometry(scene, object, ghostGeometry, fullGeometryOutFile);

						// Save the computed filtered ghost geometry to disk (combined file)
						if (precomputeParams.m_saveValidGhostGeometry)
							GhostGeometry::saveValidGhostGeometry(scene, object, ghostGeometry, validGeometryOutFile);

						// Save the computed filtered ghost geometry to disk (combined file)
						if (precomputeParams.m_saveBoundedGhostGeometry)
							GhostGeometry::saveBoundedGhostGeometry(scene, object, ghostGeometry,
								ghostAttribs.m_pupilMin, ghostAttribs.m_pupilMax, boundedGeometryOutFile);
					}
				}
			}

			Debug::log_info() << "Ghost attribs successfully computed." << Debug::end;
		}

		////////////////////////////////////////////////////////////////////////////////
		glm::vec2 calculateBounds(glm::mat2 const& abcd, const glm::vec2 height, const glm::vec2 theta)
		{
			// Compute the current bound values
			const glm::vec2 bounds = glm::vec2
			(
				(height.x - theta.x * abcd[1][0]) / abcd[0][0],
				(height.y - theta.y * abcd[1][0]) / abcd[0][0]
			);

			// Compute the min and max values
			return glm::vec2
			(
				glm::min(bounds[0], bounds[1]),
				glm::max(bounds[0], bounds[1])
			);
		}

		////////////////////////////////////////////////////////////////////////////////
		glm::vec2 calculateAngle(glm::mat2 const& abcd, const glm::vec2 height, const glm::vec2 theta)
		{
			return glm::vec2
			(
				(abcd * glm::vec2(height.x, theta.x)).y,
				(abcd * glm::vec2(height.y, theta.y)).y
			);
		}

		////////////////////////////////////////////////////////////////////////////////
		PrecomputeGhostAttribs computeGhostAttribMatrixMethod(Scene::Scene& scene, Scene::Object* object, const size_t ghostID, const size_t channelID, const float theta)
		{
			// Camera parameters
			auto const& camera = object->component<TiledLensFlareComponent>().m_camera;
			const int apertureId = PhysicalCamera::getApertureId(camera);
			const float outerHeight = camera.m_lenses[0].m_height * object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_pupilExpansion;

			// Ghost indices
			auto const& ghostIndices = object->component<TiledLensFlareComponent>().m_ghostIndices;
			std::array<int, 2> ghost = { ghostIndices[ghostID][1], ghostIndices[ghostID][0] };

			// List of ABCD matrices
			auto const& transferMatrices = object->component<TiledLensFlareComponent>().m_transferMatrices[ghostID][channelID];

			// Resulting bounding information
			PrecomputeGhostAttribs result;

			// Initialize the resulting structures
			result.m_pupilMin = glm::vec2(-outerHeight);
			result.m_pupilMax = glm::vec2(outerHeight);
			result.m_sensorMin = glm::vec2(-outerHeight);
			result.m_sensorMax = glm::vec2(outerHeight);
			result.m_avgIntensity = 1.0f;

			// Compute the physical aperture height
			const float physicalApertureHeight = Common::getPhysicalApertureHeight(scene, object, camera);

			// Bounds of the ghost on the pupil
			glm::vec2 pupilBounds{ -FLT_MAX, FLT_MAX };
			glm::vec2 zeroPupilBounds{ -FLT_MAX, FLT_MAX };

			// Follow the ray through the system
			int passId = 0, direction = 1;
			for (int i = 1, mat = 0; i < camera.m_lenses.size() && result.m_avgIntensity > 0.0f; i += direction, ++mat)
			{
				// Extract the reference height
				const float height = (i == apertureId) ? (physicalApertureHeight) : (camera.m_lenses[i].m_height);

				// Compute the current bound values for theta and 0�
				const glm::vec2 currentPupilBounds = calculateBounds(transferMatrices[mat], glm::vec2(-height, height), glm::vec2(theta, theta));
				const glm::vec2 currentZeroPupilBounds = calculateBounds(transferMatrices[mat], glm::vec2(-height, height), glm::vec2(0.0f, 0.0f));

				// Discard the ghost if it misses
				if (currentPupilBounds[1] < -outerHeight || currentPupilBounds[0] > outerHeight)
					result.m_avgIntensity = 0.0f;

				// Update the actual bounds
				pupilBounds[0] = glm::max(pupilBounds[0], currentPupilBounds[0]);
				pupilBounds[1] = glm::min(pupilBounds[1], currentPupilBounds[1]);
				zeroPupilBounds[0] = glm::max(zeroPupilBounds[0], currentZeroPupilBounds[0]);
				zeroPupilBounds[1] = glm::min(zeroPupilBounds[1], currentZeroPupilBounds[1]);

				// Change direction
				if (passId < ghost.size() && ghost[passId] == i)
				{
				direction *= -1;
					++passId;
				}
			}

			if (result.m_avgIntensity == 0.0f)
				return result;

			// Compute the pupil center and length values
			glm::vec2 pupilCenter = glm::vec2((pupilBounds[0] + pupilBounds[1]) / 2.0f, 0.0f);
			glm::vec2 pupilLength = glm::max(glm::vec2(pupilBounds[1] - pupilBounds[0]), glm::vec2(zeroPupilBounds[1] - zeroPupilBounds[0]));
			switch (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_paraxialBoundsSizeMethod)
			{
			case RenderGhostsParameters::ParaxialBoundsSizeMethod::CurrentAngle:
				pupilLength = glm::vec2(pupilBounds[1] - pupilBounds[0]);
				break;
			case RenderGhostsParameters::ZeroAngle:
				pupilLength = glm::vec2(zeroPupilBounds[1] - zeroPupilBounds[0]);
				break;
			case RenderGhostsParameters::MaxCurrentZero:
				pupilLength = glm::vec2(glm::max(pupilBounds[1] - pupilBounds[0], zeroPupilBounds[1] - zeroPupilBounds[0]));
				break;
			case RenderGhostsParameters::AverageCurrentZero:
				pupilLength = glm::vec2((pupilBounds[1] - pupilBounds[0] + zeroPupilBounds[1] - zeroPupilBounds[0]) / 2.0f);
				break;
			}

			// Add slack to the size
			pupilLength *= (1.0f + object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_paraxialBoundsSlack);

			// Write out the computed values
			result.m_pupilMin = pupilCenter - pupilLength / 2.0f;
			result.m_pupilMax = pupilCenter + pupilLength / 2.0f;

			// Clamp the bounds
			result.m_pupilMin = glm::max(glm::vec2(-outerHeight), result.m_pupilMin);
			result.m_pupilMax = glm::min(glm::vec2(outerHeight), result.m_pupilMax);

			// Calculate the sensor bounds
			auto const& abcd = transferMatrices.back();
			glm::vec2 currentSensorBounds =
			{
				pupilBounds[0] * abcd[0][0] + theta * abcd[1][0],
				pupilBounds[1] * abcd[0][0] + theta * abcd[1][0],
			};
			glm::vec2 sensorBounds =
			{
				glm::min(currentSensorBounds[0], currentSensorBounds[1]),
				glm::max(currentSensorBounds[0], currentSensorBounds[1])
			};

			// Compute the sensor center and length values
			glm::vec2 sensorCenter = glm::vec2((sensorBounds[0] + sensorBounds[1]) / 2.0f, 0.0f);
			glm::vec2 sensorLength = glm::vec2(sensorBounds[1] - sensorBounds[0]);

			// Add slack to the size
			sensorLength *= (1.0f + object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_paraxialBoundsSlack);

			// Write out the computed values
			result.m_sensorMin = sensorCenter - sensorLength / 2.0f;
			result.m_sensorMax = sensorCenter + sensorLength / 2.0f;

			// Clamp the bounds
			result.m_sensorMin = glm::max(-camera.m_filmSize * 0.5f, result.m_sensorMin);
			result.m_sensorMax = glm::min(camera.m_filmSize * 0.5f, result.m_sensorMax);

			return result;
		}

		////////////////////////////////////////////////////////////////////////////////
		struct RenderGhostAttribs
		{
			glm::vec2 m_pupilMin{ FLT_MAX, FLT_MAX };
			glm::vec2 m_pupilMax{ -FLT_MAX, -FLT_MAX };
			glm::vec2 m_pupilCenter{ FLT_MAX, FLT_MAX };
			glm::vec2 m_pupilRadius{ -FLT_MAX, -FLT_MAX };
			glm::vec2 m_sensorMin{ FLT_MAX, FLT_MAX };
			glm::vec2 m_sensorMax{ -FLT_MAX, -FLT_MAX };

			float m_intensityScale{ 1.0f };
			float m_avgIntensity{ -1.0f };

			int m_rayGridSize{ 128 };
		};

		////////////////////////////////////////////////////////////////////////////////
		bool isGhostValid(Scene::Scene& scene, Scene::Object* object, PhysicalCamera::PhysicalCameraAttributes const& camera, RenderGhostAttribs const& ghostAttribs)
		{
			if (isinf(ghostAttribs.m_pupilMin.x) || isnan(ghostAttribs.m_pupilMin.y) ||
				isinf(ghostAttribs.m_pupilMax.x) || isnan(ghostAttribs.m_pupilMax.x) ||
				isinf(ghostAttribs.m_sensorMin.x) || isnan(ghostAttribs.m_sensorMin.y) ||
				isinf(ghostAttribs.m_sensorMax.x) || isnan(ghostAttribs.m_sensorMax.x) ||
				isinf(ghostAttribs.m_avgIntensity) || isnan(ghostAttribs.m_avgIntensity) ||
				ghostAttribs.m_avgIntensity <= 0.0f)
				return false;

			return true;
		}

		////////////////////////////////////////////////////////////////////////////////
		bool isGhostValid(Scene::Scene& scene, Scene::Object* object, RenderGhostAttribs const& ghostAttribs)
		{
			return isGhostValid(scene, object, object->component<TiledLensFlareComponent>().m_camera, ghostAttribs);
		}

		////////////////////////////////////////////////////////////////////////////////
		bool isGhostVisible(Scene::Scene& scene, Scene::Object* object, PhysicalCamera::PhysicalCameraAttributes const& camera, 
			PrecomputeGhostsParameters const& precomputeParams, RenderGhostsParameters const& renderParams,
			RenderGhostAttribs const& ghostAttribs)
		{
			// Clip the intensity first
			if (ghostAttribs.m_avgIntensity <= renderParams.m_intensityClipping)
				return false;

			// Clip the sensor
			const glm::vec2 filmLength = renderParams.m_clipSensor ? glm::vec2(0.5f * glm::length(camera.m_filmSize)) : glm::vec2(FLT_MAX);
			if (glm::any(glm::lessThan(ghostAttribs.m_sensorMax, -filmLength)) || glm::any(glm::greaterThan(ghostAttribs.m_sensorMin, filmLength)))
				return false;

			// Make sure the ghost matches a minimum size
			const glm::vec2 extent = ghostAttribs.m_pupilMax - ghostAttribs.m_pupilMin;
			const float minGhostSize = renderParams.m_sizeClipping * 2.0f * camera.m_lenses[0].m_height * precomputeParams.m_pupilExpansion / float(precomputeParams.m_rayCount - 1);
			if (glm::any(glm::lessThan(extent, glm::vec2(minGhostSize))))
				return false;

			// The ghost is visible
			return true;
		}

		////////////////////////////////////////////////////////////////////////////////
		bool isGhostVisible(Scene::Scene& scene, Scene::Object* object, PhysicalCamera::PhysicalCameraAttributes const& camera, RenderGhostAttribs const& ghostAttribs)
		{
			return isGhostVisible(scene, object, camera,
				object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters,
				object->component<TiledLensFlareComponent>().m_renderGhostsParameters,
				ghostAttribs);
		}

		////////////////////////////////////////////////////////////////////////////////
		bool isGhostVisible(Scene::Scene& scene, Scene::Object* object, RenderGhostAttribs const& ghostAttribs)
		{
			return isGhostVisible(scene, object, object->component<TiledLensFlareComponent>().m_camera, ghostAttribs);
		}

		////////////////////////////////////////////////////////////////////////////////
		std::ostream& operator<<(std::ostream& ostream, RenderGhostAttribs const& ghostAttribs)
		{
			ostream << "attribs{ " << std::endl;
			ostream << " - pupilMin: " << ghostAttribs.m_pupilMin << std::endl;
			ostream << " - pupilMax: " << ghostAttribs.m_pupilMax << std::endl;
			ostream << " - sensorMin: " << ghostAttribs.m_sensorMin << std::endl;
			ostream << " - sensorMax: " << ghostAttribs.m_sensorMax << std::endl;
			ostream << " - intensityScale: " << ghostAttribs.m_intensityScale << std::endl;
			ostream << " - avgIntensity: " << ghostAttribs.m_avgIntensity << std::endl;
			ostream << " - rayGridSize: " << ghostAttribs.m_rayGridSize << " }";
			return ostream;
		}

		////////////////////////////////////////////////////////////////////////////////
		void rotateGrid(glm::vec2& gridMin, glm::vec2& gridMax, const float rotation)
		{
			// Compute a 2D rotation matrix
			const glm::mat2 rotationMatrix = glm::mat2(glm::rotate(rotation, glm::vec3(0.0f, 0.0f, 1.0f)));

			// Edge vector of the grid
			const glm::vec2 gridEdge = (gridMax - gridMin);

			// Compute the rotated corners of the bounding rectangles
			const glm::vec2 gridCorners[4] =
			{
				rotationMatrix * (gridMin + glm::vec2(0.0f,       0.0f)),
				rotationMatrix * (gridMin + glm::vec2(gridEdge.x, 0.0f)),
				rotationMatrix * (gridMin + glm::vec2(0.0f,       gridEdge.y)),
				rotationMatrix * (gridMin + glm::vec2(gridEdge.x, gridEdge.y)),
			};

			// Compute the rectangle enclosing all 4 rotated grid corners
			gridMin = glm::vec2(FLT_MAX); gridMax = glm::vec2(-FLT_MAX);
			for (int i = 0; i < 4; ++i)
			{
				gridMin = glm::min(gridMin, gridCorners[i]);
				gridMax = glm::max(gridMax, gridCorners[i]);
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		template<typename GhostAttribs>
		float getProjectedGhostIntensity(Scene::Scene& scene, Scene::Object* object, GhostAttribs const& ghostAttribs)
		{
			if (!isGhostValid(scene, object, ghostAttribs)) return 0.0f;

			// Extract the camera
			PhysicalCamera::PhysicalCameraAttributes const& camera = object->component<TiledLensFlareComponent>().m_camera;
			
			// Surface area of the whole pupil and sensor
			const float fullPupilArea = glm::pow(camera.m_lenses[0].m_height * 2.0f, 2.0f);
			const float fullSensorArea = camera.m_filmSize.x * camera.m_filmSize.y;

			// Surface area of the ghost on the pupil and the sensor
			const float ghostPupilArea = glm::max(1.0f, glm::dot(ghostAttribs.m_pupilMax - ghostAttribs.m_pupilMin, glm::vec2(1.0f)));
			const float ghostSensorArea = glm::max(1.0f, glm::dot(ghostAttribs.m_sensorMax - ghostAttribs.m_sensorMin, glm::vec2(1.0f)));

			// Ghost intensity, based on the relative surface areas
			return (ghostPupilArea / fullPupilArea) * (ghostPupilArea / ghostSensorArea);
		}

		////////////////////////////////////////////////////////////////////////////////
		float computeGhostIntensityScale(Scene::Scene& scene, Scene::Object* object, const float lambert, const float projectedGhostIntensity)
		{
			return projectedGhostIntensity * lambert * object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_intensityScale;
		}

		////////////////////////////////////////////////////////////////////////////////
		template<typename GhostAttribs>
		float computeGhostIntensityScale(Scene::Scene& scene, Scene::Object* object, const float lambert, GhostAttribs const& ghostAttribs)
		{
			return computeGhostIntensityScale(scene, object, lambert, getProjectedGhostIntensity(scene, object, ghostAttribs));
		}

		////////////////////////////////////////////////////////////////////////////////
		int computeReducedRayGridSize(Scene::Scene& scene, Scene::Object* object, RenderGhostAttribs const& ghostAttribs)
		{
			const glm::vec2 filmSize = object->component<TiledLensFlareComponent>().m_camera.m_filmSize;
			const glm::vec2 projectedSize = ghostAttribs.m_sensorMax - ghostAttribs.m_sensorMin;
			const float areaRatio = (projectedSize.x * projectedSize.y) / (filmSize.x * filmSize.y);
			const float reductionScale = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_rayCountReductionScale;
			const float reductionPower = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_rayCountReductionPower;
			const float reduction = glm::clamp(glm::pow(areaRatio * reductionScale, reductionPower), 0.05f, 1.0f);

			return glm::ceil(ghostAttribs.m_rayGridSize * reduction);
		}

		////////////////////////////////////////////////////////////////////////////////
		// theta    - rotation on the xz plane
		// rotation - rotation about the optical axis
		RenderGhostAttribs getGhostAttribsNoGhostAttribs(Scene::Scene& scene, Scene::Object* object, 
			const size_t ghostID, const size_t channelID, const float theta, const float rotation, const float lambert)
		{
			RenderGhostAttribs result;

			// Initialize the resulting entry with default pupil coordinates
			const float pupilHeight = object->component<TiledLensFlareComponent>().m_camera.m_lenses[0].m_height * object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_pupilExpansion;
			result.m_pupilMin = glm::vec2(-pupilHeight);
			result.m_pupilMax = glm::vec2(pupilHeight);
			result.m_pupilCenter = glm::vec2(0.0f);
			result.m_pupilRadius= glm::vec2(0.0f);

			const glm::vec2 filmSize = object->component<TiledLensFlareComponent>().m_camera.m_filmSize;
			result.m_sensorMin = -0.5f * filmSize;
			result.m_sensorMax = 0.5f * filmSize;

			// Compute the total intensity scale factor based on the incident light angle and the ghost intensity scaling
			result.m_avgIntensity = 1.0f;
			result.m_intensityScale = computeGhostIntensityScale(scene, object, lambert, result);
			result.m_avgIntensity = 1.0f * result.m_intensityScale;

			// Initialize the ray grid size using to the fixed size
			result.m_rayGridSize = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_rayCount;

			// Return this as the result
			return result;
		}

		////////////////////////////////////////////////////////////////////////////////
		// theta    - rotation on the xz plane
		// rotation - rotation about the optical axis
		RenderGhostAttribs getGhostAttribsRayTraced(Scene::Scene& scene, Scene::Object* object, 
			const size_t ghostID, const size_t channelID, const float theta, const float rotation, const float lambert,
			const bool lookForward = true, const bool applyRotation = true, const bool addPolynomialSlack = true)
		{
			// Compute the 2 neighboring angle ids
			auto const& angleIDs = getAngleIndices(scene, object, object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters, theta);
			auto const& attribNames = getGhostAttribNames(scene, object, ghostID, channelID, angleIDs);

			// Make sure that the corresponding ghost attribute entries exist
			auto& ghostAttribs = object->component<TiledLensFlareComponent>().m_precomputedGhostAttribs;
			if (ghostAttribs.find(attribNames[0]) == ghostAttribs.end() || ghostAttribs.find(attribNames[1]) == ghostAttribs.end())
				return getGhostAttribsNoGhostAttribs(scene, object, ghostID, channelID, theta, rotation, lambert);

			// Construct the partial result
			RenderGhostAttribs result;
			result.m_rayGridSize = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_rayCount;

			// Extract the two neighboring ghost parameters
			std::array<PrecomputeGhostAttribs, 2> neighboringGhostAttribs = { ghostAttribs[attribNames[0]], ghostAttribs[attribNames[1]] };
			const std::array<bool, 2> neighborValidities = { isGhostValid(scene, object, neighboringGhostAttribs[0]), isGhostValid(scene, object, neighboringGhostAttribs[1]) };

			// Compute the pupil rectangle enclosing the two neighboring ghost attribs
			for (size_t i = 0; i < (lookForward ? 2 : 1); ++i)
			{
				PrecomputeGhostAttribs const& ghostAttribs = neighboringGhostAttribs[i];
				//Debug::log_debug() << "Ghost[" << ghostID << "][" << channelID << "][" << i << "]: " << to_string(ghostAttribs) << ": " << isGhostValid(scene, object, ghostAttribs) << Debug::end;
				if (neighborValidities[i])
				{
					result.m_pupilMin = glm::min(result.m_pupilMin, ghostAttribs.m_pupilMin);
					result.m_pupilMax = glm::max(result.m_pupilMax, ghostAttribs.m_pupilMax);
					result.m_pupilCenter = glm::min(result.m_pupilCenter, (ghostAttribs.m_pupilMax + ghostAttribs.m_pupilMin) / 2.0f);
					result.m_pupilRadius = glm::max(result.m_pupilRadius, (ghostAttribs.m_pupilMax - ghostAttribs.m_pupilMin) / 2.0f);
					result.m_sensorMin = glm::min(result.m_sensorMin, ghostAttribs.m_sensorMin);
					result.m_sensorMax = glm::max(result.m_sensorMax, ghostAttribs.m_sensorMax);
					result.m_intensityScale = glm::max(result.m_intensityScale, computeGhostIntensityScale(scene, object, lambert, ghostAttribs));
					result.m_avgIntensity = glm::max(result.m_avgIntensity, ghostAttribs.m_avgIntensity);
				}
			}

			// Mix the neighbor intensities if both neighbors are valid
			if (lookForward && neighborValidities[0] && neighborValidities[1])
			{
				result.m_pupilCenter = glm::mix(
					(neighboringGhostAttribs[0].m_pupilMax + neighboringGhostAttribs[0].m_pupilMin) / 2.0f,
					(neighboringGhostAttribs[1].m_pupilMax + neighboringGhostAttribs[1].m_pupilMin) / 2.0f,
					angleIDs.m_lerpFactor);
				result.m_pupilRadius = glm::mix(
					(neighboringGhostAttribs[0].m_pupilMax - neighboringGhostAttribs[0].m_pupilMin) / 2.0f,
					(neighboringGhostAttribs[1].m_pupilMax - neighboringGhostAttribs[1].m_pupilMin) / 2.0f,
					angleIDs.m_lerpFactor);
				result.m_intensityScale = glm::mix(
					computeGhostIntensityScale(scene, object, lambert, neighboringGhostAttribs[0]),
					computeGhostIntensityScale(scene, object, lambert, neighboringGhostAttribs[1]),
					angleIDs.m_lerpFactor);
				result.m_avgIntensity = glm::mix(
					neighboringGhostAttribs[0].m_avgIntensity, 
					neighboringGhostAttribs[1].m_avgIntensity, 
					angleIDs.m_lerpFactor
				);
			}

			// Weigh the average intensity by the intensity scale
			result.m_avgIntensity *= result.m_intensityScale;

			// Compute the dynamic ray grid size
			if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_rayCountMethod == RenderGhostsParameters::DynamicSensorSizeBased)
				result.m_rayGridSize = computeReducedRayGridSize(scene, object, result);

			// Rotate the grids
			switch (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod)
			{
			case RenderGhostsParameters::Analytical:
				if (applyRotation) rotateGrid(result.m_pupilMin, result.m_pupilMax, rotation);
				break;
			case RenderGhostsParameters::PolynomialFullFit:
			case RenderGhostsParameters::PolynomialPartialFit:
				if (applyRotation && object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_numRotations > 1)
					rotateGrid(result.m_pupilMin, result.m_pupilMax, rotation);
				else if (addPolynomialSlack)
				{
					const float slackFactor = glm::abs(glm::sin(rotation * 2.0f));
					const float pupilHeight = object->component<TiledLensFlareComponent>().m_camera.m_lenses[0].m_height;
					addSlackPercentage(result.m_pupilMin, result.m_pupilMax, slackFactor * object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_rotatedBoundsSlackPercentage);
					addSlackAbsolute(result.m_pupilMin, result.m_pupilMax, slackFactor * pupilHeight * object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_rotatedBoundsSlackAbsolute);
				}
				/*
				else
				{
					rotateGrid(result.m_pupilMin, result.m_pupilMax, rotation);
					rotateGrid(result.m_pupilMin, result.m_pupilMax, -rotation);
				}
				*/
				break;
			}
			if (applyRotation) rotateGrid(result.m_sensorMin, result.m_sensorMax, rotation);

			return result;
		}

		////////////////////////////////////////////////////////////////////////////////
		// theta    - rotation on the xz plane
		// rotation - rotation about the optical axis
		RenderGhostAttribs getGhostAttribsParaxial(Scene::Scene& scene, Scene::Object* object, 
			const size_t ghostID, const size_t channelID, const float theta, const float rotation, const float lambert)
		{
			DateTime::ScopedTimer timer = DateTime::ScopedTimer(Debug::Debug, 1, DateTime::Milliseconds, "Ghost Bounding");

			// Resulting bounding information
			RenderGhostAttribs result;

			// Initialize the resulting entry with default pupil coordinates
			const float pupilHeight = object->component<TiledLensFlareComponent>().m_camera.m_lenses[0].m_height;
			result.m_pupilMin = glm::vec2(-pupilHeight);
			result.m_pupilMax = glm::vec2(pupilHeight);

			const glm::vec2 filmSize = object->component<TiledLensFlareComponent>().m_camera.m_filmSize;
			result.m_sensorMin = -0.5f * filmSize;
			result.m_sensorMax = 0.5f * filmSize;

			// Compute the total intensity scale factor based on the incident light angle and the ghost intensity scaling
			result.m_intensityScale = computeGhostIntensityScale(scene, object, lambert, 1.0f);
			result.m_avgIntensity = 1.0f * result.m_intensityScale;

			// Initialize the ray grid size using to the fixed size
			result.m_rayGridSize = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_rayCount;

			// Calculate the ghost attribs
			PrecomputeGhostAttribs ghostAttribs = computeGhostAttribMatrixMethod(scene, object, ghostID, channelID, theta);

			// Early out
			if (ghostAttribs.m_avgIntensity == 0.0f)
				return result;

			// Copy over the attributes
			result.m_pupilMin = ghostAttribs.m_pupilMin;
			result.m_pupilMax = ghostAttribs.m_pupilMax;
			result.m_sensorMin = ghostAttribs.m_sensorMin;
			result.m_sensorMax = ghostAttribs.m_sensorMax;

			// In analytical mode, we also rotate the grid
			if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod == RenderGhostsParameters::Analytical)
			{
				rotateGrid(result.m_pupilMin, result.m_pupilMax, rotation);
			}

			// Compute the sensor rectangle enclosing the two neighboring ghost attribs
			rotateGrid(result.m_sensorMin, result.m_sensorMax, rotation);

			// Compute the intensity scale factor by lerping the two neighboring ghost intensities
			const float projectedGhostIntensity = getProjectedGhostIntensity(scene, object, result);
			result.m_intensityScale = computeGhostIntensityScale(scene, object, lambert, projectedGhostIntensity);
			result.m_avgIntensity = 1.0f * result.m_intensityScale;

			// Compute the ray grid size
			result.m_rayGridSize = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_rayCount;
			if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_rayCountMethod == RenderGhostsParameters::DynamicSensorSizeBased)
				result.m_rayGridSize = computeReducedRayGridSize(scene, object, result);

			// Return the result
			return result;
		}

		////////////////////////////////////////////////////////////////////////////////
		// theta    - rotation on the xz plane
		// rotation - rotation about the optical axis
		RenderGhostAttribs getGhostAttribs(Scene::Scene& scene, Scene::Object* object, 
			const size_t ghostID, const size_t channelID, const float angle, const float rotation, const float lambert)
		{
			switch (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_ghostAttribsMethod)
			{
			case RenderGhostsParameters::RayTracedGhostAttribs: return getGhostAttribsRayTraced(scene, object, ghostID, channelID, angle, rotation, lambert);
			case RenderGhostsParameters::ParaxialGhostAttribs: return getGhostAttribsParaxial(scene, object, ghostID, channelID, angle, rotation, lambert);
			case RenderGhostsParameters::NoGhostAttribs: return getGhostAttribsNoGhostAttribs(scene, object, ghostID, channelID, angle, rotation, lambert);
			}
			return getGhostAttribsNoGhostAttribs(scene, object, ghostID, channelID, angle, rotation, lambert);
		}

		////////////////////////////////////////////////////////////////////////////////
		// theta    - rotation on the xz plane
		// rotation - rotation about the optical axis
		RenderGhostAttribs getGhostAttribs(Scene::Scene& scene, Scene::Object* object, const size_t ghostID, const size_t channelID, LightSources::LightSourceData const& lightData)
		{
			return getGhostAttribs(scene, object, ghostID, channelID, lightData.m_angle, lightData.m_rotation, lightData.m_lambert);
		}

		////////////////////////////////////////////////////////////////////////////////
		void setDynamicIntensityScale(Scene::Scene& scene, Scene::Object* object)
		{
			// Only do this if a dynamic intensity scale is usaed
			if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_intensityMethod != RenderGhostsParameters::IntensityMethod::DynamicIntensity)
				return;

			// Precomputation parameters
			PrecomputeGhostsParameters const& precomputeParams = object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters;
			auto& ghostAttribsMap = object->component<TiledLensFlareComponent>().m_precomputedGhostAttribs;

			// Ghost parameters
			auto const& ghostIndices = object->component<TiledLensFlareComponent>().m_ghostIndices;
			const size_t numGhosts = ghostIndices.size();
			const size_t numAngles = size_t(precomputeParams.m_maxAngle / precomputeParams.m_angleStep) + 1;
			const size_t numChannels = precomputeParams.m_numChannels;

			// Calculate the average intensity
			float totalIntensity = 0.0f;
			int numValidGhosts = 0;
			for (size_t ghostID = 0; ghostID < numGhosts; ++ghostID)
			for (size_t channelID = 0; channelID < numChannels; ++channelID)
			{
				if (ghostAttribsMap.find(getGhostAttribName(scene, object, ghostID, channelID, 0)) == ghostAttribsMap.end()) 
					continue;

				for (size_t angleID = 0; angleID < numAngles; ++angleID)
				{
					auto const& attribName = getGhostAttribName(scene, object, ghostID, channelID, angleID);
					if (ghostAttribsMap.find(attribName) == ghostAttribsMap.end()) continue;
					PrecomputeGhostAttribs const& attrib = ghostAttribsMap[attribName];

					if (isGhostValid(scene, object, attrib))
					{
						totalIntensity += attrib.m_avgIntensity;
						++numValidGhosts;
					}
				}
			}

			// Make sure there are valid ghosts
			if (numValidGhosts == 0) return;

			// Compute the final intensity scale
			const float avgIntensity = totalIntensity / numValidGhosts;
			const float targetIntensity = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_targetIntensity;
			object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_intensityScale = targetIntensity / avgIntensity;
		}

		////////////////////////////////////////////////////////////////////////////////
		void runPerformanceBenchmark(Scene::Scene& scene, Scene::Object* object)
		{
			Debug::log_info() << "Running performance benchmark for " << object->component<TiledLensFlareComponent>().m_camera.m_name << Debug::end;

			PrecomputeGhostsParameters const& precomputeParams = object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters;

			// Enumerate the possible ghosts
			auto const& ghostIndices = object->component<TiledLensFlareComponent>().m_ghostIndices;
			const size_t numGhosts = ghostIndices.size();
			const size_t numAngles = size_t(precomputeParams.m_maxAngle / precomputeParams.m_angleStep) + 1;
			const size_t numChannels = precomputeParams.m_numChannels;
			const size_t numAttribs = numChannels * numAngles * numGhosts;

			// Various timers
			DateTime::Timer transferMatrices;
			DateTime::Timer matrixMethod;
			DateTime::Timer raytracedMethod;

			// Calculate transfer matrices
			{
				transferMatrices.start();
				TransferMatrices::computeRaytraceLensFlareTransferMatrices(scene, object);
				transferMatrices.stop();
			}

			// Calculate bounds with the matrix method
			{
				matrixMethod.start();

				for (size_t angleID = 0; angleID < numAngles; ++angleID)
				for (size_t ghostID = 0; ghostID < numGhosts; ++ghostID)
				for (size_t channelID = 0; channelID < numChannels; ++channelID)
					computeGhostAttribMatrixMethod(scene, object, ghostID, channelID, precomputeParams.m_angleStep * angleID);

				matrixMethod.stop();
			}

			// Calculate bounds with the ray-traced
			{
				raytracedMethod.start();

				computeAllGhostAttribs(scene, object);

				raytracedMethod.stop();
			}

			Debug::log_info() << " > Transfer matrices total time: " << transferMatrices.getElapsedTime(DateTime::Milliseconds) << ", "
				<< "average: " << transferMatrices.getAvgTime(numGhosts, DateTime::Microseconds) << Debug::end;
			Debug::log_info() << " > Matrix method total time: " << matrixMethod.getElapsedTime(DateTime::Milliseconds) << ", "
				<< "average: " << matrixMethod.getAvgTime(numAngles, DateTime::Microseconds) << Debug::end;
			Debug::log_info() << " > Ray-traced method total time: " << raytracedMethod.getElapsedTime(DateTime::Milliseconds) << ", "
				<< "average: " << raytracedMethod.getAvgTime(numAngles, DateTime::Microseconds) << Debug::end;
		}

		////////////////////////////////////////////////////////////////////////////////
		void runAccuracyBenchmark(Scene::Scene& scene, Scene::Object* object,
			std::vector<PrecomputeGhostAttribs> const& matrixMethodAttribs,
			std::vector<PrecomputeGhostAttribs> const& raytraceMethodAttribs,
			const size_t minIndex, const size_t maxIndex, const size_t numAngles)
		{
			// Camera parameters
			auto const& camera = object->component<TiledLensFlareComponent>().m_camera;
			const float outerHeight = camera.m_lenses[0].m_height;

			// Process each ghost
			int rtInvis = 0, mInvis = 0, totalVis = 0;
			float totalCenterDiff = 0.0f, totalSizeDiff = 0.0f;
			for (size_t ghostId = minIndex; ghostId < maxIndex; ++ghostId)
			{
				// Extract the structures
				auto const& rtBounds = raytraceMethodAttribs[ghostId];
				auto const& mBounds = matrixMethodAttribs[ghostId];

				// Compute the squared pupil bounds difference
				const float pupilDiff = glm::sqrt(
					glm::pow(glm::length(rtBounds.m_pupilMin - mBounds.m_pupilMin), 2.0f) +
					glm::pow(glm::length(rtBounds.m_pupilMax - mBounds.m_pupilMax), 2.0f));
				const float sensorDiff = glm::sqrt(
					glm::pow(glm::length(rtBounds.m_sensorMin - mBounds.m_sensorMin), 2.0f) +
					glm::pow(glm::length(rtBounds.m_sensorMax - mBounds.m_sensorMax), 2.0f));

				// Log the pupil comparisons
				Debug::log_debug() << ghostId << "  >> raytraced pupil bounds, area: " <<
					rtBounds.m_pupilMin << ", " << rtBounds.m_pupilMax << ", " <<
					glm::dot(rtBounds.m_pupilMax - rtBounds.m_pupilMin, glm::vec2(1.0)) << Debug::end;
				Debug::log_debug() << ghostId << "  >> matrix pupil bounds, area: " <<
					mBounds.m_pupilMin << ", " << mBounds.m_pupilMax << ", " <<
					glm::dot(mBounds.m_pupilMax - mBounds.m_pupilMin, glm::vec2(1.0)) << Debug::end;
				Debug::log_debug() << ghostId << "  >> pupil diff: " << pupilDiff << Debug::end;

				// Log the sensor comparisons
				Debug::log_debug() << ghostId << "  >> raytraced sensor bounds, area: " <<
					rtBounds.m_sensorMin << ", " << rtBounds.m_sensorMax << ", " <<
					glm::dot(rtBounds.m_sensorMax - rtBounds.m_sensorMin, glm::vec2(1.0)) << Debug::end;
				Debug::log_debug() << ghostId << "  >> matrix sensor bounds, area: " <<
					mBounds.m_sensorMin << ", " << mBounds.m_sensorMax << ", " <<
					glm::dot(mBounds.m_sensorMax - mBounds.m_sensorMin, glm::vec2(1.0)) << Debug::end;
				Debug::log_debug() << ghostId << "  >> sensor diff: " << sensorDiff << Debug::end;

				// Log other attributes
				Debug::log_debug() << ghostId << "  >> raytraced intensity: " << rtBounds.m_avgIntensity << Debug::end;
				Debug::log_debug() << ghostId << "  >> matrix intensity: " << mBounds.m_avgIntensity << Debug::end;

				// Count the invisible ghosts
				if (rtBounds.m_avgIntensity == 0.0f)
				{
					Debug::log_debug() << "  >> ghost[" << ghostId << "]: invisible in ray-traced." << Debug::end;
					++rtInvis;
				}
				if (mBounds.m_avgIntensity == 0.0f)
				{
					Debug::log_debug() << "  >> ghost[" << ghostId << "]: invisible in matrix." << Debug::end;
					++mInvis;
				}

				// Skip invisible ghosts
				if (rtBounds.m_avgIntensity == 0.0f || mBounds.m_avgIntensity == 0.0f)
					continue;

				// Compute the center distance
				const float centerDist = glm::distance(
					(mBounds.m_pupilMin + mBounds.m_pupilMax) * 0.5f,
					(rtBounds.m_pupilMin + rtBounds.m_pupilMax) * 0.5f);
				totalCenterDiff += centerDist;

				// Compute the squared pupil bounds difference
				const float sizeDiff = glm::length(rtBounds.m_pupilMax - mBounds.m_pupilMax);
				totalSizeDiff += sizeDiff;

				// Increment the visible ghost counter
				++totalVis;
			}

			// Print the ratio of invisible ghosts
			const float averageCenterDiff = totalCenterDiff / totalVis;
			const float averageCenterDiffNorm = averageCenterDiff / outerHeight;
			const float averageSizeDiff = totalSizeDiff / totalVis;
			const float averageSizeDiffNorm = averageSizeDiff / outerHeight;
			const float invisiblesFound = ((float)mInvis / (float)rtInvis);
			Debug::log_info() << "  >> Average center diff: " << averageCenterDiff << " (" << averageCenterDiffNorm << ")" << Debug::end;
			Debug::log_info() << "  >> Average size diff: " << averageSizeDiff << " (" << averageSizeDiffNorm << ")" << Debug::end;
			Debug::log_info() << "  >> Invisible ghosts: " << rtInvis << ", " << mInvis << ", " << invisiblesFound << " (" << invisiblesFound * 100 << "%)" << Debug::end;
		}

		////////////////////////////////////////////////////////////////////////////////
		void runAccuracyBenchmark(Scene::Scene& scene, Scene::Object* object)
		{
			Debug::log_info() << "Running accuracy benchmark for " << object->component<TiledLensFlareComponent>().m_camera.m_name << Debug::end;

			PrecomputeGhostsParameters const& precomputeParams = object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters;

			// Enumerate the possible ghosts
			auto const& ghostIndices = object->component<TiledLensFlareComponent>().m_ghostIndices;
			const size_t numGhosts = ghostIndices.size();
			const size_t numAngles = size_t(precomputeParams.m_maxAngle / precomputeParams.m_angleStep) + 1;
			const size_t numChannels = precomputeParams.m_numChannels;
			const size_t numAttribs = numChannels * numAngles * numGhosts;

			// Ghost attributes
			std::vector<PrecomputeGhostAttribs> matrixMethodAttribs(numAttribs);
			std::vector<PrecomputeGhostAttribs> raytraceMethodAttribs(numAttribs);

			// Extract the bounds
			for (size_t angleID = 0; angleID < numAngles; ++angleID)
			for (size_t ghostID = 0; ghostID < numGhosts; ++ghostID)
			for (size_t channelID = 0; channelID < numChannels; ++channelID)
			{
				const float theta = angleID * object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_maxAngle / (numAngles - 1);
				const size_t attribID = angleID * (numGhosts * numChannels) + ghostID * numChannels + channelID;
				auto const& attribName = getGhostAttribName(scene, object, ghostID, channelID, angleID);
				raytraceMethodAttribs[attribID] = object->component<TiledLensFlareComponent>().m_precomputedGhostAttribs[attribName];
				matrixMethodAttribs[attribID] = computeGhostAttribMatrixMethod(scene, object, ghostID, channelID, theta);
			}

			// Run the global benchmark
			Debug::log_info() << " > Global: " << Debug::end;
			runAccuracyBenchmark(scene, object, matrixMethodAttribs, raytraceMethodAttribs, 0, raytraceMethodAttribs.size(), numAngles);
			std::vector<float> anglesToTest = { 0.0f, 22.5f, 45.0f };

			for (const float thetaDeg : anglesToTest)
			{
				const float theta = glm::radians(thetaDeg);
				const size_t startId = size_t(theta / (object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_maxAngle / (numAngles - 1)));
				const size_t endId = startId + numChannels * numGhosts;

				Debug::log_info() << " > Angle[" << thetaDeg << "]" << Debug::end;

				runAccuracyBenchmark(scene, object, matrixMethodAttribs, raytraceMethodAttribs, startId, endId, 1);
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		void runBenchmark(Scene::Scene& scene, Scene::Object* object)
		{
			runPerformanceBenchmark(scene, object);
			runAccuracyBenchmark(scene, object);
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	#define STATIC_STRING_ARRAY(NAME, ...) \
		static const std::string _##NAME##[] = { __VA_ARGS__ }; \
		static const std::array<std::string, ARRAYSIZE(_##NAME##)> s_##NAME## = { __VA_ARGS__ }

	////////////////////////////////////////////////////////////////////////////////
	namespace PolynomialsCommon
	{
		////////////////////////////////////////////////////////////////////////////////
		STATIC_STRING_ARRAY(polynomialInputVariables,
			"pupil_pos_cartesian_x",
			"pupil_pos_cartesian_y",
			"pupil_pos_polar_normalized_x",
			"pupil_pos_polar_normalized_inverted_x",
			"angle_deg",
			"lambda_nm"
		);

		////////////////////////////////////////////////////////////////////////////////
		STATIC_STRING_ARRAY(polynomialOutputVariables,
			"aperture_pos_x",
			"aperture_pos_y",
			"intensity",
			"sensor_pos_x",
			"sensor_pos_y",
			"radius"
		);

		////////////////////////////////////////////////////////////////////////////////
		static const size_t NUM_POLYNOMIAL_INPUT_VARIABLES = ARRAYSIZE(_polynomialInputVariables);
		static const size_t NUM_POLYNOMIAL_OUTPUT_VARIABLES = ARRAYSIZE(_polynomialOutputVariables);

		///////////////////////////////////////////////////////////////////////////////
		#ifdef AUTODIFF_FORWARD
		using AutodiffVar = autodiff::real;
		using AutodiffVarArray = autodiff::ArrayXreal;
		#endif

		///////////////////////////////////////////////////////////////////////////////
		#ifdef AUTODIFF_REVERSE
		using AutodiffVar = autodiff::var;
		using AutodiffVarArray = autodiff::ArrayXvar;
		#endif

		////////////////////////////////////////////////////////////////////////////////
		// Ghost geometry for fitting
		// - #1: angle
		// - #2: rotation
		// - #3: wavelength
		using PrecomputedGhostGeometry = boost::multi_array<GhostGeometry::GhostGeometry, 3>;

		////////////////////////////////////////////////////////////////////////////////
		template<size_t N>
		using PolynomialInputN = std::array<float, N>;

		////////////////////////////////////////////////////////////////////////////////
		struct FitDataSamplePointBase
		{
			float* m_inputPtr;
			float m_value;
			float m_weight;
			GhostGeometry::ValidityFlags m_validityFlags;
		};

		////////////////////////////////////////////////////////////////////////////////
		template<size_t N>
		struct FitDataSamplePointN: public FitDataSamplePointBase
		{
			PolynomialInputN<N> m_input;
			FitDataSamplePointN():
				FitDataSamplePointBase{ m_input.data() }
			{}
		};

		////////////////////////////////////////////////////////////////////////////////
		// Data points for a single variable
		template<size_t N>
		using FitDataPointsN = std::vector<FitDataSamplePointN<N>>;

		////////////////////////////////////////////////////////////////////////////////
		// Data points for a single variable and multiple incidence angles
		// - #1: angle
		// - #2: rotation
		// - #3: wavelength
		template<size_t N>
		using FitDataSetN = boost::multi_array<FitDataPointsN<N>, 3>;

		////////////////////////////////////////////////////////////////////////////////
		struct FitStatsData
		{
			size_t m_numPermutationsTested = 0;
			size_t m_numIterations = 0;
			size_t m_numTerms = 0;
			float m_finalErrorSubSampled = 0.0f;
			float m_finalErrorFullDataset = 0.0f;
			DateTime::Timer m_computationTimer;

			size_t m_numComputations = 0;

			FitStatsData& operator+=(FitStatsData const& other)
			{
				m_numPermutationsTested += other.m_numPermutationsTested;
				m_numIterations += other.m_numIterations;
				m_numTerms += other.m_numTerms;
				m_finalErrorSubSampled += other.m_finalErrorSubSampled;
				m_finalErrorFullDataset += other.m_finalErrorFullDataset;
				++m_numComputations;
				return *this;
			}

			size_t numComputations() const { return std::max(size_t(1), m_numComputations); }
		};

		////////////////////////////////////////////////////////////////////////////////
		// Fit results a single variable
		template<size_t N>
		struct FitResultEntryN
		{
			PolynomialN<N> m_polynomial;
			FitStatsData m_fitStats;
		};

		////////////////////////////////////////////////////////////////////////////////
		// Fit results a single variable and multiple incidence angles
		// - #1: angle
		// - #2: rotation
		// - #3: wavelength
		template<size_t N>
		using FitResultN = boost::multi_array<FitResultEntryN<N>, 3>;

		////////////////////////////////////////////////////////////////////////////////
		template<size_t N>
		std::ostream& operator<<(std::ostream& ostream, FitDataSamplePointN<N> const& sample)
		{
			ostream << "Sample" << "{ ";
			ostream << "input: (";
			for (size_t variableID = 0; variableID < N; ++variableID)
			{
				if (variableID > 0) ostream << ", ";
				ostream << sample.m_input[variableID];
			}
			ostream << ")" << ", ";
			ostream << "weight: " << sample.m_value << ", ";
			ostream << "val: " << sample.m_value << " } ";
			return ostream;
		}

		////////////////////////////////////////////////////////////////////////////////
		std::ostream& operator<<(std::ostream& ostream, FitStatsData const& fitStats)
		{
			const size_t numComputations = fitStats.numComputations();
			if (numComputations <= 1)
			{
				ostream << "   = #computation time: "
					<< fitStats.m_computationTimer.getElapsedTime(DateTime::Seconds)
					<< std::endl;
				ostream << "   = #iterations: " 
					<< fitStats.m_numIterations 
					<< std::endl;
				ostream << "   = #permutations: " 
					<< fitStats.m_numPermutationsTested 
					<< std::endl;
				ostream << "   = #terms: " 
					<< fitStats.m_numTerms 
					<< std::endl;
			}
			else
			{
				ostream << "   = #computation time: "
					<< fitStats.m_computationTimer.getElapsedTime(DateTime::Seconds) << " (total)" << ", "
					<< fitStats.m_computationTimer.getAvgTime(numComputations, DateTime::Seconds) << " (average)"
					<< std::endl;
				ostream << "   = #iterations: " 
					<< fitStats.m_numIterations << " (total)" << ", "
					<< float(fitStats.m_numIterations) / float(numComputations) << " (average)"
					<< std::endl;
				ostream << "   = #permutations: " 
					<< fitStats.m_numPermutationsTested << " (total)" << ", "
					<< float(fitStats.m_numPermutationsTested) / float(numComputations) << " (average)"
					<< std::endl;
				ostream << "   = #terms: "
					<< fitStats.m_numTerms << " (total)" << ", "
					<< float(fitStats.m_numTerms) / float(numComputations) << " (average)"
					<< std::endl;
				ostream << "   = #computations: "
					<< numComputations
					<< std::endl;
			}
			ostream << "   = final error: " 
				<< float(fitStats.m_finalErrorSubSampled / numComputations) << " (subsampled)" << ", "
				<< float(fitStats.m_finalErrorFullDataset / numComputations) << " (full)";
			return ostream;
		}

		///////////////////////////////////////////////////////////////////////////////
		namespace Monomials
		{
			////////////////////////////////////////////////////////////////////////////////
			double upow(const float x, const int y)
			{
				if (y == 0) return                       1.0;
				if ((y % 2) == 1 && x <= 0.0f) return   -glm::pow(double(-x), double(y));
				return                                   glm::pow(double(glm::abs(x)), double(y));
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			double evalMonomial(PolynomialInputN<N> const& x, const float coefficient, const int* degrees)
			{
				double result = coefficient;
				for (size_t i = 0; i < N; ++i)
					result *= upow(x[i], degrees[i]);
				return result;
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			AutodiffVar evalMonomial(PolynomialInputN<N> const& x, AutodiffVar coefficient, const int* degrees)
			{
				return coefficient * evalMonomial(x, 1.0f, degrees);
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			double evalMonomial(FitDataSamplePointN<N> const& sample, MonomialN<N> const& monomial)
			{
				return evalMonomial(sample.m_input, monomial.m_coefficient, monomial.m_degrees);
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			double evalMonomial(FitDataSamplePointN<N> const& sample, MonomialN<N> const& monomial, const float coefficient)
			{
				return evalMonomial(sample.m_input, coefficient, monomial.m_degrees);
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			AutodiffVar evalMonomial(FitDataSamplePointN<N> const& sample, MonomialN<N> const& monomial, AutodiffVar coefficient)
			{
				return evalMonomial(sample.m_input, coefficient, monomial.m_degrees);
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			float evalPolynomial(PolynomialN<N> const& polynomial, FitDataSamplePointN<N> const& sample)
			{
				float sampleVal = 0.0f;
				for (size_t termID = 0; termID < polynomial.size(); ++termID)
					sampleVal += Monomials::evalMonomial(sample, polynomial[termID]);
				return sampleVal;
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			Eigen::VectorXd evalPolynomial(PolynomialN<N> const& polynomial, FitDataPointsN<N> const& dataPoints, Eigen::VectorXd const& x)
			{
				Eigen::VectorXd result = Eigen::VectorXd::Zero(dataPoints.size());
				for (size_t termID = 0; termID < polynomial.size(); ++termID)
					for (size_t sampleID = 0; sampleID < dataPoints.size(); ++sampleID)
						result[sampleID] += evalMonomial(dataPoints[sampleID], polynomial[termID], x[termID]);
				return result;
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			AutodiffVarArray evalPolynomial(PolynomialN<N> const& polynomial, FitDataPointsN<N> const& dataPoints, AutodiffVarArray const& x)
			{
				AutodiffVarArray result = autodiff::ArrayXreal::Zero(dataPoints.size());
				for (size_t termID = 0; termID < polynomial.size(); ++termID)
					for (size_t sampleID = 0; sampleID < dataPoints.size(); ++sampleID)
						result[sampleID] += evalMonomial(dataPoints[sampleID], polynomial[termID], x(termID));
				return result;
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		namespace SampleValidity
		{
			////////////////////////////////////////////////////////////////////////////////
			bool isGeometryEntryValid(Scene::Scene& scene, Scene::Object* object,
				PolynomialFitParameters const& fitParams, GhostGeometry::Filtering::ClipParameters clipParameters,
				GhostGeometry::GeometryEntry const& entry)
			{
				return 
					(fitParams.m_ghostGeometryParameters.m_smoothenData || entry.m_validityFlags.m_isTractable) &&
					(!fitParams.m_ghostGeometryParameters.m_ghostClipping.m_clipSensor || entry.m_validityFlags.m_isOnSensor);
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			bool isSampleValidForDataset(PolynomialFitParameters const& fitParameters, FitDataSamplePointN<N> const& sample)
			{
				return (fitParameters.m_ghostGeometryParameters.m_includeZeroWeightedDataset || sample.m_weight > 0.0f);
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			bool isSampleValidForFit(PolynomialFitParameters const& fitParameters, FitDataSamplePointN<N> const& sample)
			{
				return (fitParameters.m_ghostGeometryParameters.m_includeZeroWeightedFit || sample.m_weight > 0.0f);
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		namespace ComputeGhostGeometry
		{
			///////////////////////////////////////////////////////////////////////////////
			float getAngleById(PolynomialFitParameters const& fitParams, const size_t angleID)
			{
				return angleID * fitParams.m_ghostGeometryParameters.m_maxAngle / float(std::max(fitParams.m_ghostGeometryParameters.m_numAngles - 1, 1));
			}

			////////////////////////////////////////////////////////////////////////////////
			float getRotationById(PolynomialFitParameters const& fitParams, const size_t rotationID)
			{
				return rotationID * fitParams.m_ghostGeometryParameters.m_maxRotation / float(std::max(fitParams.m_ghostGeometryParameters.m_numRotations - 1, 1));
			}

			///////////////////////////////////////////////////////////////////////////////
			struct GhostGeometryIndex
			{
				int m_ghostID = 0;
				int m_channelID = 0;
				int m_angleID = 0;
				int m_rotationID = 0;

				GhostGeometryIndex() = default;
				GhostGeometryIndex(GhostGeometryIndex const& other) = default;

				GhostGeometryIndex(const int ghostID, const int channelID, const int angleID, const int rotationID,
					const int ghostOffset = 0, const int channelOffset = 0, const int angleOffset = 0, const int rotationOffset = 0):
					m_ghostID(ghostID + ghostOffset),
					m_channelID(channelID + channelOffset),
					m_angleID(angleID + angleOffset),
					m_rotationID(rotationID + rotationOffset)
				{}

				GhostGeometryIndex(GhostGeometryIndex index, const int ghostOffset, const int channelOffset, const int angleOffset, const int rotationOffset) :
					m_ghostID(index.m_ghostID + ghostOffset),
					m_channelID(index.m_channelID + channelOffset),
					m_angleID(index.m_angleID + angleOffset),
					m_rotationID(index.m_rotationID + rotationOffset)
				{}

				float getAngle(PolynomialFitParameters const& fitParams) const { return getAngleById(fitParams, m_angleID); }
				float getRotation(PolynomialFitParameters const& fitParams) const { return getRotationById(fitParams, m_rotationID); }
				template<typename T> T& accessArray(boost::multi_array<T, 3>& geometries) const { return geometries[m_angleID][m_rotationID][m_channelID]; }
				template<typename T> T const& accessArray(boost::multi_array<T, 3> const& geometries) const { return geometries[m_angleID][m_rotationID][m_channelID]; }
				bool isValid(PrecomputedGhostGeometry const& geometries) const 
				{
					return 
						m_angleID >= 0 && m_angleID < geometries.shape()[0] &&
						m_rotationID >= 0 && m_rotationID < geometries.shape()[1] &&
						m_channelID >= 0 && m_channelID < geometries.shape()[2];
				}

				std::string to_string(PolynomialFitParameters const& fitParams) const
				{
					std::stringstream sstream;
					sstream << "[" << m_ghostID << "]";
					sstream << "[" << m_channelID << "]";
					sstream << "[" << glm::degrees(getAngle(fitParams)) << "]";
					sstream << "[" << glm::degrees(getRotation(fitParams)) << "]";
					return sstream.str();
				}
			};

			////////////////////////////////////////////////////////////////////////////////
			bool isGhostValid(Scene::Scene& scene, Scene::Object* object, PhysicalCamera::PhysicalCameraAttributes const& camera, 
				PrecomputeGhostsParameters const& precomputeParams, PolynomialFitParameters const& fitParams,
				GhostAttribs::RenderGhostAttribs const& renderGhostAttribs)
			{
				// Make sure the ghost is actually valid
				if (!GhostAttribs::isGhostValid(scene, object, renderGhostAttribs))
					return false;

				// Clip the intensity first
				if (renderGhostAttribs.m_avgIntensity <= fitParams.m_ghostGeometryParameters.m_ghostClipping.m_intensityClipping)
					return false;

				// Clip the sensor
				const glm::vec2 filmLength = fitParams.m_ghostGeometryParameters.m_ghostClipping.m_clipSensor ? glm::vec2(0.5f * glm::length(camera.m_filmSize)) : glm::vec2(FLT_MAX);
				if (glm::any(glm::lessThan(renderGhostAttribs.m_sensorMax, -filmLength)) || glm::any(glm::greaterThan(renderGhostAttribs.m_sensorMin, filmLength)))
					return false;

				// Make sure the ghost matches a minimum size
				const glm::vec2 extent = renderGhostAttribs.m_pupilMax - renderGhostAttribs.m_pupilMin;
				const float raySize = 2.0f * camera.m_lenses[0].m_height * precomputeParams.m_pupilExpansion / float(precomputeParams.m_rayCount - 1);
				const float minGhostSize = fitParams.m_ghostGeometryParameters.m_ghostClipping.m_sizeClipping * raySize;
				if (glm::any(glm::lessThan(extent, glm::vec2(minGhostSize))))
					return false;

				// The ghost is visible
				return true;
			}

			////////////////////////////////////////////////////////////////////////////////
			bool isGhostValid(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParams, 
				GhostAttribs::RenderGhostAttribs const& renderGhostAttribs)
			{
				return isGhostValid(scene, object,
					object->component<TiledLensFlareComponent>().m_camera,
					object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters,
					fitParams, renderGhostAttribs);
			}

			////////////////////////////////////////////////////////////////////////////////
			GhostAttribs::RenderGhostAttribs getGhostAttribs(Scene::Scene& scene, Scene::Object* object, GhostGeometryIndex index,
				PolynomialFitParameters const& fitParams)
			{
				// Extract the relevant indices
				const size_t ghostID = index.m_ghostID;
				const size_t channelID = index.m_channelID;
				const float theta = index.getAngle(fitParams);
				auto& ghostAttribsMap = object->component<TiledLensFlareComponent>().m_precomputedGhostAttribs;

				// Compute the 2 neighboring angle ids
				auto const& angleIDs = GhostAttribs::getAngleIndices(scene, object, theta);
				std::string const& attribName = GhostAttribs::getGhostAttribName(scene, object, ghostID, channelID, angleIDs.m_prevID);

				// Make sure that the corresponding ghost attribute entries exist
				if (ghostAttribsMap.find(attribName) == ghostAttribsMap.end())
				{
					Debug::log_error() << "Unable to perform polynomial fitting without the proper precomputed ghost attributes." << Debug::end;
					return GhostAttribs::RenderGhostAttribs{};
				}

				// Extract the corresponding ghost attrib
				PrecomputeGhostAttribs const& ghostAttribs = ghostAttribsMap[attribName];
				if (!GhostAttribs::isGhostValid(scene, object, ghostAttribs))
					return GhostAttribs::RenderGhostAttribs{};

				// Calculate the corresponding attributes
				GhostAttribs::RenderGhostAttribs result;
				result.m_pupilMin = ghostAttribs.m_pupilMin;
				result.m_pupilMax = ghostAttribs.m_pupilMax;
				result.m_pupilCenter = (ghostAttribs.m_pupilMax + ghostAttribs.m_pupilMin) / 2.0f;
				result.m_pupilRadius = (ghostAttribs.m_pupilMax - ghostAttribs.m_pupilMin) / 2.0f;
				result.m_sensorMin = ghostAttribs.m_sensorMin;
				result.m_sensorMax = ghostAttribs.m_sensorMax;
				result.m_avgIntensity = ghostAttribs.m_avgIntensity * GhostAttribs::getProjectedGhostIntensity(scene, object, ghostAttribs);

				// Return the result
				return result;
			}

			////////////////////////////////////////////////////////////////////////////////
			GhostAttribs::RenderGhostAttribs getAllEnclosingGhostAttribs(Scene::Scene& scene, Scene::Object* object, GhostGeometryIndex index,
				PrecomputedGhostGeometry const& geometries, boost::multi_array<bool, 3> const& validFlags, PolynomialFitParameters const& fitParams)
			{
				// Construct an empty ghost attrib
				GhostAttribs::RenderGhostAttribs renderGhostAttribs;

				// Check neighboring entries
				GhostGeometryIndex indices[9] =
				{
					GhostGeometryIndex{ index, 0, 0, -1, -1 },
					GhostGeometryIndex{ index, 0, 0, -1,  0 },
					GhostGeometryIndex{ index, 0, 0, -1,  1 },
					GhostGeometryIndex{ index, 0, 0,  0, -1 },
					GhostGeometryIndex{ index, 0, 0,  0,  0 },
					GhostGeometryIndex{ index, 0, 0,  0,  1 },
					GhostGeometryIndex{ index, 0, 0,  1, -1 },
					GhostGeometryIndex{ index, 0, 0,  1,  0 },
					GhostGeometryIndex{ index, 0, 0,  1,  1 },
				};

				std::for_each_n(indices, ARRAY_SIZE(indices), [&](GhostGeometryIndex const& testIndex)
				{
					if (!testIndex.isValid(geometries) || !testIndex.accessArray(validFlags)) return;

					//Debug::log_debug() << "Valid geometry found for ghost" << index.to_string(fitParams) << ": " << to_string(getGhostAttribs(scene, object, index, fitParams)) << Debug::end;

					GhostAttribs::RenderGhostAttribs const& renderGhostAttribsTest = getGhostAttribs(scene, object, testIndex, fitParams);

					renderGhostAttribs.m_pupilMin = glm::min(renderGhostAttribs.m_pupilMin, renderGhostAttribsTest.m_pupilMin);
					renderGhostAttribs.m_pupilMax = glm::max(renderGhostAttribs.m_pupilMax, renderGhostAttribsTest.m_pupilMax);
					renderGhostAttribs.m_sensorMin = glm::min(renderGhostAttribs.m_sensorMin, renderGhostAttribsTest.m_sensorMin);
					renderGhostAttribs.m_sensorMax = glm::max(renderGhostAttribs.m_sensorMax, renderGhostAttribsTest.m_sensorMax);
					renderGhostAttribs.m_rayGridSize = glm::max(renderGhostAttribs.m_rayGridSize, renderGhostAttribsTest.m_rayGridSize);
					if (testIndex.m_angleID == index.m_angleID && testIndex.m_rotationID == index.m_rotationID)
					{
						renderGhostAttribs.m_pupilCenter = renderGhostAttribsTest.m_pupilCenter;
						renderGhostAttribs.m_pupilRadius = renderGhostAttribsTest.m_pupilRadius;
					}

					renderGhostAttribs.m_avgIntensity = 1.0f;
					renderGhostAttribs.m_intensityScale = 1.0f;
				});

				// Return the result
				return renderGhostAttribs;
			}

			////////////////////////////////////////////////////////////////////////////////
			GhostGeometry::GhostGeometry computeGeometry(Scene::Scene& scene, Scene::Object* object, GhostGeometryIndex index,
				PolynomialFitParameters const& fitParams, GhostAttribs::RenderGhostAttribs const& renderGhostAttribs)
			{
				// Return an empty geometry for invalid ghosts
				if (!isGhostValid(scene, object, fitParams, renderGhostAttribs))
					return GhostGeometry::GhostGeometry{};

				// Compute the corresponding geometry
				return GhostGeometry::computeGhostGeometry(scene, object, 
					index.m_ghostID, index.m_channelID, index.getAngle(fitParams), index.getRotation(fitParams),
					fitParams.m_ghostGeometryParameters.m_numChannels, fitParams.m_ghostGeometryParameters.m_rayCount,
					renderGhostAttribs.m_pupilMin, renderGhostAttribs.m_pupilMax,
					renderGhostAttribs.m_pupilCenter, renderGhostAttribs.m_pupilRadius,
					fitParams.m_ghostGeometryParameters.m_geometryClipping.m_radiusClipping,
					fitParams.m_ghostGeometryParameters.m_geometryClipping.m_irisClipping,
					fitParams.m_ghostGeometryParameters.m_geometryClipping.m_intensityClipping,
					fitParams.m_ghostGeometryParameters.m_geometryClipping.m_refractionClipping);
			}

			////////////////////////////////////////////////////////////////////////////////
			namespace PostProcessing
			{
				////////////////////////////////////////////////////////////////////////////////
				bool isGeometryEntryValid(GhostGeometry::GeometryEntry const& entry, GhostGeometry::Filtering::ClipParameters const& clipParameters)
				{
					return GhostGeometry::Filtering::isEntryValid(entry, clipParameters);
					//return GhostGeometry::Filtering::isEntryNonTractable(entry);
				}

				////////////////////////////////////////////////////////////////////////////////
				bool shouldProcessEntry(GhostGeometry::GeometryEntry const& entry, GhostGeometry::Filtering::ClipParameters const& clipParameters)
				{
					return !isGeometryEntryValid(entry, clipParameters);
				}

				////////////////////////////////////////////////////////////////////////////////
				struct SmoothenIndices
				{
					int m_numRays;
					int m_direction;
					int m_axis;
					int m_otherAxis;
					int m_otherIndex;

					int m_indices[3][2];

					SmoothenIndices(const int numRays, const int direction, const int axis, const int otherIndex):
						m_numRays(numRays),
						m_direction(direction),
						m_axis(axis),
						m_otherAxis(1 - axis),
						m_otherIndex(otherIndex)
					{
						for (int indexID = 0, invIndexID = 2; indexID < 3; ++indexID, --invIndexID)
						{
							m_indices[indexID][m_axis] = (direction < 0) ? (m_numRays - 1 - invIndexID) : (invIndexID);
							m_indices[indexID][m_otherAxis] = m_otherIndex;
						}
					}

					int& operator()(const int index, const int axis)
					{
						return m_indices[index][axis];
					}

					operator bool() const
					{
						return m_direction < 0 ? m_indices[0][m_axis] >= 0 : m_indices[0][m_axis] < m_numRays;
					}

					SmoothenIndices& operator++()
					{
						m_indices[0][m_axis] += m_direction;
						m_indices[1][m_axis] += m_direction;
						m_indices[2][m_axis] += m_direction;
						return *this;
					}

					GhostGeometry::GeometryEntry const& extractRay(GhostGeometry::GhostGeometry const& geometry, const int indexId) const
					{
						return GhostGeometry::extractRay(geometry, m_numRays, m_indices[indexId][0], m_indices[indexId][1]);
					}

					GhostGeometry::GeometryEntry& extractRay(GhostGeometry::GhostGeometry& geometry, const int indexId) const
					{
						return GhostGeometry::extractRay(geometry, m_numRays, m_indices[indexId][0], m_indices[indexId][1]);
					}

					GhostGeometry::GeometryEntry const& invalidRay(GhostGeometry::GhostGeometry const& geometry) const
					{
						return extractRay(geometry, 0);
					}

					GhostGeometry::GeometryEntry& invalidRay(GhostGeometry::GhostGeometry& geometry) const
					{
						return extractRay(geometry, 0);
					}

					GhostGeometry::GeometryEntry const& validRay(GhostGeometry::GhostGeometry const& geometry) const
					{
						return extractRay(geometry, 1);
					}

					GhostGeometry::GeometryEntry& validRay(GhostGeometry::GhostGeometry& geometry) const
					{
						return extractRay(geometry, 1);
					}

					GhostGeometry::GeometryEntry const& anchorRay(GhostGeometry::GhostGeometry const& geometry) const
					{
						return extractRay(geometry, 2);
					}

					GhostGeometry::GeometryEntry& anchorRay(GhostGeometry::GhostGeometry& geometry) const
					{
						return extractRay(geometry, 2);
					}
				};

				////////////////////////////////////////////////////////////////////////////////
				// indexID - 0: row, 1: col
				SmoothenIndices findStartIndex(GhostGeometry::GhostGeometry& geometry, 
					GhostGeometry::Filtering::ClipParameters const& clipParameters, SmoothenIndices indices)
				{
					while (indices)
					{
						if
						(
							shouldProcessEntry(indices.invalidRay(geometry), clipParameters) && 
							!shouldProcessEntry(indices.validRay(geometry), clipParameters) && 
							!shouldProcessEntry(indices.anchorRay(geometry), clipParameters)
						)
						{
							/*
							Debug::log_debug() << "Anchor found" << Debug::end;
							Debug::log_debug() << "invalid[" << indices.m_indices[0][0] << ", " << indices.m_indices[0][1] << "]: " << indices.invalidRay(geometry) << Debug::end;
							Debug::log_debug() << "valid[" << indices.m_indices[1][0] << ", " << indices.m_indices[1][1] << "]: " << indices.validRay(geometry) << Debug::end;
							Debug::log_debug() << "other[" << indices.m_indices[2][0] << ", " << indices.m_indices[2][1] << "]: " << indices.anchorRay(geometry) << Debug::end;
							*/
							break;
						}
						++indices;
					}
					return indices;
				}

				////////////////////////////////////////////////////////////////////////////////
				void smothenValueLinear(float& invalid, const float valid, const float otherNeighbor)
				{
					invalid = valid + (valid - otherNeighbor);
				}

				////////////////////////////////////////////////////////////////////////////////
				void smothValueQuadratic(float& invalid, const float valid, const float otherNeighbor)
				{
					const float diff = valid - otherNeighbor;
					const float absDiff = glm::abs(diff);
					if (absDiff < 1e-5f) { invalid = valid; return; }
					const float diffSqrt = glm::sqrt(absDiff);
					const float validSqrt = glm::sqrt(glm::abs(valid));
					const float updated = diff > 0.0f ? validSqrt + diffSqrt : validSqrt - diffSqrt;
					//invalid = updated < 0.0f ? -(updated * updated) : (updated * updated);
					invalid = updated;

					//Debug::log_debug() << invalid << ", " << valid << ", " << otherNeighbor << ", " << diff << ", " << absDiff << ", " << diffSqrt << ", " << updated << Debug::end;
				}

				////////////////////////////////////////////////////////////////////////////////
				bool smoothenEntries(GhostGeometry::Filtering::ClipParameters const& clipParameters,
					GhostGeometry::GeometryEntry& invalid, GhostGeometry::GeometryEntry const& valid, GhostGeometry::GeometryEntry const& anchor)
				{
					// Make sure the entry should actually be processed
					if (!shouldProcessEntry(invalid, clipParameters))
						return true;

					/*
					// Make sure the 'anchor' (other neighbor) is valid
					if (isEntryInvalid(otherNeighbor, clipParameters))
					{
						Debug::log_error() << "Unable to smoothen entry; the other neighbor is invalid too!" << Debug::end;
						return false;
					}
					*/

					// Smoothen the values
					smothenValueLinear(invalid.m_aperturePos.x, valid.m_aperturePos.x, anchor.m_aperturePos.x);
					smothenValueLinear(invalid.m_aperturePos.y, valid.m_aperturePos.y, anchor.m_aperturePos.y);
					smothenValueLinear(invalid.m_sensorPos.x, valid.m_sensorPos.x, anchor.m_sensorPos.x);
					smothenValueLinear(invalid.m_sensorPos.y, valid.m_sensorPos.y, anchor.m_sensorPos.y);
					smothenValueLinear(invalid.m_relativeRadius, valid.m_relativeRadius, anchor.m_relativeRadius);
					smothenValueLinear(invalid.m_clipFactor, valid.m_clipFactor, anchor.m_clipFactor);
					smothenValueLinear(invalid.m_apertureDistTexture, valid.m_apertureDistTexture, anchor.m_apertureDistTexture);
					smothenValueLinear(invalid.m_apertureDistAnalytical, valid.m_apertureDistAnalytical, anchor.m_apertureDistAnalytical);
					smothenValueLinear(invalid.m_intensity, valid.m_intensity, anchor.m_intensity);

					return true;
				}

				////////////////////////////////////////////////////////////////////////////////
				void smoothenAxes(Scene::Scene& scene, Scene::Object* object,
					PolynomialFitParameters const& fitParams, GhostGeometry::GhostGeometry& geometry,
					GhostGeometry::Filtering::ClipParameters const& clipParameters)
				{
					// Extract the ray grid size
					const size_t numRays = fitParams.m_ghostGeometryParameters.m_rayCount;

					// Process both axes, in both directions
					for (int outerAxis = 0, innerAxis = 1; outerAxis < 2; ++outerAxis, --innerAxis)
					for (int outerAxisIndex = 0; outerAxisIndex < numRays; ++outerAxisIndex)
					for (int direction = -1; direction <= 1; direction += 2)
					for (SmoothenIndices indices = findStartIndex(geometry, clipParameters, SmoothenIndices(numRays, direction, innerAxis, outerAxisIndex)); indices; ++indices)
					{
						/*
						Debug::log_debug() << outerAxis << ", " << innerAxis << ", " << direction << Debug::end;
						Debug::log_debug() << "invalid(" << indices[0][0] << ", " << indices[0][1] << "): " << extractRay(geometry, numRays, indices[0][0], indices[0][1]) << Debug::end;
						Debug::log_debug() << "valid(" << indices[1][0] << ", " << indices[1][1] << "): " << extractRay(geometry, numRays, indices[1][0], indices[1][1]) << Debug::end;
						Debug::log_debug() << "other(" << indices[2][0] << ", " << indices[2][1] << "): " << extractRay(geometry, numRays, indices[2][0], indices[2][1]) << Debug::end;
						*/

						const bool success = smoothenEntries(clipParameters, indices.validRay(geometry), indices.invalidRay(geometry), indices.anchorRay(geometry));

						/*
						if (!success)
						{
							Debug::log_error() << "Unable to smoothen entry "
								<< "[" << indices[0][0] << "][" << indices[0][1] << "] "
								<< (outerAxis == 0 ? "column-wise" : "row-wise") << " "
								<< (direction < 0 ? "backward" : "forward")
								<< Debug::end;
						}
						*/
					}
				}

				////////////////////////////////////////////////////////////////////////////////
				void setGeometryEntryFlags(Scene::Scene& scene, Scene::Object* object,
					PolynomialFitParameters const& fitParams, GhostGeometry::GhostGeometry& geometry,
					GhostGeometry::Filtering::ClipParameters const& clipParameters)
				{
					// Fill in the per-ray validity attributes of the ghost geometry
					for (size_t i = 0; i < geometry.size(); ++i)
					{
						geometry[i].m_validityFlags.m_isTractable = GhostGeometry::Filtering::isEntryTractable(geometry, i);
						geometry[i].m_validityFlags.m_isValid = GhostGeometry::Filtering::isEntryValid(geometry, i, clipParameters);
						geometry[i].m_validityFlags.m_isInternal = GhostGeometry::Filtering::isEntryInternal(geometry, i, clipParameters);
						geometry[i].m_validityFlags.m_isBoundary = GhostGeometry::Filtering::isEntryBoundary(geometry, i, clipParameters);
						geometry[i].m_validityFlags.m_isOnSensor = GhostGeometry::Filtering::isEntryOnSensor(geometry, i, clipParameters);
					}
				}

				////////////////////////////////////////////////////////////////////////////////
				bool keepGeometryEntry(Scene::Scene& scene, Scene::Object* object,
					PolynomialFitParameters const& fitParams, GhostGeometry::Filtering::ClipParameters clipParameters,
					GhostGeometry::GeometryEntry const& entry)
				{
					return (fitParams.m_ghostGeometryParameters.m_smoothenData || entry.m_validityFlags.m_isTractable);
					//return entry.m_isOnSensor || entry.m_isInternal;
					//return (entry.m_isValid || entry.m_isInternal);
				}

				////////////////////////////////////////////////////////////////////////////////
				void filterGeometry(Scene::Scene& scene, Scene::Object* object, 
					PolynomialFitParameters const& fitParams, GhostGeometry::Filtering::ClipParameters const& clipParameters,
					GhostGeometry::GhostGeometry& geometry)
				{
					geometry.erase(std::remove_if(geometry.begin(), geometry.end(), [&](GhostGeometry::GeometryEntry const& entry)
						{ return !SampleValidity::isGeometryEntryValid(scene, object, fitParams, clipParameters, entry); }), geometry.end());
				}

				////////////////////////////////////////////////////////////////////////////////
				GhostGeometry::GhostGeometry postProcessGeometry(Scene::Scene& scene, Scene::Object* object, 
					PolynomialFitParameters const& fitParams, GhostGeometry::GhostGeometry geometry,
					const bool flags = true, const bool smoothen = true, const bool filter = true)
				{
					// Construct the clip parameters
					GhostGeometry::Filtering::ClipParameters clipParameters{ scene, object, fitParams };

					// Fill in the per-ray validity attributes of the ghost geometry
					if (flags) setGeometryEntryFlags(scene, object, fitParams, geometry, clipParameters);

					// Smoothen the entries col-wise
					if (smoothen && fitParams.m_ghostGeometryParameters.m_smoothenData && geometry.size() >= 3)
					{
						// Fill in the per-ray validity attributes of the ghost geometry
						smoothenAxes(scene, object, fitParams, geometry, clipParameters);

						// Fill in the per-ray validity attributes of the ghost geometry
						setGeometryEntryFlags(scene, object, fitParams, geometry, clipParameters);
					}

					// Perform filtering on the geometry
					if (filter) filterGeometry(scene, object, fitParams, clipParameters, geometry);

					// Return the modified geometry
					return geometry;
				}
			}

			////////////////////////////////////////////////////////////////////////////////
			GhostGeometry::GhostGeometry postProcessGeometry(Scene::Scene& scene, Scene::Object* object,
				PolynomialFitParameters const& fitParams, GhostAttribs::RenderGhostAttribs const& renderGhostAttribs,
				GhostGeometry::GhostGeometry ghostGeometry,
				const bool flags = true, const bool smoothen = true, const bool filter = true)
			{
				return PostProcessing::postProcessGeometry(scene, object, fitParams, ghostGeometry, flags, smoothen, filter);
			}

			////////////////////////////////////////////////////////////////////////////////
			GhostGeometry::GhostGeometry computeGhostGeometry(Scene::Scene& scene, Scene::Object* object, GhostGeometryIndex index,
				PolynomialFitParameters const& fitParams, GhostAttribs::RenderGhostAttribs const& renderGhostAttribs)
			{
				// Return an empty geometry for invalid ghosts
				if (!isGhostValid(scene, object, fitParams, renderGhostAttribs))
					return GhostGeometry::GhostGeometry{};

				// Compute the corresponding geometry
				return GhostGeometry::computeGhostGeometry(scene, object,
					index.m_ghostID, index.m_channelID, index.getAngle(fitParams), index.getRotation(fitParams),
					fitParams.m_ghostGeometryParameters.m_numChannels, fitParams.m_ghostGeometryParameters.m_rayCount,
					renderGhostAttribs.m_pupilMin, renderGhostAttribs.m_pupilMax,
					fitParams.m_ghostGeometryParameters.m_geometryClipping.m_radiusClipping,
					fitParams.m_ghostGeometryParameters.m_geometryClipping.m_irisClipping,
					fitParams.m_ghostGeometryParameters.m_geometryClipping.m_intensityClipping,
					fitParams.m_ghostGeometryParameters.m_geometryClipping.m_refractionClipping);
			}

			////////////////////////////////////////////////////////////////////////////////
			GhostGeometry::GhostGeometry computeGhostGeometry(Scene::Scene& scene, Scene::Object* object, GhostGeometryIndex index,
				PolynomialFitParameters const& fitParams)
			{
				return computeGhostGeometry(scene, object, index, fitParams, getGhostAttribs(scene, object, index, fitParams));
			}

			////////////////////////////////////////////////////////////////////////////////
			GhostGeometry::GhostGeometry computeGhostGeometryForValidEntry(Scene::Scene& scene, Scene::Object* object, GhostGeometryIndex index,
				PrecomputedGhostGeometry const& geometries, boost::multi_array<bool, 3> const& validFlags, PolynomialFitParameters const& fitParams)
			{
				GhostAttribs::RenderGhostAttribs const& renderGhostAttribs = getAllEnclosingGhostAttribs(scene, object, index, geometries, validFlags, fitParams);
				GhostGeometry::GhostGeometry geometry = computeGhostGeometry(scene, object, index, fitParams, renderGhostAttribs);
				return postProcessGeometry(scene, object, fitParams, renderGhostAttribs, geometry);
			}

			////////////////////////////////////////////////////////////////////////////////
			GhostGeometry::GhostGeometry computeGhostGeometryForInvalidEntry(Scene::Scene& scene, Scene::Object* object, GhostGeometryIndex index,
				PrecomputedGhostGeometry const& geometries, boost::multi_array<bool, 3> const& validFlags, PolynomialFitParameters const& fitParams)
			{
				//return GhostGeometry::GhostGeometry{};
				//return computeGhostGeometryForValidEntry(scene, object, index, geometries, validFlags, fitParams);

				// Check neighboring entries
				GhostGeometryIndex indices[8] =
				{
					GhostGeometryIndex{ index, 0, 0, -1, -1 },
					GhostGeometryIndex{ index, 0, 0, -1,  0 },
					GhostGeometryIndex{ index, 0, 0, -1,  1 },
					GhostGeometryIndex{ index, 0, 0,  0, -1 },
					GhostGeometryIndex{ index, 0, 0,  0,  1 },
					GhostGeometryIndex{ index, 0, 0,  1, -1 },
					GhostGeometryIndex{ index, 0, 0,  1,  0 },
					GhostGeometryIndex{ index, 0, 0,  1,  1 },
				};

				// Make sure any of the neighbors are valid
				const bool anyValid = std::any_of(indices, indices + ARRAY_SIZE(indices), [&](GhostGeometryIndex const& neighborIndex)
					{ return neighborIndex.isValid(geometries) && neighborIndex.accessArray(validFlags); });

				// Early out
				if (!anyValid) return GhostGeometry::GhostGeometry{};

				// Process the neighbors
				GhostGeometry::GhostGeometry result;
				std::for_each_n(indices, ARRAY_SIZE(indices), [&](GhostGeometryIndex const& neighborIndex)
				{
					if (!neighborIndex.isValid(geometries) || !neighborIndex.accessArray(validFlags)) return;

					if (fitParams.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug)
						Debug::log_debug() << "Valid geometry found for ghost" << index.to_string(fitParams) << ": " << to_string(getGhostAttribs(scene, object, neighborIndex, fitParams)) << Debug::end;

					GhostAttribs::RenderGhostAttribs renderGhostAttribs = getGhostAttribs(scene, object, neighborIndex, fitParams);
					GhostGeometry::GhostGeometry ghostGeometryValid = computeGhostGeometry(scene, object, neighborIndex, fitParams, renderGhostAttribs);
					GhostGeometry::GhostGeometry ghostGeometryInvalid = computeGhostGeometry(scene, object, index, fitParams, renderGhostAttribs);
					ghostGeometryValid = postProcessGeometry(scene, object, fitParams, renderGhostAttribs, ghostGeometryValid, true, false, false);
					ghostGeometryInvalid = postProcessGeometry(scene, object, fitParams, renderGhostAttribs, ghostGeometryInvalid, true, false, false);

					if (fitParams.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug)
						Debug::log_debug() << "Valid size: " << ghostGeometryValid.size() << ", invalid size: " << ghostGeometryInvalid.size() << Debug::end;

					for (size_t i = 0; i < ghostGeometryInvalid.size(); ++i)
					{
						auto& entryInvalid = ghostGeometryInvalid[i];
						auto const& entryValid = ghostGeometryValid[i];

						//entryInvalid.m_sensorPos = entryValid.m_sensorPos;
						//entryInvalid.m_sensorPosNormalized = entryValid.m_sensorPosNormalized;
						//entryInvalid.m_intensity = entryValid.m_intensity;
						//entryInvalid.m_relativeRadius = entryValid.m_relativeRadius;

						//entryInvalid.m_isTractable = entryValid.m_isTractable;
						//entryInvalid.m_isValid = entryValid.m_isValid;
						entryInvalid.m_validityFlags.m_isOnSensor = entryValid.m_validityFlags.m_isOnSensor;
						entryInvalid.m_validityFlags.m_isInternal = entryValid.m_validityFlags.m_isInternal;
						entryInvalid.m_validityFlags.m_isBoundary = entryValid.m_validityFlags.m_isBoundary;
					}
					ghostGeometryInvalid = postProcessGeometry(scene, object, fitParams, renderGhostAttribs, ghostGeometryInvalid, false, true, true);

					if (fitParams.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug)
						Debug::log_debug() << "Processed invalid size: " << ghostGeometryInvalid.size() << Debug::end;

					result.insert(result.end(), ghostGeometryInvalid.begin(), ghostGeometryInvalid.end());
				});

				return result;
			}

			////////////////////////////////////////////////////////////////////////////////
			PrecomputedGhostGeometry computeGhostGeometries(Scene::Scene& scene, Scene::Object* object, const size_t ghostID)
			{
				// Precompute parameters
				PolynomialFitParameters const& fitParams = object->component<TiledLensFlareComponent>().m_polynomialFitParameters;

				// Number of different inputs
				const size_t numAngles = fitParams.m_ghostGeometryParameters.m_numAngles;
				const size_t numRotations = fitParams.m_ghostGeometryParameters.m_numRotations;
				const size_t numChannels = fitParams.m_ghostGeometryParameters.m_numChannels;

				Debug::log_debug() << "Computing ghost geometries for ghost #" << ghostID << Debug::end;

				// Set the valid flags
				if (fitParams.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug)
					Debug::log_debug() << "Validity flags: " << Debug::end;

				boost::multi_array<bool, 3> validFlags(boost::extents[numAngles][numRotations][numChannels]);
				for (size_t angleID = 0; angleID < numAngles; ++angleID)
				for (size_t rotationID = 0; rotationID < numRotations; ++rotationID)
				for (size_t channelID = 0; channelID < numChannels; ++channelID)
				{
					GhostGeometryIndex index{ int(ghostID), int(channelID), int(angleID), int(rotationID) };
					GhostAttribs::RenderGhostAttribs const& renderGhostAttribs = getGhostAttribs(scene, object, index, fitParams);
					const bool isEntryValid = isGhostValid(scene, object, fitParams, renderGhostAttribs);
					index.accessArray(validFlags) = isEntryValid;

					if (fitParams.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug)
						Debug::log_debug() << "Ghost" << index.to_string(fitParams) << ": " << isEntryValid << ", " << to_string(renderGhostAttribs) << Debug::end;
				}

				// Compute the geometries
				PrecomputedGhostGeometry ghostGeometries(boost::extents[numAngles][numRotations][numChannels]);
				for (size_t angleID = 0; angleID < numAngles; ++angleID)
				for (size_t rotationID = 0; rotationID < numRotations; ++rotationID)
				for (size_t channelID = 0; channelID < numChannels; ++channelID)
				{
					GhostGeometryIndex index{ int(ghostID), int(channelID), int(angleID), int(rotationID) };
					index.accessArray(ghostGeometries) = index.accessArray(validFlags) ?
						computeGhostGeometryForValidEntry(scene, object, index, ghostGeometries, validFlags, fitParams) :
						computeGhostGeometryForInvalidEntry(scene, object, index, ghostGeometries, validFlags, fitParams);
				}

				// Copy over neighboring elements (angle)
				if (fitParams.m_ghostGeometryParameters.m_shareNeighboringGeometries && numAngles > 1)
				{
					Debug::log_debug() << "Sharing neighboring geometries (angle)" << Debug::end;

					for (size_t angleID = 0; angleID < numAngles - 1; ++angleID)
					for (size_t rotationID = 0; rotationID < numRotations; ++rotationID)
					for (size_t channelID = 0; channelID < numChannels; ++channelID)
					{
						GhostGeometryIndex index{ int(ghostID), int(channelID), int(angleID), int(rotationID), 0, 0, +1, 0 };
						GhostGeometryIndex targetIndex{ int(ghostID), int(channelID), int(angleID), int(rotationID) };
						auto const& geometry = index.accessArray(ghostGeometries);
						auto& targetGeometry = targetIndex.accessArray(ghostGeometries);
						if (index.accessArray(validFlags))
							targetGeometry.insert(targetGeometry.end(), geometry.begin(), geometry.end());
					}
				}

				// Copy over neighboring elements (rotation)
				if (fitParams.m_ghostGeometryParameters.m_shareNeighboringGeometries && numRotations > 1)
				{
					Debug::log_debug() << "Sharing neighboring geometries (rotation)" << Debug::end;

					for (size_t rotationID = 0; rotationID < numRotations - 1; ++rotationID)
					for (size_t angleID = 0; angleID < numAngles; ++angleID)
					for (size_t channelID = 0; channelID < numChannels; ++channelID)
					{
						GhostGeometryIndex index{ int(ghostID), int(channelID), int(angleID), int(rotationID), 0, 0, 0, +1 };
						GhostGeometryIndex targetIndex{ int(ghostID), int(channelID), int(angleID), int(rotationID) };
						auto const& geometry = index.accessArray(ghostGeometries);
						auto& targetGeometry = targetIndex.accessArray(ghostGeometries);
						if (index.accessArray(validFlags))
							targetGeometry.insert(targetGeometry.end(), geometry.begin(), geometry.end());
					}
				}

				if (fitParams.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug)
				{
					Debug::log_debug() << "Final dataset sizes:" << Debug::end;
					for (size_t angleID = 0; angleID < numAngles; ++angleID)
					for (size_t rotationID = 0; rotationID < numRotations; ++rotationID)
					for (size_t channelID = 0; channelID < numChannels; ++channelID)
					{
						GhostGeometryIndex index{ int(ghostID), int(channelID), int(angleID), int(rotationID) };
						Debug::log_debug() << "Ghost" << index.to_string(fitParams) << ": " << index.accessArray(ghostGeometries).size() << Debug::end;
					}
				}

				// Return the result
				return ghostGeometries;
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		namespace Errors
		{
			////////////////////////////////////////////////////////////////////////////////
			struct ErrorSettings
			{
				bool m_isBounded = false;
				float m_bounds[2] = { -FLT_MAX, FLT_MAX };
				float m_boundPenalty = 1000.0f;

				ErrorSettings() :
					m_isBounded(false)
				{}

				ErrorSettings(const float lowerBound, const float upperBound, const float boundPenalty = 10000.0f) :
					m_isBounded(lowerBound != upperBound),
					m_bounds{ lowerBound , upperBound },
					m_boundPenalty(boundPenalty)
				{}
			};

			////////////////////////////////////////////////////////////////////////////////
			struct ErrorFn
			{
				virtual void initialize() = 0;
				virtual void finalize() = 0;
				virtual float calculateSampleError(FitDataSamplePointBase const& sample, const float pred) = 0;
				virtual void addSample(FitDataSamplePointBase const& sample, const float pred) = 0;
				virtual void logSample(FitDataSamplePointBase const& sample, const float pred) = 0;
				virtual float error() const = 0;

				template<size_t N>
				void addSample(FitDataSamplePointN<N> const& sample, PolynomialN<N> const& polynomial)
				{
					addSample(sample, Monomials::evalPolynomial(polynomial, sample));
				}

				template<size_t N>
				void logSample(FitDataSamplePointN<N> const& sample, PolynomialN<N> const& polynomial)
				{
					logSample(sample, Monomials::evalPolynomial(polynomial, sample));
				}
			};

			////////////////////////////////////////////////////////////////////////////////
			struct ErrorFnBase : public ErrorFn
			{
				std::string m_variableName;
				ErrorSettings m_errorSettings;
				int m_numThreads = 0;

				int m_numSamples = 0;
				float m_sum = 0.0f;
				float m_error = 0.0f;

				ErrorFnBase(std::string const& variableName, ErrorSettings const& errorSettings = {}, int numThreads = 0) :
					m_variableName(variableName),
					m_errorSettings(errorSettings),
					m_numThreads(numThreads)
				{}

				void initialize()
				{
					m_numSamples = 0;
					m_sum = 0.0f;
					m_error = 0.0f;
				}

				float calculateSampleError(FitDataSamplePointBase const& sample, const float pred)
				{
					if ((m_errorSettings.m_isBounded && m_errorSettings.m_boundPenalty > 0.0f) && (
						sample.m_value <= m_errorSettings.m_bounds[0] && pred > m_errorSettings.m_bounds[0] ||
						sample.m_value >= m_errorSettings.m_bounds[0] && pred < m_errorSettings.m_bounds[0] ||
						sample.m_value >= m_errorSettings.m_bounds[1] && pred < m_errorSettings.m_bounds[1] ||
						sample.m_value <= m_errorSettings.m_bounds[1] && pred > m_errorSettings.m_bounds[1]
					))
						return m_errorSettings.m_boundPenalty * (sample.m_value - pred);
					return sample.m_weight > 0.0f ? sample.m_weight * (sample.m_value - pred) : 0.0f;
				}

				void addSample(FitDataSamplePointBase const& sample, const float pred)
				{
					m_sum += calculateSampleError(sample, pred);
					++m_numSamples;
				}

				void logSample(FitDataSamplePointBase const& sample, const float pred)
				{
					Debug::log_debug() << "Target: " << sample.m_value << ", pred: " << pred << ", error: " << calculateSampleError(sample, pred) << Debug::end;
				}

				void finalize()
				{
					m_error = m_sum / std::max(float(m_numSamples), 1.0f);
				}
			};

			////////////////////////////////////////////////////////////////////////////////
			struct ErrorFnMae : public ErrorFnBase
			{
				using ErrorFnBase::ErrorFnBase;

				float calculateSampleError(FitDataSamplePointBase const& sample, const float pred)
				{
					return glm::abs(ErrorFnBase::calculateSampleError(sample, pred));
				}

				float error() const
				{
					return m_error;
				}
			};

			////////////////////////////////////////////////////////////////////////////////
			struct ErrorFnMse : public ErrorFnBase
			{
				using ErrorFnBase::ErrorFnBase;

				float calculateSampleError(FitDataSamplePointBase const& sample, const float pred)
				{
					return glm::pow(ErrorFnBase::calculateSampleError(sample, pred), 2.0f);
				}

				float error() const
				{
					return glm::sqrt(m_error);
				}
			};
		}

		////////////////////////////////////////////////////////////////////////////////
		namespace Attribs
		{
			////////////////////////////////////////////////////////////////////////////////
			using AttribValueExtractor = std::add_pointer<float(GhostGeometry::GeometryEntry const&)>::type;
			using AttribDefaultValueExtractor = std::add_pointer<float(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName)>::type;
			using AttribErrorFnFactory = std::add_pointer<Errors::ErrorFn* (Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName, int numThreads)>::type;

			////////////////////////////////////////////////////////////////////////////////
			namespace ValueExtractors
			{
				// Input extractors
				// - common properties
				float getSampleAngleRad(GhostGeometry::GeometryEntry const& sample) { return sample.m_angle; }
				float getSampleAngleDeg(GhostGeometry::GeometryEntry const& sample) { return glm::degrees(sample.m_angle); }
				float getSampleRotationRad(GhostGeometry::GeometryEntry const& sample) { return sample.m_rotation; }
				float getSampleRotationDeg(GhostGeometry::GeometryEntry const& sample) { return glm::degrees(sample.m_rotation); }
				float getSampleLambdaMuM(GhostGeometry::GeometryEntry const& sample) { return sample.m_wavelength; }
				float getSampleLambdaNM(GhostGeometry::GeometryEntry const& sample) { return sample.m_wavelength * 1e-3f; }
				// - pupil pos
				float getSamplePupilPosCartesianX(GhostGeometry::GeometryEntry const& sample) { return sample.m_pupilPosCartesian.x; }
				float getSamplePupilPosCartesianY(GhostGeometry::GeometryEntry const& sample) { return sample.m_pupilPosCartesian.y; }
				float getSamplePupilPosCartesianNormalizedX(GhostGeometry::GeometryEntry const& sample) { return sample.m_pupilPosCartesianNormalized.x; }
				float getSamplePupilPosCartesianNormalizedY(GhostGeometry::GeometryEntry const& sample) { return sample.m_pupilPosCartesianNormalized.y; }
				float getSamplePupilPosPolarX(GhostGeometry::GeometryEntry const& sample) { return sample.m_pupilPosPolar.x; }
				float getSamplePupilPosPolarY(GhostGeometry::GeometryEntry const& sample) { return sample.m_pupilPosPolar.y; }
				float getSamplePupilPosPolarNormalizedX(GhostGeometry::GeometryEntry const& sample) { return sample.m_pupilPosPolarNormalized.x; }
				float getSamplePupilPosPolarNormalizedY(GhostGeometry::GeometryEntry const& sample) { return sample.m_pupilPosPolarNormalized.y; }
				float getSamplePupilPosPolarNormalizedInvertedX(GhostGeometry::GeometryEntry const& sample) { return 1.0f - sample.m_pupilPosPolarNormalized.x; }
				// - centered pupil pos
				float getSampleCenteredPupilPosCartesianX(GhostGeometry::GeometryEntry const& sample) { return sample.m_centeredPupilPosCartesian.x; }
				float getSampleCenteredPupilPosCartesianY(GhostGeometry::GeometryEntry const& sample) { return sample.m_centeredPupilPosCartesian.y; }
				float getSampleCenteredPupilPosCartesianNormalizedX(GhostGeometry::GeometryEntry const& sample) { return sample.m_centeredPupilPosCartesianNormalized.x; }
				float getSampleCenteredPupilPosCartesianNormalizedY(GhostGeometry::GeometryEntry const& sample) { return sample.m_centeredPupilPosCartesianNormalized.y; }
				float getSampleCenteredPupilPosPolarX(GhostGeometry::GeometryEntry const& sample) { return sample.m_centeredPupilPosPolar.x; }
				float getSampleCenteredPupilPosPolarY(GhostGeometry::GeometryEntry const& sample) { return sample.m_centeredPupilPosPolar.y; }
				float getSampleCenteredPupilPosPolarNormalizedX(GhostGeometry::GeometryEntry const& sample) { return sample.m_centeredPupilPosPolarNormalized.x; }
				float getSampleCenteredPupilPosPolarNormalizedY(GhostGeometry::GeometryEntry const& sample) { return sample.m_centeredPupilPosPolarNormalized.y; }
				float getSampleCenteredPupilPosPolarNormalizedInvertedX(GhostGeometry::GeometryEntry const& sample) { return 1.0f - sample.m_centeredPupilPosPolarNormalized.x; }
				// - entrance pupil pos
				float getSampleEntrancePupilPosCartesianX(GhostGeometry::GeometryEntry const& sample) { return sample.m_entrancePupilPosCartesian.x; }
				float getSampleEntrancePupilPosCartesianY(GhostGeometry::GeometryEntry const& sample) { return sample.m_entrancePupilPosCartesian.y; }
				float getSampleEntrancePupilPosCartesianNormalizedX(GhostGeometry::GeometryEntry const& sample) { return sample.m_entrancePupilPosCartesianNormalized.x; }
				float getSampleEntrancePupilPosCartesianNormalizedY(GhostGeometry::GeometryEntry const& sample) { return sample.m_entrancePupilPosCartesianNormalized.y; }
				float getSampleEntrancePupilPosPolarX(GhostGeometry::GeometryEntry const& sample) { return sample.m_entrancePupilPosPolar.x; }
				float getSampleEntrancePupilPosPolarY(GhostGeometry::GeometryEntry const& sample) { return sample.m_entrancePupilPosPolar.y; }
				float getSampleEntrancePupilPosPolarNormalizedX(GhostGeometry::GeometryEntry const& sample) { return sample.m_entrancePupilPosPolarNormalized.x; }
				float getSampleEntrancePupilPosPolarNormalizedY(GhostGeometry::GeometryEntry const& sample) { return sample.m_entrancePupilPosPolarNormalized.y; }
				// - pupil aperture dist
				float getSamplePupilApertureDistAbsolute(GhostGeometry::GeometryEntry const& sample) { return sample.m_pupilApertureDistAbsolute; }
				float getSamplePupilApertureDistBounded(GhostGeometry::GeometryEntry const& sample) { return sample.m_pupilApertureDistBounded; }

				// Output extractors
				float getSampleAperturePosX(GhostGeometry::GeometryEntry const& sample) { return sample.m_aperturePos.x; }
				float getSampleAperturePosY(GhostGeometry::GeometryEntry const& sample) { return sample.m_aperturePos.y; }
				float getSampleSensorPosX(GhostGeometry::GeometryEntry const& sample) { return sample.m_sensorPos.x; }
				float getSampleSensorPosY(GhostGeometry::GeometryEntry const& sample) { return sample.m_sensorPos.y; }
				float getSampleRadius(GhostGeometry::GeometryEntry const& sample) { return sample.m_relativeRadius; }
				float getSampleApertureDistAnalytical(GhostGeometry::GeometryEntry const& sample) { return sample.m_apertureDistAnalytical; }
				float getSampleApertureDistTexture(GhostGeometry::GeometryEntry const& sample) { return sample.m_apertureDistTexture; }
				float getSampleClipFactor(GhostGeometry::GeometryEntry const& sample) { return sample.m_clipFactor; }
				float getSampleIntensity(GhostGeometry::GeometryEntry const& sample) { return sample.m_intensity; }

				// Other extractors
				float getConstantZero(GhostGeometry::GeometryEntry const& sample) { return 0.0f; }
				float getConstantOne(GhostGeometry::GeometryEntry const& sample) { return 1.0f; }
			}

			////////////////////////////////////////////////////////////////////////////////
			namespace DefaultValueExtractors
			{
				////////////////////////////////////////////////////////////////////////////////
				float getAperturePosXDefaultValue(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName)
				{
					const float apertureHeight = Common::getPhysicalApertureHeight(scene, object);
					return apertureHeight + 0.01f;
				}

				////////////////////////////////////////////////////////////////////////////////
				float getAperturePosYDefaultValue(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName)
				{
					const float apertureHeight = Common::getPhysicalApertureHeight(scene, object);
					return apertureHeight + 0.01f;
				}

				////////////////////////////////////////////////////////////////////////////////
				float getSensorPosXDefaultValue(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName)
				{
					const glm::vec2 filmSize = object->component<TiledLensFlareComponent>().m_camera.m_filmSize;
					return filmSize.x * 0.5f + 0.01f;
				}

				////////////////////////////////////////////////////////////////////////////////
				float getSensorPosYDefaultValue(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName)
				{
					const glm::vec2 filmSize = object->component<TiledLensFlareComponent>().m_camera.m_filmSize;
					return filmSize.y * 0.5f + 0.01f;
				}

				////////////////////////////////////////////////////////////////////////////////
				float getApertureDistAnalyticalDefaultValue(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName)
				{
					return 2.0f;
				}

				////////////////////////////////////////////////////////////////////////////////
				float getApertureDistTextureDefaultValue(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName)
				{
					return 2.0f;
				}

				////////////////////////////////////////////////////////////////////////////////
				float getRadiusDefaultValue(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName)
				{
					return 2.0f;
				}

				////////////////////////////////////////////////////////////////////////////////
				float getClipFactorDefaultValue(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName)
				{
					return 2.0f;
				}

				////////////////////////////////////////////////////////////////////////////////
				float getIntensityDefaultValue(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName)
				{
					return 0.0f;
				}
			}

			////////////////////////////////////////////////////////////////////////////////
			namespace ErrorFnFactories
			{
				////////////////////////////////////////////////////////////////////////////////
				template<typename T>
				Errors::ErrorFn* makeErrorFn(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName, int numThreads, const float lowerBound, const float upperBound)
				{
					float boundaryPenalty = 0.0f;
					if (auto const& it = fitParameters.m_outputVariableParams.find(variableName); it != fitParameters.m_outputVariableParams.end())
						boundaryPenalty = it->second.m_boundaryPenalty;
					else
						Debug::log_warning() << "Constructing error function for unknown output variable \"" << variableName << "\"" << Debug::end;
					return new T(variableName, { lowerBound, upperBound, 1.0f + boundaryPenalty }, numThreads);
				}

				////////////////////////////////////////////////////////////////////////////////
				template<typename T>
				Errors::ErrorFn* makeErrorFn(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName, int numThreads, const float bounds)
				{
					return makeErrorFn<T>(scene, object, fitParameters, variableName, numThreads, -bounds, bounds);
				}

				////////////////////////////////////////////////////////////////////////////////
				Errors::ErrorFn* makeAperturePosXErrorFn(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName, int numThreads)
				{
					//const float apertureHeight = Common::getPhysicalApertureHeight(scene, object);
					//return makeErrorFn<Errors::ErrorFnMse>(scene, object, fitParameters, variableName, numThreads, apertureHeight);
					return makeErrorFn<Errors::ErrorFnMse>(scene, object, fitParameters, variableName, numThreads, FLT_MAX);
				}

				////////////////////////////////////////////////////////////////////////////////
				Errors::ErrorFn* makeAperturePosYErrorFn(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName, int numThreads)
				{
					//const float apertureHeight = Common::getPhysicalApertureHeight(scene, object);
					//return makeErrorFn<Errors::ErrorFnMse>(scene, object, fitParameters, variableName, numThreads, apertureHeight);
					return makeErrorFn<Errors::ErrorFnMse>(scene, object, fitParameters, variableName, numThreads, FLT_MAX);
				}

				////////////////////////////////////////////////////////////////////////////////
				Errors::ErrorFn* makeSensorPosXErrorFn(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName, int numThreads)
				{
					//const glm::vec2 filmSize = object->component<TiledLensFlareComponent>().m_camera.m_filmSize;
					//return makeErrorFn<Errors::ErrorFnMse>(scene, object, fitParameters, variableName, numThreads, filmSize.x * 0.5f);
					return makeErrorFn<Errors::ErrorFnMse>(scene, object, fitParameters, variableName, numThreads, FLT_MAX);
				}

				////////////////////////////////////////////////////////////////////////////////
				Errors::ErrorFn* makeSensorPosYErrorFn(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName, int numThreads)
				{
					//const glm::vec2 filmSize = object->component<TiledLensFlareComponent>().m_camera.m_filmSize;
					//return makeErrorFn<Errors::ErrorFnMse>(scene, object, fitParameters, variableName, numThreads, filmSize.y * 0.5f);
					return makeErrorFn<Errors::ErrorFnMse>(scene, object, fitParameters, variableName, numThreads, FLT_MAX);
				}

				////////////////////////////////////////////////////////////////////////////////
				Errors::ErrorFn* makeApertureDistAnalyticalErrorFn(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName, int numThreads)
				{
					return makeErrorFn<Errors::ErrorFnMse>(scene, object, fitParameters, variableName, numThreads, 0.0f, 1.0f);
				}

				////////////////////////////////////////////////////////////////////////////////
				Errors::ErrorFn* makeApertureDistTextureErrorFn(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName, int numThreads)
				{
					return makeErrorFn<Errors::ErrorFnMse>(scene, object, fitParameters, variableName, numThreads, 0.0f, 1.0f);
				}

				////////////////////////////////////////////////////////////////////////////////
				Errors::ErrorFn* makeRadiusErrorFn(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName, int numThreads)
				{
					return makeErrorFn<Errors::ErrorFnMse>(scene, object, fitParameters, variableName, numThreads, 0.0f, 1.0f);
				}

				////////////////////////////////////////////////////////////////////////////////
				Errors::ErrorFn* makeClipFactorErrorFn(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName, int numThreads)
				{
					return makeErrorFn<Errors::ErrorFnMse>(scene, object, fitParameters, variableName, numThreads, 0.0f, 1.0f);
				}

				////////////////////////////////////////////////////////////////////////////////
				Errors::ErrorFn* makeIntensityErrorFn(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, std::string const& variableName, int numThreads)
				{
					return makeErrorFn<Errors::ErrorFnMse>(scene, object, fitParameters, variableName, numThreads, 0.0f, 1.0f);
				}
			}

			////////////////////////////////////////////////////////////////////////////////
			struct AttribData
			{
				AttribValueExtractor m_valueExtractor{ nullptr };
				AttribDefaultValueExtractor m_defaultValueExtractor{ nullptr };
				AttribErrorFnFactory m_errorFnFactory{ nullptr };
			};

			////////////////////////////////////////////////////////////////////////////////
			static const std::unordered_map<std::string, AttribData> s_variableData =
			{
				{ "constant_zero",                                    AttribData{ &ValueExtractors::getConstantZero } },
				{ "constant_one",                                     AttribData{ &ValueExtractors::getConstantOne } },

				{ "pupil_pos_cartesian_x",                            AttribData{ &ValueExtractors::getSamplePupilPosCartesianX } },
				{ "pupil_pos_cartesian_y",                            AttribData{ &ValueExtractors::getSamplePupilPosCartesianY } },
				{ "pupil_pos_cartesian_normalized_x",                 AttribData{ &ValueExtractors::getSamplePupilPosCartesianNormalizedX } },
				{ "pupil_pos_cartesian_normalized_y",                 AttribData{ &ValueExtractors::getSamplePupilPosCartesianNormalizedY } },
				{ "pupil_pos_polar_x",                                AttribData{ &ValueExtractors::getSamplePupilPosPolarX } },
				{ "pupil_pos_polar_y",                                AttribData{ &ValueExtractors::getSamplePupilPosPolarY } },
				{ "pupil_pos_polar_normalized_x",                     AttribData{ &ValueExtractors::getSamplePupilPosPolarNormalizedX } },
				{ "pupil_pos_polar_normalized_y",                     AttribData{ &ValueExtractors::getSamplePupilPosPolarNormalizedY } },
				{ "pupil_pos_polar_normalized_inverted_x",            AttribData{ &ValueExtractors::getSamplePupilPosPolarNormalizedInvertedX } },
				{ "centered_pupil_pos_cartesian_x",                   AttribData{ &ValueExtractors::getSampleCenteredPupilPosCartesianX } },
				{ "centered_pupil_pos_cartesian_y",                   AttribData{ &ValueExtractors::getSampleCenteredPupilPosCartesianY } },
				{ "centered_pupil_pos_cartesian_normalized_x",        AttribData{ &ValueExtractors::getSampleCenteredPupilPosCartesianNormalizedX } },
				{ "centered_pupil_pos_cartesian_normalized_y",        AttribData{ &ValueExtractors::getSampleCenteredPupilPosCartesianNormalizedY } },
				{ "centered_pupil_pos_polar_x",                       AttribData{ &ValueExtractors::getSampleCenteredPupilPosPolarX } },
				{ "centered_pupil_pos_polar_y",                       AttribData{ &ValueExtractors::getSampleCenteredPupilPosPolarY } },
				{ "centered_pupil_pos_polar_normalized_x",            AttribData{ &ValueExtractors::getSampleCenteredPupilPosPolarNormalizedX } },
				{ "centered_pupil_pos_polar_normalized_y",            AttribData{ &ValueExtractors::getSampleCenteredPupilPosPolarNormalizedY } },
				{ "centered_pupil_pos_polar_normalized_inverted_x",   AttribData{ &ValueExtractors::getSampleCenteredPupilPosPolarNormalizedInvertedX } },
				{ "entrance_pupil_pos_cartesian_x",                   AttribData{ &ValueExtractors::getSampleEntrancePupilPosCartesianX } },
				{ "entrance_pupil_pos_cartesian_y",                   AttribData{ &ValueExtractors::getSampleEntrancePupilPosCartesianY } },
				{ "entrance_pupil_pos_cartesian_normalized_x",        AttribData{ &ValueExtractors::getSampleEntrancePupilPosCartesianNormalizedX } },
				{ "entrance_pupil_pos_cartesian_normalized_y",        AttribData{ &ValueExtractors::getSampleEntrancePupilPosCartesianNormalizedY } },
				{ "entrance_pupil_pos_polar_x",                       AttribData{ &ValueExtractors::getSampleEntrancePupilPosPolarX } },
				{ "entrance_pupil_pos_polar_y",                       AttribData{ &ValueExtractors::getSampleEntrancePupilPosPolarY } },
				{ "entrance_pupil_pos_polar_normalized_x",            AttribData{ &ValueExtractors::getSampleEntrancePupilPosPolarNormalizedX } },
				{ "entrance_pupil_pos_polar_normalized_y",            AttribData{ &ValueExtractors::getSampleEntrancePupilPosPolarNormalizedY } },
				{ "angle_rad",                                        AttribData{ &ValueExtractors::getSampleAngleRad } },
				{ "angle_deg",                                        AttribData{ &ValueExtractors::getSampleAngleDeg } },
				{ "rotation_rad",                                     AttribData{ &ValueExtractors::getSampleRotationRad } },
				{ "rotation_deg",                                     AttribData{ &ValueExtractors::getSampleRotationDeg } },
				{ "lambda_mum",                                       AttribData{ &ValueExtractors::getSampleLambdaMuM } },
				{ "lambda_nm",                                        AttribData{ &ValueExtractors::getSampleLambdaNM } },
				{ "aperture_dist_absolute",                           AttribData{ &ValueExtractors::getSamplePupilApertureDistAbsolute } },
				{ "aperture_dist_bounded",                            AttribData{ &ValueExtractors::getSamplePupilApertureDistBounded } },

				{ 
					"aperture_pos_x",
					AttribData
					{ 
						&ValueExtractors::getSampleAperturePosX, 
						&DefaultValueExtractors::getAperturePosXDefaultValue,
						&ErrorFnFactories::makeAperturePosXErrorFn 
					} 
				},
				{ 
					"aperture_pos_y",
					AttribData
					{ 
						&ValueExtractors::getSampleAperturePosY,
						&DefaultValueExtractors::getAperturePosYDefaultValue,
						&ErrorFnFactories::makeAperturePosYErrorFn 
					} 
				},
				{ 
					"sensor_pos_x",
					AttribData
					{ 
						&ValueExtractors::getSampleSensorPosX,
						&DefaultValueExtractors::getSensorPosXDefaultValue,
						&ErrorFnFactories::makeSensorPosXErrorFn 
					} 
				},
				{ 
					"sensor_pos_y",
					AttribData
					{ 
						&ValueExtractors::getSampleSensorPosY,
						&DefaultValueExtractors::getSensorPosYDefaultValue,
						&ErrorFnFactories::makeSensorPosYErrorFn
					}
				},
				{ 
					"aperture_dist_analytical",
					AttribData
					{ 
						&ValueExtractors::getSampleApertureDistAnalytical,
						&DefaultValueExtractors::getApertureDistAnalyticalDefaultValue,
						&ErrorFnFactories::makeApertureDistAnalyticalErrorFn
					}
				},
				{ 
					"aperture_dist_texture",
					AttribData
					{ 
						&ValueExtractors::getSampleApertureDistTexture,
						&DefaultValueExtractors::getApertureDistTextureDefaultValue,
						&ErrorFnFactories::makeApertureDistTextureErrorFn 
					} 
				},
				{ 
					"radius", 
					AttribData
					{ 
						&ValueExtractors::getSampleRadius,
						&DefaultValueExtractors::getRadiusDefaultValue,
						&ErrorFnFactories::makeRadiusErrorFn 
					} 
				},
				{ 
					"clip_factor", 
					AttribData
					{ 
						&ValueExtractors::getSampleClipFactor,
						&DefaultValueExtractors::getClipFactorDefaultValue,
						&ErrorFnFactories::makeClipFactorErrorFn 
					} 
				},
				{ 
					"intensity", 
					AttribData
					{ 
						&ValueExtractors::getSampleIntensity,
						&DefaultValueExtractors::getIntensityDefaultValue,
						&ErrorFnFactories::makeIntensityErrorFn
					} 
				},
			};

			////////////////////////////////////////////////////////////////////////////////
			AttribData const& getVariableData(std::string const& name)
			{
				for (auto const& attribData : s_variableData)
					if (attribData.first == name)
						return attribData.second;
				static const AttribData s_emptyAttribData{ nullptr, nullptr, nullptr };
				return s_emptyAttribData;
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		namespace DensePolynomial
		{
			////////////////////////////////////////////////////////////////////////////////
			namespace NonlinearOptimization
			{
				////////////////////////////////////////////////////////////////////////////////
				namespace NLO_Eigen
				{
					////////////////////////////////////////////////////////////////////////////////
					template<size_t N>
					struct HybirdSolverFunctorBase
					{
						// Required by numerical differentiation module
						typedef double Scalar;
						enum
						{
							InputsAtCompileTime = Eigen::Dynamic,
							ValuesAtCompileTime = Eigen::Dynamic
						};
						typedef Eigen::Matrix<Scalar, InputsAtCompileTime, 1> InputType;
						typedef Eigen::Matrix<Scalar, ValuesAtCompileTime, 1> ValueType;
						typedef Eigen::Matrix<Scalar, ValuesAtCompileTime, InputsAtCompileTime> JacobianType;

						HybirdSolverFunctorBase(PolynomialN<N> const& polynomial, FitDataPointsN<N> const& dataPoints) :
							m_numSamples(dataPoints.size()),
							m_numTerms(polynomial.size()),
							m_polynomial(polynomial),
							m_dataPoints(dataPoints)
						{
							m_b = Eigen::VectorXd::Zero(m_numSamples);
							m_w = Eigen::VectorXd::Zero(m_numSamples);
							std::transform(m_dataPoints.begin(), m_dataPoints.end(), m_b.data(), [](FitDataSamplePointN<N> const& sample) { return double(sample.m_value); });
							std::transform(m_dataPoints.begin(), m_dataPoints.end(), m_w.data(), [](FitDataSamplePointN<N> const& sample) { return double(sample.m_weight); });
						}

						int operator()(const InputType& x, ValueType& fvec) const
						{
							fvec = (Monomials::evalPolynomial(m_polynomial, m_dataPoints, x) - m_b).cwiseAbs2().cwiseProduct(m_w);
							return 0;
						}

						int values() const { return m_numSamples; }
						int inputs() const { return m_numTerms; }

						size_t m_numSamples;
						size_t m_numTerms;
						PolynomialN<N> const& m_polynomial;
						FitDataPointsN<N> const& m_dataPoints;
						Eigen::VectorXd m_b;
						Eigen::VectorXd m_w;
					};

					template<size_t N> using HybirdSolverFunctor = Eigen::NumericalDiff<HybirdSolverFunctorBase<N>, Eigen::Forward>;
					template<size_t N> using LevenbergMarquardtSolver = Eigen::LevenbergMarquardt<HybirdSolverFunctor<N>, double>;
					template<size_t N> using HybridNonlinearSolver = Eigen::HybridNonLinearSolver<HybirdSolverFunctor<N>, double>;

					////////////////////////////////////////////////////////////////////////////////
					void evalNonlinearSolverStatus(const Debug::DebugOutputLevel outputLevel, const int code, const char* message)
					{
						if (code < 0)
							Debug::log_output(outputLevel) << "Non-linear solver finished with status: " << message << Debug::end;
					}

					////////////////////////////////////////////////////////////////////////////////
					#define EVAL_LEVMAR_SOLVER_STATUS(X) \
						case Eigen::LevenbergMarquardtSpace::##X: \
							evalNonlinearSolverStatus(outputLevel, int(Eigen::LevenbergMarquardtSpace::X), STRINGIFY(X)); \
							break;

					////////////////////////////////////////////////////////////////////////////////
					#define EVAL_HYBRID_SOLVER_STATUS(X) \
						case Eigen::HybridNonLinearSolverSpace::##X: \
							evalNonlinearSolverStatus(outputLevel, int(Eigen::HybridNonLinearSolverSpace::X), STRINGIFY(X)); \
							break;

					////////////////////////////////////////////////////////////////////////////////
					template<size_t N>
					bool optimizeLevMar(PolynomialFitParameters const& fitParameters, Eigen::VectorXd& X,
						PolynomialN<N> const& polynomial, FitDataPointsN<N> const& dataPoints)
					{
						Debug::DebugOutputLevel outputLevel = fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug ? Debug::Info : Debug::Debug;

						HybirdSolverFunctor<N> functor(polynomial, dataPoints);
						LevenbergMarquardtSolver<N> solver(functor);
						auto status = solver.minimize(X);
						switch (status)
						{
							EVAL_LEVMAR_SOLVER_STATUS(NotStarted);
							EVAL_LEVMAR_SOLVER_STATUS(Running);
							EVAL_LEVMAR_SOLVER_STATUS(ImproperInputParameters);
							EVAL_LEVMAR_SOLVER_STATUS(RelativeReductionTooSmall);
							EVAL_LEVMAR_SOLVER_STATUS(RelativeErrorTooSmall);
							EVAL_LEVMAR_SOLVER_STATUS(RelativeErrorAndReductionTooSmall);
							EVAL_LEVMAR_SOLVER_STATUS(CosinusTooSmall);
							EVAL_LEVMAR_SOLVER_STATUS(TooManyFunctionEvaluation);
							EVAL_LEVMAR_SOLVER_STATUS(FtolTooSmall);
							EVAL_LEVMAR_SOLVER_STATUS(XtolTooSmall);
							EVAL_LEVMAR_SOLVER_STATUS(GtolTooSmall);
							EVAL_LEVMAR_SOLVER_STATUS(UserAsked);
						}
						return status >= 0;
					}

					////////////////////////////////////////////////////////////////////////////////
					template<size_t N>
					bool optimizeHybridSolver(PolynomialFitParameters const& fitParameters, Eigen::VectorXd& X,
						PolynomialN<N> const& polynomial, FitDataPointsN<N> const& dataPoints)
					{
						Debug::DebugOutputLevel outputLevel = fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug ? Debug::Info : Debug::Debug;

						HybirdSolverFunctor<N> functor(polynomial, dataPoints);
						HybridNonlinearSolver<N> solver(functor);
						auto status = solver.solve(X);
						switch (status)
						{
							EVAL_HYBRID_SOLVER_STATUS(Running);
							EVAL_HYBRID_SOLVER_STATUS(ImproperInputParameters);
							EVAL_HYBRID_SOLVER_STATUS(RelativeErrorTooSmall);
							EVAL_HYBRID_SOLVER_STATUS(TooManyFunctionEvaluation);
							EVAL_HYBRID_SOLVER_STATUS(TolTooSmall);
							EVAL_HYBRID_SOLVER_STATUS(NotMakingProgressJacobian);
							EVAL_HYBRID_SOLVER_STATUS(NotMakingProgressIterations);
							EVAL_HYBRID_SOLVER_STATUS(UserAsked);
						}
						return status >= 0;
					}
				}

				////////////////////////////////////////////////////////////////////////////////
				namespace NLO_OptimLib
				{
					/*
					gd_settings.method = 0 Vanilla GD
					gd_settings.method = 1 GD with momentum
					gd_settings.method = 2 Nesterov accelerated gradient descent (NAG)
					gd_settings.method = 3 AdaGrad
					gd_settings.method = 4 RMSProp
					gd_settings.method = 5 AdaDelta
					gd_settings.method = 6 Adam and AdaMax
					gd_settings.method = 7 Nadam and NadaMax
					gd_settings.ada_max = false/true
					*/

					static const int DEBUG_LEVEL = 0;

					////////////////////////////////////////////////////////////////////////////////
					optim::algo_settings_t commonSettings(PolynomialFitParameters const& fitParameters, const size_t method)
					{
						optim::algo_settings_t settings;
						settings.print_level = fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug ? DEBUG_LEVEL : 0;
						settings.iter_max = 500;
						settings.grad_err_tol = 1E-05;
						settings.rel_sol_change_tol = 1E-05;
						settings.gd_settings.method = method;
						settings.gd_settings.par_step_size = 1e-1f;
						settings.gd_settings.step_decay = true;
						settings.gd_settings.step_decay_periods = 1;
						settings.gd_settings.step_decay_val = 0.98;
						settings.gd_settings.clip_grad = true;
						settings.gd_settings.clip_norm_type = 5;
						settings.gd_settings.clip_norm_bound = 10.0;
						return settings;
					}

					////////////////////////////////////////////////////////////////////////////////
					template<size_t N>
					struct OptimFunctor
					{
						OptimFunctor(PolynomialN<N> const& polynomial, FitDataPointsN<N> const& dataPoints) :
							m_numSamples(dataPoints.size()),
							m_numTerms(polynomial.size()),
							m_polynomial(polynomial),
							m_dataPoints(dataPoints)
						{
							Eigen::VectorXd b = Eigen::VectorXd::Zero(m_numSamples);
							Eigen::VectorXd w = Eigen::VectorXd::Zero(m_numSamples);
							std::transform(m_dataPoints.begin(), m_dataPoints.end(), b.data(), [](FitDataSamplePointN<N> const& sample) { return double(sample.m_value); });
							std::transform(m_dataPoints.begin(), m_dataPoints.end(), w.data(), [](FitDataSamplePointN<N> const& sample) { return double(sample.m_weight); });
							m_b = b.eval();
							m_w = w.eval();
						}

						AutodiffVar evaluate(AutodiffVarArray const& x) const
						{
							return (Monomials::evalPolynomial(m_polynomial, m_dataPoints, x) - m_b).cwiseAbs2().cwiseProduct(m_w).mean();
						}

						size_t m_numSamples;
						size_t m_numTerms;
						PolynomialN<N> const& m_polynomial;
						FitDataPointsN<N> const& m_dataPoints;
						AutodiffVarArray m_b;
						AutodiffVarArray m_w;
					};

					#ifdef AUTODIFF_FORWARD
					////////////////////////////////////////////////////////////////////////////////
					template<size_t N>
					double targetFunction(Eigen::VectorXd const& x, Eigen::VectorXd* gradOut, void* optData)
					{
						OptimFunctor<N>* functor = (OptimFunctor<N>*)optData;
						autodiff::real u;
						autodiff::ArrayXreal xd = x.eval();

						if (gradOut)
							*gradOut = autodiff::gradient([&](autodiff::ArrayXreal const& xx) { return functor->evaluate(xx); }, autodiff::wrt(xd), autodiff::at(xd), u);
						else
							u = functor->evaluate(xd);

						return u.val();
					}
					#endif

					#ifdef AUTODIFF_REVERSE
					////////////////////////////////////////////////////////////////////////////////
					template<size_t N>
					double targetFunction(Eigen::VectorXd const& x, Eigen::VectorXd* gradOut, void* optData)
					{
						OptimFunctor<N>* functor = (OptimFunctor<N>*)optData;
						autodiff::ArrayXvar xd = x.eval();
						autodiff::var y = functor->evaluate(xd);
						if (gradOut) *gradOut = autodiff::gradient(y, xd);
						return autodiff::val(y);
					}
					#endif

					////////////////////////////////////////////////////////////////////////////////
					template<size_t N>
					bool optimize(PolynomialFitParameters const& fitParameters, Eigen::VectorXd& X, PolynomialN<N> const& polynomial,
						FitDataPointsN<N> const& dataPoints, optim::algo_settings_t settings)
					{
						OptimFunctor<N> functor(polynomial, dataPoints);
						return optim::gd(X, &targetFunction<N>, &functor, settings);
					}
				}
			}

			////////////////////////////////////////////////////////////////////////////////
			namespace MakeDensePolynomialImpl
			{
				////////////////////////////////////////////////////////////////////////////////
				template<size_t N> struct MakeDensePolynomial {};

				////////////////////////////////////////////////////////////////////////////////
				template<> struct MakeDensePolynomial<2>
				{
					static PolynomialN<2> makeDensePolynomial(const int degree, const bool isMaxDegreeTerm = false)
					{
						PolynomialN<2> result;

						if (isMaxDegreeTerm)
						{
							for (int i = 0; i < degree; ++i)
							for (int j = 0; j < degree - i; ++j)
							{
								result.push_back(MonomialN<2>{ i, j });
							}
						}
						else
						{
							for (int i = 0; i < degree; ++i)
							for (int j = 0; j < degree; ++j)
							{
								result.push_back(MonomialN<2>{ i, j });
							}
						}

						return result;
					}
				};

				////////////////////////////////////////////////////////////////////////////////
				template<> struct MakeDensePolynomial<3>
				{
					static PolynomialN<3> makeDensePolynomial(const int degree, const bool isMaxDegreeTerm = false)
					{
						PolynomialN<3> result;

						if (isMaxDegreeTerm)
						{
							for (int i = 0; i < degree; ++i)
							for (int j = 0; j < degree - i; ++j)
							for (int k = 0; k < degree - (i + j); ++k)
							{
								result.push_back(MonomialN<3>{ i, j, k });
							}
						}
						else
						{
							for (int i = 0; i < degree; ++i)
							for (int j = 0; j < degree; ++j)
							for (int k = 0; k < degree; ++k)
							{
								result.push_back(MonomialN<3>{ i, j, k });
							}
						}

						return result;
					}
				};

				////////////////////////////////////////////////////////////////////////////////
				template<> struct MakeDensePolynomial<4>
				{
					static PolynomialN<4> makeDensePolynomial(const int degree, const bool isMaxDegreeTerm = false)
					{
						PolynomialN<4> result;

						if (isMaxDegreeTerm)
						{
							for (int i = 0; i < degree; ++i)
							for (int j = 0; j < degree - i; ++j)
							for (int k = 0; k < degree - (i + j); ++k)
							for (int l = 0; l < degree - (i + j + k); ++l)
							{
								result.push_back(MonomialN<4>{ i, j, k, l });
							}
						}
						else
						{
							for (int i = 0; i < degree; ++i)
							for (int j = 0; j < degree; ++j)
							for (int k = 0; k < degree; ++k)
							for (int l = 0; l < degree; ++l)
							{
								result.push_back(MonomialN<4>{ i, j, k, l });
							}
						}

						return result;
					}
				};

				////////////////////////////////////////////////////////////////////////////////
				template<> struct MakeDensePolynomial<5>
				{
					static PolynomialN<5> makeDensePolynomial(const int degree, const bool isMaxDegreeTerm = false)
					{
						PolynomialN<5> result;

						if (isMaxDegreeTerm)
						{
							for (int i = 0; i < degree; ++i)
							for (int j = 0; j < degree - i; ++j)
							for (int k = 0; k < degree - (i + j); ++k)
							for (int l = 0; l < degree - (i + j + k); ++l)
							for (int m = 0; m < degree - (i + j + k + l); ++m)
							{
								result.push_back(MonomialN<5>{ i, j, k, l, m });
							}
						}
						else
						{
							for (int i = 0; i < degree; ++i)
							for (int j = 0; j < degree; ++j)
							for (int k = 0; k < degree; ++k)
							for (int l = 0; l < degree; ++l)
							for (int m = 0; m < degree; ++m)
							{
								result.push_back(MonomialN<5>{ i, j, k, l, m });
							}
						}

						return result;
					}
				};

				////////////////////////////////////////////////////////////////////////////////
				template<> struct MakeDensePolynomial<6>
				{
					static PolynomialN<6> makeDensePolynomial(const int degree, const bool isMaxDegreeTerm = false)
					{
						PolynomialN<6> result;

						if (isMaxDegreeTerm)
						{
							for (int i = 0; i < degree; ++i)
							for (int j = 0; j < degree - i; ++j)
							for (int k = 0; k < degree - (i + j); ++k)
							for (int l = 0; l < degree - (i + j + k); ++l)
							for (int m = 0; m < degree - (i + j + k + l); ++m)
							for (int n = 0; n < degree - (i + j + k + l + m); ++n)
							{
								result.push_back(MonomialN<6>{ i, j, k, l, m, n });
							}
						}
						else
						{
							for (int i = 0; i < degree; ++i)
							for (int j = 0; j < degree; ++j)
							for (int k = 0; k < degree; ++k)
							for (int l = 0; l < degree; ++l)
							for (int m = 0; m < degree; ++m)
							for (int n = 0; n < degree; ++n)
							{
								result.push_back(MonomialN<6>{ i, j, k, l, m, n });
							}
						}

						return result;
					}
				};

				////////////////////////////////////////////////////////////////////////////////
				template<> struct MakeDensePolynomial<7>
				{
					static PolynomialN<7> makeDensePolynomial(const int degree, const bool isMaxDegreeTerm = false)
					{
						PolynomialN<7> result;

						if (isMaxDegreeTerm)
						{
							for (int i = 0; i < degree; ++i)
							for (int j = 0; j < degree - i; ++j)
							for (int k = 0; k < degree - (i + j); ++k)
							for (int l = 0; l < degree - (i + j + k); ++l)
							for (int m = 0; m < degree - (i + j + k + l); ++m)
							for (int n = 0; n < degree - (i + j + k + l + m); ++n)
							for (int o = 0; o < degree - (i + j + k + l + m + n); ++o)
							{
								result.push_back(MonomialN<7>{ i, j, k, l, m, n, o });
							}
						}
						else
						{
							for (int i = 0; i < degree; ++i)
							for (int j = 0; j < degree; ++j)
							for (int k = 0; k < degree; ++k)
							for (int l = 0; l < degree; ++l)
							for (int m = 0; m < degree; ++m)
							for (int n = 0; n < degree; ++n)
							for (int o = 0; o < degree; ++o)
							{
								result.push_back(MonomialN<7>{ i, j, k, l, m, n, o });
							}
						}

						return result;
					}
				};

				////////////////////////////////////////////////////////////////////////////////
				template<> struct MakeDensePolynomial<8>
				{
					static PolynomialN<8> makeDensePolynomial(const int degree, const bool isMaxDegreeTerm = false)
					{
						PolynomialN<8> result;

						if (isMaxDegreeTerm)
						{
							for (int i = 0; i < degree; ++i)
							for (int j = 0; j < degree - i; ++j)
							for (int k = 0; k < degree - (i + j); ++k)
							for (int l = 0; l < degree - (i + j + k); ++l)
							for (int m = 0; m < degree - (i + j + k + l); ++m)
							for (int n = 0; n < degree - (i + j + k + l + m); ++n)
							for (int o = 0; o < degree - (i + j + k + l + m + n); ++o)
							for (int p = 0; p < degree - (i + j + k + l + m + n + o); ++p)
							{
								result.push_back(MonomialN<8>{ i, j, k, l, m, n, o, p });
							}
						}
						else
						{
							for (int i = 0; i < degree; ++i)
							for (int j = 0; j < degree; ++j)
							for (int k = 0; k < degree; ++k)
							for (int l = 0; l < degree; ++l)
							for (int m = 0; m < degree; ++m)
							for (int n = 0; n < degree; ++n)
							for (int o = 0; o < degree; ++o)
							for (int p = 0; p < degree; ++p)
							{
								result.push_back(MonomialN<8>{ i, j, k, l, m, n, o, p });
							}
						}

						return result;
					}
				};
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			PolynomialN<N> makeDensePolynomial(const int degree, const bool isMaxDegreeTerm = false)
			{
				return MakeDensePolynomialImpl::MakeDensePolynomial<N>::makeDensePolynomial(degree, isMaxDegreeTerm);
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			PolynomialN<N> makeSparsePolynomial(PolynomialN<N> const& densePolynomial, std::vector<size_t>& ids, int numTerms)
			{
				// Make a random set of indices
				std::random_device rd;
				std::mt19937 gen(rd());
				std::shuffle(ids.begin(), ids.end(), gen);

				// Copy over the new terms
				PolynomialN<N> result(numTerms);
				for (size_t i = 0; i < numTerms; ++i)
					result[i] = densePolynomial[ids[i]];

				// Return the result
				return result;
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			PolynomialN<N> swapSparseTerms(PolynomialN<N> sparsePolynomial, PolynomialN<N> const& densePolynomial, std::vector<size_t>& ids, int numTerms)
			{
				std::random_device rd;
				std::mt19937 gen(rd());
				std::uniform_int_distribution<int> rng(sparsePolynomial.size(), densePolynomial.size() - 1);

				// Make a random set of swap ids
				std::vector<size_t> swapIds = std::iota<size_t>(sparsePolynomial.size());
				std::shuffle(swapIds.begin(), swapIds.end(), gen);

				// Randomly swap terms
				for (size_t i = 0; i < numTerms; ++i)
				{
					const size_t swapId = swapIds[i];
					const size_t idx = rng(gen);
					sparsePolynomial[swapId] = densePolynomial[ids[idx]];
					std::swap(ids[swapId], ids[idx]);
				}

				// Return the result
				return sparsePolynomial;
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			PolynomialN<N> fitDenseCoefficients(PolynomialFitParameters const& fitParameters, PolynomialN<N> polynomial,
				FitDataPointsN<N> const& dataPoints, const bool fitLinear, const bool fitNonlinear)
			{
				const size_t numTerms = polynomial.size();
				const size_t numSamples = dataPoints.size();

				// Zero out the coefficients first
				for (size_t termID = 0; termID < numTerms; ++termID)
					polynomial[termID].m_coefficient = 0.0f;

				// Early out if we have no samples
				if (numSamples == 0) return polynomial;

				// Construct the X matrix
				Eigen::MatrixXd A = Eigen::MatrixXd::Zero(numSamples, numTerms); // rows, columns
				for (size_t termID = 0; termID < numTerms; ++termID)
				for (size_t sampleID = 0; sampleID < numSamples; ++sampleID)
					A(sampleID, termID) = Monomials::evalMonomial(dataPoints[sampleID], polynomial[termID], 1.0f);

				// Construct the Y vector
				Eigen::VectorXd b = Eigen::VectorXd::Zero(numSamples);
				std::transform(dataPoints.begin(), dataPoints.end(), b.data(),
					[](FitDataSamplePointN<N> const& sample) { return double(sample.m_value); });

				// Perform the fitting (linear)
				Eigen::VectorXd x = Eigen::VectorXd::Ones(numTerms);
				if (fitLinear && fitParameters.m_denseFitLinearMethod != PolynomialFitParameters::DenseFitLinearMethod::SkipLinear)
				{
					switch (fitParameters.m_denseFitLinearMethod)
					{
					case PolynomialFitParameters::DenseFitLinearMethod::BDCSVD:
						x = A.bdcSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(b);
						break;
					case PolynomialFitParameters::DenseFitLinearMethod::JacobiSVD:
						x = A.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(b);
						break;
					case PolynomialFitParameters::DenseFitLinearMethod::PartialPivLU:
						x = A.partialPivLu().solve(b);
						break;
					case PolynomialFitParameters::DenseFitLinearMethod::FullPivLU:
						x = A.fullPivLu().solve(b);
						break;
					case PolynomialFitParameters::DenseFitLinearMethod::ColPivHouseholderQr:
						x = A.colPivHouseholderQr().solve(b);
						break;
					case PolynomialFitParameters::DenseFitLinearMethod::FullPivHouseholderQr:
						x = A.fullPivHouseholderQr().solve(b);
						break;
					case PolynomialFitParameters::DenseFitLinearMethod::LLT:
						x = (A.transpose() * A).llt().solve(A.transpose() * b);
						break;
					case PolynomialFitParameters::DenseFitLinearMethod::LDLT:
						x = (A.transpose() * A).ldlt().solve(A.transpose() * b);
						break;
					case PolynomialFitParameters::DenseFitLinearMethod::CompleteOrthogonalDecomposition:
						x = A.completeOrthogonalDecomposition().solve(b);
						break;
					}
				}

				// Perform the fitting (non-linear)
				if (fitNonlinear && fitParameters.m_denseFitNonlinearMethod != PolynomialFitParameters::DenseFitNonlinearMethod::SkipNonLinear)
				{
					switch (fitParameters.m_denseFitNonlinearMethod)
					{
					case PolynomialFitParameters::DenseFitNonlinearMethod::LevMar:
						NonlinearOptimization::NLO_Eigen::optimizeLevMar(fitParameters, x, polynomial, dataPoints);
						break;
					case PolynomialFitParameters::DenseFitNonlinearMethod::HybridNonlinearSolver:
						NonlinearOptimization::NLO_Eigen::optimizeHybridSolver(fitParameters, x, polynomial, dataPoints);
						break;
					case PolynomialFitParameters::DenseFitNonlinearMethod::GradientDescent:
						NonlinearOptimization::NLO_OptimLib::optimize(fitParameters, x, polynomial, dataPoints,
							NonlinearOptimization::NLO_OptimLib::commonSettings(fitParameters, 0));
						break;
					case PolynomialFitParameters::DenseFitNonlinearMethod::NAG:
						NonlinearOptimization::NLO_OptimLib::optimize(fitParameters, x, polynomial, dataPoints,
							NonlinearOptimization::NLO_OptimLib::commonSettings(fitParameters, 2));
						break;
					case PolynomialFitParameters::DenseFitNonlinearMethod::RMSProp:
						NonlinearOptimization::NLO_OptimLib::optimize(fitParameters, x, polynomial, dataPoints,
							NonlinearOptimization::NLO_OptimLib::commonSettings(fitParameters, 4));
						break;
					case PolynomialFitParameters::DenseFitNonlinearMethod::Adam:
						NonlinearOptimization::NLO_OptimLib::optimize(fitParameters, x, polynomial, dataPoints,
							NonlinearOptimization::NLO_OptimLib::commonSettings(fitParameters, 6));
						break;
					case PolynomialFitParameters::DenseFitNonlinearMethod::Nadam:
						NonlinearOptimization::NLO_OptimLib::optimize(fitParameters, x, polynomial, dataPoints,
							NonlinearOptimization::NLO_OptimLib::commonSettings(fitParameters, 7));
						break;
					}
				}

				// Write out the coefficients and scales
				const glm::vec2 coefficientLimits = fitParameters.m_coefficientLimits;
				for (size_t termID = 0; termID < numTerms; ++termID)
				{
					const float coefficient = float(x[termID]);
					//polynomial[termID].m_coefficient = coefficient;
					
					polynomial[termID].m_coefficient = 
						glm::sign(coefficient) * glm::clamp(glm::abs(coefficient), coefficientLimits[0], coefficientLimits[1]);
				}

				// Return the result
				return polynomial;
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			PolynomialN<N> fitDenseCoefficients(PolynomialFitParameters const& fitParameters, PolynomialN<N> const& polynomial, FitDataPointsN<N> const& dataPoints)
			{
				return fitDenseCoefficients(fitParameters, polynomial, dataPoints, true,
					fitParameters.m_nonlinearFitFrequency == PolynomialFitParameters::NonlinearFitFrequency::EveryIteration);
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		namespace Fitting
		{
			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			FitDataSamplePointN<N> makeSamplePoint(Scene::Scene& scene, Scene::Object* object, 
				PolynomialFitParameters const& fitParameters, PhysicalCamera::PhysicalCameraAttributes const& camera, 
				GhostGeometry::GeometryEntry const& sample, std::array<std::string, N> const& inputVariables, 
				std::string const& variableName, Attribs::AttribData const& attribData)
			{
				FitDataSamplePointN<N> result;
				for (size_t variableID = 0; variableID < inputVariables.size(); ++variableID)
					result.m_input[variableID] = Attribs::getVariableData(inputVariables[variableID]).m_valueExtractor(sample);
				if      (!sample.m_validityFlags.m_isTractable)  result.m_weight = 0.0f;
				else if (sample.m_validityFlags.m_isBoundary)    result.m_weight = fitParameters.m_outputVariableParams.find(variableName)->second.m_boundaryWeight;
				else if (sample.m_validityFlags.m_isInternal)    result.m_weight = fitParameters.m_outputVariableParams.find(variableName)->second.m_interiorWeight;
				else                                             result.m_weight = fitParameters.m_outputVariableParams.find(variableName)->second.m_exteriorWeight;
				result.m_value = 
					sample.m_validityFlags.m_isTractable ? 
					(*attribData.m_valueExtractor)(sample) : 
					(*attribData.m_defaultValueExtractor)(scene, object, fitParameters, variableName);
				result.m_validityFlags = sample.m_validityFlags;
				return result;
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			FitDataSamplePointN<N> makeSamplePoint(Scene::Scene& scene, Scene::Object* object,
				PolynomialFitParameters const& fitParameters, PhysicalCamera::PhysicalCameraAttributes const& camera, 
				GhostGeometry::GeometryEntry const& sample, std::array<std::string, N> const& inputVariables, std::string const& variableName)
			{
				return makeSamplePoint(scene, object, fitParameters, camera, sample, inputVariables, variableName, Attribs::getVariableData(variableName));
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			float getApproximationError(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, PolynomialN<N> const& polynomial,
				FitDataPointsN<N> const& dataPoints, std::string const& variableName, bool threaded = false)
			{
				auto const& errorFnFactory = Attribs::getVariableData(variableName).m_errorFnFactory;
				std::unique_ptr<Errors::ErrorFn> errorFunction{ errorFnFactory(scene, object, fitParameters, variableName, 1) };
				errorFunction->initialize();
				for (size_t sampleID = 0; sampleID < dataPoints.size(); ++sampleID)
					errorFunction->addSample(dataPoints[sampleID], polynomial);
				errorFunction->finalize();
				return errorFunction->error();
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			void logApproximationError(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters, PolynomialN<N> const& polynomial,
				FitDataPointsN<N> const& dataPoints, std::string const& variableName)
			{
				auto const& errorFnFactory = Attribs::getVariableData(variableName).m_errorFnFactory;
				std::unique_ptr<Errors::ErrorFn> errorFunction{ errorFnFactory(scene, object, fitParameters, variableName, 1) };
				errorFunction->initialize();
				for (size_t sampleID = 0; sampleID < dataPoints.size(); ++sampleID)
					errorFunction->logSample(dataPoints[sampleID], polynomial);
				errorFunction->finalize();
			}

			////////////////////////////////////////////////////////////////////////////////
			namespace PartialCollapseImpl
			{
				////////////////////////////////////////////////////////////////////////////////
				struct CollapseMin
				{
					float m_partialResult = FLT_MAX;
					void addError(const float error) { m_partialResult = glm::min(m_partialResult, error); }
					float getResult(const size_t numElements) { return m_partialResult; }
				};

				////////////////////////////////////////////////////////////////////////////////
				struct CollapseMax
				{
					float m_partialResult = -FLT_MAX;
					void addError(const float error) { m_partialResult = glm::max(m_partialResult, error); }
					float getResult(const size_t numElements) { return m_partialResult; }
				};

				////////////////////////////////////////////////////////////////////////////////
				struct CollapseMeanAbsolute
				{
					float m_partialResult = 0.0f;
					void addError(const float error) { m_partialResult += glm::abs(error); }
					float getResult(const size_t numElements) { return m_partialResult / std::max(float(numElements), 1.0f); }
				};

				////////////////////////////////////////////////////////////////////////////////
				struct CollapseMeanSquared
				{
					float m_partialResult = 0.0f;
					void addError(const float error) { m_partialResult += glm::pow(error, 2.0f); }
					float getResult(const size_t numElements) { return m_partialResult / std::max(float(numElements), 1.0f); }
				};

				////////////////////////////////////////////////////////////////////////////////
				struct CollapseRootMeanSquared
				{
					float m_partialResult = 0.0f;
					void addError(const float error) { m_partialResult += glm::pow(error, 2.0f); }
					float getResult(const size_t numElements) { return numElements == 0 || m_partialResult <= 0.0f ? 0.0f : glm::sqrt(m_partialResult / std::max(float(numElements), 1.0f)); }
				};

				////////////////////////////////////////////////////////////////////////////////
				template<typename CollapseFn, size_t N>
				float getCollapsedApproximationErrorSingleThread(Scene::Scene& scene, Scene::Object* object,
					PolynomialFitParameters const& fitParameters, PolynomialN<N> const& polynomial,
					FitDataSetN<N> const& datasetFit, FitDataSetN<N> const& dataset, std::string const& variableName, CollapseFn collapseFn)
				{
					// Pointer to the dataset data
					auto datasetFitPtr = dataset.data();
					auto datasetPtr = dataset.data();

					// Process the entries
					for (size_t i = 0; i < dataset.num_elements(); ++i)
					{
						const PolynomialN<N> fitPolynomial = DensePolynomial::fitDenseCoefficients(fitParameters, polynomial, datasetFitPtr[i]);
						const float error = getApproximationError(scene, object, fitParameters, fitPolynomial, datasetPtr[i], variableName, false);
						collapseFn.addError(error);
					}
					return collapseFn.getResult(dataset.num_elements());
				}

				////////////////////////////////////////////////////////////////////////////////
				template<typename CollapseFn, size_t N>
				float getCollapsedApproximationErrorMultiThread(Scene::Scene& scene, Scene::Object* object,
					PolynomialFitParameters const& fitParameters, PolynomialN<N> const& polynomial,
					FitDataSetN<N> const& datasetFit, FitDataSetN<N> const& dataset, std::string const& variableName, CollapseFn collapseFn)
				{
					// Pointer to the dataset data
					auto datasetFitPtr = dataset.data();
					auto datasetPtr = dataset.data();

					// Build a list of valid entries to process
					std::vector<size_t> validEntries = std::iota<size_t>(dataset.num_elements(), 0);
					validEntries.erase(std::remove_if(validEntries.begin(), validEntries.end(), 
						[&](const size_t id) { return datasetPtr[id].empty(); }),
						validEntries.end());

					// Process the valid entries
					std::mutex updateMutex;
					Threading::threadedExecuteIndices(Threading::numThreads(),
						[&](Threading::ThreadedExecuteEnvironment const& environment, size_t i)
						{
							const size_t entryId = validEntries[i];
							const PolynomialN<N> fitPolynomial = DensePolynomial::fitDenseCoefficients(fitParameters, polynomial, datasetFitPtr[entryId]);
							const float error = getApproximationError(scene, object, fitParameters, fitPolynomial, datasetPtr[entryId], variableName, false);
							{
								std::lock_guard<std::mutex>	lock(updateMutex);
								collapseFn.addError(error);
							}
						},
						validEntries.size());
					return collapseFn.getResult(dataset.num_elements());
				}

				////////////////////////////////////////////////////////////////////////////////
				template<typename CollapseFn, size_t N>
				float getCollapsedApproximationError(Scene::Scene& scene, Scene::Object* object, 
					PolynomialFitParameters const& fitParameters, PolynomialN<N> const& polynomial,
					FitDataSetN<N> const& datasetFit, FitDataSetN<N> const& dataset, std::string const& variableName, 
					CollapseFn collapseFn, const bool parallelEval)
				{
					if (parallelEval) return getCollapsedApproximationErrorMultiThread(scene, object, fitParameters, polynomial, datasetFit, dataset, variableName, collapseFn);
					else              return getCollapsedApproximationErrorSingleThread(scene, object, fitParameters, polynomial, datasetFit, dataset, variableName, collapseFn);
				}
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			float getApproximationError(Scene::Scene& scene, Scene::Object* object,
				PolynomialFitParameters const& fitParameters, PolynomialN<N> const& polynomial,
				FitDataSetN<N> const& datasetFit, FitDataSetN<N> const& dataset, std::string const& variableName)
			{
				const bool parallelEval = dataset.num_elements() > 1 && fitParameters.m_evaluateEntriesInParallel;
				switch (fitParameters.m_partialErrorCollapseMethod)
				{
				case PolynomialFitParameters::PartialErrorCollapseMethod::PCM_Min:
					return PartialCollapseImpl::getCollapsedApproximationError(scene, object, fitParameters, polynomial, datasetFit, dataset, variableName, PartialCollapseImpl::CollapseMin{}, parallelEval);
				case PolynomialFitParameters::PartialErrorCollapseMethod::PCM_Max:
					return PartialCollapseImpl::getCollapsedApproximationError(scene, object, fitParameters, polynomial, datasetFit, dataset, variableName, PartialCollapseImpl::CollapseMax{}, parallelEval);
				case PolynomialFitParameters::PartialErrorCollapseMethod::PCM_MAE:
					return PartialCollapseImpl::getCollapsedApproximationError(scene, object, fitParameters, polynomial, datasetFit, dataset, variableName, PartialCollapseImpl::CollapseMeanAbsolute{}, parallelEval);
				case PolynomialFitParameters::PartialErrorCollapseMethod::PCM_MSE:
					return PartialCollapseImpl::getCollapsedApproximationError(scene, object, fitParameters, polynomial, datasetFit, dataset, variableName, PartialCollapseImpl::CollapseMeanSquared{}, parallelEval);
				case PolynomialFitParameters::PartialErrorCollapseMethod::PCM_RMSE:
					return PartialCollapseImpl::getCollapsedApproximationError(scene, object, fitParameters, polynomial, datasetFit, dataset, variableName, PartialCollapseImpl::CollapseRootMeanSquared{}, parallelEval);
				}
				return PartialCollapseImpl::getCollapsedApproximationError(scene, object, fitParameters, polynomial, datasetFit, dataset, variableName, PartialCollapseImpl::CollapseMeanAbsolute{}, parallelEval);
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			FitDataPointsN<N> subsampleDatapoints(PolynomialFitParameters const& fitParameters, FitDataPointsN<N> dataPoints, const size_t numSamples)
			{
				// Early out for empty datasets
				if (dataPoints.empty()) return dataPoints;

				// Count the relevant samples
				const size_t relevantSamples = std::count_if(dataPoints.begin(), dataPoints.end(), 
					[&](FitDataSamplePointN<N> const& sample){ return SampleValidity::isSampleValidForDataset(fitParameters, sample); });

				// Fall back to simpler implementation if all samples are valid/invalid
				if (relevantSamples == 0 || relevantSamples == dataPoints.size())
					return std::random_samples(dataPoints, numSamples);

				// Separate the valid and invalid samples
				auto validSamplesEnd = std::partition(dataPoints.begin(), dataPoints.end(), 
					[&](FitDataSamplePointN<N> const& sample) { return SampleValidity::isSampleValidForDataset(fitParameters, sample); });

				// Fall back to simpler implementation if all samples are valid/invalid
				if (validSamplesEnd == dataPoints.begin() || validSamplesEnd == dataPoints.end())
					return std::random_samples(dataPoints, numSamples);

				// Calculate the valid and invalid samples
				const int numValidPoints = std::distance(dataPoints.begin(), validSamplesEnd);
				const int numInvalidPoints = dataPoints.size() - numValidPoints;

				// Determine how many samples to take
				const float validRatio = fitParameters.m_ghostGeometryParameters.m_validSampleRatio;
				const int numTargetValidPoints = std::max(int(numSamples) - numInvalidPoints, int(float(numSamples) * validRatio));
				const int numResultValidPoints = std::min(numValidPoints, numTargetValidPoints);

				const int numTargetInvalidPoints = numSamples - numResultValidPoints;
				const int numResultInvalidPoints = std::min(numInvalidPoints, numTargetInvalidPoints);

				// Make a random set of indices
				std::random_device rd;
				std::mt19937 gen(rd());
				std::vector<size_t> validIds = std::iota<size_t>(numValidPoints, 0);
				std::vector<size_t> invalidIds = std::iota<size_t>(numInvalidPoints, numValidPoints);
				shuffle(validIds.begin(), validIds.end(), gen);
				shuffle(invalidIds.begin(), invalidIds.end(), gen);

				// Construct the result
				FitDataPointsN<N> result(numResultValidPoints + numResultInvalidPoints);
				std::transform(validIds.begin(), validIds.begin() + numResultValidPoints, result.begin(), [&](const size_t idx) { return dataPoints[idx]; });
				std::transform(invalidIds.begin(), invalidIds.begin() + numResultInvalidPoints, result.begin() + numResultValidPoints, [&](const size_t idx) { return dataPoints[idx]; });

				// Return the result
				return result;
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			FitDataSetN<N> subsampleDataset(PolynomialFitParameters const& fitParameters, FitDataSetN<N> const& dataset, const size_t numSamples)
			{
				FitDataSetN<N> result(boost::extents[dataset.shape()[0]][dataset.shape()[1]][dataset.shape()[2]]);
				std::transform(dataset.data(), dataset.data() + dataset.num_elements(), result.data(), 
					[&](FitDataPointsN<N> const& dataPoints){ return subsampleDatapoints(fitParameters, dataPoints, numSamples); });
				return result;
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			FitDataPointsN<N> constructFitDatapoints(PolynomialFitParameters const& fitParameters, FitDataPointsN<N> dataPoints)
			{
				dataPoints.erase(std::remove_if(dataPoints.begin(), dataPoints.end(), [&](FitDataSamplePointN<N> const& sample)
					{ return !SampleValidity::isSampleValidForFit(fitParameters, sample); }), dataPoints.end());
				return dataPoints;
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N>
			FitDataSetN<N> constructFitDataset(PolynomialFitParameters const& fitParameters, FitDataSetN<N> const& dataset)
			{
				FitDataSetN<N> result(boost::extents[dataset.shape()[0]][dataset.shape()[1]][dataset.shape()[2]]);
				std::transform(dataset.data(), dataset.data() + dataset.num_elements(), result.data(), [&](FitDataPointsN<N> const& dataPoints) 
					{ return constructFitDatapoints(fitParameters, dataPoints); });
				return result;
			}

			////////////////////////////////////////////////////////////////////////////////
			namespace SimulatedAnnealing
			{
				////////////////////////////////////////////////////////////////////////////////
				float getTemperature(PolynomialFitParameters const& fitParameters, 
					PolynomialFitParameters::AnnealingParams const& annealingParameters,
					const size_t iterationID)
				{
					return float(fitParameters.m_annealingParams.m_numIterations - iterationID) / float(fitParameters.m_annealingParams.m_numIterations);
				}

				////////////////////////////////////////////////////////////////////////////////
				int getNumSwap(PolynomialFitParameters const& fitParameters, 
					PolynomialFitParameters::AnnealingParams const& annealingParameters,
					const float temperature)
				{
					const float t = glm::pow(temperature, fitParameters.m_annealingParams.m_swapAnnealingRate);
					if (fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug)
						Debug::log_debug() << "Temperature: " << temperature << ", " << "t: " << t << Debug::end;
					return glm::clamp(int(t * fitParameters.m_numSparseTerms), 1, fitParameters.m_numSparseTerms);
				}

				////////////////////////////////////////////////////////////////////////////////
				float getAcceptanceProbability(PolynomialFitParameters const& fitParameters, 
					PolynomialFitParameters::AnnealingParams const& annealingParameters,
					const float temperature, const float prevError, const float newError)
				{
					if (newError < prevError)
						return 1.0f;

					const float errorMag = glm::max(std::log10(1.0f / prevError), std::log10(1.0f / newError));
					const float multiplier = glm::pow(10.0f, errorMag + fitParameters.m_annealingParams.m_acceptanceOffset);
					if (fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug)
						Debug::log_debug() << "Temperature: " << temperature << ", " << "multiplier: " << multiplier << ", " << "x: " << -((newError - prevError) * multiplier) / temperature << Debug::end;
					return glm::exp(-((newError - prevError) * multiplier) / temperature);
				}

				////////////////////////////////////////////////////////////////////////////////
				float getAcceptance()
				{
					// Random number generator for the acceptance evaluation
					std::random_device rd;
					std::mt19937 gen(rd());
					std::uniform_real_distribution<float> rng(0.0f, 1.0f);
					return rng(gen);
				}

				////////////////////////////////////////////////////////////////////////////////
				template<size_t N>
				FitResultN<N> fit(Scene::Scene& scene, Scene::Object* object, std::string const& variableName,
					FitDataSetN<N> const& datasets, FitDataSetN<N> const& datasetsSubsampled,
					PolynomialFitParameters const& fitParameters, PolynomialFitParameters::AnnealingParams const& annealingParameters)
				{
					Debug::DebugOutputLevel outputLevel = fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug ? Debug::Info : Debug::Debug;

					const size_t numAngles = datasets.shape()[0];
					const size_t numRotations = datasets.shape()[1];
					const size_t numWavelengths = datasets.shape()[2];

					// Calculate the weighed error threshold
					const float errorThreshold = annealingParameters.m_outputVariableParams.find(variableName)->second.m_stopErrorThreshold;

					// Dense polynomial to sparsify
					PolynomialN<N> densePolynomial = DensePolynomial::makeDensePolynomial<N>(fitParameters.m_maxTermDegree, true);
					std::vector<size_t> coefficientIds = std::iota<size_t>(densePolynomial.size(), 0);

					// Results of the fitting
					FitResultN<N> result(boost::extents[numAngles][numRotations][numWavelengths]);
					PolynomialN<N> resultPolynomial = DensePolynomial::makeSparsePolynomial(densePolynomial, coefficientIds, fitParameters.m_numSparseTerms);
					FitStatsData resultStats;
					resultStats.m_computationTimer.start();

					FitDataSetN<N> const& datasetsSubsampledFit = constructFitDataset(fitParameters, datasetsSubsampled);
					FitDataSetN<N> const& datasetsFit = constructFitDataset(fitParameters, datasets);

					// Get the initial fitting error
					float prevError = getApproximationError(scene, object, fitParameters, resultPolynomial, datasetsSubsampledFit, datasetsSubsampled, variableName);

					Debug::log_output(outputLevel) << "Initial error: " << prevError << Debug::end;
					if (fitParameters.m_debugComputation)
						Debug::log_debug() << "Initial polynomial: " << resultPolynomial << Debug::end;

					// Perform the iterations
					size_t iterationID;
					for (iterationID = 0; iterationID < annealingParameters.m_numIterations; ++iterationID)
					{
						// Calculate the current temperature
						const float temperature = getTemperature(fitParameters, annealingParameters, iterationID);

						// Number of terms to swap
						const int numTermsSwap = getNumSwap(fitParameters, annealingParameters, temperature);

						// Construct the new polynomial
						const PolynomialN<N> newPolynomial = DensePolynomial::swapSparseTerms(resultPolynomial, densePolynomial, coefficientIds, numTermsSwap);
						
						// Evaluate the new error
						const float newError = getApproximationError(scene, object, fitParameters, newPolynomial, datasetsSubsampledFit, datasetsSubsampled, variableName);

						// Evaluate the acceptance probablity
						const float acceptanceProbability = getAcceptanceProbability(fitParameters, annealingParameters, temperature, prevError, newError);
						const float acceptance = getAcceptance();

						Debug::log_output(outputLevel) << " - Iteration #" << iterationID << ", "
							<< "error: " << newError << " (" << prevError << ")" ", "
							<< "p_accept: " << acceptanceProbability << ", "
							<< "p: " << acceptance << ", "
							<< "swap: " << numTermsSwap
							<< Debug::end;
						if (fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug)
							Debug::log_debug() << "   " << newPolynomial << Debug::end;

						// Evaluate the acceptance
						if (acceptance <= acceptanceProbability)
						{
							prevError = newError;
							resultPolynomial = newPolynomial;
						}

						// Early stopping
						if (prevError < errorThreshold)
						{
							if (fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug)
								Debug::log_debug() << "   " << "Error threshold (" << errorThreshold << ") reached, stopping" << Debug::end;
							break;
						}
					}

					// Fill in the stats
					resultStats.m_computationTimer.stop();

					for (size_t angleID = 0; angleID < numAngles; ++angleID)
					for (size_t rotationID = 0; rotationID < numRotations; ++rotationID)
					for (size_t wavelengthID = 0; wavelengthID < numWavelengths; ++wavelengthID)
					{
						// Perform the final fit
						result[angleID][rotationID][wavelengthID].m_polynomial = DensePolynomial::fitDenseCoefficients(fitParameters, resultPolynomial,
							datasetsFit[angleID][rotationID][wavelengthID], true,
							fitParameters.m_nonlinearFitFrequency != PolynomialFitParameters::NonlinearFitFrequency::Never);

						// Evaluate the fitting errors and store the result
						resultStats.m_numIterations = iterationID;
						resultStats.m_numTerms = resultPolynomial.size();
						resultStats.m_finalErrorSubSampled = getApproximationError(scene, object, fitParameters, result[angleID][rotationID][wavelengthID].m_polynomial, datasetsSubsampled[angleID][rotationID][wavelengthID], variableName);
						resultStats.m_finalErrorFullDataset = getApproximationError(scene, object, fitParameters, result[angleID][rotationID][wavelengthID].m_polynomial, datasets[angleID][rotationID][wavelengthID], variableName);
						result[angleID][rotationID][wavelengthID].m_fitStats = resultStats;
					}

					return result;
				}
			}

			////////////////////////////////////////////////////////////////////////////////
			namespace PolynomialRegression
			{
				////////////////////////////////////////////////////////////////////////////////	
				template<size_t N>
				void evalPolynomial(Scene::Scene& scene, Scene::Object* object, 
					PolynomialFitParameters const& fitParameters,
					PolynomialFitParameters::PolynomialRegressionParams const& regressionParams,
					FitDataSetN<N> const& datasetFit, FitDataSetN<N> const& dataset, std::string const& variableName,
					PolynomialN<N>& result, float& resultError, FitStatsData& resultStats,
					std::optional<PolynomialN<N>> const& testPolynomial)
				{
					// Do nothing it the polynomial is not valid
					if (!testPolynomial.has_value()) return;

					// Calculate the fitting error of the polynomial
					const float testError = getApproximationError(scene, object, fitParameters, testPolynomial.value(), datasetFit, dataset, variableName);

					// Evaluate the fit
					static std::mutex s_swapMutex;
					{
						std::lock_guard<std::mutex>	lock(s_swapMutex);

						if (testError < resultError)
						{
							result = testPolynomial.value();
							resultError = testError;
						}

						++resultStats.m_numPermutationsTested;
					}
				}

				////////////////////////////////////////////////////////////////////////////////	
				template<size_t N>
				bool useThreading(Scene::Scene& scene, Scene::Object* object,
					PolynomialFitParameters const& fitParameters,
					PolynomialFitParameters::PolynomialRegressionParams const& regressionParams,
					FitDataSetN<N> const& datasetFit)
				{
					return datasetFit.num_elements() == 1 || !fitParameters.m_evaluateEntriesInParallel;
				}

				////////////////////////////////////////////////////////////////////////////////	
				template<size_t N>
				std::optional<PolynomialN<N>> expandPolynomial(PolynomialFitParameters const& fitParameters,
					PolynomialFitParameters::PolynomialRegressionParams const& regressionParams,
					PolynomialN<N> currentPolynomial, MonomialN<N> const& newTerm)
				{
					// Return a null polynomial if the term has a negative degree
					if (newTerm.isAnyDegreeNegative()) return std::nullopt;
					if (newTerm.maxDegree() >= fitParameters.m_maxTermDegree) return std::nullopt;

					// Return a null polynomial if the term is already present
					auto it = std::find_if(currentPolynomial.begin(), currentPolynomial.end(), [&](MonomialN<N> const& testTerm)
						{ return testTerm.sameDegrees(newTerm); });
					if (it != currentPolynomial.end()) return std::nullopt;

					// Construct the resulting polynomial
					currentPolynomial.push_back(newTerm);
					return currentPolynomial;
				}

				////////////////////////////////////////////////////////////////////////////////
				template<size_t N>
				std::optional<PolynomialN<N>> expandF1(PolynomialFitParameters const& fitParameters,
					PolynomialFitParameters::PolynomialRegressionParams const& regressionParams,
					PolynomialN<N> const& currentPolynomial, const size_t i, const size_t id, const int depth)
				{
					return expandPolynomial(fitParameters, regressionParams, currentPolynomial, currentPolynomial[i].increaseDegree(id, depth));
				}

				////////////////////////////////////////////////////////////////////////////////
				template<size_t N>
				std::optional<PolynomialN<N>> expandF2(PolynomialFitParameters const& fitParameters, 
					PolynomialFitParameters::PolynomialRegressionParams const& regressionParams,
					PolynomialN<N> const& currentPolynomial, const size_t id, const int depth)
				{
					return expandPolynomial(fitParameters, regressionParams, currentPolynomial, MonomialN<N>{}.increaseDegree(id, depth));
				}

				////////////////////////////////////////////////////////////////////////////////
				template<size_t N>
				std::pair<PolynomialN<N>, float> expandPolynomial(Scene::Scene& scene, Scene::Object* object,
					PolynomialFitParameters const& fitParameters,
					PolynomialFitParameters::PolynomialRegressionParams const& regressionParams, FitStatsData& resultStats,
					PolynomialN<N> const& currentPolynomial, FitDataSetN<N> const& datasetFit, FitDataSetN<N> const& datasets,
					std::string const& variableName, const int depth)
				{
					PolynomialN<N> result = currentPolynomial;
					float resultError = FLT_MAX;

					if (useThreading(scene, object, fitParameters, regressionParams, datasetFit))
					{
						// F1 operator: duplicate one entry and increase its exponents by d
						Threading::threadedExecuteIndices(Threading::numThreads(),
							[&](Threading::ThreadedExecuteEnvironment const& environment, size_t i, size_t id)
							{
								evalPolynomial(scene, object, fitParameters, regressionParams,
									datasetFit, datasets, variableName, result, resultError, resultStats,
									expandF1(fitParameters, regressionParams, currentPolynomial, i, id, depth));
							},
							currentPolynomial.size(),
							N);

						// F2 operator: add entries with only one term set to d
						Threading::threadedExecuteIndices(Threading::numThreads(),
							[&](Threading::ThreadedExecuteEnvironment const& environment, size_t id)
							{
								evalPolynomial(scene, object, fitParameters, regressionParams,
									datasetFit, datasets, variableName, result, resultError, resultStats,
									expandF2(fitParameters, regressionParams, currentPolynomial, id, depth));
							},
							N);
					}
					else
					{
						// F1 operator: duplicate one entry and increase its exponents by d
						for (size_t i = 0; i < currentPolynomial.size(); ++i)
						for (size_t id = 0; id < N; ++id)
							evalPolynomial(scene, object, fitParameters, regressionParams,
								datasetFit, datasets, variableName, result, resultError, resultStats,
								expandF1(fitParameters, regressionParams, currentPolynomial, i, id, depth));

						// F2 operator: add entries with only one term set to d
						for (size_t id = 0; id < N; ++id)
							evalPolynomial(scene, object, fitParameters, regressionParams,
								datasetFit, datasets, variableName, result, resultError, resultStats,
								expandF2(fitParameters, regressionParams, currentPolynomial, id, depth));
					}

					return { result, resultError };
				}

				////////////////////////////////////////////////////////////////////////////////
				template<size_t N>
				std::optional<PolynomialN<N>> replaceTerm(PolynomialFitParameters const& fitParameters,
					PolynomialFitParameters::PolynomialRegressionParams const& regressionParams,
					PolynomialN<N> currentPolynomial, const size_t i)
				{
					currentPolynomial[i] = currentPolynomial.back();
					currentPolynomial.pop_back();
					return currentPolynomial;
				}

				////////////////////////////////////////////////////////////////////////////////
				template<size_t N>
				std::pair<PolynomialN<N>, float> findBestReplace(Scene::Scene& scene, Scene::Object* object,
					PolynomialFitParameters const& fitParameters,
					PolynomialFitParameters::PolynomialRegressionParams const& regressionParams, FitStatsData& resultStats,
					PolynomialN<N> const& currentPolynomial, FitDataSetN<N> const& datasetFit, FitDataSetN<N> const& datasets,
					std::string const& variableName)
				{
					PolynomialN<N> result = currentPolynomial;
					result.pop_back();
					float resultError = FLT_MAX;

					if (useThreading(scene, object, fitParameters, regressionParams, datasetFit))
					{
						// Try to replace each term with the new one
						Threading::threadedExecuteIndices(Threading::numThreads(),
							[&](Threading::ThreadedExecuteEnvironment const& environment, size_t i)
							{
								evalPolynomial(scene, object, fitParameters, regressionParams,
									datasetFit, datasets, variableName, result, resultError, resultStats,
									replaceTerm(fitParameters, regressionParams, currentPolynomial, i));
							},
							currentPolynomial.size() - 1);
					}
					else
					{
						// Try to replace each term with the new one
						for (size_t i = 0; i < currentPolynomial.size() - 1; ++i)
							evalPolynomial(scene, object, fitParameters, regressionParams,
								datasetFit, datasets, variableName, result, resultError, resultStats,
								replaceTerm(fitParameters, regressionParams, currentPolynomial, i));
					}

					return { result, resultError };
				}

				////////////////////////////////////////////////////////////////////////////////
				template<size_t N>
				std::optional<PolynomialN<N>> simplifyB1(PolynomialFitParameters const& fitParameters,
					PolynomialFitParameters::PolynomialRegressionParams const& regressionParams, 
					PolynomialN<N> currentPolynomial, const size_t i, const size_t id, const int depth)
				{
					// Decrease the term's degree
					currentPolynomial[i].m_degrees[id] -= depth;
					// Return a null polynomial if the term has a negative degree
					if (currentPolynomial[i].isAnyDegreeNegative()) return std::nullopt;
					// Return the modified polynomial
					return currentPolynomial;
				}

				////////////////////////////////////////////////////////////////////////////////
				template<size_t N>
				std::optional<PolynomialN<N>> simplifyB2(PolynomialFitParameters const& fitParameters,
					PolynomialFitParameters::PolynomialRegressionParams const& regressionParams, 
					PolynomialN<N> currentPolynomial, const size_t i)
				{
					currentPolynomial.erase(currentPolynomial.begin() + i);
					return currentPolynomial;
				}

				////////////////////////////////////////////////////////////////////////////////
				template<size_t N>
				std::pair<PolynomialN<N>, float> simplifyPolynomial(Scene::Scene& scene, Scene::Object* object,
					PolynomialFitParameters const& fitParameters,
					PolynomialFitParameters::PolynomialRegressionParams const& regressionParams, FitStatsData& resultStats,
					PolynomialN<N> const& currentPolynomial, FitDataSetN<N> const& datasetFit, FitDataSetN<N> const& datasets,
					std::string const& variableName, const int depth)
				{
					PolynomialN<N> result = currentPolynomial;
					float resultError = FLT_MAX;

					if (useThreading(scene, object, fitParameters, regressionParams, datasetFit))
					{
						// B1 operator: decrease the the exponent of one term by d
						Threading::threadedExecuteIndices(Threading::numThreads(),
							[&](Threading::ThreadedExecuteEnvironment const& environment, size_t i, size_t id)
							{
								evalPolynomial(scene, object, fitParameters, regressionParams,
									datasetFit, datasets, variableName, result, resultError, resultStats,
									simplifyB1(fitParameters, regressionParams, currentPolynomial, i, id, depth));
							},
							currentPolynomial.size(),
							N);

						// B2 operator: remove one entry if there are more than 2 entries in the polynomial
						if (currentPolynomial.size() > 2)
							Threading::threadedExecuteIndices(Threading::numThreads(),
								[&](Threading::ThreadedExecuteEnvironment const& environment, size_t i)
								{
									evalPolynomial(scene, object, fitParameters, regressionParams,
										datasetFit, datasets, variableName, result, resultError, resultStats,
										simplifyB2(fitParameters, regressionParams, currentPolynomial, i));
								},
								currentPolynomial.size());
					}
					else
					{
						// B1 operator: decrease the the exponent of one term by d
						for (size_t i = 0; i < currentPolynomial.size(); ++i)
						for (size_t id = 0; id < N; ++id)
							evalPolynomial(scene, object, fitParameters, regressionParams,
								datasetFit, datasets, variableName, result, resultError, resultStats,
								simplifyB1(fitParameters, regressionParams, currentPolynomial, i, id, depth));

						// B2 operator: remove one entry if there are more than 2 entries in the polynomial
						if (currentPolynomial.size() > 2)
							for (size_t i = 0; i < currentPolynomial.size(); ++i)
								evalPolynomial(scene, object, fitParameters, regressionParams,
									datasetFit, datasets, variableName, result, resultError, resultStats,
									simplifyB2(fitParameters, regressionParams, currentPolynomial, i));
					}

					return { result, resultError };
				}

				////////////////////////////////////////////////////////////////////////////////
				template<size_t N>
				FitResultN<N> fit(Scene::Scene& scene, Scene::Object* object, std::string const& variableName,
					FitDataSetN<N> const& datasets, FitDataSetN<N> const& datasetsSubsampled,
					PolynomialFitParameters const& fitParameters, PolynomialFitParameters::PolynomialRegressionParams const& regressionParams)
				{
					Debug::DebugOutputLevel outputLevel = fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug ? Debug::Info : Debug::Debug;

					const size_t numAngles = datasets.shape()[0];
					const size_t numRotations = datasets.shape()[1];
					const size_t numWavelengths = datasets.shape()[2];

					// Calculate the weighed error threshold
					const float stopErrorThreshold = regressionParams.m_outputVariableParams.find(variableName)->second.m_stopErrorThreshold;
					const float stopErrorDecrease = regressionParams.m_outputVariableParams.find(variableName)->second.m_stopErrorDecreaseThreshold;
					const float expansionErrorThreshold = regressionParams.m_outputVariableParams.find(variableName)->second.m_expansionErrorThreshold;
					const float expansionErrorDecrease = regressionParams.m_outputVariableParams.find(variableName)->second.m_expansionErrorDecrease;
					const float simplificationErrorIncrease = regressionParams.m_outputVariableParams.find(variableName)->second.m_simplificationErrorIncrease;

					// Results of the fitting
					FitResultN<N> result(boost::extents[numAngles][numRotations][numWavelengths]);
					PolynomialN<N> resultPolynomial = DensePolynomial::makeDensePolynomial<N>(1, false);
					FitStatsData resultStats;
					resultStats.m_computationTimer.start();

					FitDataSetN<N> const& datasetsSubsampledFit = constructFitDataset(fitParameters, datasetsSubsampled);
					FitDataSetN<N> const& datasetsFit = constructFitDataset(fitParameters, datasets);

					// Get the initial fitting error
					float prevError = getApproximationError(scene, object, fitParameters, resultPolynomial, datasetsSubsampledFit, datasetsSubsampled, variableName);

					Debug::log_output(outputLevel) << "Initial error: " << prevError << Debug::end;
					if (fitParameters.m_debugComputation)
					{
						Debug::log_debug() << "Initial polynomial: " << resultPolynomial << Debug::end;
					}

					// State variables
					int depth = 1;
					bool stuck = false;
					bool updated = false;
					bool expanded = false;
					size_t iterationID = 0;
					while (true)
					{
						Debug::log_output(outputLevel)
							<< " - Iteration #" << (iterationID + 1) << ", "
							<< "error: " << prevError << ", "
							<< "#terms: " << resultPolynomial.size() << ", "
							<< "depth: " << depth << ", "
							<< Debug::end;

						if (fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug)
							Debug::log_debug() << "Trying to expand polynomial" << Debug::end;

						// Expand the polynomial
						expanded = false;
						auto [expandedPolynomial, expandedError] = expandPolynomial(scene, object, fitParameters, regressionParams, resultStats,
							resultPolynomial, datasetsSubsampledFit, datasetsSubsampled, variableName, depth);
						
						if ((expandedPolynomial.size() > resultPolynomial.size()) && (regressionParams.m_fixedTermNumber || expandedError <= (prevError - stopErrorDecrease)))
						{
							if (prevError >= expansionErrorThreshold && expandedPolynomial.size() <= fitParameters.m_numSparseTerms && expandedError <= (prevError - expansionErrorDecrease))
							{
								if (fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug)
								{
									Debug::log_debug() << "Polynom successfully expanded" << Debug::end;
									Debug::log_debug() << "   " << expandedPolynomial << Debug::end;
								}

								Debug::log_output(outputLevel) << "   " << "error after expansion : " << expandedError << Debug::end;

								resultPolynomial = expandedPolynomial;
								prevError = expandedError;
								expanded = true;
							}
							else
							{
								if (fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug)
									Debug::log_debug() << "Looking for best replacement" << Debug::end;

								auto [replacedPolynomial, replacedError] = findBestReplace(scene, object, fitParameters, regressionParams, resultStats,
									expandedPolynomial, datasetsSubsampledFit, datasetsSubsampled, variableName);
								if (replacedError < prevError)
								{
									Debug::log_output(outputLevel) << "   " << "error after expansion & replacement: " << replacedError << Debug::end;
									if (fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug)
									{
										Debug::log_debug() << "   " << replacedPolynomial << Debug::end;
									}

									resultPolynomial = replacedPolynomial;
									prevError = replacedError;
								}
								else
								{
									Debug::log_output(outputLevel) << "   " << "stuck with best possible error: " << replacedError << Debug::end;

									stuck = true;
								}
							}
						}
						else
						{
							Debug::log_output(outputLevel) << "   " << "unable to expand/replace; current error: " << prevError << ", next best error: " << expandedError << Debug::end;

							stuck = true;
							//break;
						}

						// Try to simplify the polynomial
						updated = false;
						if (!stuck && resultPolynomial.size() >= regressionParams.m_simplificationThreshold)
						{
							while (true)
							{
								if (fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug)
									Debug::log_debug() << "Trying to simplify polynomial" << Debug::end;

								// Simplify the polynomial
								auto [simplifiedPolynomial, simplifiedError] = simplifyPolynomial(scene, object, fitParameters, regressionParams, resultStats,
									resultPolynomial, datasetsSubsampledFit, datasetsSubsampled, variableName, depth);
								if (simplifiedError < (prevError + simplificationErrorIncrease))
								{
									Debug::log_output(outputLevel) << "   " << "error after simplification: " << simplifiedError << Debug::end;
									if (fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug)
									{
										Debug::log_debug() << "   " << simplifiedPolynomial << Debug::end;
									}

									resultPolynomial = simplifiedPolynomial;
									prevError = simplifiedError;
									updated = true;
								}
								else
								{
									Debug::log_output(outputLevel) << "   " << "Unable to simplify; current error: " << prevError << ", next best error: " << simplifiedError << Debug::end;

									break;
								}
							}
						}

						// Early stopping
						if (prevError < stopErrorThreshold)
						{
							Debug::log_debug() << "   " << "error threshold (" << stopErrorThreshold << ") reached, stopping" << Debug::end;

							break;
						}

						// Increase the recursion depth if we are stuck
						if (!updated && stuck)
						{
							if (depth < regressionParams.m_recursionDepth)
							{
								Debug::log_output(outputLevel) << "   " << "stuck; increasing step size" << Debug::end;

								++depth;
								stuck = false;
							}
							else
							{
								Debug::log_debug() << "   " << "recursion limit reached, stopping" << Debug::end;

								break;
							}
						}
						else if (depth > 1)
						{
							if (expanded)
							{
								Debug::log_output(outputLevel) << "   " << "expanded, resetting depth" << Debug::end;

								depth = 1;
							}
							else
							{
								Debug::log_output(outputLevel) << "   " << "updated, decreasing depth" << Debug::end;

								--depth;
							}
						}

						// Log the current fit error
						if (fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug)
						{
							Debug::log_debug() << "Individual errors in the current iteration:" << Debug::end;
							for (size_t angleID = 0; angleID < numAngles; ++angleID)
							for (size_t rotationID = 0; rotationID < numRotations; ++rotationID)
							for (size_t wavelengthID = 0; wavelengthID < numWavelengths; ++wavelengthID)
							{
								auto polynomial = DensePolynomial::fitDenseCoefficients(fitParameters, resultPolynomial, datasetsSubsampledFit[angleID][rotationID][wavelengthID]);
								const float errorSubsampled = getApproximationError(scene, object, fitParameters, polynomial, datasetsSubsampled[angleID][rotationID][wavelengthID], variableName);
								const float errorFull = getApproximationError(scene, object, fitParameters, polynomial, datasets[angleID][rotationID][wavelengthID], variableName);
								const size_t numSamplesSubsampled = datasetsSubsampled[angleID][rotationID][wavelengthID].size();
								const size_t numSamplesFull = datasets[angleID][rotationID][wavelengthID].size();

								if (errorFull == 0.0f) continue;
								
								Debug::log_debug() << "[" << angleID << "][" << rotationID << "][" << wavelengthID << "]: "
									<< errorSubsampled << " (subsampled, " << numSamplesSubsampled << " samples)" << ", "
									<< errorFull << " (full, " << numSamplesFull << " samples)" << ", "
									<< Debug::end;

								if (isinf(errorSubsampled) || isnan(errorSubsampled) || errorSubsampled > 1e4f)
								{
									auto const& dataPoints = datasetsSubsampled[angleID][rotationID][wavelengthID];
									for (size_t sampleID = 0; sampleID < dataPoints.size(); ++sampleID)
									{
										auto const& sample = dataPoints[sampleID];
										Debug::log_debug() << " - sample[" << sampleID << "]: "
											<< "target: " << sample.m_value << " (" << sample.m_weight << "), "
											<< "pred: " << Monomials::evalPolynomial(polynomial, sample)
											<< Debug::end;
									}
								}
							}
						}

						++iterationID;
					}

					// Fill in the stats
					resultStats.m_computationTimer.stop();

					if (fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug)
						Debug::log_debug() << "Individual errors after the fit:" << Debug::end;
					for (size_t angleID = 0; angleID < numAngles; ++angleID)
					for (size_t rotationID = 0; rotationID < numRotations; ++rotationID)
					for (size_t wavelengthID = 0; wavelengthID < numWavelengths; ++wavelengthID)
					{
						// Perform the final fit
						result[angleID][rotationID][wavelengthID].m_polynomial = DensePolynomial::fitDenseCoefficients(fitParameters, resultPolynomial,
							datasetsSubsampledFit[angleID][rotationID][wavelengthID], true,
							fitParameters.m_nonlinearFitFrequency != PolynomialFitParameters::NonlinearFitFrequency::Never);
						
						// Evaluate the fitting errors and store the result
						resultStats.m_numIterations = iterationID;
						resultStats.m_numTerms = resultPolynomial.size();
						resultStats.m_finalErrorSubSampled = getApproximationError(scene, object, fitParameters, result[angleID][rotationID][wavelengthID].m_polynomial, datasetsSubsampledFit[angleID][rotationID][wavelengthID], variableName);
						resultStats.m_finalErrorFullDataset = getApproximationError(scene, object, fitParameters, result[angleID][rotationID][wavelengthID].m_polynomial, datasetsFit[angleID][rotationID][wavelengthID], variableName);
						result[angleID][rotationID][wavelengthID].m_fitStats = resultStats;

						if (resultStats.m_finalErrorFullDataset == 0.0f) continue;

						if (fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug)
							Debug::log_debug() << "[" << angleID << "][" << rotationID << "][" << wavelengthID << "]: "
								<< resultStats.m_finalErrorSubSampled << " (subsampled, " << datasetsSubsampled[angleID][rotationID][wavelengthID].size() << " samples)" << ", "
								<< resultStats.m_finalErrorFullDataset << " (full, " << datasets[angleID][rotationID][wavelengthID].size() << " samples)"
								<< Debug::end;

						if (fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::DetailedDebug && resultStats.m_finalErrorSubSampled > prevError * 5.0f)
						{
							Debug::log_debug() << "Outlier found; individual sample errors: " << Debug::end;
							logApproximationError(scene, object, fitParameters, result[angleID][rotationID][wavelengthID].m_polynomial, datasets[angleID][rotationID][wavelengthID], variableName);
						}
					}

					return result;
				}
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		namespace GpuMonomial
		{
			////////////////////////////////////////////////////////////////////////////////
			GLuint packFlags(Scene::Scene& scene, Scene::Object* object, const int degree, const int arrayId)
			{
				return degree << (arrayId * 8);
			}

			////////////////////////////////////////////////////////////////////////////////
			template<size_t N, size_t M, typename P>
			Uniforms::GhostPolynomialMonomialFull populateGpuMonomial(Scene::Scene& scene, Scene::Object* object, P const& polynomials, const size_t termID)
			{
				Uniforms::GhostPolynomialMonomialFull result{};
				for (size_t outputVariableID = 0; outputVariableID < M; ++outputVariableID)
				{
					// Array ID and index for the GPU monomials
					const size_t arrayID = outputVariableID / (M / 2);
					const size_t arrayIndex = outputVariableID % (M / 2);

					// Determine the corresponding monomial
					PolynomialN<N> const& polynomial = polynomials[outputVariableID];

					// Write out the coefficient and degrees
					if (termID < polynomial.size())
					{
						MonomialN<N> const& monomial = polynomial[termID];
						result.m_coefficient[arrayID][arrayIndex] = monomial.m_coefficient;
						for (size_t inputVariableID = 0; inputVariableID < N; ++inputVariableID)
						{
							const int degree = monomial.m_degrees[inputVariableID];
							result.m_degrees[arrayID][inputVariableID][arrayIndex] = degree;
						}
					}
				}
				return result;
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		template<size_t N, size_t M, typename P>
		Uniforms::GhostPolynomialMonomialFull populateGpuMonomial(Scene::Scene& scene, Scene::Object* object, P const& polynomials, const size_t termID)
		{
			return GpuMonomial::populateGpuMonomial<N, M, P>(scene, object, polynomials, termID);
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	namespace PolynomialsFull
	{
		////////////////////////////////////////////////////////////////////////////////
		using PolynomialsCommon::NUM_POLYNOMIAL_INPUT_VARIABLES;
		using PolynomialsCommon::NUM_POLYNOMIAL_OUTPUT_VARIABLES;
		using PolynomialsCommon::s_polynomialInputVariables;
		using PolynomialsCommon::s_polynomialOutputVariables;

		////////////////////////////////////////////////////////////////////////////////
		/** Represents a single monomial of a polinomial. */
		using Monomial = MonomialN<NUM_POLYNOMIAL_INPUT_VARIABLES>;

		////////////////////////////////////////////////////////////////////////////////
		// Represents a single polynomial for a single variable
		using Polynomial = PolynomialN<NUM_POLYNOMIAL_INPUT_VARIABLES>;

		////////////////////////////////////////////////////////////////////////////////
		// Represents the polynomials of all variables in a full system
		// - #1: ghost
		// - #2: variable
		template<typename T> using FitData = boost::multi_array<T, 2>;
		using PolynomialFit = FitData<Polynomial>;

		////////////////////////////////////////////////////////////////////////////////
		using PolynomialInput = PolynomialsCommon::PolynomialInputN<NUM_POLYNOMIAL_INPUT_VARIABLES>;
		using FitDataSamplePoint = PolynomialsCommon::FitDataSamplePointN<NUM_POLYNOMIAL_INPUT_VARIABLES>;
		using FitDataPoints = PolynomialsCommon::FitDataPointsN<NUM_POLYNOMIAL_INPUT_VARIABLES>;
		using FitDataSet = PolynomialsCommon::FitDataSetN<NUM_POLYNOMIAL_INPUT_VARIABLES>;
		using SingleFitResult = PolynomialsCommon::FitResultN<NUM_POLYNOMIAL_INPUT_VARIABLES>;
		using FitStatsData = PolynomialsCommon::FitStatsData;
		using FitDataSets = std::vector<FitDataSet>;
		struct FitStatsFit
		{
			FitStatsData m_global;
			std::vector<FitStatsData> m_perVariable;
			FitData<FitStatsData> m_perEntry;
		};

		////////////////////////////////////////////////////////////////////////////////
		namespace Fitting
		{
			////////////////////////////////////////////////////////////////////////////////
			std::pair<FitDataSets, size_t> convertGhostGeometryToFitDataset(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters,
				PolynomialsCommon::PrecomputedGhostGeometry const& ghostGeometry)
			{
				// Extract the camera
				PhysicalCamera::PhysicalCameraAttributes const& camera = object->component<TiledLensFlareComponent>().m_camera;

				// Various angles
				const size_t numAngles = ghostGeometry.shape()[0];
				const size_t numRotations = ghostGeometry.shape()[1];
				const size_t numWavelengths = ghostGeometry.shape()[2];
				const size_t numVariables = NUM_POLYNOMIAL_OUTPUT_VARIABLES;

				// Construct the output dataset
				FitDataSets result(numVariables);
				size_t numValidEntries = 0;

				// Count the number of samples
				size_t numDataPoints = 0;
				for (size_t angleID = 0; angleID < numAngles; ++angleID)
				for (size_t rotationID = 0; rotationID < numRotations; ++rotationID)
				for (size_t wavelengthID = 0; wavelengthID < numWavelengths; ++wavelengthID)
				{
					const size_t numSamples = ghostGeometry[angleID][rotationID][wavelengthID].size();
					if (numSamples > 0)
					{
						numDataPoints += numSamples;
						++numValidEntries;
					}
				}

				// Resize the result vectors
				for (size_t variableID = 0; variableID < numVariables; ++variableID)
				{
					result[variableID].resize(boost::extents[1][1][1]);
					result[variableID][0][0][0].resize(numDataPoints);
				}

				// Collect the samples
				for (size_t variableID = 0; variableID < numVariables; ++variableID)
				{
					size_t outID = 0;
					for (size_t angleID = 0; angleID < numAngles; ++angleID)
					for (size_t rotationID = 0; rotationID < numRotations; ++rotationID)
					for (size_t wavelengthID = 0; wavelengthID < numWavelengths; ++wavelengthID)
					{
						auto const& samples = ghostGeometry[angleID][rotationID][wavelengthID];
						for (size_t sampleID = 0; sampleID < samples.size(); ++sampleID)
						{
							GhostGeometry::GeometryEntry const& sample = samples[sampleID];
							FitDataSamplePoint const& outSample = PolynomialsCommon::Fitting::makeSamplePoint(
								scene, object, fitParameters, camera, sample, s_polynomialInputVariables,
								s_polynomialOutputVariables[variableID]);
							result[variableID][0][0][0][outID++] = outSample;
						}
					}
					result[variableID][0][0][0].resize(outID);
				}

				// Return the result
				return std::make_pair(result, numValidEntries);
			}

			////////////////////////////////////////////////////////////////////////////////
			template<typename T>
			FitData<T> makeFitResultData(Scene::Scene& scene, Scene::Object* object, const size_t numGhosts,
				PolynomialFitParameters const& fitParameters)
			{
				return FitData<T>(boost::extents[numGhosts][s_polynomialOutputVariables.size()]);
			}

			////////////////////////////////////////////////////////////////////////////////
			template<typename T>
			void makeFitResultData(Scene::Scene& scene, Scene::Object* object, FitData<T>& result, const size_t numGhosts,
				PolynomialFitParameters const& fitParameters)
			{
				result.resize(boost::extents[numGhosts][s_polynomialOutputVariables.size()]);
			}

			////////////////////////////////////////////////////////////////////////////////
			PolynomialFit makeFitResult(Scene::Scene& scene, Scene::Object* object, const size_t numGhosts,
				PolynomialFitParameters const& fitParameters)
			{
				return makeFitResultData<Polynomial>(scene, object, numGhosts, fitParameters);
			}

			////////////////////////////////////////////////////////////////////////////////
			FitStatsFit makeFitStats(Scene::Scene& scene, Scene::Object* object, const size_t numGhosts,
				PolynomialFitParameters const& fitParameters)
			{
				FitStatsFit result;
				result.m_perVariable.resize(s_polynomialOutputVariables.size());
				makeFitResultData<FitStatsData>(scene, object, result.m_perEntry, numGhosts, fitParameters);
				return result;
			}

			////////////////////////////////////////////////////////////////////////////////
			void storeFitResultEntry(Scene::Scene& scene, Scene::Object* object, PolynomialFit& polynomials,
				FitStatsFit& fitStats, const size_t ghostID, const size_t variableID, SingleFitResult const& fitResult)
			{
				polynomials[ghostID][variableID] = fitResult[0][0][0].m_polynomial;
				fitStats.m_perEntry[ghostID][variableID] = fitResult[0][0][0].m_fitStats;
				fitStats.m_perVariable[variableID] += fitResult[0][0][0].m_fitStats;
				fitStats.m_global += fitResult[0][0][0].m_fitStats;
			}

			////////////////////////////////////////////////////////////////////////////////
			namespace SimulatedAnnealing
			{
				////////////////////////////////////////////////////////////////////////////////
				void fit(Scene::Scene& scene, Scene::Object* object, PolynomialFit& polynomials,
					FitStatsFit& fitStats, const size_t ghostID, FitDataSets const& dataset,
					const size_t numValidEntries, PolynomialFitParameters const& fitParameters,
					PolynomialFitParameters::AnnealingParams const& annealingParams)
				{
					const size_t numVariables = dataset.size();

					Debug::DebugOutputLevel outputLevel = fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug ? Debug::Info : Debug::Debug;

					DateTime::ScopedTimer timer(outputLevel, numVariables, DateTime::Seconds, "Polynomial Fitting [Simulated Annealing]");

					// Compute the number of samples to use
					//size_t numSamples = annealingParams.m_numSamples * fitParameters.m_ghostGeometryParameters.m_numAngles;
					size_t numSamples = annealingParams.m_numSamples * numValidEntries;
					if (fitParameters.m_ghostGeometryParameters.m_shareNeighboringGeometries)
					{
						if (fitParameters.m_ghostGeometryParameters.m_numAngles > 1) numSamples *= 2;
						if (fitParameters.m_ghostGeometryParameters.m_numRotations > 1) numSamples *= 2;
					}

					// Construct the output dataset
					for (size_t variableID = 0; variableID < numVariables; ++variableID)
					{
						std::string const& variableName = s_polynomialOutputVariables[variableID];

						// Log the progress
						Debug::log_info() << "Computing polynomials for \"" << variableName << "\" (" << (variableID + 1) << "/" << numVariables << ")" << Debug::end;

						// Initialize the fitting
						FitDataSet datasetSubsampled = PolynomialsCommon::Fitting::subsampleDataset(fitParameters, dataset[variableID], annealingParams.m_numSamples);
						fitStats.m_perVariable[variableID].m_computationTimer.start();

						// Perform the fitting
						auto fitResult = PolynomialsCommon::Fitting::SimulatedAnnealing::fit(scene, object,
							variableName, dataset[variableID], datasetSubsampled, fitParameters, annealingParams);

						// Finalize the fitting
						fitStats.m_perVariable[variableID].m_computationTimer.stop();
						storeFitResultEntry(scene, object, polynomials, fitStats, ghostID, variableID, fitResult);
					}
				}
			}

			////////////////////////////////////////////////////////////////////////////////
			namespace PolynomialRegression
			{
				////////////////////////////////////////////////////////////////////////////////
				void fit(Scene::Scene& scene, Scene::Object* object, PolynomialFit& polynomials,
					FitStatsFit& fitStats, const size_t ghostID, FitDataSets const& dataset,
					const size_t numValidEntries, PolynomialFitParameters const& fitParameters,
					PolynomialFitParameters::PolynomialRegressionParams const& regressionParams)
				{
					const size_t numVariables = dataset.size();

					Debug::DebugOutputLevel outputLevel = fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug ? Debug::Info : Debug::Debug;

					DateTime::ScopedTimer timer(outputLevel, numVariables, DateTime::Seconds, "Polynomial Fitting [Polynomial Regression]");

					// Compute the number of samples to use
					//size_t numSamples = regressionParams.m_numSamples * fitParameters.m_ghostGeometryParameters.m_numAngles;
					size_t numSamples = regressionParams.m_numSamples * numValidEntries;
					if (fitParameters.m_ghostGeometryParameters.m_shareNeighboringGeometries)
					{
						if (fitParameters.m_ghostGeometryParameters.m_numAngles > 1) numSamples *= 2;
						if (fitParameters.m_ghostGeometryParameters.m_numRotations > 1) numSamples *= 2;
					}

					// Construct the output dataset
					for (size_t variableID = 0; variableID < numVariables; ++variableID)
					{
						std::string const& variableName = s_polynomialOutputVariables[variableID];

						// Log the progress
						Debug::log_info() << "Computing polynomials for \"" << variableName << "\" (" << (variableID + 1) << "/" << numVariables << ")" << Debug::end;

						// Initialize the fitting
						FitDataSet datasetSubsampled = PolynomialsCommon::Fitting::subsampleDataset(fitParameters, dataset[variableID], numSamples);
						fitStats.m_perVariable[variableID].m_computationTimer.start();

						// Perform the fitting
						auto fitResult = PolynomialsCommon::Fitting::PolynomialRegression::fit(scene, object,
							variableName, dataset[variableID], datasetSubsampled, fitParameters, regressionParams);

						// Finalize the fitting
						fitStats.m_perVariable[variableID].m_computationTimer.stop();
						storeFitResultEntry(scene, object, polynomials, fitStats, ghostID, variableID, fitResult);
					}
				}
			}

			////////////////////////////////////////////////////////////////////////////////
			void fitPolynomials(Scene::Scene& scene, Scene::Object* object, PolynomialFit& polynomials, FitStatsFit& fitStats,
				const size_t ghostID, PolynomialFitParameters const& fitParameters, FitDataSets const& datasets, const size_t numValidEntries)
			{
				Debug::log_debug() << "Fitting full polynomials to data..." << Debug::end;

				// Perform the fitting
				switch (fitParameters.m_fitMethod)
				{
				case PolynomialFitParameters::SimulatedAnnealing:
					SimulatedAnnealing::fit(scene, object, polynomials, fitStats, ghostID, datasets, numValidEntries, fitParameters, fitParameters.m_annealingParams);
					break;
				case PolynomialFitParameters::PolynomialRegression:
					PolynomialRegression::fit(scene, object, polynomials, fitStats, ghostID, datasets, numValidEntries, fitParameters, fitParameters.m_polynomialRegressionParams);
					break;
				}

				Debug::log_debug() << "Polynomial fit successfuly!" << Debug::end;
			}

			////////////////////////////////////////////////////////////////////////////////
			void saveFitStats(Scene::Scene& scene, Scene::Object* object, std::string const& fileName, std::stringstream const& ss)
			{
				std::string filePath = GhostFilePaths::getPolynomialWeightsFullFitFilePath(scene, object, fileName + ".txt");
				EnginePaths::makeDirectoryStructure(filePath, true);
				std::ofstream of(filePath);
				of << ss.str();
				of.close();
			}

			////////////////////////////////////////////////////////////////////////////////
			void reportFitStats(Scene::Scene& scene, Scene::Object* object, PolynomialFit const&polynomials, FitStatsFit const& fitStats)
			{
				// Precompute parameters
				PolynomialFitParameters const& fitParams = object->component<TiledLensFlareComponent>().m_polynomialFitParameters;
				PhysicalCamera::PhysicalCameraAttributes const& camera = object->component<TiledLensFlareComponent>().m_camera;
				auto& ghostAttribsMap = object->component<TiledLensFlareComponent>().m_precomputedGhostAttribs;

				// Enumerate the possible ghosts
				auto const& ghostIndices = object->component<TiledLensFlareComponent>().m_ghostIndices;
				const size_t numAllGhosts = ghostIndices.size();
				const size_t firstGhost = fitParams.m_ghostGeometryParameters.m_firstGhost;
				const size_t numGhosts = std::min(size_t(fitParams.m_ghostGeometryParameters.m_numGhost), numAllGhosts - firstGhost);
				const size_t numAngles = fitParams.m_ghostGeometryParameters.m_numAngles;
				const size_t numRotations = fitParams.m_ghostGeometryParameters.m_numRotations;
				const size_t numChannels = fitParams.m_ghostGeometryParameters.m_numChannels;
				const size_t numRays = fitParams.m_ghostGeometryParameters.m_rayCount;
				const size_t numVariables = NUM_POLYNOMIAL_OUTPUT_VARIABLES;
				
				// Save the fitting stats
				{
					std::stringstream statsSS;

					// Global
					{
						std::stringstream ss;
						ss << fitStats.m_global << std::endl;
						saveFitStats(scene, object, "stats_g", ss);
					}

					// Per-variable
					{
						std::stringstream ss;
						for (size_t variableID = 0; variableID < numVariables; ++variableID)
						{
							ss << " - " << s_polynomialOutputVariables[variableID] << ":" << std::endl;
							ss << fitStats.m_perVariable[variableID] << std::endl;
							ss << std::string(80, '-') << std::endl;
						}
						saveFitStats(scene, object, "stats_pv", ss);
					}

					// Per-ghost
					{
						std::stringstream ss;
						for (size_t ghostID = firstGhost; ghostID < firstGhost + numGhosts && ghostID < numAllGhosts; ++ghostID)
						{
							ss << "Ghost #" << ghostID << std::endl;
							for (size_t variableID = 0; variableID < numVariables; ++variableID)
							{
								ss << " - " << s_polynomialOutputVariables[variableID] << ":" << std::endl;
								ss << fitStats.m_perEntry[ghostID][variableID] << std::endl;
								ss << std::string(80, '-') << std::endl;
							}
							ss << std::string(80, '=') << std::endl;
						}
						saveFitStats(scene, object, "stats_pe", ss);
					}
				}

				// Save the errors
				{
					// Global
					{
						std::stringstream ss;
						ss << " - " << "subsampled: " << fitStats.m_global.m_finalErrorSubSampled / fitStats.m_global.numComputations() << std::endl;
						ss << " - " << "full: " << fitStats.m_global.m_finalErrorFullDataset / fitStats.m_global.numComputations() << std::endl;
						saveFitStats(scene, object, "errors_g", ss);
					}

					// Per-variable
					{
						std::stringstream ss;
						for (size_t variableID = 0; variableID < numVariables; ++variableID)
						{
							auto const& fitStatsVar = fitStats.m_perVariable[variableID];
							if (fitStatsVar.m_finalErrorFullDataset == 0.0f) continue;
							ss 
								<< " - " << s_polynomialOutputVariables[variableID] << ": "
								<< fitStatsVar.m_finalErrorSubSampled / fitStatsVar.numComputations() << " (subsampled)" << ", "
								<< fitStatsVar.m_finalErrorFullDataset / fitStatsVar.numComputations() << " (full)"
								<< std::endl;
						}
						saveFitStats(scene, object, "errors_pv", ss);
					}

					// Per-ghost
					{
						std::stringstream ss;
						for (size_t ghostID = firstGhost; ghostID < firstGhost + numGhosts && ghostID < numAllGhosts; ++ghostID)
						for (size_t variableID = 0; variableID < numVariables; ++variableID)
						{
							auto const& fitStatsVar = fitStats.m_perEntry[ghostID][variableID];
							if (fitStatsVar.m_finalErrorFullDataset == 0.0f) continue;
							ss
								<< "Ghost[" << ghostID << "][" << s_polynomialOutputVariables[variableID] << "]: "
								<< fitStatsVar.m_finalErrorSubSampled << " (subsampled)" << ", "
								<< fitStatsVar.m_finalErrorFullDataset << " (full)"
								<< std::endl;
							ss << std::string(80, '=') << std::endl;
						}
						saveFitStats(scene, object, "errors_pe", ss);
					}
				}

				// Log and save the polynomials
				{
					std::stringstream ss;
					for (size_t ghostID = firstGhost; ghostID < firstGhost + numGhosts && ghostID < numAllGhosts; ++ghostID)
					for (size_t variableID = 0; variableID < numVariables; ++variableID)
					{
						ss << "Ghost #" << ghostID << " - " << s_polynomialOutputVariables[variableID] << ": " << polynomials[ghostID][variableID] << std::endl;
					}
					saveFitStats(scene, object, "polynomials", ss);
				}
			}

			////////////////////////////////////////////////////////////////////////////////
			PolynomialFit performFit(Scene::Scene& scene, Scene::Object* object)
			{
				Debug::log_info() << "Performing full polynomial set fitting..." << Debug::end;

				// Precompute parameters
				PolynomialFitParameters const& fitParams = object->component<TiledLensFlareComponent>().m_polynomialFitParameters;
				PhysicalCamera::PhysicalCameraAttributes const& camera = object->component<TiledLensFlareComponent>().m_camera;
				auto& ghostAttribsMap = object->component<TiledLensFlareComponent>().m_precomputedGhostAttribs;

				// Enumerate the possible ghosts
				auto const& ghostIndices = object->component<TiledLensFlareComponent>().m_ghostIndices;
				const size_t numAllGhosts = ghostIndices.size();
				const size_t firstGhost = fitParams.m_ghostGeometryParameters.m_firstGhost;
				const size_t numGhosts = std::min(size_t(fitParams.m_ghostGeometryParameters.m_numGhost), numAllGhosts - firstGhost);
				const size_t numAngles = fitParams.m_ghostGeometryParameters.m_numAngles;
				const size_t numRotations = fitParams.m_ghostGeometryParameters.m_numRotations;
				const size_t numChannels = fitParams.m_ghostGeometryParameters.m_numChannels;
				const size_t numRays = fitParams.m_ghostGeometryParameters.m_rayCount;
				const size_t numVariables = NUM_POLYNOMIAL_OUTPUT_VARIABLES;

				DateTime::ScopedTimer timer = DateTime::ScopedTimer(Debug::Debug, numGhosts, DateTime::Milliseconds, "Polynomial Fitting");

				// Result of the fit
				PolynomialFit polynomials = Fitting::makeFitResult(scene, object, numAllGhosts, fitParams);
				FitStatsFit fitStats = Fitting::makeFitStats(scene, object, numAllGhosts, fitParams);

				// Initiate the computation
				fitStats.m_global.m_computationTimer.start();

				// Render the ghosts themselves
				for (int ghostID = firstGhost; ghostID < firstGhost + numGhosts && ghostID < numAllGhosts; ++ghostID)
				{
					Debug::log_info()
						<< "Computing polynomials for "
						<< "ghost #" << (ghostID + 1) << "/" << numGhosts
						<< Debug::end;

					// List of ghost geometries for polynomial fitting
					const PolynomialsCommon::PrecomputedGhostGeometry ghostGeometries = PolynomialsCommon::ComputeGhostGeometry::computeGhostGeometries(scene, object, ghostID);

					// Get the data points
					auto [datasets, numValidEntries] = Fitting::convertGhostGeometryToFitDataset(scene, object, fitParams, ghostGeometries);

					// Perform the polynomial fitting
					Fitting::fitPolynomials(scene, object, polynomials, fitStats, ghostID, fitParams, datasets, numValidEntries);
				}

				// Finalize the computation
				fitStats.m_global.m_computationTimer.stop();

				Debug::log_info() << "Polynomial weights successfully computed." << Debug::end;

				// Report the fitting stats
				reportFitStats(scene, object, polynomials, fitStats);

				// Return the result
				return polynomials;
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		int getNumWeightsPerGhostFull(Scene::Scene& scene, Scene::Object* object)
		{
			return object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_numSparseTerms;
		}

		////////////////////////////////////////////////////////////////////////////////
		int getNumWeightsPerGhostBaked(Scene::Scene& scene, Scene::Object* object)
		{
			return object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_numSparseTerms *
				object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_numChannels;
		}

		////////////////////////////////////////////////////////////////////////////////
		int getNumWeightsAllGhosts(Scene::Scene& scene, Scene::Object* object)
		{
			return
				PhysicalCamera::getNumGhosts(object->component<TiledLensFlareComponent>().m_camera) *
				getNumWeightsPerGhostFull(scene, object);
		}

		////////////////////////////////////////////////////////////////////////////////
		int getNumWeightsAllGhostsBaked(Scene::Scene& scene, Scene::Object* object)
		{
			return
				PhysicalCamera::getNumGhosts(object->component<TiledLensFlareComponent>().m_camera) *
				getNumWeightsPerGhostBaked(scene, object);
		}

		////////////////////////////////////////////////////////////////////////////////
		namespace Serialization
		{
			////////////////////////////////////////////////////////////////////////////////
			PolynomialFit loadGhostWeights(Scene::Scene& scene, Scene::Object* object, std::string filePath)
			{
				Debug::log_debug() << "Attempting to load fully fit polynomial ghost weights from file: " << filePath << Debug::end;

				std::ifstream file(filePath, std::ios::binary);
				if (!file)
				{
					Debug::log_debug() << "Unable to open polynomial ghost weights file: " << filePath << Debug::end;
					return PolynomialFit{};
				}

				size_t numGhosts;
				size_t numVariables;
				file.read((char*)&numGhosts, sizeof(numGhosts));
				file.read((char*)&numVariables, sizeof(numVariables));

				PolynomialFit weights(boost::extents[numGhosts][numVariables]);
				for (size_t ghostID = 0; ghostID < numGhosts; ++ghostID)
				for (size_t variableID = 0; variableID < numVariables; ++variableID)
				{
					size_t numWeights;
					file.read((char*)&numWeights, sizeof(numWeights));
					Polynomial polynomial{ numWeights };
					file.read((char*)polynomial.data(), numWeights * sizeof(Monomial));
					weights[ghostID][variableID] = polynomial;
				}
				file.close();

				Debug::log_debug() << "Ghost weights successfully loaded from file: " << filePath << Debug::end;
				Debug::log_debug() << "Resulting weights:" << Debug::end;
				for (size_t ghostID = 0; ghostID < numGhosts; ++ghostID)
				for (size_t variableID = 0; variableID < numVariables; ++variableID)
				{
					Debug::log_debug() << "Ghost[" << ghostID << "][" << variableID << "]: " << weights[ghostID][variableID] << Debug::end;
				}

				return weights;
			}

			////////////////////////////////////////////////////////////////////////////////
			PolynomialFit loadGhostWeights(Scene::Scene& scene, Scene::Object* object)
			{
				return loadGhostWeights(scene, object, GhostFilePaths::getPolynomialWeightsFullFitFilePath(scene, object));
			}

			////////////////////////////////////////////////////////////////////////////////
			void saveGhostWeights(Scene::Scene& scene, Scene::Object* object, std::string const& filePath, PolynomialFit const& polynomials)
			{
				EnginePaths::makeDirectoryStructure(filePath, true);
				std::ofstream outFile(filePath, std::ofstream::binary);

				const size_t numGhosts = polynomials.shape()[0];
				const size_t numVariables = polynomials.shape()[1];
				outFile.write((char*)&numGhosts, sizeof(numGhosts));
				outFile.write((char*)&numVariables, sizeof(numVariables));
				for (size_t ghostID = 0; ghostID < numGhosts; ++ghostID)
				for (size_t variableID = 0; variableID < numVariables; ++variableID)
				{
					Polynomial const& polynomial = polynomials[ghostID][variableID];
					size_t numWeights = polynomial.size();
					outFile.write((char*)&numWeights, sizeof(numWeights));
					outFile.write((char*)polynomial.data(), numWeights * sizeof(Monomial));
				}
			}

			////////////////////////////////////////////////////////////////////////////////
			void saveGhostWeights(Scene::Scene& scene, Scene::Object* object, PolynomialFit const& polynomials)
			{
				saveGhostWeights(scene, object, GhostFilePaths::getPolynomialWeightsFullFitFilePath(scene, object), polynomials);
			}

			////////////////////////////////////////////////////////////////////////////////
			template<typename P>
			Uniforms::GhostPolynomialMonomialFull populateGpuMonomial(Scene::Scene& scene, Scene::Object* object, P const& polynomials, const size_t termID)
			{
				return PolynomialsCommon::populateGpuMonomial<NUM_POLYNOMIAL_INPUT_VARIABLES, NUM_POLYNOMIAL_OUTPUT_VARIABLES>(scene, object, polynomials, termID);
			}

			////////////////////////////////////////////////////////////////////////////////
			void uploadGhostWeights(Scene::Scene& scene, Scene::Object* object, PolynomialFit const& polynomials)
			{
				if (polynomials.empty())
				{
					object->component<TiledLensFlareComponent>().m_numPolynomialTermsFullFit = std::vector<size_t>(1000, 0);
					return;
				}

				const size_t numGhosts = polynomials.shape()[0];
				const size_t numVariables = polynomials.shape()[1];
				const size_t numMaxSparseTerms = object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_numSparseTerms;

				Debug::log_debug() << "Uploading polynomial ghost weights..." << Debug::end;

				// Calculate the number of terms necessary
				Debug::log_debug() << "Number of polynomial terms:" << Debug::end;

				size_t numTotalTerms = 0;
				object->component<TiledLensFlareComponent>().m_numPolynomialTermsFullFit.resize(numGhosts);
				for (size_t ghostID = 0; ghostID < numGhosts; ++ghostID)
				{
					size_t numTerms = 0;
					for (size_t variableID = 0; variableID < numVariables; ++variableID)
					{
						const size_t numTermsGhost = polynomials[ghostID][variableID].size();
						numTerms = std::max(numTerms, numTermsGhost);
						numTotalTerms += numTermsGhost;
					}
					object->component<TiledLensFlareComponent>().m_numPolynomialTermsFullFit[ghostID] = numTerms;
					Debug::log_debug() << " - Ghost #" << ghostID << ": " << object->component<TiledLensFlareComponent>().m_numPolynomialTermsFullFit[ghostID] << Debug::end;
				}

				const size_t totalMemory =
					numTotalTerms * sizeof(float) +  // coefficients
					numTotalTerms * 6 * sizeof(int); // degrees;
				Debug::log_debug() << "Total memory needed (full model): " << Units::bytesToString(totalMemory) << Debug::end;

				// Populate the data vector and upload it
				std::vector<Uniforms::GhostPolynomialMonomialFull> allGhostWeightsGPU;
				allGhostWeightsGPU.reserve(numGhosts * numMaxSparseTerms);
				for (size_t ghostID = 0; ghostID < numGhosts; ++ghostID)
				for (size_t termID = 0; termID < numMaxSparseTerms; ++termID)
					allGhostWeightsGPU.push_back(populateGpuMonomial(scene, object, polynomials[ghostID], termID));
				uploadBufferData(scene, "TiledLensFlarePolynomialWeightsBufferFullFit", allGhostWeightsGPU);

				Debug::log_debug() << "Ghost weights uploaded!" << Debug::end;
			}

			////////////////////////////////////////////////////////////////////////////////
			void uploadGhostWeights(Scene::Scene& scene, Scene::Object* object)
			{
				uploadGhostWeights(scene, object, loadGhostWeights(scene, object));
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		void performFit(Scene::Scene& scene, Scene::Object* object)
		{
			// Perform the fitting
			PolynomialFit polynomials = Fitting::performFit(scene, object);

			// Save the ghost weights
			Serialization::saveGhostWeights(scene, object, polynomials);

			// Upload the ghost weights
			Serialization::uploadGhostWeights(scene, object, polynomials);
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	namespace PolynomialsPartial
	{
		////////////////////////////////////////////////////////////////////////////////
		using PolynomialsCommon::NUM_POLYNOMIAL_INPUT_VARIABLES;
		using PolynomialsCommon::NUM_POLYNOMIAL_OUTPUT_VARIABLES;
		using PolynomialsCommon::s_polynomialInputVariables;
		using PolynomialsCommon::s_polynomialOutputVariables;

		////////////////////////////////////////////////////////////////////////////////
		/** Represents a single monomial of a polinomial. */
		using Monomial = MonomialN<NUM_POLYNOMIAL_INPUT_VARIABLES>;

		////////////////////////////////////////////////////////////////////////////////
		// Represents a single polynomial for a single variable
		using Polynomial = PolynomialN<NUM_POLYNOMIAL_INPUT_VARIABLES>;

		////////////////////////////////////////////////////////////////////////////////
		// Represents the polynomials of all variables in a full system
		// - #1: ghost
		// - #2: angle
		// - #3: rotation
		// - #4: wavelength
		// - #5: variable
		template<typename T> using FitData = boost::multi_array<T, 5>;
		using PolynomialFit = FitData<Polynomial>;

		////////////////////////////////////////////////////////////////////////////////
		using PolynomialInput = PolynomialsCommon::PolynomialInputN<NUM_POLYNOMIAL_INPUT_VARIABLES>;
		using FitDataSamplePoint = PolynomialsCommon::FitDataSamplePointN<NUM_POLYNOMIAL_INPUT_VARIABLES>;
		using FitDataPoints = PolynomialsCommon::FitDataPointsN<NUM_POLYNOMIAL_INPUT_VARIABLES>;
		using FitDataSet = PolynomialsCommon::FitDataSetN<NUM_POLYNOMIAL_INPUT_VARIABLES>;
		using SingleFitResult = PolynomialsCommon::FitResultN<NUM_POLYNOMIAL_INPUT_VARIABLES>;
		using FitStatsData = PolynomialsCommon::FitStatsData;
		using FitDataSets = std::vector<FitDataSet>;
		struct FitStatsFit
		{
			FitStatsData m_global;
			std::vector<FitStatsData> m_perVariable;
			FitData<FitStatsData> m_perEntry;
		};

		////////////////////////////////////////////////////////////////////////////////
		namespace Fitting
		{
			////////////////////////////////////////////////////////////////////////////////
			FitDataSets convertGhostGeometryToFitDataset(Scene::Scene& scene, Scene::Object* object, PolynomialFitParameters const& fitParameters,
				PolynomialsCommon::PrecomputedGhostGeometry const& ghostGeometry)
			{
				// Extract the camera
				PhysicalCamera::PhysicalCameraAttributes const& camera = object->component<TiledLensFlareComponent>().m_camera;

				// Various angles
				const size_t numAngles = ghostGeometry.shape()[0];
				const size_t numRotations = ghostGeometry.shape()[1];
				const size_t numWavelengths = ghostGeometry.shape()[2];
				const size_t numVariables = NUM_POLYNOMIAL_OUTPUT_VARIABLES;

				// Construct the output dataset
				FitDataSets result(numVariables);

				// Resize the result vectors
				for (size_t variableID = 0; variableID < numVariables; ++variableID)
				{
					result[variableID].resize(boost::extents[numAngles][numRotations][numWavelengths]);

					for (size_t angleID = 0; angleID < numAngles; ++angleID)
					for (size_t rotationID = 0; rotationID < numRotations; ++rotationID)
					for (size_t wavelengthID = 0; wavelengthID < numWavelengths; ++wavelengthID)
					{
						// Extract the sample list
						auto const& samples = ghostGeometry[angleID][rotationID][wavelengthID];

						// Collect the samples
						result[variableID][angleID][rotationID][wavelengthID].resize(samples.size());

						size_t outID = 0;
						for (size_t sampleID = 0; sampleID < samples.size(); ++sampleID)
						{
							GhostGeometry::GeometryEntry const& sample = samples[sampleID];
							FitDataSamplePoint const& outSample = PolynomialsCommon::Fitting::makeSamplePoint(
								scene, object, fitParameters, camera, sample, s_polynomialInputVariables,
								s_polynomialOutputVariables[variableID]);
							result[variableID][angleID][rotationID][wavelengthID][outID++] = outSample;
						}
						result[variableID][angleID][rotationID][wavelengthID].resize(outID);
					}
				}

				// Return the result
				return result;
			}

			////////////////////////////////////////////////////////////////////////////////
			template<typename T>
			FitData<T> makeFitResultData(Scene::Scene& scene, Scene::Object* object, const size_t numGhosts,
				PolynomialFitParameters const& fitParameters)
			{
				return FitData<T>(boost::extents
					[numGhosts]
					[fitParameters.m_ghostGeometryParameters.m_numAngles]
					[fitParameters.m_ghostGeometryParameters.m_numRotations]
					[fitParameters.m_ghostGeometryParameters.m_numChannels]
					[s_polynomialOutputVariables.size()]);
			}

			////////////////////////////////////////////////////////////////////////////////
			template<typename T>
			void makeFitResultData(Scene::Scene& scene, Scene::Object* object, FitData<T>& result, const size_t numGhosts,
				PolynomialFitParameters const& fitParameters)
			{
				result.resize(boost::extents
					[numGhosts]
					[fitParameters.m_ghostGeometryParameters.m_numAngles]
					[fitParameters.m_ghostGeometryParameters.m_numRotations]
					[fitParameters.m_ghostGeometryParameters.m_numChannels]
					[s_polynomialOutputVariables.size()]);
			}

			////////////////////////////////////////////////////////////////////////////////
			PolynomialFit makeFitResult(Scene::Scene& scene, Scene::Object* object, const size_t numGhosts,
				PolynomialFitParameters const& fitParameters)
			{
				return makeFitResultData<Polynomial>(scene, object, numGhosts, fitParameters);
			}

			////////////////////////////////////////////////////////////////////////////////
			FitStatsFit makeFitStats(Scene::Scene& scene, Scene::Object* object, const size_t numGhosts,
				PolynomialFitParameters const& fitParameters)
			{
				FitStatsFit result;
				result.m_perVariable.resize(s_polynomialOutputVariables.size());
				makeFitResultData<FitStatsData>(scene, object, result.m_perEntry, numGhosts, fitParameters);
				return result;
			}

			////////////////////////////////////////////////////////////////////////////////
			void storeFitResultEntry(Scene::Scene& scene, Scene::Object* object, PolynomialFit& polynomials,
				FitStatsFit& fitStats, const size_t ghostID, const size_t variableID, SingleFitResult const& fitResult)
			{
				const size_t numAngles = fitResult.shape()[0];
				const size_t numRotations = fitResult.shape()[1];
				const size_t numWavelengths = fitResult.shape()[2];

				for (size_t angleID = 0; angleID < numAngles; ++angleID)
				for (size_t rotationID = 0; rotationID < numRotations; ++rotationID)
				for (size_t wavelengthID = 0; wavelengthID < numWavelengths; ++wavelengthID)
				{
					polynomials[ghostID][angleID][rotationID][wavelengthID][variableID] = fitResult[angleID][rotationID][wavelengthID].m_polynomial;
					fitStats.m_perEntry[ghostID][angleID][rotationID][wavelengthID][variableID] = fitResult[angleID][rotationID][wavelengthID].m_fitStats;
					fitStats.m_perVariable[variableID] += fitResult[angleID][rotationID][wavelengthID].m_fitStats;
					fitStats.m_global += fitResult[angleID][rotationID][wavelengthID].m_fitStats;
				}
			}

			////////////////////////////////////////////////////////////////////////////////
			namespace SimulatedAnnealing
			{
				////////////////////////////////////////////////////////////////////////////////
				void fit(Scene::Scene& scene, Scene::Object* object, PolynomialFit& polynomials,
					FitStatsFit& fitStats, const size_t ghostID, FitDataSets const& dataset,
					PolynomialFitParameters const& fitParameters,
					PolynomialFitParameters::AnnealingParams const& annealingParams)
				{
					const size_t numVariables = dataset.size();
					const size_t numAngles = dataset[0].shape()[0];
					const size_t numRotations = dataset[0].shape()[1];
					const size_t numWavelengths = dataset[0].shape()[2];

					Debug::DebugOutputLevel outputLevel = fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug ? Debug::Info : Debug::Debug;

					DateTime::ScopedTimer timer(outputLevel, numVariables, DateTime::Seconds, "Polynomial Fitting [Simulated Annealing]");

					// Construct the output dataset
					for (size_t variableID = 0; variableID < numVariables; ++variableID)
					{
						std::string const& variableName = s_polynomialOutputVariables[variableID];

						// Log the progress
						Debug::log_info() << "Computing polynomials for \"" << variableName << "\" (" << (variableID + 1) << "/" << numVariables << ")" << Debug::end;

						// Initialize the fitting
						FitDataSet datasetSubsampled = PolynomialsCommon::Fitting::subsampleDataset(fitParameters, dataset[variableID], annealingParams.m_numSamples);
						fitStats.m_perVariable[variableID].m_computationTimer.start();

						// Perform the fitting
						auto fitResult = PolynomialsCommon::Fitting::SimulatedAnnealing::fit(scene, object,
							variableName, dataset[variableID], datasetSubsampled, fitParameters, annealingParams);

						// Finalize the fitting
						fitStats.m_perVariable[variableID].m_computationTimer.stop();
						storeFitResultEntry(scene, object, polynomials, fitStats, ghostID, variableID, fitResult);
					}
				}
			}

			////////////////////////////////////////////////////////////////////////////////
			namespace PolynomialRegression
			{
				////////////////////////////////////////////////////////////////////////////////
				void fit(Scene::Scene& scene, Scene::Object* object, PolynomialFit& polynomials,
					FitStatsFit& fitStats, const size_t ghostID, FitDataSets const& dataset,
					PolynomialFitParameters const& fitParameters,
					PolynomialFitParameters::PolynomialRegressionParams const& regressionParams)
				{
					const size_t numVariables = dataset.size();
					const size_t numAngles = dataset[0].shape()[0];
					const size_t numRotations = dataset[0].shape()[1];
					const size_t numWavelengths = dataset[0].shape()[2];

					Debug::DebugOutputLevel outputLevel = fitParameters.m_debugComputation >= PolynomialFitParameters::DebugLevel::LightDebug ? Debug::Info : Debug::Debug;

					DateTime::ScopedTimer timer(outputLevel, numVariables, DateTime::Seconds, "Polynomial Fitting [Polynomial Regression]");

					// Compute the number of samples to use
					size_t numSamples = regressionParams.m_numSamples;
					if (fitParameters.m_ghostGeometryParameters.m_shareNeighboringGeometries)
					{
						if (fitParameters.m_ghostGeometryParameters.m_numAngles > 1) numSamples *= 2;
						if (fitParameters.m_ghostGeometryParameters.m_numRotations > 1) numSamples *= 2;
					}

					// Construct the output dataset
					for (size_t variableID = 0; variableID < numVariables; ++variableID)
					{
						std::string const& variableName = s_polynomialOutputVariables[variableID];

						// Log the progress
						Debug::log_info() << "Computing polynomials for \"" << variableName << "\" (" << (variableID + 1) << "/" << numVariables << ")" << Debug::end;

						// Initialize the fitting
						FitDataSet datasetSubsampled = PolynomialsCommon::Fitting::subsampleDataset(fitParameters, dataset[variableID], numSamples);
						fitStats.m_perVariable[variableID].m_computationTimer.start();

						// Perform the fitting
						auto fitResult = PolynomialsCommon::Fitting::PolynomialRegression::fit(scene, object,
							variableName, dataset[variableID], datasetSubsampled, fitParameters, regressionParams);

						// Finalize the fitting
						fitStats.m_perVariable[variableID].m_computationTimer.stop();
						storeFitResultEntry(scene, object, polynomials, fitStats, ghostID, variableID, fitResult);
					}
				}
			}

			////////////////////////////////////////////////////////////////////////////////
			void fitPolynomials(Scene::Scene& scene, Scene::Object* object, PolynomialFit& polynomials, FitStatsFit& fitStats,
				const size_t ghostID, PolynomialFitParameters const& fitParameters, FitDataSets const& datasets)
			{
				Debug::log_debug() << "Fitting full polynomials to data..." << Debug::end;

				// Perform the fitting
				switch (fitParameters.m_fitMethod)
				{
				case PolynomialFitParameters::SimulatedAnnealing:
					SimulatedAnnealing::fit(scene, object, polynomials, fitStats, ghostID, datasets, fitParameters, fitParameters.m_annealingParams);
					break;
				case PolynomialFitParameters::PolynomialRegression:
					PolynomialRegression::fit(scene, object, polynomials, fitStats, ghostID, datasets, fitParameters, fitParameters.m_polynomialRegressionParams);
					break;
				}

				Debug::log_debug() << "Polynomial fit successfuly!" << Debug::end;
			}

			////////////////////////////////////////////////////////////////////////////////
			void saveFitStats(Scene::Scene& scene, Scene::Object* object, std::string const& fileName, std::stringstream const& ss)
			{
				std::string filePath = GhostFilePaths::getPolynomialWeightsPartialFitFilePath(scene, object, fileName + ".txt");
				EnginePaths::makeDirectoryStructure(filePath, true);
				std::ofstream of(filePath);
				of << ss.str();
				of.close();
			}

			////////////////////////////////////////////////////////////////////////////////
			void reportFitStats(Scene::Scene& scene, Scene::Object* object, PolynomialFit const& polynomials, FitStatsFit const& fitStats)
			{
				// Precompute parameters
				PolynomialFitParameters const& fitParams = object->component<TiledLensFlareComponent>().m_polynomialFitParameters;
				PhysicalCamera::PhysicalCameraAttributes const& camera = object->component<TiledLensFlareComponent>().m_camera;
				auto& ghostAttribsMap = object->component<TiledLensFlareComponent>().m_precomputedGhostAttribs;

				// Enumerate the possible ghosts
				auto const& ghostIndices = object->component<TiledLensFlareComponent>().m_ghostIndices;
				const size_t numAllGhosts = ghostIndices.size();
				const size_t firstGhost = fitParams.m_ghostGeometryParameters.m_firstGhost;
				const size_t numGhosts = std::min(size_t(fitParams.m_ghostGeometryParameters.m_numGhost), numAllGhosts - firstGhost);
				const size_t numAngles = fitParams.m_ghostGeometryParameters.m_numAngles;
				const size_t numRotations = fitParams.m_ghostGeometryParameters.m_numRotations;
				const size_t numChannels = fitParams.m_ghostGeometryParameters.m_numChannels;
				const size_t numRays = fitParams.m_ghostGeometryParameters.m_rayCount;
				const size_t numVariables = NUM_POLYNOMIAL_OUTPUT_VARIABLES;

				// Save the fitting stats
				{
					// Global
					{
						std::stringstream ss;
						ss << fitStats.m_global << std::endl;
						saveFitStats(scene, object, "stats_g", ss);
					}

					// Per-variable
					{
						std::stringstream ss;
						for (size_t variableID = 0; variableID < numVariables; ++variableID)
						{
							ss << " - " << s_polynomialOutputVariables[variableID] << ":" << std::endl;
							ss << fitStats.m_perVariable[variableID] << std::endl;
							ss << std::string(80, '-') << std::endl;
						}
						saveFitStats(scene, object, "stats_pv", ss);
					}

					// Per-ghost
					{
						std::stringstream ss;
						for (size_t ghostID = firstGhost; ghostID < firstGhost + numGhosts && ghostID < numAllGhosts; ++ghostID)
						for (size_t angleID = 0; angleID < numAngles; ++angleID)
						for (size_t rotationID = 0; rotationID < numRotations; ++rotationID)
						for (size_t wavelengthID = 0; wavelengthID < numChannels; ++wavelengthID)
						{
							for (size_t variableID = 0; variableID < numVariables; ++variableID)
							{
								ss << "Ghost[" << ghostID << "][" << angleID << "][" << rotationID << "][" << wavelengthID << "][" << s_polynomialOutputVariables[variableID] << "]" << ": " << std::endl;
								ss << fitStats.m_perEntry[ghostID][angleID][rotationID][wavelengthID][variableID] << std::endl;
								ss << std::string(80, '-') << std::endl;
							}
							ss << std::string(80, '=') << std::endl;
						}
						saveFitStats(scene, object, "stats_pe", ss);
					}
				}

				// Save the errors
				{
					// Global
					{
						std::stringstream ss;
						ss << " - " << "subsampled: " << fitStats.m_global.m_finalErrorSubSampled / fitStats.m_global.numComputations() << std::endl;
						ss << " - " << "full: " << fitStats.m_global.m_finalErrorFullDataset / fitStats.m_global.numComputations() << std::endl;
						saveFitStats(scene, object, "errors_g", ss);
					}

					// Per-variable
					{
						std::stringstream ss;
						for (size_t variableID = 0; variableID < numVariables; ++variableID)
						{
							auto const& fitStatsVar = fitStats.m_perVariable[variableID];
							if (fitStatsVar.m_finalErrorFullDataset == 0.0f) continue;
							ss
								<< " - " << s_polynomialOutputVariables[variableID] << ": "
								<< fitStatsVar.m_finalErrorSubSampled / fitStatsVar.numComputations() << " (subsampled)" << ", "
								<< fitStatsVar.m_finalErrorFullDataset / fitStatsVar.numComputations() << " (full)"
								<< std::endl;
						}
						saveFitStats(scene, object, "errors_pv", ss);
					}

					// Per-ghost
					{
						std::stringstream ss;
						for (size_t ghostID = firstGhost; ghostID < firstGhost + numGhosts && ghostID < numAllGhosts; ++ghostID)
						for (size_t variableID = 0; variableID < numVariables; ++variableID)
						{
							ss << "Ghost[" << ghostID << "][" << s_polynomialOutputVariables[variableID] << "]: " << std::endl;
							for (size_t angleID = 0; angleID < numAngles; ++angleID)
							for (size_t rotationID = 0; rotationID < numRotations; ++rotationID)
								for (size_t wavelengthID = 0; wavelengthID < numChannels; ++wavelengthID)
							{
								auto const& fitStatsVar = fitStats.m_perEntry[ghostID][angleID][rotationID][wavelengthID][variableID];
								if (fitStatsVar.m_finalErrorFullDataset == 0.0f) continue;
								ss
									<< "- [" << angleID << "][" << rotationID << "][" << wavelengthID << "]" << ": "
									<< fitStatsVar.m_finalErrorSubSampled << " (subsampled)" << ", "
									<< fitStatsVar.m_finalErrorFullDataset << " (full)"
									<< std::endl;
							}
							ss << std::string(80, '=') << std::endl;
						}
						saveFitStats(scene, object, "errors_pe", ss);
					}
				}

				// Log and save the polynomials
				{
					std::stringstream ss;
					for (size_t ghostID = firstGhost; ghostID < firstGhost + numGhosts && ghostID < numAllGhosts; ++ghostID)
					for (size_t angleID = 0; angleID < numAngles; ++angleID)
					for (size_t rotationID = 0; rotationID < numRotations; ++rotationID)
					for (size_t wavelengthID = 0; wavelengthID < numChannels; ++wavelengthID)
					for (size_t variableID = 0; variableID < numVariables; ++variableID)
					{
						ss
							<< "Ghost[" << ghostID << "][" << angleID << "][" << rotationID << "][" << wavelengthID << "][" << s_polynomialOutputVariables[variableID] << "]" << ": "
							<< polynomials[ghostID][angleID][rotationID][wavelengthID][variableID]
							<< std::endl;
					}
					saveFitStats(scene, object, "polynomials", ss);
				}
			}

			////////////////////////////////////////////////////////////////////////////////
			PolynomialFit performFit(Scene::Scene& scene, Scene::Object* object)
			{
				Debug::log_info() << "Performing partial polynomial set fitting..." << Debug::end;

				// Precompute parameters
				PolynomialFitParameters const& fitParams = object->component<TiledLensFlareComponent>().m_polynomialFitParameters;
				PhysicalCamera::PhysicalCameraAttributes const& camera = object->component<TiledLensFlareComponent>().m_camera;
				auto& ghostAttribsMap = object->component<TiledLensFlareComponent>().m_precomputedGhostAttribs;

				// Enumerate the possible ghosts
				auto const& ghostIndices = object->component<TiledLensFlareComponent>().m_ghostIndices;
				const size_t numAllGhosts = ghostIndices.size();
				const size_t firstGhost = fitParams.m_ghostGeometryParameters.m_firstGhost;
				const size_t numGhosts = std::min(size_t(fitParams.m_ghostGeometryParameters.m_numGhost), numAllGhosts - firstGhost);
				const size_t numAngles = fitParams.m_ghostGeometryParameters.m_numAngles;
				const size_t numRotations = fitParams.m_ghostGeometryParameters.m_numRotations;
				const size_t numChannels = fitParams.m_ghostGeometryParameters.m_numChannels;
				const size_t numRays = fitParams.m_ghostGeometryParameters.m_rayCount;
				const size_t numVariables = NUM_POLYNOMIAL_OUTPUT_VARIABLES;

				DateTime::ScopedTimer timer = DateTime::ScopedTimer(Debug::Debug, numGhosts, DateTime::Milliseconds, "Polynomial Fitting");

				// Result of the fit
				PolynomialFit polynomials = Fitting::makeFitResult(scene, object, numAllGhosts, fitParams);
				FitStatsFit fitStats = Fitting::makeFitStats(scene, object, numAllGhosts, fitParams);

				// Initiate the computation
				fitStats.m_global.m_computationTimer.start();

				// Fit the ghosts themselves
				for (int ghostID = firstGhost; ghostID < firstGhost + numGhosts && ghostID < numAllGhosts; ++ghostID)
				{
					Debug::log_info()
						<< "Computing polynomials for "
						<< "ghost #" << (ghostID + 1) << "/" << numGhosts
						<< Debug::end;

					// List of ghost geometries for polynomial fitting
					const PolynomialsCommon::PrecomputedGhostGeometry ghostGeometries = PolynomialsCommon::ComputeGhostGeometry::computeGhostGeometries(scene, object, ghostID);

					// Get the data points
					const FitDataSets datasets = Fitting::convertGhostGeometryToFitDataset(scene, object, fitParams, ghostGeometries);

					// Perform the polynomial fitting
					Fitting::fitPolynomials(scene, object, polynomials, fitStats, ghostID, fitParams, datasets);
				}

				// Finalize the computation
				fitStats.m_global.m_computationTimer.stop();

				Debug::log_info() << "Polynomial weights successfully computed." << Debug::end;

				// Report the fitting stats
				reportFitStats(scene, object, polynomials, fitStats);

				// Return the result
				return polynomials;
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		int getNumWeightsPerGhostFull(Scene::Scene& scene, Scene::Object* object)
		{
			return
				object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_numSparseTerms *
				object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_numAngles *
				object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_numRotations *
				object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_numChannels;
		}

		////////////////////////////////////////////////////////////////////////////////
		int getNumWeightsPerGhostBaked(Scene::Scene& scene, Scene::Object* object)
		{
			return 
				object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_numSparseTerms *
				object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_numChannels;
		}

		////////////////////////////////////////////////////////////////////////////////
		int getNumWeightsAllGhosts(Scene::Scene& scene, Scene::Object* object)
		{
			return
				PhysicalCamera::getNumGhosts(object->component<TiledLensFlareComponent>().m_camera) *
				getNumWeightsPerGhostFull(scene, object);
		}

		////////////////////////////////////////////////////////////////////////////////
		int getNumWeightsAllGhostsBaked(Scene::Scene& scene, Scene::Object* object)
		{
			return
				PhysicalCamera::getNumGhosts(object->component<TiledLensFlareComponent>().m_camera) *
				getNumWeightsPerGhostBaked(scene, object);
		}

		////////////////////////////////////////////////////////////////////////////////
		namespace Serialization
		{
			////////////////////////////////////////////////////////////////////////////////
			PolynomialFit loadGhostWeights(Scene::Scene& scene, Scene::Object* object, std::string filePath)
			{
				Debug::log_debug() << "Attempting to load partially fit polynomial ghost weights from file: " << filePath << Debug::end;

				std::ifstream file(filePath, std::ios::binary);
				if (!file)
				{
					Debug::log_debug() << "Unable to open polynomial ghost weights file: " << filePath << Debug::end;
					return PolynomialFit{};
				}

				size_t numGhosts;
				size_t numAngles;
				size_t numRotations;
				size_t numWavelengths;
				size_t numVariables;
				file.read((char*)&numGhosts, sizeof(numGhosts));
				file.read((char*)&numAngles, sizeof(numAngles));
				file.read((char*)&numRotations, sizeof(numRotations));
				file.read((char*)&numWavelengths, sizeof(numWavelengths));
				file.read((char*)&numVariables, sizeof(numVariables));

				PolynomialFit weights(boost::extents[numGhosts][numAngles][numRotations][numWavelengths][numVariables]);
				for (size_t ghostID = 0; ghostID < numGhosts; ++ghostID)
				for (size_t angleID = 0; angleID < numAngles; ++angleID)
				for (size_t rotationID = 0; rotationID < numRotations; ++rotationID)
				for (size_t wavelengthID = 0; wavelengthID < numWavelengths; ++wavelengthID)
				for (size_t variableID = 0; variableID < numVariables; ++variableID)
				{
					size_t numWeights;
					file.read((char*)&numWeights, sizeof(numWeights));
					Polynomial polynomial{ numWeights };
					file.read((char*)polynomial.data(), numWeights * sizeof(Monomial));
					weights[ghostID][angleID][rotationID][wavelengthID][variableID] = polynomial;
				}
				file.close();

				Debug::log_debug() << "Ghost weights successfully loaded from file: " << filePath << Debug::end;
				/*
				Debug::log_debug() << "Resulting weights:" << Debug::end;
				for (size_t ghostID = 0; ghostID < numGhosts; ++ghostID)
				for (size_t angleID = 0; angleID < numAngles; ++angleID)
				for (size_t rotationID = 0; rotationID < numRotations; ++rotationID)
				for (size_t wavelengthID = 0; wavelengthID < numWavelengths; ++wavelengthID)
				for (size_t variableID = 0; variableID < numVariables; ++variableID)
				{
					Debug::log_debug() << "Ghost" 
						<< "[" << ghostID << "][" << angleID << "][" << rotationID << "][" << wavelengthID << "][" << variableID << "]: "
						<< weights[ghostID][angleID][rotationID][wavelengthID][variableID]
						<< Debug::end;
				}
				*/

				return weights;
			}

			////////////////////////////////////////////////////////////////////////////////
			PolynomialFit loadGhostWeights(Scene::Scene& scene, Scene::Object* object)
			{
				return loadGhostWeights(scene, object, GhostFilePaths::getPolynomialWeightsPartialFitFilePath(scene, object));
			}

			////////////////////////////////////////////////////////////////////////////////
			void saveGhostWeights(Scene::Scene& scene, Scene::Object* object, std::string const& filePath, PolynomialFit const& polynomials)
			{
				EnginePaths::makeDirectoryStructure(filePath, true);
				std::ofstream outFile(filePath, std::ofstream::binary);

				const size_t numGhosts = polynomials.shape()[0];
				const size_t numAngles = polynomials.shape()[1];
				const size_t numRotations = polynomials.shape()[2];
				const size_t numWavelengths = polynomials.shape()[3];
				const size_t numVariables = polynomials.shape()[4];
				outFile.write((char*)&numGhosts, sizeof(numGhosts));
				outFile.write((char*)&numAngles, sizeof(numAngles));
				outFile.write((char*)&numRotations, sizeof(numRotations));
				outFile.write((char*)&numWavelengths, sizeof(numWavelengths));
				outFile.write((char*)&numVariables, sizeof(numVariables));
				for (size_t ghostID = 0; ghostID < numGhosts; ++ghostID)
				for (size_t angleID = 0; angleID < numAngles; ++angleID)
				for (size_t rotationID = 0; rotationID < numRotations; ++rotationID)
				for (size_t wavelengthID = 0; wavelengthID < numWavelengths; ++wavelengthID)
				for (size_t variableID = 0; variableID < numVariables; ++variableID)
				{
					Polynomial const& polynomial = polynomials[ghostID][angleID][rotationID][wavelengthID][variableID];
					size_t numWeights = polynomial.size();
					outFile.write((char*)&numWeights, sizeof(numWeights));
					outFile.write((char*)polynomial.data(), numWeights * sizeof(Monomial));
				}
			}

			////////////////////////////////////////////////////////////////////////////////
			void saveGhostWeights(Scene::Scene& scene, Scene::Object* object, PolynomialFit const& polynomials)
			{
				saveGhostWeights(scene, object, GhostFilePaths::getPolynomialWeightsPartialFitFilePath(scene, object), polynomials);
			}

			////////////////////////////////////////////////////////////////////////////////
			template<typename P>
			Uniforms::GhostPolynomialMonomialFull populateGpuMonomial(Scene::Scene& scene, Scene::Object* object, P const& polynomials, const size_t termID)
			{
				return PolynomialsCommon::populateGpuMonomial<NUM_POLYNOMIAL_INPUT_VARIABLES, NUM_POLYNOMIAL_OUTPUT_VARIABLES>(scene, object, polynomials, termID);
			}

			////////////////////////////////////////////////////////////////////////////////
			void uploadGhostWeights(Scene::Scene& scene, Scene::Object* object, PolynomialFit const& polynomials)
			{
				if (polynomials.empty())
				{
					object->component<TiledLensFlareComponent>().m_numPolynomialTermsPartialFit = std::vector<size_t>(1000, 0);
					return;
				}
			
				const size_t numGhosts = polynomials.shape()[0];
				const size_t numAngles = polynomials.shape()[1];
				const size_t numRotations = polynomials.shape()[2];
				const size_t numWavelengths = polynomials.shape()[3];
				const size_t numVariables = polynomials.shape()[4];
				const size_t numMaxSparseTerms = object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_numSparseTerms;

				Debug::log_debug() << "Uploading polynomial ghost weights..." << Debug::end;

				// Calculate the number of terms necessary
				Debug::log_debug() << "Number of polynomial terms:" << Debug::end;

				object->component<TiledLensFlareComponent>().m_numPolynomialTermsPartialFit.resize(numGhosts);
				for (size_t ghostID = 0; ghostID < numGhosts; ++ghostID)
				{
					size_t numTerms = 0;
					for (size_t variableID = 0; variableID < numVariables; ++variableID)
					for (size_t angleID = 0; angleID < numAngles; ++angleID)
					for (size_t rotationID = 0; rotationID < numRotations; ++rotationID)
					for (size_t wavelengthID = 0; wavelengthID < numWavelengths; ++wavelengthID)
						numTerms = std::max(numTerms, polynomials[ghostID][angleID][rotationID][wavelengthID][variableID].size());
					object->component<TiledLensFlareComponent>().m_numPolynomialTermsPartialFit[ghostID] = numTerms;
					Debug::log_debug() << " - Ghost #" << ghostID << ": " << object->component<TiledLensFlareComponent>().m_numPolynomialTermsPartialFit[ghostID] << Debug::end;
				}

				// Calculate the memory requirements
				size_t numTotalTerms = 0;
				for (size_t ghostID = 0; ghostID < numGhosts; ++ghostID)
				for (size_t variableID = 0; variableID < numVariables; ++variableID)
					numTotalTerms += polynomials[ghostID][0][0][0][variableID].size();

				const size_t totalMemory =
					numTotalTerms * numAngles * numRotations * numWavelengths * sizeof(float) +  // coefficients
					numTotalTerms * 4 * sizeof(int); // degrees;
				Debug::log_debug() << "Total memory needed (partial model): " << Units::bytesToString(totalMemory) << Debug::end;

				// Populate the data vector and upload it
				std::vector<Uniforms::GhostPolynomialMonomialFull> allGhostWeightsGPU;
				allGhostWeightsGPU.reserve(numGhosts * numAngles * numRotations * numWavelengths * numMaxSparseTerms);
				for (size_t ghostID = 0; ghostID < numGhosts; ++ghostID)
				for (size_t wavelengthID = 0; wavelengthID < numWavelengths; ++wavelengthID)
				for (size_t angleID = 0; angleID < numAngles; ++angleID)
				for (size_t rotationID = 0; rotationID < numRotations; ++rotationID)
				for (size_t termID = 0; termID < numMaxSparseTerms; ++termID)
					allGhostWeightsGPU.push_back(populateGpuMonomial(scene, object, polynomials[ghostID][angleID][rotationID][wavelengthID], termID));
				uploadBufferData(scene, "TiledLensFlarePolynomialWeightsBufferPartialFit", allGhostWeightsGPU);
			
				Debug::log_debug() << "Ghost weights uploaded!" << Debug::end;
			}

			////////////////////////////////////////////////////////////////////////////////
			void uploadGhostWeights(Scene::Scene& scene, Scene::Object* object)
			{
				uploadGhostWeights(scene, object, loadGhostWeights(scene, object));
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		void performFit(Scene::Scene& scene, Scene::Object* object)
		{
			// Perform the fitting
			PolynomialFit polynomials = Fitting::performFit(scene, object);

			// Save the ghost weights
			Serialization::saveGhostWeights(scene, object, polynomials);

			// Upload the ghost weights
			Serialization::uploadGhostWeights(scene, object, polynomials);
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	namespace Uniforms
	{
		////////////////////////////////////////////////////////////////////////////////
		std::vector<Uniforms::GhostParams> uploadGhostParametersRender(Scene::Scene& scene, Scene::Object* object,
			LightSources::LightSourceData const& lightData)
		{
			// Extract the ghost parameters
			std::vector<PhysicalCamera::GhostIndices> const& ghostIndices = object->component<TiledLensFlareComponent>().m_ghostIndices;
			const int firstGhost = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_firstGhost;
			const int numGhosts = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_numGhost;
			const int numWavelengths = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_numWavelengths;
			auto const& ignoredGhosts = object->component<TiledLensFlareComponent>().m_commonParameters.m_ignoredGhosts[Common::getCamera(scene, object).m_name];

			std::vector<Uniforms::GhostParams> ghostParams;

			int gridStartID = 0;
			for (int ghostID = firstGhost; ghostID < firstGhost + numGhosts && ghostID < ghostIndices.size(); ++ghostID)
			for (int channelID = 0; channelID < numWavelengths; ++channelID)
			{
				// Ghost attributes
				GhostAttribs::RenderGhostAttribs ghostAttribs = GhostAttribs::getGhostAttribs(scene, object, ghostID, channelID, lightData);

				// Clip low intensity ghosts
				if (!GhostAttribs::isGhostValid(scene, object, ghostAttribs) || !GhostAttribs::isGhostVisible(scene, object, ghostAttribs))
					continue;

				// Ignore ghosts on the ignore list
				if (std::find(ignoredGhosts.begin(), ignoredGhosts.end(), ghostID) != ignoredGhosts.end())
					continue;

				// Store the ghost parameters
				GhostParams uniformDataGhostParams;
				uniformDataGhostParams.m_ghostIndices = glm::ivec2(ghostIndices[ghostID][1], ghostIndices[ghostID][0]);
				uniformDataGhostParams.m_minPupil = ghostAttribs.m_pupilMin;
				uniformDataGhostParams.m_maxPupil = ghostAttribs.m_pupilMax;
				uniformDataGhostParams.m_centerPupil = ghostAttribs.m_pupilCenter;
				uniformDataGhostParams.m_radiusPupil = ghostAttribs.m_pupilRadius;
				uniformDataGhostParams.m_minSensor = ghostAttribs.m_sensorMin;
				uniformDataGhostParams.m_maxSensor = ghostAttribs.m_sensorMax;
				uniformDataGhostParams.m_rayCount = ghostAttribs.m_rayGridSize;
				uniformDataGhostParams.m_ghostId = ghostID;
				uniformDataGhostParams.m_channelId = channelID;
				uniformDataGhostParams.m_lambda = object->component<TiledLensFlareComponent>().m_camera.m_wavelengths[numWavelengths][channelID];
				uniformDataGhostParams.m_intensityScale = ghostAttribs.m_intensityScale;
				uniformDataGhostParams.m_gridStartId = gridStartID;
				if (object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_useDynamicTermCount)
				{
					if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod == RenderGhostsParameters::PolynomialFullFit)
					{
						uniformDataGhostParams.m_numPolynomialTerms = object->component<TiledLensFlareComponent>().m_numPolynomialTermsFullFit[ghostID];
					}
					if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod == RenderGhostsParameters::PolynomialPartialFit)
					{
						uniformDataGhostParams.m_numPolynomialTerms = object->component<TiledLensFlareComponent>().m_numPolynomialTermsPartialFit[ghostID];
					}
				}
				else
				{
					uniformDataGhostParams.m_numPolynomialTerms = object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_numSparseTerms;
				}
				ghostParams.push_back(uniformDataGhostParams);

				// Increment the ray grid start ID
				gridStartID += uniformDataGhostParams.m_rayCount * uniformDataGhostParams.m_rayCount;
			}

			return ghostParams;
		}

		////////////////////////////////////////////////////////////////////////////////
		Uniforms::RenderGhostsLensUniforms uploadLensUniformsRender(Scene::Scene& scene, Scene::Object* object, LightSources::LightSourceData const& lightData)
		{
			return uploadLensUniformsCommon(scene, object, lightData, 
				object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_numWavelengths,
				object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_radiusClipping,
				object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_irisClipping,
				object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_intensityClipping,
				object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_refractionClipping);
		}

		////////////////////////////////////////////////////////////////////////////////
		Uniforms::RenderGhostsCommonUniforms uploadCommonUniformsRender(Scene::Scene& scene, Scene::Object* object,
			std::vector<Uniforms::GhostParams> const& ghostParams)
		{
			RenderGhostsCommonUniforms lensFlareDataCommon;

			// Common render settings
			lensFlareDataCommon.m_renderResolution = Common::computeRenderResolution(scene, object);
			lensFlareDataCommon.m_gridMethod = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_gridMethod;
			lensFlareDataCommon.m_raytraceMethod = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod;
			lensFlareDataCommon.m_projectionMethod = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_projectionMethod;
			lensFlareDataCommon.m_shadingMethod = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_shadingMethod;
			lensFlareDataCommon.m_overlayMethod = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_overlayMethod;
			lensFlareDataCommon.m_isWireFrame = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_wireframe ? 1 : 0;
			lensFlareDataCommon.m_clipRays = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_clipRays ? 1 : 0;
			lensFlareDataCommon.m_clipPixels = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_clipPixels ? 1 : 0;
			lensFlareDataCommon.m_filmStretch = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_filmStretch;

			// Various numbers
			lensFlareDataCommon.m_numGhosts = ghostParams.size();
			lensFlareDataCommon.m_numChannels = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_numWavelengths;
			lensFlareDataCommon.m_maxNumGhosts = object->component<TiledLensFlareComponent>().m_ghostIndices.size();
			lensFlareDataCommon.m_maxNumChannels = 3;//object->component<TiledLensFlareComponent>().m_wavelengths.size();
			lensFlareDataCommon.m_maxRayGridSize = ghostParams.empty() ? 0 : std::max_element(ghostParams.begin(), ghostParams.end(),
				[](auto const& a, auto const& b) { return a.m_rayCount > b.m_rayCount; })->m_rayCount;

			// Polynomial fit parameters
			lensFlareDataCommon.m_numPolynomialAngles = object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_numAngles;
			lensFlareDataCommon.m_numPolynomialRotations = object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_numRotations;
			lensFlareDataCommon.m_maxPolynomialAngles = object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_maxAngle;
			lensFlareDataCommon.m_maxPolynomialRotations = object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_maxRotation;
			lensFlareDataCommon.m_polynomialAnglesStep = lensFlareDataCommon.m_maxPolynomialAngles / float(lensFlareDataCommon.m_numPolynomialAngles - 1);
			lensFlareDataCommon.m_polynomialRotationsStep = lensFlareDataCommon.m_maxPolynomialRotations / float(lensFlareDataCommon.m_numPolynomialRotations - 1);

			return lensFlareDataCommon;
		}

		////////////////////////////////////////////////////////////////////////////////
		Uniforms::RenderGhostsCommonUniformsDirect uploadCommonUniformsRenderDirect(Scene::Scene& scene, Scene::Object* object)
		{
			RenderGhostsCommonUniformsDirect lensFlareDataCommonDirect;
			return lensFlareDataCommonDirect;
		}

		////////////////////////////////////////////////////////////////////////////////
		Uniforms::RenderGhostsCommonUniformsTiled uploadCommonUniformsRenderTiled(Scene::Scene& scene, Scene::Object* object)
		{
			Uniforms::RenderGhostsCommonUniformsTiled lensFlareDataCommonTiled;
			lensFlareDataCommonTiled.m_coarseTileSize = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_coarseTileSize;
			lensFlareDataCommonTiled.m_numCoarseTiles = Common::computeNumCoarseTiles(scene, object);
			lensFlareDataCommonTiled.m_maxCoarseTileEntries = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_maxCoarseTileEntries;
			lensFlareDataCommonTiled.m_tileSize = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_tileSize;
			lensFlareDataCommonTiled.m_numTiles = Common::computeNumTiles(scene, object);
			lensFlareDataCommonTiled.m_maxTileEntries = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_maxTileEntries;
			lensFlareDataCommonTiled.m_numQuadMergeSteps = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_numQuadMergeSteps;
			lensFlareDataCommonTiled.m_quadMergeEdgeThreshold = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_quadMergeEdgeThreshold;
			lensFlareDataCommonTiled.m_sampleBaseScene = !Common::needsTempTexture(scene, object) ? 1 : 0;
			return lensFlareDataCommonTiled;
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	namespace InitResources
	{
		////////////////////////////////////////////////////////////////////////////////
		void initTextures(Scene::Scene& scene, Scene::Object* object)
		{
			PhysicalCamera::initApertureTextures(scene);

			// Max supported resolution
			float resolutionScaling = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_resolutionScaling;
			auto resolution = glm::ivec2(glm::vec2(Config::AttribValue("max_resolution").get<glm::ivec2>()) * resolutionScaling);

			Scene::createTexture(scene, "TiledLensFlare_Accumulation_0", GL_TEXTURE_2D, resolution[0], resolution[1], 1, GL_RGBA16F, GL_RGBA, GL_LINEAR, GL_LINEAR);
			Scene::createTexture(scene, "TiledLensFlare_Accumulation_1", GL_TEXTURE_2D, resolution[0], resolution[1], 1, GL_RGBA16F, GL_RGBA, GL_LINEAR, GL_LINEAR);
		}

		////////////////////////////////////////////////////////////////////////////////
		void initShaders(Scene::Scene& scene, Scene::Object* object)
		{
			// Camera-related shaders
			PhysicalCamera::initApertureShaders(scene);

			// Common defines for all shaders
			std::vector<std::string> commonDefinesLenses =
			{
				"MAX_LENSES " + std::to_string(MAX_LENSES),
				"MAX_CHANNELS " + std::to_string(MAX_CHANNELS)
			};
			std::vector<std::string> commonDefinesPolynomials =
			{
				"BAKE_POLYNOMIALS_GROUP_SIZE " + std::to_string(object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_bakePolynomialsGroupSize),
				"NUM_POLYNOMIAL_TERMS " + std::to_string(object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_numSparseTerms),
				"BAKE_POLYNOMIAL_INVARIANTS " + (object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_bakeInvariants ? "1"s : "0"s),
				"USE_DYNAMIC_POLYNOMIAL_TERM_COUNTS " + (object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_useDynamicTermCount ? "1"s : "0"s),
				"USE_GROUPSHARED_MEMORY_POLYNOMIAL " + (object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_useGroupSharedMemory ? "1"s : "0"s),
				"LERP_POLYNOMIAL_COEFFICIENTS " + (object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_shareNeighboringGeometries ? "0"s : "1"s)
			};
			std::vector<std::string> commonEnums = Asset::generateMetaEnumDefines
			(
				RenderGhostsParameters::GridMethod_meta,
				RenderGhostsParameters::RaytraceMethod_meta,
				RenderGhostsParameters::ProjectionMethod_meta,
				RenderGhostsParameters::ShadingMethod_meta,
				RenderGhostsParameters::OverlayMethod_meta
			);

			// Precomputation shaders
			Asset::ShaderParameters shaderParametersPrecompute;
			shaderParametersPrecompute.m_allowCache = false;
			shaderParametersPrecompute.m_defines =
			{
				"RAY_TRACE_METHOD " + std::to_string(RenderGhostsParameters::RaytraceMethod::Analytical),
			};
			shaderParametersPrecompute.m_defines.insert(shaderParametersPrecompute.m_defines.end(), commonDefinesLenses.begin(), commonDefinesLenses.end());
			shaderParametersPrecompute.m_defines.insert(shaderParametersPrecompute.m_defines.end(), commonDefinesPolynomials.begin(), commonDefinesPolynomials.end());
			shaderParametersPrecompute.m_enums.insert(shaderParametersPrecompute.m_enums.end(), commonEnums.begin(), commonEnums.end());

			Asset::loadShader(scene, "LensFlare/RayTraceLensFlare/PrecomputeGhosts", "precompute_ghosts", "RayTraceLensFlare/precompute_ghosts", shaderParametersPrecompute);

			// Commonly used shaders
			Asset::ShaderParameters shaderParametersCommon;
			shaderParametersCommon.m_allowCache = false;
			shaderParametersCommon.m_defines =
			{
				"RAY_TRACE_METHOD " + std::to_string(object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod),
			};
			shaderParametersCommon.m_defines.insert(shaderParametersCommon.m_defines.end(), commonDefinesLenses.begin(), commonDefinesLenses.end());
			shaderParametersCommon.m_defines.insert(shaderParametersCommon.m_defines.end(), commonDefinesPolynomials.begin(), commonDefinesPolynomials.end());
			shaderParametersCommon.m_enums.insert(shaderParametersCommon.m_enums.end(), commonEnums.begin(), commonEnums.end());
			Asset::loadShader(scene, "LensFlare/RayTraceLensFlare/TraceRays", "bake_polynomial_invariants", "RayTraceLensFlare/bake_polynomial_invariants", shaderParametersCommon);

			// Direct rasterization shaders
			Asset::ShaderParameters shaderParametersDirect;
			shaderParametersDirect.m_allowCache = false;
			shaderParametersDirect.m_defines =
			{
				"RAY_TRACE_METHOD " + std::to_string(object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod),
			};
			shaderParametersDirect.m_defines.insert(shaderParametersDirect.m_defines.end(), commonDefinesLenses.begin(), commonDefinesLenses.end());
			shaderParametersDirect.m_defines.insert(shaderParametersDirect.m_defines.end(), commonDefinesPolynomials.begin(), commonDefinesPolynomials.end());
			shaderParametersDirect.m_enums.insert(shaderParametersDirect.m_enums.end(), commonEnums.begin(), commonEnums.end());

			Asset::loadShader(scene, "LensFlare/RayTraceLensFlare/RenderGhosts", "compute_grid", "RayTraceLensFlare/compute_grid", shaderParametersDirect);
			Asset::loadShader(scene, "LensFlare/RayTraceLensFlare/RenderGhosts", "render_ghosts", "RayTraceLensFlare/render_ghosts", shaderParametersDirect);

			// Tiled rasterization shaders
			Asset::ShaderParameters shaderParametersTiled;
			shaderParametersTiled.m_allowCache = false;
			shaderParametersTiled.m_defines =
			{
				"RAY_TRACE_METHOD " + std::to_string(object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod),
				"COARSE_TILE_SIZE " + std::to_string(object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_coarseTileSize),
				"TILE_SIZE " + std::to_string(object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_tileSize),
				"TRACE_RAYS_GROUP_SIZE " + std::to_string(object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_traceRaysGroupSize),
				"TILE_SPLAT_GROUP_SIZE " + std::to_string(object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_splatTrianglesGroupSize),
			};
			shaderParametersTiled.m_defines.insert(shaderParametersTiled.m_defines.end(), commonDefinesLenses.begin(), commonDefinesLenses.end());
			shaderParametersTiled.m_defines.insert(shaderParametersTiled.m_defines.end(), commonDefinesPolynomials.begin(), commonDefinesPolynomials.end());
			shaderParametersTiled.m_enums.insert(shaderParametersTiled.m_enums.end(), commonEnums.begin(), commonEnums.end());

			Asset::loadShader(scene, "LensFlare/RayTraceLensFlare/RenderGhostsTiled", "trace_rays", "RayTraceLensFlare/tiled_trace_rays", shaderParametersTiled);
			Asset::loadShader(scene, "LensFlare/RayTraceLensFlare/RenderGhostsTiled", "build_tiles_command", "RayTraceLensFlare/tiled_build_tiles_command", shaderParametersTiled);
			Asset::loadShader(scene, "LensFlare/RayTraceLensFlare/RenderGhostsTiled", "build_tiles", "RayTraceLensFlare/tiled_build_tiles", shaderParametersTiled);
			Asset::loadShader(scene, "LensFlare/RayTraceLensFlare/RenderGhostsTiled", "render_ghosts", "RayTraceLensFlare/tiled_render_ghosts", shaderParametersTiled);
		}

		////////////////////////////////////////////////////////////////////////////////
		void initGPUBuffers(Scene::Scene& scene, Scene::Object* object)
		{
			// Uniform buffers
			Scene::createGPUBuffer(scene, "TiledLensFlareTracedGhostParams", GL_SHADER_STORAGE_BUFFER, false, true, GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_1,
				GL_DYNAMIC_STORAGE_BIT, 0, sizeof(Uniforms::GhostParams));
			Scene::createGPUBuffer(scene, "TiledLensFlareLens", GL_UNIFORM_BUFFER, false, true, GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_2);
			Scene::createGPUBuffer(scene, "TiledLensFlareCommon", GL_UNIFORM_BUFFER, false, true, GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_3);
			Scene::createGPUBuffer(scene, "TiledLensFlareCommonPrecompute", GL_UNIFORM_BUFFER, false, true, GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_4);
			Scene::createGPUBuffer(scene, "TiledLensFlareCommonDirect", GL_UNIFORM_BUFFER, false, true, GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_4);
			Scene::createGPUBuffer(scene, "TiledLensFlareCommonTiled", GL_UNIFORM_BUFFER, false, true, GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_4);

			// Common ray tracing buffers
			//    List of polynomial weights (FullFit)
			Scene::createGPUBuffer(scene, "TiledLensFlarePolynomialWeightsBufferFullFit", GL_SHADER_STORAGE_BUFFER, true, true, GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_5,
				GL_DYNAMIC_STORAGE_BIT, 0, sizeof(Uniforms::GhostPolynomialMonomialFull));

			//    List of partial polynomial weights (FullFit)
			Scene::createGPUBuffer(scene, "TiledLensFlareBakedPolynomialWeightsBufferFullFit", GL_SHADER_STORAGE_BUFFER, true, true, GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_6,
				GL_DYNAMIC_STORAGE_BIT, 0, sizeof(Uniforms::GhostPolynomialMonomialBaked));

			//    List of polynomial weights (PartialFit)
			Scene::createGPUBuffer(scene, "TiledLensFlarePolynomialWeightsBufferPartialFit", GL_SHADER_STORAGE_BUFFER, true, true, GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_5,
				GL_DYNAMIC_STORAGE_BIT, 0, sizeof(Uniforms::GhostPolynomialMonomialFull));

			//    List of partial polynomial weights (PartialFit)
			Scene::createGPUBuffer(scene, "TiledLensFlareBakedPolynomialWeightsBufferPartialFit", GL_SHADER_STORAGE_BUFFER, true, true, GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_6,
				GL_DYNAMIC_STORAGE_BIT, 0, sizeof(Uniforms::GhostPolynomialMonomialBaked));

			// Direct buffers
			//    List of all the rays/primitives
			Scene::createGPUBuffer(scene, "TiledLensFlareTracedRayBuffer", GL_SHADER_STORAGE_BUFFER, true, true, GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_7,
				GL_DYNAMIC_STORAGE_BIT, 0, sizeof(Uniforms::GhostEntryDirectTracedRay));

			// Precompute buffers
			//    Resulting ghost geometry
			Scene::createGPUBuffer(scene, "TiledLensFlarePrecomputeGeometry", GL_SHADER_STORAGE_BUFFER, true, true,
				GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_4, GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT,
				0, sizeof(GhostGeometry::GeometryEntryGPU));

			// Tiled buffers
			//    Dispatch data
			Scene::createGPUBuffer(scene, "TiledLensFlareDispatchBuffer", GL_SHADER_STORAGE_BUFFER, true, true, GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_7,
				GL_DYNAMIC_STORAGE_BIT, 4 * sizeof(GLuint), sizeof(GLuint));

			//    Global properties
			Scene::createGPUBuffer(scene, "TiledLensFlareGlobalPropertiesBuffer", GL_SHADER_STORAGE_BUFFER, true, true, GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_8,
				GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT, sizeof(Uniforms::GhostGlobalProperties), sizeof(Uniforms::GhostGlobalProperties));

			//    List of all the rays/primitives
			Scene::createGPUBuffer(scene, "TiledLensFlareTracedPrimitiveBuffer", GL_SHADER_STORAGE_BUFFER, true, true, GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_9,
				GL_DYNAMIC_STORAGE_BIT, 0, sizeof(Uniforms::GhostEntryTiledTracedPrimitive));

			//    List of properties for each primitive
			Scene::createGPUBuffer(scene, "TiledLensFlareTracedPrimitivePropertiesBuffer", GL_SHADER_STORAGE_BUFFER, true, true, GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_10,
				GL_DYNAMIC_STORAGE_BIT, 0, sizeof(glm::vec4));

			//   List of global primitive indices inside each coarse tile
			Scene::createGPUBuffer(scene, "TiledLensFlareCoarseTileBuffer", GL_SHADER_STORAGE_BUFFER, true, true, GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_11,
				GL_DYNAMIC_STORAGE_BIT, 0, sizeof(Uniforms::GhostEntryTile));

			//    List of properties for each coarse tile
			Scene::createGPUBuffer(scene, "TiledLensFlareCoarseTilePropertiesBuffer", GL_SHADER_STORAGE_BUFFER, true, true, GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_12,
				GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT, 0, sizeof(Uniforms::GhostEntryTileProperties));

			//    List of global primitive indices inside each tile
			Scene::createGPUBuffer(scene, "TiledLensFlareTileBuffer", GL_SHADER_STORAGE_BUFFER, true, true, GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_13,
				GL_DYNAMIC_STORAGE_BIT, 0, sizeof(Uniforms::GhostEntryTile));

			//    List of properties for each tile
			Scene::createGPUBuffer(scene, "TiledLensFlareTilePropertiesBuffer", GL_SHADER_STORAGE_BUFFER, true, true, GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_14,
				GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT, 0, sizeof(Uniforms::GhostEntryTileProperties));
		}

		////////////////////////////////////////////////////////////////////////////////
		void updateGPUBuffers(Scene::Scene& scene, Scene::Object* object)
		{
			glm::vec2 numTiles2D = Common::computeNumTiles(scene, object);
			glm::vec2 numCoarseTiles2D = Common::computeNumCoarseTiles(scene, object);

			const size_t numGhosts = PhysicalCamera::getNumGhosts(object->component<TiledLensFlareComponent>().m_camera);
			const size_t numChannels = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_numWavelengths;

			const size_t numTiles = numTiles2D.x * numTiles2D.y;
			const size_t numCoarseTiles = numCoarseTiles2D.x * numCoarseTiles2D.y;
			const size_t maxNumRays = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_rayCount;
			const size_t maxQuads = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_maxQuads;
			const size_t maxTileEntries = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_maxTileEntries;
			const size_t maxCoarseTileEntries = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_maxCoarseTileEntries;
			const size_t numPolynomialWeightsFull = PolynomialsFull::getNumWeightsAllGhosts(scene, object);
			const size_t numPolynomialWeightsFullBaked = PolynomialsFull::getNumWeightsAllGhostsBaked(scene, object);
			const size_t numPolynomialWeightsPartial = PolynomialsPartial::getNumWeightsAllGhosts(scene, object);
			const size_t numPolynomialWeightsPartialBaked = PolynomialsPartial::getNumWeightsAllGhostsBaked(scene, object);

			// Direct buffers
			//    List of all the rays
			Scene::resizeGPUBuffer(scene, "TiledLensFlareTracedRayBuffer", maxQuads * 4 * sizeof(Uniforms::GhostEntryDirectTracedRay));

			// Tiled buffers
			//    Parameters for each individual ghost
			Scene::resizeGPUBuffer(scene, "TiledLensFlareTracedGhostParams", numGhosts * numChannels * sizeof(Uniforms::GhostParams));

			//    List of all the primitives
			Scene::resizeGPUBuffer(scene, "TiledLensFlareTracedPrimitiveBuffer", maxQuads * sizeof(Uniforms::GhostEntryTiledTracedPrimitive));

			//    List of properties for each primitive
			Scene::resizeGPUBuffer(scene, "TiledLensFlareTracedPrimitivePropertiesBuffer", maxQuads * sizeof(glm::vec4));

			//   List of global primitive indices inside each coarse tile
			Scene::resizeGPUBuffer(scene, "TiledLensFlareCoarseTileBuffer", numCoarseTiles * maxCoarseTileEntries * sizeof(Uniforms::GhostEntryTile));

			//    List of properties for each coarse tile
			Scene::resizeGPUBuffer(scene, "TiledLensFlareCoarseTilePropertiesBuffer", numCoarseTiles * sizeof(Uniforms::GhostEntryTileProperties));

			//    List of global primitive indices inside each tile
			Scene::resizeGPUBuffer(scene, "TiledLensFlareTileBuffer", numTiles * maxTileEntries * sizeof(Uniforms::GhostEntryTile));

			//    List of properties for each tile
			Scene::resizeGPUBuffer(scene, "TiledLensFlareTilePropertiesBuffer", numTiles * sizeof(Uniforms::GhostEntryTileProperties));

			//    List of polynomial weights
			Scene::resizeGPUBuffer(scene, "TiledLensFlarePolynomialWeightsBufferFullFit", numPolynomialWeightsFull * sizeof(Uniforms::GhostPolynomialMonomialFull));

			//    List of partial polynomial weights
			Scene::resizeGPUBuffer(scene, "TiledLensFlareBakedPolynomialWeightsBufferFullFit", numPolynomialWeightsFullBaked * sizeof(Uniforms::GhostPolynomialMonomialBaked));


			//    List of polynomial weights
			Scene::resizeGPUBuffer(scene, "TiledLensFlarePolynomialWeightsBufferPartialFit", numPolynomialWeightsPartial * sizeof(Uniforms::GhostPolynomialMonomialFull));

			//    List of partial polynomial weights
			Scene::resizeGPUBuffer(scene, "TiledLensFlareBakedPolynomialWeightsBufferPartialFit", numPolynomialWeightsPartialBaked * sizeof(Uniforms::GhostPolynomialMonomialBaked));

			// Display memory usage statistics
			Debug::log_debug() << "Max quads buffer storage: " <<
				Units::bytesToString(numGhosts * numChannels * (maxNumRays - 1) * (maxNumRays - 1) * sizeof(Uniforms::GhostEntryTiledTracedPrimitive))
				<< Debug::end;
		}

		////////////////////////////////////////////////////////////////////////////////
		void initCameraPatents(Scene::Scene& scene, Scene::Object* object = nullptr)
		{
			auto& presets = object->component<TiledLensFlareComponent>().m_cameraPresets;
			auto& camera = object->component<TiledLensFlareComponent>().m_camera;

			presets = PhysicalCamera::initCameraPatents(scene);

			if (presets.find(camera.m_name) != presets.end())
				camera = presets[camera.m_name];
		}

		////////////////////////////////////////////////////////////////////////////////
		void initGhostAttribs(Scene::Scene& scene, Scene::Object* object = nullptr)
		{
			// Get the root filesystem path
			std::filesystem::path root = EnginePaths::generatedFilesFolder() / "TiledLensFlare" / "GhostAttribs";

			// Make sure the path exists
			if (std::filesystem::exists(root) == false) return;

			// Go through each lens patent
			for (auto const& ghostAttribs : std::filesystem::directory_iterator(root))
			{
				// Make sure its a lens patent xml
				if (ghostAttribs.path().extension() != ".dat")
					continue;

				GhostAttribs::loadSingleGhostAttribsFile(scene, object, ghostAttribs.path().string());
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		void uploadPolynomialWeightsFullFit(Scene::Scene& scene, Scene::Object* object = nullptr)
		{
			PolynomialsFull::Serialization::uploadGhostWeights(scene, object);
		}

		////////////////////////////////////////////////////////////////////////////////
		void uploadPolynomialWeightsPartialFit(Scene::Scene& scene, Scene::Object* object = nullptr)
		{
			PolynomialsPartial::Serialization::uploadGhostWeights(scene, object);
		}

		////////////////////////////////////////////////////////////////////////////////
		void setDynamicGhostIntensityScale(Scene::Scene& scene, Scene::Object* object = nullptr)
		{
			GhostAttribs::setDynamicIntensityScale(scene, object);
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	void initObject(Scene::Scene& scene, Scene::Object& object)
	{
		Scene::appendResourceInitializer(scene, object.m_name, Scene::Texture, InitResources::initTextures, "Textures");
		Scene::appendResourceInitializer(scene, object.m_name, Scene::Shader, InitResources::initShaders, "Shaders");
		Scene::appendResourceInitializer(scene, object.m_name, Scene::GenericBuffer, InitResources::initGPUBuffers, "Generic GPU Buffers");
		Scene::appendResourceInitializer(scene, object.m_name, Scene::Custom, InitResources::initCameraPatents, "Camera Patents");
		Scene::appendResourceInitializer(scene, object.m_name, Scene::Custom, InitResources::initGhostAttribs, "Ghost Attributes");

		Common::computeBlurKernels(scene, &object);

		DelayedJobs::postJob(scene, &object, "Update Buffers", false, 1, [](Scene::Scene& scene, Scene::Object& object)
		{
			InitResources::updateGPUBuffers(scene, &object);
		});

		if (object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod == RenderGhostsParameters::RaytraceMethod::PolynomialFullFit)
		{
			DelayedJobs::postJob(scene, &object, "Init Polynomial Ghost Weights", [](Scene::Scene& scene, Scene::Object& object)
			{
				InitResources::uploadPolynomialWeightsFullFit(scene, &object);
			});
		}

		if (object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod == RenderGhostsParameters::RaytraceMethod::PolynomialPartialFit)
		{
			DelayedJobs::postJob(scene, &object, "Init Polynomial Ghost Weights", [](Scene::Scene& scene, Scene::Object& object)
			{
				InitResources::uploadPolynomialWeightsPartialFit(scene, &object);
			});
		}

		DelayedJobs::postJob(scene, &object, "Set Dynamic Intensity scale", false, 2, [](Scene::Scene& scene, Scene::Object& object)
		{
			InitResources::setDynamicGhostIntensityScale(scene, &object);
		});
	}

	////////////////////////////////////////////////////////////////////////////////
	void releaseObject(Scene::Scene& scene, Scene::Object& object)
	{

	}

	////////////////////////////////////////////////////////////////////////////////
	void updateObject(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* object)
	{
		// Extract the camera
		auto& camera = object->component<TiledLensFlareComponent>().m_camera;

		// Keep the refraction indices updated
		PhysicalCamera::computeWavelengths(camera);
		PhysicalCamera::propagateCoatingInformation(camera);

		// Compute and store the ghost indices
		object->component<TiledLensFlareComponent>().m_ghostIndices = PhysicalCamera::enumerateGhosts(camera);

		// Compute the ray transfer matrices
		if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_ghostAttribsMethod == RenderGhostsParameters::GhostAttribsMethod::ParaxialGhostAttribs)
			TransferMatrices::computeRaytraceLensFlareTransferMatrices(scene, object);
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateGui(Scene::Scene& scene, Scene::Object* guiSettings, Scene::Object* object)
	{
		size_t numGhosts = object->component<TiledLensFlareComponent>().m_ghostIndices.size();

		if (ImGui::BeginTabBar(object->m_name.c_str()) == false)
			return;

		// Restore the selected tab id
		std::string activeTab;
		if (auto activeTabSynced = EditorSettings::consumeEditorProperty<std::string>(scene, object, "MainTabBar_SelectedTab#Synced"); activeTabSynced.has_value())
			activeTab = activeTabSynced.value();

		bool bufferParamsChanged = false;
		bool shaderChanged = false;
		bool cameraChanged = false;
		bool precomputeParamsChanged = false;
		bool polynomialParamsChanged = false;

		// Camera settings
		if (ImGui::BeginTabItem("Camera", activeTab.c_str()))
		{
			// Lenses
			auto [descriptionChanged, systemChanged, lensesChanged] = PhysicalCamera::generateGui(scene, guiSettings, object,
				object->component<TiledLensFlareComponent>().m_cameraPresets,
				object->component<TiledLensFlareComponent>().m_camera);
			cameraChanged |= descriptionChanged;

			EditorSettings::editorProperty<std::string>(scene, object, "MainTabBar_SelectedTab") = ImGui::CurrentTabItemName();
			ImGui::EndTabItem();
		}

		// Common settings
		if (ImGui::BeginTabItem("Common", activeTab.c_str()))
		{
			ImGui::Combo("Pupil Size Method", &object->component<TiledLensFlareComponent>().m_commonParameters.m_physicalPupilSizeMethod,
				CommonParameters::PhysicalPupilSizeMethod_meta);
			if (ImGui::TreeNode("Ignored Ghosts"))
			{
				auto& ignoredGhosts = object->component<TiledLensFlareComponent>().m_commonParameters.m_ignoredGhosts[Common::getCamera(scene, object).m_name];
				for (size_t i = 0; i < ignoredGhosts.size(); ++i)
				{
					std::string label = "Skip #" + std::to_string(i + 1);
					ImGui::SliderInt(label.c_str(), &ignoredGhosts[i], 0, numGhosts - 1);
				}
				if (ImGui::Button("Add"))
				{
					ignoredGhosts.push_back(0);
				}
				ImGui::TreePop();
			}
			EditorSettings::editorProperty<std::string>(scene, object, "MainTabBar_SelectedTab") = ImGui::CurrentTabItemName();
			ImGui::EndTabItem();
		}

		// Render settings
		if (ImGui::BeginTabItem("Render", activeTab.c_str()))
		{
			ImGui::TextDisabled("Properties");

			ImGui::Combo("Ghost Attribs", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_ghostAttribsMethod,
				RenderGhostsParameters::GhostAttribsMethod_meta);
			if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_ghostAttribsMethod == RenderGhostsParameters::GhostAttribsMethod::ParaxialGhostAttribs)
			{
				ImGui::Combo("Paraxial Ghost Bounding Size", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_paraxialBoundsSizeMethod,
					RenderGhostsParameters::ParaxialBoundsSizeMethod_meta);
				ImGui::SliderFloat("Paraxial Bounds Slack", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_paraxialBoundsSlack, 0.0f, 4.0f);
			}

			ImGui::Combo("Intensity Scale Method", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_intensityMethod,
				RenderGhostsParameters::IntensityMethod_meta);
			ImGui::Combo("Render Method", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_renderMethod,
				RenderGhostsParameters::RenderMethod_meta);
			shaderChanged |= ImGui::Combo("Raytrace Method", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod,
				RenderGhostsParameters::RaytraceMethod_meta);
			if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_renderMethod == RenderGhostsParameters::RenderMethod::Direct)
			{
				ImGui::Combo("Grid Method", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_gridMethod,
					RenderGhostsParameters::GridMethod_meta);
			}
			ImGui::Combo("Projection Method", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_projectionMethod,
				RenderGhostsParameters::ProjectionMethod_meta);
			ImGui::Combo("Shading Method", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_shadingMethod,
				RenderGhostsParameters::ShadingMethod_meta);
			ImGui::Combo("Overlay Method", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_overlayMethod,
				RenderGhostsParameters::OverlayMethod_meta);
			ImGui::Combo("Ray Count Method", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_rayCountMethod,
				RenderGhostsParameters::RayCountMethod_meta);

			if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_rayCountMethod == RenderGhostsParameters::DynamicSensorSizeBased)
			{
				ImGui::SliderFloat("Ray Count Reduction Scale", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_rayCountReductionScale, 0.0f, 4.0f);
				ImGui::SliderFloat("Ray Count Reduction Power", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_rayCountReductionPower, 0.0f, 4.0f);
			}

			ImGui::Checkbox("Lock Angle", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_lockAngle);
			ImGui::SameLine();
			ImGui::Checkbox("Lock Rotation", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_lockRotation);
			ImGui::SliderAngle("Max Angle", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_maxAngle, 0.0f, 90.0f);

			if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_lockAngle)
				ImGui::DragFloatAngle("Fixed Angle", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_fixedAngle, 0.01f, 0.0f, glm::degrees(object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_maxAngle));

			if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_lockRotation)
				ImGui::DragFloatAngle("Fixed Rotation", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_fixedRotation, 0.01f, 0.0f, 360.0f);

			ImGui::Separator();
			ImGui::TextDisabled("Ghosts");

			ImGui::SliderInt("First Ghost", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_firstGhost, 0, numGhosts - 1);
			ImGui::SliderInt("Number of Ghosts", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_numGhost, 0, numGhosts);
			ImGui::SliderInt("Number of Wavelengths", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_numWavelengths, 1, object->component<TiledLensFlareComponent>().m_camera.m_maxNumWavelengths); bufferParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
			if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_intensityMethod == RenderGhostsParameters::IntensityMethod::FixedIntensity)
			{
				ImGui::SliderFloat("Intensity Scale", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_intensityScale, 0.0f, 100000.0f);
			}
			else if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_intensityMethod == RenderGhostsParameters::IntensityMethod::DynamicIntensity)
			{
				precomputeParamsChanged |= ImGui::SliderFloat("Target Intensity", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_targetIntensity, 0.0f, 1.0f);
			}
			ImGui::SliderFloat("Radius Clipping", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_radiusClipping, 0.0f, 1.0f);
			ImGui::SliderFloat("Iris Clipping", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_irisClipping, 0.0f, 2.0f);
			ImGui::SliderFloat("Intensity Clipping", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_intensityClipping, 0.0f, 1.0f, "%.10f");
			ImGui::SliderFloat("Size Clipping", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_sizeClipping, 0.0f, 1.0f);
			ImGui::SliderAngle("Refraction Clipping", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_refractionClipping, 0.0f, 180.0f);

			ImGui::Checkbox("Clip Sensor", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_clipSensor);
			ImGui::SameLine();
			ImGui::Checkbox("Clip Rays", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_clipRays);
			ImGui::SameLine();
			ImGui::Checkbox("Clip Pixels", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_clipPixels);

			ImGui::Separator();
			ImGui::TextDisabled("Rays");

			ImGui::SliderInt("Ray Count", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_rayCount, 5, 512); bufferParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
			ImGui::SliderInt("Quad Merge Steps", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_numQuadMergeSteps, 0, 
				int(glm::log2(float(object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_traceRaysGroupSize))));
			ImGui::SliderFloat("Quad Merge Edge Threshold", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_quadMergeEdgeThreshold, 0.0f, 0.01f);

			ImGui::Separator();

			if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_renderMethod == RenderGhostsParameters::RenderMethod::Tiled)
			{
				ImGui::TextDisabled("Tiling");

				ImGui::SliderInt("Tile Size", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_tileSize, 8, 32); shaderChanged |= ImGui::IsItemDeactivatedAfterChange(); bufferParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
				ImGui::SliderInt("Max Quads", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_maxQuads, 1, 1000000); bufferParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
				ImGui::SliderInt("Max Tile Entries", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_maxTileEntries, 1, 1000000); bufferParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
				ImGui::SliderInt("Coarse Tile Size", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_coarseTileSize, 64, 256); shaderChanged |= ImGui::IsItemDeactivatedAfterChange(); bufferParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
				ImGui::SliderInt("Max Coarse Tile Entries", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_maxCoarseTileEntries, 1, 1000000); bufferParamsChanged |= ImGui::IsItemDeactivatedAfterChange();

				ImGui::Separator();
			}


			ImGui::TextDisabled("Groups");
			ImGui::SliderInt("Bake Polynomials Group Size", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_bakePolynomialsGroupSize, 8,102432); shaderChanged |= ImGui::IsItemDeactivatedAfterChange();
			ImGui::SliderInt("Trace Rays Group Size", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_traceRaysGroupSize, 8, 32); shaderChanged |= ImGui::IsItemDeactivatedAfterChange();
			ImGui::SliderInt("Splat Triangles Group Size", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_splatTrianglesGroupSize, 8, 1024); shaderChanged |= ImGui::IsItemDeactivatedAfterChange();

			ImGui::Separator();
			ImGui::TextDisabled("Resolution");

			ImGui::SliderFloat("Film Stretch", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_filmStretch, 0.0f, 4.0f);

			if (ImGui::SliderFloat("Resolution Scaling", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_resolutionScaling, 0.25f, 4.0f))
			{
				DelayedJobs::postJob(scene, object, "Recreate Textures", [](Scene::Scene& scene, Scene::Object& object)
				{
					InitResources::initTextures(scene, &object);
				});
				bufferParamsChanged = true;
			}

			if (ImGui::SliderFloat("Blur Sigma", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_blurSigma, 0.0f, 16.0f))
			{
				DelayedJobs::postJob(scene, object, "Compute Blur Kernels", [](Scene::Scene& scene, Scene::Object& object)
				{
					Common::computeBlurKernels(scene, &object);
				});
			}
			ImGui::Checkbox("Wireframe", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_wireframe);
			ImGui::SameLine();
			ImGui::Checkbox("Statistics", &object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_readBackStatistics);

			EditorSettings::editorProperty<std::string>(scene, object, "MainTabBar_SelectedTab") = ImGui::CurrentTabItemName();
			ImGui::EndTabItem();
		}

		// Polynomial fit settings
		if (ImGui::BeginTabItem("Polynomial Fit", activeTab.c_str()))
		{
			ImGui::SliderInt("Ray Count", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_rayCount, 5, 1024); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
			ImGui::SliderInt("Number of Channels", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_numChannels, 1, 3); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
			ImGui::SliderInt("First Ghost", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_firstGhost, 0, numGhosts - 1); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
			ImGui::SliderInt("Number of Ghosts", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_numGhost, 1, numGhosts); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
			ImGui::SliderInt("Number of Angles", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_numAngles, 0, 181); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
			ImGui::SliderInt("Number of Rotations", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_numRotations, 0, 181); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
			ImGui::SliderAngle("Max Angle", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_maxAngle, 0.0f, 90.0f); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
			ImGui::SliderAngle("Max Rotation", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_maxRotation, 0.0f, 90.0f); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();

			ImGui::SliderFloat("Size Clipping", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_ghostClipping.m_sizeClipping, 0.0f, 10.0f); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();

			ImGui::SliderFloat("Intensity Clipping (Ghost)", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_ghostClipping.m_intensityClipping, 0.0f, 1.0f, "%.10f"); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
			polynomialParamsChanged |= ImGui::Checkbox("Clip Sensor (Ghost)", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_ghostClipping.m_clipSensor);

			ImGui::SliderFloat("Radius Clipping (Bounds)", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_boundsClipping.m_radiusClipping, 0.0f, 1.0f); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
			ImGui::SliderFloat("Iris Clipping (Bounds)", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_boundsClipping.m_irisClipping, 0.0f, 1.0f); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
			ImGui::SliderFloat("Intensity Clipping (Bounds)", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_boundsClipping.m_intensityClipping, 0.0f, 1.0f, "%.10f"); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
			ImGui::SliderAngle("Refraction Clipping (Bounds)", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_boundsClipping.m_refractionClipping, 0.0f, 180.0f); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
			ImGui::SliderFloat("Radius Clipping (Geometry)", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_geometryClipping.m_radiusClipping, 0.0f, 1.0f); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
			ImGui::SliderFloat("Iris Clipping (Geometry)", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_geometryClipping.m_irisClipping, 0.0f, 1.0f); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
			ImGui::SliderFloat("Intensity Clipping (Geometry)", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_geometryClipping.m_intensityClipping, 0.0f, 1.0f, "%.10f"); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
			ImGui::SliderAngle("Refraction Clipping (Geometry)", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_geometryClipping.m_refractionClipping, 0.0f, 180.0f); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
			ImGui::SliderFloat("Valid Sample Ratio", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_validSampleRatio, 0.0f, 1.0f); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
			polynomialParamsChanged |= ImGui::Checkbox("Clip Sensor", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_geometryClipping.m_clipSensor);
			ImGui::SameLine();
			polynomialParamsChanged |= ImGui::Checkbox("Zero Weighted (Dataset)", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_includeZeroWeightedDataset);
			ImGui::SameLine();
			polynomialParamsChanged |= ImGui::Checkbox("Zero Weighted (Fit)", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_includeZeroWeightedFit);

			polynomialParamsChanged |= ImGui::Checkbox("Share Neighbors", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_shareNeighboringGeometries);
			ImGui::SameLine();
			polynomialParamsChanged |= ImGui::Checkbox("Smoothen Data", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_smoothenData);

			ImGui::Separator();

			polynomialParamsChanged |= ImGui::Combo("Fit Method", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_fitMethod,
				PolynomialFitParameters::FitMethod_meta);
			polynomialParamsChanged |= ImGui::Combo("Dense Fit Method (Linear)", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_denseFitLinearMethod,
				PolynomialFitParameters::DenseFitLinearMethod_meta);
			polynomialParamsChanged |= ImGui::Combo("Dense Fit Method (Non-linear)", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_denseFitNonlinearMethod,
				PolynomialFitParameters::DenseFitNonlinearMethod_meta);
			polynomialParamsChanged |= ImGui::Combo("Non-linear Fit Frequency", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_nonlinearFitFrequency,
				PolynomialFitParameters::NonlinearFitFrequency_meta);
			polynomialParamsChanged |= ImGui::Combo("Partial Fit Error Collapse Method", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_partialErrorCollapseMethod,
				PolynomialFitParameters::PartialErrorCollapseMethod_meta);
			ImGui::SliderInt("Num Sparse Terms", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_numSparseTerms, 1, 1000); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange(); shaderChanged |= ImGui::IsItemDeactivatedAfterChange(); bufferParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
			ImGui::SliderInt("Max Degree", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_maxTermDegree, 1, 100);
			ImGui::DragFloat2("Coefficient Limits", glm::value_ptr(object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_coefficientLimits), 0.01f, 0.0f, 1e6f, "%.10f");
			ImGui::SliderFloat("Rotated Bounds Slack (Absolute)", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_rotatedBoundsSlackAbsolute, 0.0f, 1.0f);
			ImGui::SliderFloat("Rotated Bounds Slack (Percentage)", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_rotatedBoundsSlackPercentage, 0.0f, 1.0f);

			shaderChanged |= ImGui::Checkbox("Bake Invariants", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_bakeInvariants);
			ImGui::SameLine();
			shaderChanged |= ImGui::Checkbox("Dynamic Term Conts", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_useDynamicTermCount);
			ImGui::SameLine();
			shaderChanged |= ImGui::Checkbox("Groupshared Memory", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_useGroupSharedMemory);
			ImGui::Checkbox("Eval Entries in Parallel", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_evaluateEntriesInParallel);

			ImGui::Separator();

			if (object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_fitMethod == PolynomialFitParameters::FitMethod::SimulatedAnnealing)
			{
				ImGui::SliderInt("Num Samples", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_annealingParams.m_numSamples, 1, 10000);
				ImGui::SliderInt("Num Iterations", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_annealingParams.m_numIterations, 1, 10000);
				ImGui::SliderFloat("Swap Annealing", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_annealingParams.m_swapAnnealingRate, 1.0f, 10.0f);
				ImGui::SliderFloat("Acceptance Offset", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_annealingParams.m_acceptanceOffset, 0.0f, 10.0f);
			}
			if (object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_fitMethod == PolynomialFitParameters::FitMethod::PolynomialRegression)
			{
				ImGui::SliderInt("Num Samples", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_polynomialRegressionParams.m_numSamples, 1, 10000); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
				ImGui::SliderInt("Max Depth", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_polynomialRegressionParams.m_recursionDepth, 1, 32); polynomialParamsChanged |= ImGui::IsItemDeactivatedAfterChange();
				ImGui::SliderInt("Simplifcation Threshold", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_polynomialRegressionParams.m_simplificationThreshold, 1, 32);
				ImGui::Checkbox("Fixed Term Number", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_polynomialRegressionParams.m_fixedTermNumber);
			}

			ImGui::Combo("Debug Level", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_debugComputation,
				PolynomialFitParameters::DebugLevel_meta);

			ImGui::Separator();

			// Per-attrib properties
			auto const& attribs = std::map_keys(object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_outputVariableParams);
			for (auto const& attrib : attribs)
			{
				if (ImGui::TreeNodeEx(attrib.c_str()))
				{
					ImGui::SliderFloat("Exterior Weight", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_outputVariableParams[attrib].m_exteriorWeight, 0.0f, 1.0f, "%.8f");
					ImGui::SliderFloat("Boundary Weight", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_outputVariableParams[attrib].m_boundaryWeight, 0.0f, 1.0f, "%.8f");
					ImGui::SliderFloat("Internal Weight", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_outputVariableParams[attrib].m_interiorWeight, 0.0f, 1.0f, "%.8f");
					ImGui::SliderFloat("Boundary Penalty", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_outputVariableParams[attrib].m_boundaryPenalty, 0.0f, 1000.0f);
					if (object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_fitMethod == PolynomialFitParameters::FitMethod::SimulatedAnnealing)
					{
						ImGui::SliderFloat("Stop Error Threshold", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_annealingParams.m_outputVariableParams[attrib].m_stopErrorThreshold, 0.0f, 1.0f, "%.8f");
					}
					if (object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_fitMethod == PolynomialFitParameters::FitMethod::PolynomialRegression)
					{
						ImGui::SliderFloat("Expansion Error Threshold", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_polynomialRegressionParams.m_outputVariableParams[attrib].m_expansionErrorThreshold, 0.0f, 1.0f, "%.8f");
						ImGui::SliderFloat("Expansion Error Decrease", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_polynomialRegressionParams.m_outputVariableParams[attrib].m_expansionErrorDecrease, 0.0f, 1.0f, "%.8f");
						ImGui::SliderFloat("Simplification Error Increase", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_polynomialRegressionParams.m_outputVariableParams[attrib].m_simplificationErrorIncrease, 0.0f, 1.0f, "%.8f");
						ImGui::SliderFloat("Stop Error Threshold", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_polynomialRegressionParams.m_outputVariableParams[attrib].m_stopErrorThreshold, 0.0f, 1.0f, "%.8f");
						ImGui::SliderFloat("Stop Error Decrease Threshold", &object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_polynomialRegressionParams.m_outputVariableParams[attrib].m_stopErrorDecreaseThreshold, 0.0f, 1.0f, "%.8f");
					}
					ImGui::TreePop();
				}
			}

			ImGui::Separator();

			if (ImGui::Button("Fit Full Polynomials"))
			{
				DelayedJobs::postJob(scene, object, "Fit Full Polynomials", [](Scene::Scene& scene, Scene::Object& object)
				{
					PolynomialsFull::performFit(scene, &object);
				});
			}

			ImGui::SameLine();

			if (ImGui::Button("Fit Partial Polynomials"))
			{
				DelayedJobs::postJob(scene, object, "Fit Partial Polynomials", [](Scene::Scene& scene, Scene::Object& object)
				{
					PolynomialsPartial::performFit(scene, &object);
				});
			}

			EditorSettings::editorProperty<std::string>(scene, object, "MainTabBar_SelectedTab") = ImGui::CurrentTabItemName();
			ImGui::EndTabItem();
		}

		// Precompute settings
		if (ImGui::BeginTabItem("Precompute", activeTab.c_str()))
		{
			precomputeParamsChanged |= ImGui::SliderInt("Ray Count", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_rayCount, 5, 513);
			precomputeParamsChanged |= ImGui::SliderInt("Refinement Steps", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_refinementSteps, 0, 5);
			precomputeParamsChanged |= ImGui::SliderInt("Number of Channels", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_numChannels, 1, 3);
			precomputeParamsChanged |= ImGui::SliderInt("First Ghost", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_firstGhost, 0, numGhosts - 1);
			precomputeParamsChanged |= ImGui::SliderInt("Number of Ghosts", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_numGhost, 1, numGhosts);
			precomputeParamsChanged |= ImGui::SliderAngle("Max Angle", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_maxAngle, 0.0f, 90.0f);
			precomputeParamsChanged |= ImGui::SliderAngle("Angle Step", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_angleStep, 0.0f, 1.0f, "%.2f deg");
			precomputeParamsChanged |= ImGui::SliderFloat("Pupil Expansion", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_pupilExpansion, 0.0f, 2.0f);
			precomputeParamsChanged |= ImGui::SliderFloat("Radius Clipping", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_radiusClipping, 0.0f, 1.0f);
			precomputeParamsChanged |= ImGui::SliderFloat("Iris Clipping", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_irisClipping, 0.0f, 1.0f);
			precomputeParamsChanged |= ImGui::SliderFloat("Intensity Clipping", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_intensityClipping, 0.0f, 1.0f, "%.10f");
			precomputeParamsChanged |= ImGui::SliderAngle("Refraction Clipping", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_refractionClipping, 0.0f, 180.0f);
			
			ImGui::Checkbox("Clip Sensor", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_clipSensor);
			ImGui::SameLine();
			ImGui::Checkbox("Terminate on First Invalid", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_terminateOnFirstInvalid);
			
			precomputeParamsChanged |= ImGui::SliderFloat("Slack (Absolute)", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_slackAbsolute, 0.0f, 1.0f);
			precomputeParamsChanged |= ImGui::SliderFloat("Slack (Percentage)", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_slackPercentage, 0.0f, 1.0f);
			precomputeParamsChanged |= ImGui::SliderFloat("Slack (Rays)", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_slackRays, 0.0f, 1.0f);

			ImGui::Checkbox("Compute Ghost Attribs", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_computeGhostAttribs);
			ImGui::SameLine();
			ImGui::Checkbox("Save Ghost Attribs", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_saveGhostAttribs);

			ImGui::Checkbox("Save Full Geometry", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_saveFullGhostGeometry);
			ImGui::SameLine();
			ImGui::Checkbox("Save Valid Geometry", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_saveValidGhostGeometry);
			ImGui::SameLine();
			ImGui::Checkbox("Save Bounded Geometry", &object->component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_saveBoundedGhostGeometry);

			if (ImGui::Button("Perform Precomputation"))
			{
				DelayedJobs::postJob(scene, object, "Compute Ghost Attribs", [](Scene::Scene& scene, Scene::Object& object)
				{
					GhostAttribs::computeAllGhostAttribs(scene, &object);
				});
			}

			if (ImGui::Button("Benchmark Bounds"))
			{
				DelayedJobs::postJob(scene, object, "Compute Ghost Attribs", [](Scene::Scene& scene, Scene::Object& object)
				{
					GhostAttribs::runBenchmark(scene, &object);
				});
			}
			ImGui::SameLine();
			if (ImGui::Button("Benchmark Bounds Performance"))
			{
				DelayedJobs::postJob(scene, object, "Compute Ghost Attribs", [](Scene::Scene& scene, Scene::Object& object)
				{
					GhostAttribs::runPerformanceBenchmark(scene, &object);
				});
			}
			ImGui::SameLine();
			if (ImGui::Button("Benchmark Bounds Accuracy"))
			{
				DelayedJobs::postJob(scene, object, "Compute Ghost Attribs", [](Scene::Scene& scene, Scene::Object& object)
				{
					GhostAttribs::runAccuracyBenchmark(scene, &object);
				});
			}

			EditorSettings::editorProperty<std::string>(scene, object, "MainTabBar_SelectedTab") = ImGui::CurrentTabItemName();
			ImGui::EndTabItem();
		}

		// End the tab bar
		ImGui::EndTabBar();

		if (shaderChanged || polynomialParamsChanged)
		{
			DelayedJobs::postJob(scene, object, "Reload Shaders", [](Scene::Scene& scene, Scene::Object& object)
			{
				InitResources::initShaders(scene, &object);
			});
		}

		if (cameraChanged || bufferParamsChanged || polynomialParamsChanged)
		{
			DelayedJobs::postJob(scene, object, "Update Buffers", [](Scene::Scene& scene, Scene::Object& object)
			{
				InitResources::updateGPUBuffers(scene, &object);
			});
		}

		if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod == RenderGhostsParameters::RaytraceMethod::PolynomialFullFit &&
			(cameraChanged || bufferParamsChanged || polynomialParamsChanged || shaderChanged))
		{
			DelayedJobs::postJob(scene, object, "Upload Polynomial Weights", [](Scene::Scene& scene, Scene::Object& object)
			{
				InitResources::uploadPolynomialWeightsFullFit(scene, &object);
			});
		}

		if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod == RenderGhostsParameters::RaytraceMethod::PolynomialPartialFit &&
			(cameraChanged || bufferParamsChanged || polynomialParamsChanged || shaderChanged))
		{
			DelayedJobs::postJob(scene, object, "Upload Polynomial Weights", [](Scene::Scene& scene, Scene::Object& object)
			{
				InitResources::uploadPolynomialWeightsPartialFit(scene, &object);
			});
		}

		if (cameraChanged || precomputeParamsChanged)
		{
			DelayedJobs::postJob(scene, object, "Set Dynamic Intensity scale", [](Scene::Scene& scene, Scene::Object& object)
			{
				InitResources::setDynamicGhostIntensityScale(scene, &object);
			});
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	void blitResult(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* renderSettings, Scene::Object* camera, Scene::Object* object)
	{
		Profiler::ScopedGpuPerfCounter perfCounter(scene, "Blit Result");

		// Render and max resolutions
		const glm::ivec2 maxResolution = Common::computeMaxResolution(scene, object);
		const glm::ivec2 finalResolution = Common::computeFinalResolution(scene, object);
		const glm::ivec2 renderResolution = Common::computeRenderResolution(scene, object);

		// Set the OpenGL state
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);

		// Bind the previous luminance texture
		glActiveTexture(GPU::TextureEnums::TEXTURE_POST_PROCESS_1_ENUM);
		glBindTexture(GL_TEXTURE_2D, scene.m_textures["TiledLensFlare_Accumulation_0"].m_texture);

		// Bind the output buffer
		glBindFramebuffer(GL_FRAMEBUFFER, scene.m_gbuffer[renderSettings->component<RenderSettings::RenderSettingsComponent>().m_gbufferWrite].m_colorBuffersPerLayer[scene.m_gbuffer[renderSettings->component<RenderSettings::RenderSettingsComponent>().m_gbufferWrite].m_readBuffer][0]);
		glViewport(0, 0, finalResolution.x, finalResolution.y);

		// Bind the blur shader
		Scene::bindShader(scene, "Misc/copy_buffer");

		const glm::vec2 uvMin = glm::vec2(0);
		const glm::vec2 uvScale = glm::vec2(renderResolution) / glm::vec2(maxResolution);
		const glm::vec2 uvMax = uvScale;

		// Set the common uniforms
		glUniform2fv(0, 1, glm::value_ptr(uvScale));            // UV scale
		glUniform2fv(1, 1, glm::value_ptr(uvMin));              // min UV
		glUniform2fv(2, 1, glm::value_ptr(uvMax));              // max UV

		// Render the fullscreen quad
		RenderSettings::renderFullscreenPlaneOpenGL(scene, simulationSettings, renderSettings);
	}

	////////////////////////////////////////////////////////////////////////////////
	void blurResult(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* renderSettings, Scene::Object* camera, Scene::Object* object)
	{
		Profiler::ScopedGpuPerfCounter perfCounter(scene, "Blur Result");

		// Render and max resolutions
		const glm::ivec2 maxResolution = Common::computeMaxResolution(scene, object);
		const glm::ivec2 finalResolution = Common::computeFinalResolution(scene, object);
		const glm::ivec2 renderResolution = Common::computeRenderResolution(scene, object);

		// Blur parameters
		const float blurSigma = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_blurSigma;
		auto const& kernel = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_blurKernelLinear;
		const size_t blurDiameter = kernel.size();

		// Get the name of the blur shader
		std::string const& shaderName = "Misc/gaussian_blur_" + std::to_string(blurDiameter);

		// Load the shadow if not found
		if (scene.m_shaders.find(shaderName) == scene.m_shaders.end())
			RenderSettings::loadGaussianBlurShader(scene, ShadowMap::getBlurKernelDiameter(scene, object));

		// Bind the blur shader
		Scene::bindShader(scene, shaderName);

		const glm::vec2 uvMin = glm::vec2(0);
		const glm::vec2 uvScale = glm::vec2(renderResolution) / glm::vec2(maxResolution);
		const glm::vec2 uvMax = uvScale;
		const glm::vec2 stepSize = glm::vec2(1.0f) / glm::vec2(maxResolution);

		// Set the common uniforms
		glUniform2fv(1, 1, glm::value_ptr(uvScale));            // UV scale
		glUniform2fv(2, 1, glm::value_ptr(uvMin));              // min UV
		glUniform2fv(3, 1, glm::value_ptr(uvMax));              // max UV
		glUniform1ui(4, kernel.size());                         // kernel size
		glUniform2fv(5, kernel.size(), (float*)kernel.data());  // kernel weights

		// Horizontal pass
		{
			Profiler::ScopedGpuPerfCounter perfCounter(scene, "Horizontal");

			// Set the OGL state
			glDisable(GL_BLEND);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);

			// Bind the previous luminance texture
			glActiveTexture(GPU::TextureEnums::TEXTURE_POST_PROCESS_1_ENUM);
			glBindTexture(GL_TEXTURE_2D, scene.m_textures["TiledLensFlare_Accumulation_0"].m_texture);

			// Bind the intermediate buffer
			glBindFramebuffer(GL_FRAMEBUFFER, scene.m_textures["TiledLensFlare_Accumulation_1"].m_framebuffer);
			glViewport(0, 0, renderResolution.x, renderResolution.y);

			// Set the blur direction uniform
			glUniform2f(0, stepSize.x, 0.0f);

			// Render the fullscreen quad
			RenderSettings::renderFullscreenPlaneOpenGL(scene, simulationSettings, renderSettings);
		}

		// Vertical pass
		{
			Profiler::ScopedGpuPerfCounter perfCounter(scene, "Vertical");

			// Set the OpenGL state
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);

			// Bind the previous luminance texture
			glActiveTexture(GPU::TextureEnums::TEXTURE_POST_PROCESS_1_ENUM);
			glBindTexture(GL_TEXTURE_2D, scene.m_textures["TiledLensFlare_Accumulation_1"].m_texture);

			// Bind the output buffer
			glBindFramebuffer(GL_FRAMEBUFFER, scene.m_gbuffer[renderSettings->component<RenderSettings::RenderSettingsComponent>().m_gbufferWrite].m_colorBuffersPerLayer[scene.m_gbuffer[renderSettings->component<RenderSettings::RenderSettingsComponent>().m_gbufferWrite].m_readBuffer][0]);
			glViewport(0, 0, finalResolution.x, finalResolution.y);

			// Set the blur direction uniform
			glUniform2f(0, 0.0f, stepSize.y);

			// Render the fullscreen quad
			RenderSettings::renderFullscreenPlaneOpenGL(scene, simulationSettings, renderSettings);
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	void renderObjectOpenGLDirect(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* renderSettings, Scene::Object* camera, std::string const& functionName, Scene::Object* object)
	{
		// Render and max resolutions
		const glm::ivec2 finalResolution = Common::computeFinalResolution(scene, object);
		const glm::ivec2 renderResolution = Common::computeRenderResolution(scene, object);

		// Set the OpenGL state
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glBindVertexArray(scene.m_meshes["plane.obj"].m_vao);

		// Enable wireframe mode
		if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_wireframe)
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		// Bind the output buffer
		if (Common::needsTempTexture(scene, object))
		{
			glBindFramebuffer(GL_FRAMEBUFFER, scene.m_textures["TiledLensFlare_Accumulation_0"].m_framebuffer);
			glViewport(0, 0, renderResolution.x, renderResolution.y);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		else
		{
			// We don't need to modify the read buffer, so bind it
			RenderSettings::bindGbufferLayerOpenGL(scene, simulationSettings, renderSettings,
				RenderSettings::GB_WriteBuffer, RenderSettings::GB_ReadBuffer, 0);
			glViewport(0, 0, renderResolution.x, renderResolution.y);
		}

		// Bind the iris textures
		glActiveTexture(GPU::TextureEnums::TEXTURE_POST_PROCESS_1_ENUM);
		glBindTexture(GL_TEXTURE_2D, scene.m_textures[object->component<TiledLensFlareComponent>().m_camera.m_apertureTexture].m_texture);

		// Bind the necessary buffers
		Scene::bindBuffer(scene, "TiledLensFlareTracedGhostParams");
		Scene::bindBuffer(scene, "TiledLensFlareTracedRayBuffer");
		if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod == RenderGhostsParameters::PolynomialFullFit)
		{
			Scene::bindBuffer(scene, "TiledLensFlarePolynomialWeightsBufferFullFit");
			Scene::bindBuffer(scene, "TiledLensFlareBakedPolynomialWeightsBufferFullFit");
		}
		else if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod == RenderGhostsParameters::PolynomialPartialFit)
		{
			Scene::bindBuffer(scene, "TiledLensFlarePolynomialWeightsBufferPartialFit");
			Scene::bindBuffer(scene, "TiledLensFlareBakedPolynomialWeightsBufferPartialFit");
		}

		// Render lens flare for each light source
		std::vector<LightSources::LightSourceData> lightSources = LightSources::getLightSourceData(scene, renderSettings, camera, object);
		for (LightSources::LightSourceData const& lightData : lightSources)
		{
			Profiler::ScopedGpuPerfCounter perfCounter(scene, lightData.m_lightSource->m_name);

			//Debug::log_debug() << lightData.m_lightSource->m_name << ": "
			//	<< "angle: " << lightData.m_angle << " rad (" << glm::degrees(lightData.m_angle) << ")" << ", "
			//	<< "rotation: " << lightData.m_rotation << " rad (" << glm::degrees(lightData.m_rotation) << ")" << Debug::end;

			// Upload the common parameters
			std::vector<Uniforms::GhostParams> ghostParams = Uniforms::uploadGhostParametersRender(scene, object, lightData);
			uploadBufferData(scene, "TiledLensFlareTracedGhostParams", ghostParams);

			Uniforms::RenderGhostsLensUniforms lensFlareDataLens = Uniforms::uploadLensUniformsRender(scene, object, lightData);
			uploadBufferData(scene, "TiledLensFlareLens", lensFlareDataLens);

			Uniforms::RenderGhostsCommonUniforms lensFlareDataCommon = Uniforms::uploadCommonUniformsRender(scene, object, ghostParams);
			uploadBufferData(scene, "TiledLensFlareCommon", lensFlareDataCommon);

			Uniforms::RenderGhostsCommonUniformsDirect lensFlareDataCommonDirect = Uniforms::uploadCommonUniformsRenderDirect(scene, object);
			uploadBufferData(scene, "TiledLensFlareCommonDirect", lensFlareDataCommonDirect);

			Profiler::storeData(scene, { "No. Ghosts" }, (int)ghostParams.size());

			if ((object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod == RenderGhostsParameters::PolynomialFullFit ||
				object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod == RenderGhostsParameters::PolynomialPartialFit) &&
				object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_bakeInvariants)
			{
				Profiler::ScopedGpuPerfCounter perfCounter(scene, "Bake Polynomial Invariants");

				// Bind the compute shader determining the lens flare rectangles cutting into a single tile
				Scene::bindShader(scene, "RayTraceLensFlare/bake_polynomial_invariants");

				// Dispatch the computation
				const int groupSize = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_bakePolynomialsGroupSize;
				glm::ivec3 numWorkGroups = Common::getNumWorkGroups(glm::ivec3(groupSize - 1, 1, 1), glm::ivec3(lensFlareDataCommon.m_maxNumGhosts * lensFlareDataCommon.m_maxNumChannels, 1, 1));
				glDispatchCompute(numWorkGroups.x, numWorkGroups.y, numWorkGroups.z);

				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			}
			
			if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_gridMethod == RenderGhostsParameters::AdaptiveGrid)
			{
				Profiler::ScopedGpuPerfCounter perfCounter(scene, "Grid");

				// Bind the grid computation shader
				Scene::bindShader(scene, "RayTraceLensFlare/compute_grid");

				// Dispatch the computation
				const int maxRayCount = lensFlareDataCommon.m_maxRayGridSize;
				glm::ivec3 numWorkGroups = Common::getNumWorkGroups(glm::ivec3(8, 8, 1), glm::ivec3(maxRayCount, maxRayCount, ghostParams.size()));
				glDispatchCompute(numWorkGroups.x, numWorkGroups.y, numWorkGroups.z);

				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			}

			// Render the ghosts themselves
			{
				Profiler::ScopedGpuPerfCounter perfCounter(scene, "Ghosts");

				// Bind the ghost rendering shader
				Scene::bindShader(scene, "RayTraceLensFlare/render_ghosts");

				for (size_t ghostID = 0; ghostID < ghostParams.size(); ++ghostID)
				{
					Profiler::ScopedGpuPerfCounter perfCounter(scene, "Ghost #" + std::to_string(ghostID));

					// Set the ghost parameter ID
					glUniform1i(0, ghostID);

					// Render the tessellated quad
					int rayCount = ghostParams[ghostID].m_rayCount;
					int vertexCount = (rayCount - 1) * (rayCount - 1) * 6;
					glDrawArrays(GL_TRIANGLES, 0, vertexCount);
				}
			}
		}

		// Reset the OpenGL state
		glBindVertexArray(0);

		// Disable wireframe mode
		if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_wireframe)
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		// Resolve the temporary texture
		if (Common::needsTempTexture(scene, object))
		{
			if (Common::needsBlurring(scene, object))
				blurResult(scene, simulationSettings, renderSettings, camera, object);
			else
				blitResult(scene, simulationSettings, renderSettings, camera, object);
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	void renderObjectOpenGLTiled(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* renderSettings, Scene::Object* camera, std::string const& functionName, Scene::Object* object)
	{
		// Render and max resolutions
		const glm::ivec2 finalResolution = Common::computeFinalResolution(scene, object);
		const glm::ivec2 renderResolution = Common::computeRenderResolution(scene, object);

		// Bind the iris textures
		glActiveTexture(GPU::TextureEnums::TEXTURE_POST_PROCESS_1_ENUM);
		glBindTexture(GL_TEXTURE_2D, scene.m_textures[object->component<TiledLensFlareComponent>().m_camera.m_apertureTexture].m_texture);

		// Bind the original color texture
		glActiveTexture(GPU::TextureEnums::TEXTURE_ALBEDO_MAP_ENUM);
		glBindTexture(GL_TEXTURE_2D_ARRAY, scene.m_gbuffer[renderSettings->component<RenderSettings::RenderSettingsComponent>().m_gbufferWrite].m_colorTextures[scene.m_gbuffer[renderSettings->component<RenderSettings::RenderSettingsComponent>().m_gbufferWrite].m_readBuffer]);

		// Bind the output buffer
		if (Common::needsTempTexture(scene, object))
		{
			glBindFramebuffer(GL_FRAMEBUFFER, scene.m_textures["TiledLensFlare_Accumulation_0"].m_framebuffer);
			glClear(GL_COLOR_BUFFER_BIT);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			glBindImageTexture(0, scene.m_textures["TiledLensFlare_Accumulation_0"].m_texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		}
		else
		{
			glBindImageTexture(0, scene.m_gbuffer[renderSettings->component<RenderSettings::RenderSettingsComponent>().m_gbufferWrite].m_colorTextures[scene.m_gbuffer[renderSettings->component<RenderSettings::RenderSettingsComponent>().m_gbufferWrite].m_writeBuffer], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		}

		// Binding the necessary GPU buffers
		Scene::bindBuffer(scene, "TiledLensFlareTracedGhostParams");
		Scene::bindBuffer(scene, "TiledLensFlareTracedPrimitiveBuffer");
		Scene::bindBuffer(scene, "TiledLensFlareTracedPrimitivePropertiesBuffer");
		Scene::bindBuffer(scene, "TiledLensFlareTileBuffer");
		Scene::bindBuffer(scene, "TiledLensFlareTilePropertiesBuffer");
		Scene::bindBuffer(scene, "TiledLensFlareGlobalPropertiesBuffer");
		Scene::bindBuffer(scene, "TiledLensFlareDispatchBuffer");
		Scene::bindBuffer(scene, "TiledLensFlareCoarseTileBuffer");
		Scene::bindBuffer(scene, "TiledLensFlareCoarseTilePropertiesBuffer");
		if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod == RenderGhostsParameters::PolynomialFullFit)
		{
			Scene::bindBuffer(scene, "TiledLensFlarePolynomialWeightsBufferFullFit");
			Scene::bindBuffer(scene, "TiledLensFlareBakedPolynomialWeightsBufferFullFit");
		}
		if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod == RenderGhostsParameters::PolynomialPartialFit)
		{
			Scene::bindBuffer(scene, "TiledLensFlarePolynomialWeightsBufferPartialFit");
			Scene::bindBuffer(scene, "TiledLensFlareBakedPolynomialWeightsBufferPartialFit");
		}

		// Clear the tile parameters buffer
		glm::uvec4 emptyTileParams = glm::uvec4(0);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, scene.m_genericBuffers["TiledLensFlareTilePropertiesBuffer"].m_buffer);
		glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_RGBA32UI, GL_RGBA, GL_UNSIGNED_INT, glm::value_ptr(emptyTileParams));

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, scene.m_genericBuffers["TiledLensFlareCoarseTilePropertiesBuffer"].m_buffer);
		glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_RGBA32UI, GL_RGBA, GL_UNSIGNED_INT, glm::value_ptr(emptyTileParams));

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, scene.m_genericBuffers["TiledLensFlareGlobalPropertiesBuffer"].m_buffer);
		glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED, GL_UNSIGNED_INT, glm::value_ptr(emptyTileParams));

		// Bind the indirect buffer and clear it
		glm::uvec4 emptyDispatch = glm::uvec4(0);
		glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, scene.m_genericBuffers["TiledLensFlareDispatchBuffer"].m_buffer);
		glClearBufferData(GL_DISPATCH_INDIRECT_BUFFER, GL_RGBA32UI, GL_RGBA, GL_UNSIGNED_INT, glm::value_ptr(emptyDispatch));

		// Place a memory barrier for the image read operation
		glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

		// Render lens flare for each light source
		std::vector<LightSources::LightSourceData> lightSources = LightSources::getLightSourceData(scene, renderSettings, camera, object);
		for (LightSources::LightSourceData const& lightData : lightSources)
		{
			Profiler::ScopedGpuPerfCounter perfCounter(scene, lightData.m_lightSource->m_name);

			//Debug::log_debug() << lightData.m_lightSource->m_name << ": "
			//	<< "angle: " << lightData.m_angle << " rad (" << glm::degrees(lightData.m_angle) << ")" << ", "
			//	<< "rotation: " << lightData.m_rotation << " rad (" << glm::degrees(lightData.m_rotation) << ")" << Debug::end;

			// Upload the common parameters
			std::vector<Uniforms::GhostParams> ghostParams = Uniforms::uploadGhostParametersRender(scene, object, lightData);
			uploadBufferData(scene, "TiledLensFlareTracedGhostParams", ghostParams);

			Uniforms::RenderGhostsLensUniforms lensFlareDataLens = Uniforms::uploadLensUniformsRender(scene, object, lightData);
			uploadBufferData(scene, "TiledLensFlareLens", lensFlareDataLens);

			Uniforms::RenderGhostsCommonUniforms lensFlareDataCommon = Uniforms::uploadCommonUniformsRender(scene, object, ghostParams);
			uploadBufferData(scene, "TiledLensFlareCommon", lensFlareDataCommon);


			Uniforms::RenderGhostsCommonUniformsTiled lensFlareDataCommonTiled = Uniforms::uploadCommonUniformsRenderTiled(scene, object);
			uploadBufferData(scene, "TiledLensFlareCommonTiled", lensFlareDataCommonTiled);

			if ((object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod == RenderGhostsParameters::PolynomialFullFit ||
				object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod == RenderGhostsParameters::PolynomialPartialFit) &&
				object->component<TiledLensFlareComponent>().m_polynomialFitParameters.m_bakeInvariants)
			{
				Profiler::ScopedGpuPerfCounter perfCounter(scene, "Bake Polynomial Invariants");

				// Bind the compute shader determining the lens flare rectangles cutting into a single tile
				Scene::bindShader(scene, "RayTraceLensFlare/bake_polynomial_invariants");

				// Dispatch the computation
				const int groupSize = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_bakePolynomialsGroupSize;
				glm::ivec3 numWorkGroups = Common::getNumWorkGroups(glm::ivec3(groupSize - 1, 1, 1), glm::ivec3(lensFlareDataCommon.m_maxNumGhosts * lensFlareDataCommon.m_maxNumChannels, 1, 1));
				glDispatchCompute(numWorkGroups.x, numWorkGroups.y, numWorkGroups.z);

				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			}

			{
				Profiler::ScopedGpuPerfCounter perfCounter(scene, "Trace Rays");

				// Bind the compute shader determining the lens flare rectangles cutting into a single tile
				Scene::bindShader(scene, "RayTraceLensFlare/tiled_trace_rays");

				// Dispatch the computation
				const int maxRayCount = lensFlareDataCommon.m_maxRayGridSize;
				const int groupSize = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_traceRaysGroupSize;
				glm::ivec3 numWorkGroups = Common::getNumWorkGroups(glm::ivec3(groupSize - 1, groupSize - 1, 1), glm::ivec3(maxRayCount, maxRayCount, ghostParams.size()));
				glDispatchCompute(numWorkGroups.x, numWorkGroups.y, numWorkGroups.z);

				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			}

			Profiler::storeData(scene, { "No. Ghosts" }, (int)ghostParams.size());
		}

		{
			Profiler::ScopedGpuPerfCounter perfCounter(scene, "Build Tiles Command");

			// Bind the compute shader building the quads of the lens flares
			Scene::bindShader(scene, "RayTraceLensFlare/tiled_build_tiles_command");

			// Dispatch the computation
			glDispatchCompute(1, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
		}

		{
			Profiler::ScopedGpuPerfCounter perfCounter(scene, "Build Tiles");

			// Bind the compute shader building the quads of the lens flares
			Scene::bindShader(scene, "RayTraceLensFlare/tiled_build_tiles");

			// Dispatch the computation
			glDispatchComputeIndirect(0);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}

		{
			Profiler::ScopedGpuPerfCounter perfCounter(scene, "Composite Ghosts");

			// Bind the compute shader building the quads of the lens flares
			Scene::bindShader(scene, "RayTraceLensFlare/tiled_render_ghosts");

			// Dispatch the computation
			const int groupSize = object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_tileSize;
			glm::ivec3 numWorkGroups = Common::getNumWorkGroups(glm::ivec3(groupSize, groupSize, 1), glm::ivec3(renderResolution.x, renderResolution.y, 1));
			glDispatchCompute(numWorkGroups.x, numWorkGroups.y, numWorkGroups.z);
			glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		}

		// Resolve the temporary texture
		if (Common::needsTempTexture(scene, object))
		{
			if (Common::needsBlurring(scene, object))
			{
				blurResult(scene, simulationSettings, renderSettings, camera, object);
			}
			else
			{
				blitResult(scene, simulationSettings, renderSettings, camera, object);
			}
		}
		else
		{
			RenderSettings::swapGbufferBuffers(scene, simulationSettings, renderSettings);
		}

		// Read back statistics
		if (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_readBackStatistics)
		{
			const int numCoarseTiles = Common::computeNumCoarseTiles1D(scene, object);
			const int numDenseTiles = Common::computeNumTiles1D(scene, object);

			// Global properties
			glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, scene.m_genericBuffers["TiledLensFlareGlobalPropertiesBuffer"].m_buffer);
			Uniforms::GhostGlobalProperties* globalData = (Uniforms::GhostGlobalProperties*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
			Profiler::storeData(scene, "Number of Primitives", (float)globalData->m_numPrimitives);
			glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

			// Coarse per-tile properties
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, scene.m_genericBuffers["TiledLensFlareCoarseTilePropertiesBuffer"].m_buffer);
			Uniforms::GhostEntryTileProperties* coarsePerTileData = (Uniforms::GhostEntryTileProperties*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
			int minCoarsePrimitives = INT_MAX, maxCoarsePrimitives = 0, totalCoarsePrimitives = 0;
			for (size_t i = 0; i < numCoarseTiles; ++i)
			{
				minCoarsePrimitives = std::min(minCoarsePrimitives, coarsePerTileData[i].m_numPrimitives);
				maxCoarsePrimitives = std::max(maxCoarsePrimitives, coarsePerTileData[i].m_numPrimitives);
				totalCoarsePrimitives += coarsePerTileData[i].m_numPrimitives;
			}
			Profiler::storeData(scene, "[Coarse] Tot Primitives", (float)(totalCoarsePrimitives));
			Profiler::storeData(scene, "[Coarse] Min Primitives", (float)minCoarsePrimitives);
			Profiler::storeData(scene, "[Coarse] Max Primitives", (float)maxCoarsePrimitives);
			Profiler::storeData(scene, "[Coarse] Avg Primitives", (float)(totalCoarsePrimitives) / float(numCoarseTiles));
			glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

			// Dense per-tile properties
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, scene.m_genericBuffers["TiledLensFlareTilePropertiesBuffer"].m_buffer);
			Uniforms::GhostEntryTileProperties* densePerTileData = (Uniforms::GhostEntryTileProperties*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
			int minDensePrimitives = INT_MAX, maxDensePrimitives = 0, totalDensePrimitives = 0;
			for (size_t i = 0; i < numDenseTiles; ++i)
			{
				minDensePrimitives = std::min(minDensePrimitives, densePerTileData[i].m_numPrimitives);
				maxDensePrimitives = std::max(maxDensePrimitives, densePerTileData[i].m_numPrimitives);
				totalDensePrimitives += densePerTileData[i].m_numPrimitives;
			}
			Profiler::storeData(scene, "[Dense] Tot Primitives", (float)(totalDensePrimitives));
			Profiler::storeData(scene, "[Dense] Min Primitives", (float)minDensePrimitives);
			Profiler::storeData(scene, "[Dense] Max Primitives", (float)maxDensePrimitives);
			Profiler::storeData(scene, "[Dense] Avg Primitives", (float)(totalDensePrimitives) / float(numDenseTiles));
			glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	void renderObjectOpenGL(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* renderSettings, Scene::Object* camera, std::string const& functionName, Scene::Object* object)
	{
		switch (object->component<TiledLensFlareComponent>().m_renderGhostsParameters.m_renderMethod)
		{
		case RenderGhostsParameters::Direct:
			renderObjectOpenGLDirect(scene, simulationSettings, renderSettings, camera, functionName, object);
			break;
		case RenderGhostsParameters::Tiled:
			renderObjectOpenGLTiled(scene, simulationSettings, renderSettings, camera, functionName, object);
			break;
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	void demoSetup(Scene::Scene& scene)
	{
		// @CONSOLE_VAR(Object Groups, -object_group, LensFlare_TiledLensFlare)
		SimulationSettings::createGroup(scene, "LensFlare", "LensFlare_TiledLensFlare", true);

		// Add the tiled lens flare object.
		auto& tiledLensFlare = createObject(scene, Scene::OBJECT_TYPE_TILED_LENS_FLARE,
			Scene::extendDefaultObjectInitializerBefore([&](Scene::Scene& scene, Scene::Object& object)
		{
				object.m_enabled = true;
				object.m_groups = SimulationSettings::makeGroupFlags(scene, "LensFlare_TiledLensFlare");

				// Common settings
				object.component<TiledLensFlareComponent>().m_commonParameters.m_physicalPupilSizeMethod = CommonParameters::PhysicalPupilSizeMethod::ParaxialTracedPupil;

				// Camera
				object.component<TiledLensFlareComponent>().m_camera.m_name = "heliar-tronnier";

				// Precompute parameters
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_rayCount = 1024;
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_refinementSteps = 0;
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_numChannels = 3;
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_firstGhost = 0;
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_numGhost = 999;
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_maxAngle = glm::radians(60.0f);
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_angleStep = glm::radians(0.5f);
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_pupilExpansion = 1.0f;
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_radiusClipping = 1.01f;
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_irisClipping = 0.951f;
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_intensityClipping = 0.0f;
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_refractionClipping = glm::radians(180.0f);
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_slackAbsolute = 0.0f;
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_slackPercentage = 0.0f;
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_slackRays = 0.0f;
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_terminateOnFirstInvalid = true;
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_clipSensor = true;
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_computeGhostAttribs = true;
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_saveGhostAttribs = true;
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_saveFullGhostGeometry = false;
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_saveValidGhostGeometry = false;
				object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_saveBoundedGhostGeometry = false;

				// Render parameters
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_ghostAttribsMethod = RenderGhostsParameters::GhostAttribsMethod::RayTracedGhostAttribs;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_projectionMethod = RenderGhostsParameters::ProjectionMethod::SensorGrid;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_shadingMethod = RenderGhostsParameters::ShadingMethod::Shaded;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_overlayMethod = RenderGhostsParameters::OverlayMethod::DisableOverlay;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_gridMethod = RenderGhostsParameters::FixedGrid;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_renderMethod = RenderGhostsParameters::RenderMethod::Tiled;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_raytraceMethod = RenderGhostsParameters::PolynomialPartialFit;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_intensityMethod = RenderGhostsParameters::IntensityMethod::DynamicIntensity;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_targetIntensity = 1.0f;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_paraxialBoundsSlack = 0.10f;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_maxAngle = object.component<TiledLensFlareComponent>().m_precomputeGhostsParameters.m_maxAngle;

				// Ray grid settings
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_rayCountMethod = RenderGhostsParameters::RayCountMethod::FixedCount;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_rayCount = 128;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_rayCountReductionScale = 1.0f;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_rayCountReductionPower = 0.5f;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_numQuadMergeSteps = 4;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_quadMergeEdgeThreshold = 0.0005;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_clipRays = true;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_clipPixels = true;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_wireframe = false;

				// Ghost settings
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_firstGhost = 0;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_numGhost = 999;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_numWavelengths = 3;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_intensityScale = 50.0f;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_radiusClipping = 1.0f;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_irisClipping = 0.95f;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_intensityClipping = 1e-4f;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_sizeClipping = 0.0f;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_refractionClipping = glm::radians(180.0f);
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_clipSensor = true;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_filmStretch = 1.0f;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_resolutionScaling = 1.0f;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_lockAngle = false;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_lockRotation = false;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_fixedAngle = glm::radians(0.0f);
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_fixedRotation = glm::radians(0.0f);

				// Tile sizes and other parameters
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_tileSize = 8;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_coarseTileSize = 128;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_bakePolynomialsGroupSize = 128;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_traceRaysGroupSize = 17;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_splatTrianglesGroupSize = 128;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_maxQuads = 20000000;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_maxCoarseTileEntries = 5000000;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_maxTileEntries = 30000;
				object.component<TiledLensFlareComponent>().m_renderGhostsParameters.m_readBackStatistics = false;

				// Polynomial fit parameters
				//  - rendering
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_rotatedBoundsSlackAbsolute = 0.02f;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_rotatedBoundsSlackPercentage = 0.0f;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_bakeInvariants = true;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_useDynamicTermCount = true;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_useGroupSharedMemory = true;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_evaluateEntriesInParallel = false;
				//  - ghost geometry
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_rayCount = 80;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_numChannels = 3;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_firstGhost = 0;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_numGhost = 999;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_numAngles = 241;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_numRotations = 1;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_maxAngle = glm::radians(60.0f);
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_maxRotation = glm::radians(360.0f);
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_ghostClipping.m_intensityClipping = 0.0f; // 1e-4f;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_ghostClipping.m_sizeClipping = 0.0f;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_ghostClipping.m_clipSensor = true;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_boundsClipping.m_radiusClipping = 1.01f;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_boundsClipping.m_irisClipping = 100.0f;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_boundsClipping.m_intensityClipping = 0.0f; // 1e-8f;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_boundsClipping.m_refractionClipping = glm::radians(180.0f);
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_boundsClipping.m_clipSensor = true;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_geometryClipping.m_radiusClipping = 1.01f;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_geometryClipping.m_irisClipping = 0.951f;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_geometryClipping.m_intensityClipping = 0.0f; // 1e-8f;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_geometryClipping.m_refractionClipping = glm::radians(180.0f);
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_geometryClipping.m_clipSensor = true;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_includeZeroWeightedDataset = true;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_includeZeroWeightedFit = true;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_validSampleRatio = 0.5f;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_smoothenData = false;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_ghostGeometryParameters.m_shareNeighboringGeometries = false;
				//  - common fitting
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_fitMethod = PolynomialFitParameters::FitMethod::PolynomialRegression;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_denseFitLinearMethod = PolynomialFitParameters::DenseFitLinearMethod::LDLT;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_denseFitNonlinearMethod = PolynomialFitParameters::DenseFitNonlinearMethod::Nadam;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_nonlinearFitFrequency = PolynomialFitParameters::NonlinearFitFrequency::Never;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_partialErrorCollapseMethod = PolynomialFitParameters::PartialErrorCollapseMethod::PCM_RMSE;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_debugComputation = PolynomialFitParameters::DebugLevel::NoDebug;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_numSparseTerms = 5;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_maxTermDegree = 6;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_coefficientLimits = glm::vec2(1e-6f, 1e4f);
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_outputVariableParams =
				{
					//                    internal, boundary, exterior, boundaryPenalty
					{ "aperture_pos_x", { 1.0f,     1.0f,     0.0f,     0.0f  } },
					{ "aperture_pos_y", { 1.0f,     1.0f,     0.0f,     0.0f  } },
					{ "sensor_pos_x",   { 1.0f,     1.0f,     0.0f,     0.0f  } },
					{ "sensor_pos_y",   { 1.0f,     1.0f,     0.0f,     0.0f  } },
					{ "radius",         { 1.0f,     1.0f,     0.0f,     0.0f } },
					{ "clip_factor",    { 0.0f,     1.0f,     0.0f,     0.0f } },
					{ "intensity",      { 1.0f,     1.0f,     0.0f,     0.0f } },
				};
				//  - polynomial regression
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_polynomialRegressionParams.m_numSamples = 3200;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_polynomialRegressionParams.m_recursionDepth = 5;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_polynomialRegressionParams.m_fixedTermNumber = false;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_polynomialRegressionParams.m_simplificationThreshold = 1;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_polynomialRegressionParams.m_outputVariableParams =
				{
					//                    expErrThr, expErrDec, simplErrInc, stopErrDecThr, stopErrThr
					{ "aperture_pos_x", { 0.0f,      1e-4f,     -5e-5f,      5e-5f,          2e-4f } },
					{ "aperture_pos_y", { 0.0f,      1e-4f,     -5e-5f,      5e-5f,          2e-4f } },
					{ "sensor_pos_x",   { 0.0f,      1e-4f,     -5e-5f,      5e-5f,          2e-4f } },
					{ "sensor_pos_y",   { 0.0f,      1e-4f,     -5e-5f,      5e-5f,          2e-4f } },
					{ "radius",         { 0.0f,      1e-5f,     -5e-6f,      5e-6f,          1e-5f } },
					{ "clip_factor",    { 0.0f,      2e-5f,     -1e-5f,      5e-6f,          5e-6f } },
					{ "intensity",      { 0.0f,      2e-7f,     -1e-7f,      1e-8f,          1e-8f } },
				};
				//  - simulated annealing
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_annealingParams.m_numSamples = 3200;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_annealingParams.m_numIterations = 10000;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_annealingParams.m_swapAnnealingRate = 4.0f;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_annealingParams.m_acceptanceOffset = 1.5f;
				object.component<TiledLensFlareComponent>().m_polynomialFitParameters.m_annealingParams.m_outputVariableParams =
				{
					//                    stopErrThr
					{ "sensor_pos_x",   { 2e-5f } },
					{ "sensor_pos_y",   { 2e-5f } },
					{ "aperture_pos_x", { 2e-5f } },
					{ "aperture_pos_y", { 2e-5f } },
					{ "radius",         { 1e-5f } },
					{ "clip_factor",    { 1e-5f } },
					{ "intensity",      { 1e-8f } },
				};
		}));
	}
}