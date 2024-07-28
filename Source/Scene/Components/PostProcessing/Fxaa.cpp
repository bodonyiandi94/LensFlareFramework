#include "PCH.h"
#include "Fxaa.h"

namespace Fxaa
{
	////////////////////////////////////////////////////////////////////////////////
	// Define the component
	DEFINE_COMPONENT(FXAA);
	DEFINE_OBJECT(FXAA);
	REGISTER_OBJECT_RENDER_CALLBACK(FXAA, "FXAA [Post-Lighting]", OpenGL, AFTER, "Effects (LDR) [Begin]", 99, &Fxaa::renderObjectOpenGL, &RenderSettings::firstCallTypeCondition, &Fxaa::renderObjectPreconditionPostLightingOpenGL, nullptr, nullptr);
	REGISTER_OBJECT_RENDER_CALLBACK(FXAA, "FXAA [Post-Effects]", OpenGL, BEFORE, "Effects (LDR) [End]", 99, &Fxaa::renderObjectOpenGL, &RenderSettings::firstCallTypeCondition, &Fxaa::renderObjectPreconditionPostPostprocessingOpenGL, nullptr, nullptr);

	////////////////////////////////////////////////////////////////////////////////
	void initShaders(Scene::Scene& scene, Scene::Object* = nullptr)
	{
		Asset::loadShader(scene, "PostProcessing/FXAA", "fxaa", "FXAA/fxaa");
	}

	////////////////////////////////////////////////////////////////////////////////
	void initGPUBuffers(Scene::Scene& scene, Scene::Object* = nullptr)
	{
		Scene::createGPUBuffer(scene, "FXAA", GL_UNIFORM_BUFFER, false, true, GPU::UniformBufferIndices::UNIFORM_BUFFER_GENERIC_1);
	}

	////////////////////////////////////////////////////////////////////////////////
	void initObject(Scene::Scene& scene, Scene::Object& object)
	{
		Scene::appendResourceInitializer(scene, object.m_name, Scene::Shader, initShaders, "Shaders");
		Scene::appendResourceInitializer(scene, object.m_name, Scene::GenericBuffer, initGPUBuffers, "Generic GPU Buffers");
	}

	////////////////////////////////////////////////////////////////////////////////
	void releaseObject(Scene::Scene& scene, Scene::Object& object)
	{

	}

	////////////////////////////////////////////////////////////////////////////////
	void generateGui(Scene::Scene& scene, Scene::Object* guiSettings, Scene::Object* object)
	{
		ImGui::Combo("Domain", &object->component<Fxaa::FxaaComponent>().m_domain, Fxaa::FxaaComponent::ComputationDomain_meta);
		ImGui::SliderFloat("Minium Direction Reduction", &object->component<Fxaa::FxaaComponent>().m_dirReduceMin, 0.0f, 0.01f);
		ImGui::SliderFloat("Direction Reduction Multiplier", &object->component<Fxaa::FxaaComponent>().m_dirReduceMultiplier, 0.0f, 0.5f);
		ImGui::SliderFloat("Max Blur Width", &object->component<Fxaa::FxaaComponent>().m_maxBlur, 0.0f, 8.0f);
	}

	////////////////////////////////////////////////////////////////////////////////
	bool renderObjectPreconditionPostLightingOpenGL(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* renderSettings, Scene::Object* camera, std::string const& functionName, Scene::Object* object)
	{
		return object->component<Fxaa::FxaaComponent>().m_domain == Fxaa::FxaaComponent::AfterLighting && 
			RenderSettings::firstCallObjectCondition(scene, simulationSettings, renderSettings, camera, functionName, object);
	}

	////////////////////////////////////////////////////////////////////////////////
	bool renderObjectPreconditionPostPostprocessingOpenGL(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* renderSettings, Scene::Object* camera, std::string const& functionName, Scene::Object* object)
	{
		return object->component<Fxaa::FxaaComponent>().m_domain == Fxaa::FxaaComponent::AfterPostprocessing &&
			RenderSettings::firstCallObjectCondition(scene, simulationSettings, renderSettings, camera, functionName, object);
	}

	////////////////////////////////////////////////////////////////////////////////
	void renderObjectOpenGL(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* renderSettings, Scene::Object* camera, std::string const& functionName, Scene::Object* object)
	{
		// Set the OpenGL state
		glDisable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);

		// Upload the fxaa parameters
		Fxaa::UniformData fxaaData;
		fxaaData.m_dirReduceMin = object->component<Fxaa::FxaaComponent>().m_dirReduceMin;
		fxaaData.m_dirReduceMultiplier = object->component<Fxaa::FxaaComponent>().m_dirReduceMultiplier;
		fxaaData.m_maxBlur = object->component<Fxaa::FxaaComponent>().m_maxBlur;
		uploadBufferData(scene, "FXAA", fxaaData);

		// Bind the new buffer
		RenderSettings::bindGbufferLayersOpenGL(scene, simulationSettings, renderSettings);
		RenderSettings::setupViewportArrayOpenGL(scene, simulationSettings, renderSettings);
		Scene::bindShader(scene, "FXAA", "fxaa");

		// Bind the scene texture
		glActiveTexture(GPU::TextureEnums::TEXTURE_ALBEDO_MAP_ENUM);
		glBindTexture(GL_TEXTURE_2D_ARRAY, scene.m_gbuffer[renderSettings->component<RenderSettings::RenderSettingsComponent>().m_gbufferWrite].m_colorTextures[scene.m_gbuffer[renderSettings->component<RenderSettings::RenderSettingsComponent>().m_gbufferWrite].m_readBuffer]);

		// Render the fullscreen quad
		RenderSettings::renderFullscreenPlaneOpenGL(scene, simulationSettings, renderSettings);

		// Swap read buffers
		RenderSettings::swapGbufferBuffers(scene, simulationSettings, renderSettings);
	}
}