#include "PCH.h"
#include "GuiSettings.h"
#include "DelayedJobs.h"
#include "InputSettings.h"
#include "SimulationSettings.h"
#include "RenderSettings.h"

namespace GuiSettings
{
	////////////////////////////////////////////////////////////////////////////////
	// Define the component
	DEFINE_COMPONENT(GUI_SETTINGS);
	DEFINE_OBJECT(GUI_SETTINGS);
	REGISTER_OBJECT_UPDATE_CALLBACK(GUI_SETTINGS, BEFORE, DELAYED_JOBS);
	REGISTER_OBJECT_RENDER_CALLBACK(GUI_SETTINGS, "GUI", OpenGL, AFTER, "GUI [Begin]", 1, &GuiSettings::renderObjectOpenGL, &RenderSettings::firstCallTypeCondition, &RenderSettings::firstCallObjectCondition, nullptr, nullptr);

	////////////////////////////////////////////////////////////////////////////////
	void generateGuiMenuBar(Scene::Scene& scene, Scene::Object* guiSettings);
	void generateObjectsWindow(Scene::Scene& scene, Scene::Object* guiSettings);
	void generateStatsWindow(Scene::Scene& scene, Scene::Object* guiSettings);
	void generateLogWindow(Scene::Scene& scene, Scene::Object* guiSettings);
	void generateProfilerWindow(Scene::Scene& scene, Scene::Object* guiSettings);
	void generateMaterialEditorWindow(Scene::Scene& scene, Scene::Object* guiSettings);
	void generateShaderInspectorWindow(Scene::Scene& scene, Scene::Object* guiSettings);
	void generateBufferInspectorWindow(Scene::Scene& scene, Scene::Object* guiSettings);
	void generateTextureInspectorWindow(Scene::Scene& scene, Scene::Object* guiSettings);
	void generateMainRenderWindow(Scene::Scene& scene, Scene::Object* guiSettings);

	////////////////////////////////////////////////////////////////////////////////
	void restoreGuiState(Scene::Scene& scene, Scene::Object* guiSettings);

	////////////////////////////////////////////////////////////////////////////////
	void initFramebuffers(Scene::Scene& scene, Scene::Object* = nullptr)
	{
		// Max supported resolution
		auto resolution = Config::AttribValue("max_resolution").get<glm::ivec2>();

		Scene::createTexture(scene, "ImGui_MainRenderTarget", GL_TEXTURE_2D, resolution[0], resolution[1], 1, GL_RGBA8, GL_RGBA, GL_LINEAR, GL_LINEAR);
	}

	////////////////////////////////////////////////////////////////////////////////
	void initShaders(Scene::Scene& scene, Scene::Object* = nullptr)
	{	
		Asset::loadShader(scene, "Imgui", "imgui");
	}

	////////////////////////////////////////////////////////////////////////////////
	void initTextures(Scene::Scene& scene, Scene::Object* = nullptr)
	{
		Asset::loadTexture(scene, "Textures/GUI/trash_black.png", "Textures/GUI/trash_black.png");
		Asset::loadTexture(scene, "Textures/GUI/trash_white.png", "Textures/GUI/trash_white.png");
		Asset::loadTexture(scene, "Textures/GUI/x_black.png", "Textures/GUI/x_black.png");
		Asset::loadTexture(scene, "Textures/GUI/x_white.png", "Textures/GUI/x_white.png");
		Asset::loadTexture(scene, "Textures/GUI/list_black.png", "Textures/GUI/list_black.png");
		Asset::loadTexture(scene, "Textures/GUI/list_white.png", "Textures/GUI/list_white.png");
		Asset::loadTexture(scene, "Textures/GUI/checkmark-512.png", "Textures/GUI/checkmark-512.png");
		Asset::loadTexture(scene, "Textures/GUI/x-mark-512.png", "Textures/GUI/x-mark-512.png");
	}

	////////////////////////////////////////////////////////////////////////////////
	void initFonts(Scene::Scene& scene, Scene::Object* = nullptr)
	{
		// Font sizes
		const size_t textFontSize = 32;
		const size_t iconFontSize = 32;

		// External fonts
		Asset::loadExternalFont(scene, "Arial", "arialbd.ttf", textFontSize);
		Asset::loadExternalFont(scene, "Segoe UI", "segoeuib.ttf", textFontSize);
		Asset::loadExternalFont(scene, "Roboto", "Fonts/roboto-medium.ttf", textFontSize);
		Asset::loadExternalFont(scene, "Ruda", "Fonts/Ruda-Bold.ttf", textFontSize);
		Asset::loadExternalFont(scene, "Open Sans", "Fonts/OpenSans-Bold.ttf", textFontSize);
		Asset::loadExternalFont(scene, "Open Sans", "Fonts/OpenSans-Bold.ttf", textFontSize);
		Asset::loadExternalFont(scene, "FontAwesome", "Fonts/fontawesome-webfont.ttf", iconFontSize);
		Asset::loadExternalFont(scene, "MaterialIcons", "Fonts/materialdesignicons-webfont.ttf", iconFontSize);
	}

	////////////////////////////////////////////////////////////////////////////////
	void uploadFontAtlas(Scene::Scene& scene, Scene::Object* object)
	{
		Debug::log_debug() << "Uploading imgui fonts" << Debug::end;

		if (scene.m_textures.find("ImguiFont") != scene.m_textures.end())
		{
			glDeleteTextures(1, &scene.m_textures["ImguiFont"].m_texture);
		}

		// Extract the default imgui font
		int width, height;
		unsigned char* pixels;
		ImGuiIO& io{ ImGui::GetIO() };
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

		// Upload the texture data
		Scene::createTexture(scene, "ImguiFont", GL_TEXTURE_2D, width, height, 1,
			GL_RGBA8, GL_RGBA, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE,
			GL_UNSIGNED_BYTE, pixels, GL_TEXTURE0);

		// Also store it in the global ImGui context
		io.Fonts->TexID = &(scene.m_textures["ImguiFont"].m_texture);

		Debug::log_debug() << "Successfully uploaded imgui fonts" << Debug::end;
	}

	////////////////////////////////////////////////////////////////////////////////
	void initObject(Scene::Scene& scene, Scene::Object& object)
	{
		// Access the render settings option
		Scene::Object* renderSettings = filterObjects(scene, Scene::OBJECT_TYPE_RENDER_SETTINGS)[0];

		// Gui persistent state file name
		object.component<GuiSettings::GuiSettingsComponent>().m_guiStateFileName = (EnginePaths::configFilesFolder() / "gui_state.ini").string();

		// Find out the larger monitor dimension
		glm::ivec2 monitorRes = Context::getMonitorResolution(scene.m_context);
		const float maxDim = float(glm::max(monitorRes.x, monitorRes.y));

		// DPI scale factor (using 4K as reference)
		object.component<GuiSettings::GuiSettingsComponent>().m_dpiScale = glm::sqrt(maxDim / 3840.0f);

		// Initialize imgui.
		ImGuiIO& io = ImGui::GetIO();

		// Set some global settings
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		// Set the config file path
		static const std::string s_iniFileName = (EnginePaths::configFilesFolder() / "imgui.ini").string();
		io.IniFilename = s_iniFileName.c_str();

		// Set up the keymap
		io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
		io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
		io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
		io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
		io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
		io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
		io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
		io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
		io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
		io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
		io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
		io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
		io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
		io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
		io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
		io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
		io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
		io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
		io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

		// Null the draw list function (we call draw ourself)
		//io.RenderDrawListsFn = nullptr;

		// Store the display size
		io.DisplaySize = ImVec2(renderSettings->component<RenderSettings::RenderSettingsComponent>().m_resolution.x, renderSettings->component<RenderSettings::RenderSettingsComponent>().m_resolution.y);
		io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

		// Determine the font scaling
		if (object.component<GuiSettings::GuiSettingsComponent>().m_fontScale == 0.0f)
		{
			object.component<GuiSettings::GuiSettingsComponent>().m_fontScale = object.component<GuiSettings::GuiSettingsComponent>().m_dpiScale * 0.71f;
		}
		
		io.FontGlobalScale = object.component<GuiSettings::GuiSettingsComponent>().m_fontScale;

		// Generate the custom styles
		for (int i = 0; i < ImGui::GetNumStyles(); ++i)
		{
			ImGuiStyle style;
			const char* name = ImGui::StyleColorsById(i, &style, object.component<GuiSettings::GuiSettingsComponent>().m_dpiScale);
			scene.m_guiStyles[name] = style;
		}

		// Store the filter names
		object.component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_includeFilter.m_validComponents = Debug::InMemoryLogEntry::s_componentNames;
		object.component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_discardFilter.m_validComponents = Debug::InMemoryLogEntry::s_componentNames;

		// Register the available windows as open
		object.component<GuiSettings::GuiSettingsComponent>().m_guiElements =
		{
			GuiElement{ "Menu Bar", &generateGuiMenuBar, true },
			GuiElement{ "Object Editor", &generateObjectsWindow, true },
			GuiElement{ "Stats Window", &generateStatsWindow, true },
			GuiElement{ "Log Output", &generateLogWindow, true },
			GuiElement{ "Profiler", &generateProfilerWindow, true },
			GuiElement{ "Material Editor", &generateMaterialEditorWindow, true },
			GuiElement{ "Shader Inspector", &generateShaderInspectorWindow, true },
			GuiElement{ "Buffer Inspector", &generateBufferInspectorWindow, true },
			GuiElement{ "Texture Inspector", &generateTextureInspectorWindow, true },
			GuiElement{ "Render Output", &generateMainRenderWindow, true },
		};

		// Enable the currently active style
		ImGui::GetStyle() = scene.m_guiStyles[object.component<GuiSettings::GuiSettingsComponent>().m_guiStyle];

		// Append the necessary resource initializers
		Scene::appendResourceInitializer(scene, object.m_name, Scene::Font, initFonts, "Fonts");
		Scene::appendResourceInitializer(scene, object.m_name, Scene::Texture, initTextures, "Textures");
		Scene::appendResourceInitializer(scene, object.m_name, Scene::Texture, initFramebuffers, "FrameBuffers");
		Scene::appendResourceInitializer(scene, object.m_name, Scene::Shader, initShaders, "OpenGL Shaders");

		// Upload the GUI fonts
		DelayedJobs::postJob(scene, &object, "Upload GUI Fonts", [](Scene::Scene& scene, Scene::Object& object)
		{
			uploadFontAtlas(scene, &object);
		});

		// Restore the gui state
		if (object.component<GuiSettings::GuiSettingsComponent>().m_saveGuiState)
		{
			DelayedJobs::postJob(scene, &object, "Restore Gui State", [](Scene::Scene& scene, Scene::Object& object)
			{
				restoreGuiState(scene, &object);
			});
		}

		// Resize the cached state vector
		object.component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_cachedMessageState.resize(Debug::logger_impl::error_full_inmemory_buffer.m_bufferLength);
	}

	////////////////////////////////////////////////////////////////////////////////
	void releaseObject(Scene::Scene& scene, Scene::Object& object)
	{

	}

	////////////////////////////////////////////////////////////////////////////////
	void handleInput(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* input, Scene::Object* object)
	{
		// Turn the gui
		if (input->component<InputSettings::InputComponent>().m_keys[GLFW_KEY_F11] == 1)
		{
			object->component<GuiSettings::GuiSettingsComponent>().m_showGui = !object->component<GuiSettings::GuiSettingsComponent>().m_showGui;
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateGui(Scene::Scene& scene, Scene::Object* guiSettings, Scene::Object* object)
	{
		ImGuiIO& io{ ImGui::GetIO() };

		if (ImGui::Combo("Style", object->component<GuiSettings::GuiSettingsComponent>().m_guiStyle, scene.m_guiStyles))
		{
			ImGui::GetStyle() = scene.m_guiStyles[object->component<GuiSettings::GuiSettingsComponent>().m_guiStyle];
		}
		ImGui::Combo("Font", object->component<GuiSettings::GuiSettingsComponent>().m_font, scene.m_fonts);

		ImGui::SliderFloat("Font Scale", &object->component<GuiSettings::GuiSettingsComponent>().m_fontScale, 0.0f, 4.0f);
		io.FontGlobalScale = object->component<GuiSettings::GuiSettingsComponent>().m_fontScale;

		ImGui::SliderFloat2("Main Render Blit Constraints", glm::value_ptr(object->component<GuiSettings::GuiSettingsComponent>().m_blitAspectConstraint), 1.0f, 2.0f);

		if (object->component<GuiSettings::GuiSettingsComponent>().m_saveGuiState)
		{
			ImGui::SliderFloat("Gui State Save Interval", &object->component<GuiSettings::GuiSettingsComponent>().m_saveGuiStateInterval, 0.0f, 60.0f);
		}

		ImGui::Checkbox("Show While No Input", &object->component<GuiSettings::GuiSettingsComponent>().m_showGuiNoInput);
		ImGui::SameLine();
		ImGui::Checkbox("Fixed Aspect Ratio", &object->component<GuiSettings::GuiSettingsComponent>().m_blitFixedAspectRatio);
		ImGui::SameLine();
		ImGui::Checkbox("Lock Layout", &object->component<GuiSettings::GuiSettingsComponent>().m_lockLayout);
		
		ImGui::Checkbox("Group Objects By Type", &object->component<GuiSettings::GuiSettingsComponent>().m_groupObjects);
		ImGui::SameLine();
		ImGui::Checkbox("Hide Disabled Groups", &object->component<GuiSettings::GuiSettingsComponent>().m_hideDisabledGroups);
		ImGui::SameLine();
		ImGui::Checkbox("Persistent Gui State", &object->component<GuiSettings::GuiSettingsComponent>().m_saveGuiState);

		if (ImGui::TreeNode("GUI Elements"))
		{
			ImGui::TreePop();

			for (auto& element : object->component<GuiSettings::GuiSettingsComponent>().m_guiElements)
				ImGui::Checkbox(element.m_name.c_str(), &element.m_open);
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	/** Gui object header */
	void generateGuiObjectGroupEditor(Scene::Scene& scene, Scene::Object* object)
	{
		// Early out
		if (ImGui::BeginPopup("ObjectGroupEditor") == false) return;
		
		// Extract the group settings object
		auto groupSettings = Scene::findFirstObject(scene, Scene::OBJECT_TYPE_SIMULATION_SETTINGS);

		for (auto& group : groupSettings->component<SimulationSettings::SimulationSettingsComponent>().m_groups)
		{
			bool wasInGroup = SimulationSettings::isObjectInGroup(scene, object, group), inGroup = wasInGroup;
			if (ImGui::Checkbox(group.m_name.c_str(), &inGroup))
			{
				if (wasInGroup)
				{
					SimulationSettings::removeObjectFromGroup(scene, object, group);
				}
				else
				{
					SimulationSettings::addObjectToGroup(scene, object, group);
				}
			}
		}

		ImGui::EndPopup();
	}

	////////////////////////////////////////////////////////////////////////////////
	bool generateObjectHeader(Scene::Scene& scene, Scene::Object* guiSettings, Scene::Object* object)
	{
		// Whether we should keep the object or not
		bool deleteObject = false, showGroupsEditor = false;

		const char* label = object->m_name.c_str();
		bool* results[] = { &deleteObject, &showGroupsEditor, &object->m_enabled };
		const int numControls = object->component<EditorSettings::EditorSettingsComponent>().m_allowDisable ? ARRAYSIZE(results) : 0;
		ImTextureID buttonTextures[] = { &scene.m_textures["Textures/GUI/x_white.png"].m_texture, &scene.m_textures["Textures/GUI/list_white.png"].m_texture, nullptr };
		const char* buttonLabels[] = { nullptr, nullptr, nullptr };
		//ImTextureID buttonTextures[] = { nullptr, nullptr, nullptr };
		//const char* buttonLabels[] = { ICON_IGFD_CANCEL, ICON_IGFD_LIST, nullptr };
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_AllowItemOverlap; 
		if (object->component<EditorSettings::EditorSettingsComponent>().m_headerOpen)
			flags |= ImGuiTreeNodeFlags_DefaultOpen;

		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return false;

		ImGuiID id = window->GetID(label);
		bool is_open = ImGui::TreeNodeBehavior(id, flags, label);

		// Context object
		ImGuiContext& context = *GImGui;

		// Hover data backup
		ImGuiLastItemDataBackup last_item_backup;

		// Extract the previous cursor pos and padding
		ImVec2 prev_cursor_pos = ImGui::GetCursorScreenPos();
		float prev_padding = context.Style.FramePadding.y;

		// Rect of the collapsing header
		ImRect header_rect = window->DC.LastItemRect;

		// Size of a label
		ImVec2 empty_label_size = ImGui::CalcTextSize("", NULL, true);

		// Set the new padding size
		context.Style.FramePadding.y = -context.Style.FramePadding.y * 0.5f;

		// Push colors for the close button
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, context.Style.Colors[ImGuiCol_FrameBgActive]);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, context.Style.Colors[ImGuiCol_FrameBgHovered]);
		ImGui::PushStyleColor(ImGuiCol_Button, context.Style.Colors[ImGuiCol_FrameBg]);

		// Current draw pos (x of the lower left corner and y of the center
		ImVec2 drawPos = ImVec2(header_rect.Max.x, header_rect.GetCenter().y);

		// Create header controls
		for (int i = 0; i < numControls; ++i)
		{
			// Create a regular button, with text
			if (buttonLabels[i] != nullptr)
			{
				const ImVec2 label_size = ImGui::CalcTextSize(buttonLabels[i], NULL, true);

				ImVec2 button_size = ImVec2(label_size.x + context.Style.FramePadding.x * 2, label_size.y + context.Style.FramePadding.y * 2);
				ImVec2 button_pos = ImVec2(ImMin(drawPos.x, window->ClipRect.Max.x) - context.Style.FramePadding.x - button_size.x, drawPos.y - button_size.y / 2);
				ImGui::SetCursorScreenPos(button_pos);
				*results[i] = ImGui::Button(buttonLabels[i]);
			}

			// Create a textured button
			else if (buttonTextures[i] != nullptr)
			{
				ImVec2 button_size = ImVec2(empty_label_size.y + context.Style.FramePadding.y * 2, empty_label_size.y + context.Style.FramePadding.y * 2);
				ImVec2 button_pos = ImVec2(ImMin(drawPos.x, window->ClipRect.Max.x) - context.Style.FramePadding.x - button_size.x, drawPos.y - button_size.y / 2);
				ImGui::SetCursorScreenPos(button_pos);
				*results[i] = ImGui::ImageButton(buttonTextures[i], button_size, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f), 0.0f, ImVec4(0, 0, 0, 0), context.Style.Colors[ImGuiCol_CheckMark]);
			}

			// Create a checkbox
			else
			{
				// Create the enable checkbox
				ImVec2 button_size = ImVec2(empty_label_size.y + context.Style.FramePadding.y * 2, empty_label_size.y + context.Style.FramePadding.y * 2);
				ImVec2 button_pos = ImVec2(ImMin(drawPos.x, window->ClipRect.Max.x) - context.Style.FramePadding.x - button_size.y, drawPos.y - button_size.y / 2);
				ImGui::SetCursorScreenPos(button_pos);
				ImGui::Checkbox("", results[i]);
			}

			// Update the draw position
			drawPos.x = window->DC.LastItemRect.Min.x;
		}

		// Pop the close button colors
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();

		// Reset the cursor position and padding
		ImGui::SetCursorScreenPos(prev_cursor_pos);
		context.Style.FramePadding.y = prev_padding;

		// Restore hover data
		last_item_backup.Restore();

		// Store the header open flag
		object->component<EditorSettings::EditorSettingsComponent>().m_headerOpen = is_open;

		// Remove the object if it is no longer needed
		if (deleteObject) removeObject(scene, *object);

		// Open the group editor
		if (showGroupsEditor) ImGui::OpenPopup("ObjectGroupEditor");
		
		return !deleteObject;
	}

	////////////////////////////////////////////////////////////////////////////////
	/** Gui object header */
	bool generateGuiObjectHeader(Scene::Scene& scene, Scene::Object* guiSettings, Scene::Object* object)
	{
		// Generate the object header
		if (!generateObjectHeader(scene, guiSettings, object)) return false;

		// Generate the inner contents
		if (object->component<EditorSettings::EditorSettingsComponent>().m_headerOpen)
		{
			// Name of the object
			//std::string newName = object->m_name;
			//if (ImGui::InputText("Object Name", newName, ImGuiInputTextFlags_EnterReturnsTrue))
			//{
			//	object = Scene::renameObject(scene, object, newName);
			//}
		}

		// Generate the gui object editor
		generateGuiObjectGroupEditor(scene, object);

		// Return the header open status
		return object->component<EditorSettings::EditorSettingsComponent>().m_headerOpen;
	};

	////////////////////////////////////////////////////////////////////////////////
	void generateGuiMenuBar(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		Profiler::ScopedCpuPerfCounter perfCounter(scene, "Menu Bar");

		bool openDuplicateModal = false;

		// Generate the menu bar
		if (ImGui::BeginMenuBar())
		{
			// Menu
			if (ImGui::BeginMenu("Menu"))
			{
				if (ImGui::MenuItem("ImGui Demo"))
				{
					EditorSettings::editorProperty<bool>(scene, guiSettings, "GuiSettings_ShowImguiDemo") = true;
				}
				if (ImGui::MenuItem("Quit"))
				{
					glfwSetWindowShouldClose(scene.m_context.m_window, GLFW_TRUE);
				}
				ImGui::EndMenu();
			}

			// Add object
			if (ImGui::BeginMenu("Add Object"))
			{
				for (auto objectType: Scene::objectTypes())
				{
					if (ImGui::MenuItem(Scene::objectNames()[objectType].c_str()))
					{
						Scene::Object* object = &createObject(scene, Scene::objectNames()[objectType], objectType);

						object->m_enabled = true;
					}
				}

				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	bool beginObjectGroup(Scene::Scene& scene, Scene::Object* guiSettings, Scene::ObjectType objectType)
	{
		// Whether the header is open or not
		bool headerOpen = false;

		// Color of the category header
		ImVec4 headerColor = ImGui::GetStyle().Colors[ImGuiCol_Header];
		ImVec4 headerColorHovered = ImGui::GetStyle().Colors[ImGuiCol_HeaderHovered];
		ImVec4 headerColorActive = ImGui::GetStyle().Colors[ImGuiCol_HeaderActive];

		ImVec4 headerColorHSV = ImGui::ColorConvertRGBtoHSV(headerColor);
		ImVec4 headerColorHoveredHSV = ImGui::ColorConvertRGBtoHSV(headerColorHovered);
		ImVec4 headerColorActiveHSV = ImGui::ColorConvertRGBtoHSV(headerColorActive);

		headerColorHSV.z *= 0.55f;
		headerColorHoveredHSV.z *= 0.55f;
		headerColorActiveHSV.z *= 0.55f;

		ImVec4 headerColorRGB = ImGui::ColorConvertHSVtoRGB(headerColorHSV);
		ImVec4 headerColorHoveredRGB = ImGui::ColorConvertHSVtoRGB(headerColorHoveredHSV);
		ImVec4 headerColorActiveRGB = ImGui::ColorConvertHSVtoRGB(headerColorActiveHSV);

		// Push the header color
		ImGui::PushStyleColor(ImGuiCol_Header, headerColorRGB);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerColorHoveredRGB);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, headerColorActiveRGB);

		// Open the category header
		headerOpen = ImGui::CollapsingHeader(Scene::objectNames()[objectType].c_str());

		// Pop the header color
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();

		return headerOpen;
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateObjectsWindow(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		Profiler::ScopedCpuPerfCounter perfCounter(scene, "Objects Editor");

		// Extract the simulation settings object
		Scene::Object* simulationSettings = Scene::findFirstObject(scene, Scene::OBJECT_TYPE_SIMULATION_SETTINGS);

		// Count the number of objects per category
		std::unordered_map<std::string, int> guiObjectCounts;
		for (auto category : Scene::objectGuiCategories())
		{
			guiObjectCounts[category.second] += Scene::filterObjects(scene, category.first, true, true).size();
		}

		// Get the name of the focused window
		std::string focused;

		// Skip generating the stats window in the first couple of frames; this avoids a number of weird behaviours
		if (simulationSettings->component<SimulationSettings::SimulationSettingsComponent>().m_frameId >= 3)
		{
			if (auto focusedSync = EditorSettings::consumeEditorProperty<std::string>(scene, guiSettings, "FocusedWindow#Synced"); focusedSync.has_value())
			{
				focused = focusedSync.value();
			}
		}

		// Generate each category's window
		for (auto const& countIt: guiObjectCounts)
		{
			// Extract the category string and object count
			auto [category, count] = countIt;

			// Skip empty categories
			if (count == 0)
				continue;

			// Set the window settings
			ImGui::SetNextWindowPos(ImVec2(100, 600), ImGuiCond_FirstUseEver);
			std::string windowName = category;
			ImGuiWindowFlags flags = ImGuiWindowFlags_None | ImGuiWindowFlags_AlwaysAutoResize;
			if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_lockLayout) flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

			// Focus on the next window
			if (focused == category)
				ImGui::SetNextWindowFocus();

			// Open the window
			ImGui::SetNextWindowSize(ImVec2(250, 350), ImGuiCond_FirstUseEver);
			if (ImGui::Begin(windowName.c_str(), nullptr, flags))
			{
				// Store the window focus status
				if (ImGui::IsWindowFocused())
				{
					EditorSettings::editorProperty<std::string>(scene, guiSettings, "FocusedWindow") = windowName;
				}

				// Generate GUI elements for the various object types
				for (auto it : Scene::objectGuiGenerators())
				{
					// Extaxt the data
					auto [objectType, objectGuiFunction] = it;

					// Only generate objects belonging to the current category
					if (Scene::objectGuiCategories()[objectType] != category)  continue;

					// List of all the objects in this category
					auto const& objects = Scene::filterObjects(scene, objectType, true, true);

					// Skip empty groups
					if (objects.empty()) continue;

					Profiler::ScopedCpuPerfCounter perfCounter(scene, Scene::objectNames()[objectType]);

					// Determine whether we need a collapsing header for multiple objects
					if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_groupObjects == false || beginObjectGroup(scene, guiSettings, objectType))
					{
						// Go through each object of the corresponding object type
						for (auto object : objects)
						{
							Profiler::ScopedCpuPerfCounter perfCounter(scene, object->m_name);

							// Skip the object if its group is not enabled
							if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_hideDisabledGroups == true &&
								SimulationSettings::isObjectEnabledByGroups(scene, object) == false)
								continue;

							// Pust the name of the object as a unique scope ID
							ImGui::PushID(object->m_name.c_str());

							// Generate the header of the object
							if (generateGuiObjectHeader(scene, guiSettings, object))
							{
								Debug::DebugRegion debugRegion(object->m_name);

								// invoke its GUI generator callback
								objectGuiFunction(scene, guiSettings, object);
							}

							// Pop the object ID
							ImGui::PopID();
						}
					}
				}

				// Handle the scroll bar persistence
				std::string scrollBarKey = category + "_Window_ScrollY";
				if (auto synced = EditorSettings::consumeEditorProperty<float>(scene, guiSettings, scrollBarKey + "#Synced"); synced.has_value())
				{
					ImGui::SetScrollY(synced.value());
				}
				EditorSettings::editorProperty<float>(scene, guiSettings, scrollBarKey) = ImGui::GetScrollY();
			}
			ImGui::End();
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	#define DRAGDROP_PAYLOAD_TYPE_PROFILER_CATEGORY     "PROFILER_CATEGORY"

	////////////////////////////////////////////////////////////////////////////////
	void makeStatsNodeDragDropSource(Scene::Scene& scene, Scene::Object* guiSettings, Profiler::ProfilerThreadTreeIterator node)
	{
		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload(DRAGDROP_PAYLOAD_TYPE_PROFILER_CATEGORY, node->m_category.c_str(), node->m_category.size() + 1);
			ImGui::Text(node->m_name.c_str());
			ImGui::EndDragDropSource();
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateStatsTreeNode(Scene::Scene& scene, Scene::Object* guiSettings, Profiler::ProfilerThreadTree const& tree, Profiler::ProfilerThreadTreeIterator root, size_t depth, size_t threadId)
	{
		// Cursor offsets for the various columns
		static constexpr std::array<const char*, 6> s_columnNames =
		{
			"Name",
			"Current",
			"Min",
			"Avg",
			"Max",
			"Count",
		};

		static constexpr std::array<const char*, s_columnNames.size()> s_columnLengthTexts =
		{
			"##################",
			"##########",
			"##########",
			"##########",
			"##########",
			"##########",
		};

		std::array<float, s_columnNames.size()> cursorOffsets;
		std::array<float, s_columnNames.size()> clipLengths;
		std::array<bool, s_columnNames.size()> showColumns =
		{
			true,
			guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_showCurrent,
			guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_showMin,
			guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_showAvg,
			guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_showMax,
			guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_showCount
		};

		float padding = ImGui::GetStyle().FramePadding.x;
		for (int i = 0; i < s_columnNames.size(); ++i)
		{
			if (i == 0) cursorOffsets[i] = ImGui::GetFrameWidth();
			else        cursorOffsets[i] = cursorOffsets[i - 1] + (showColumns[i - 1] ? ImGui::CalcTextSize(s_columnLengthTexts[i - 1], NULL, false).x + padding : 0.0f);

			clipLengths[i] = ImGui::CalcTextSize(s_columnLengthTexts[i], NULL, false).x;
		}
		
		// Iterate over the children
		for (auto it = tree.begin(root); it != tree.end(root); ++it)
		{
			// Compute the number of child entries
			int numChildren = std::distance(tree.begin(it), tree.end(it));

			// Cursor X coordinates for the header columns
			std::array<ImVec2, s_columnNames.size()> cursorPositions;
			for (int i = 0; i < s_columnNames.size(); ++i)
			{
				cursorPositions[i].x = ImGui::GetCursorPosX() + cursorOffsets[i];
				cursorPositions[i].y = ImGui::GetCursorPosY();
			};

			// Extract the current node's data
			auto& entry = Profiler::dataEntry(scene, *it);

			// Extract the current value
			std::array<std::string, s_columnNames.size()> values =
			{
				numChildren > 0 ? it->m_name + " (" + std::to_string(numChildren) + ")" : it->m_name,
				Profiler::convertEntryData<std::string>(entry.m_current),
				Profiler::convertEntryData<std::string>(entry.m_min),
				Profiler::convertEntryData<std::string>(entry.m_avg),
				Profiler::convertEntryData<std::string>(entry.m_max),
				std::to_string(entry.m_totalEntryCount)
			};

			// Clip the values to fit into the cells
			std::array<std::string, s_columnNames.size()> valuesClipped = values;

			for (size_t i = 0; i < s_columnNames.size(); ++i)
			{
				bool clipped = false;
				while (true)
				{
					std::string testStr = valuesClipped[i] + "...";
					ImVec2 textSize = ImGui::CalcTextSize(testStr.c_str());

					if (textSize.x <= clipLengths[i]) break;
				
					clipped = true;
					valuesClipped[i].pop_back();
				}
				if (clipped) valuesClipped[i] = valuesClipped[i] + "...";
			}

			// Do this recursively, if they do
			if (numChildren > 0)
			{
				// Create the tree for the item
				ImVec2 cursorPosName = ImGui::GetCursorPos();

				// Was the node previously open
				auto nodeit = std::find(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_openNodes.begin(), guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_openNodes.end(), it->m_category);
				bool wasNodeOpen = nodeit != guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_openNodes.end();

				// Node flags
				ImGuiTreeNodeFlags flags = wasNodeOpen ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;

				// Generate the treenode header
				bool nodeOpen = ImGui::TreeNodeEx(valuesClipped[0].c_str(), flags);

				// Update the open-flag of the node
				if (nodeOpen == true && wasNodeOpen == false)
				{
					guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_openNodes.push_back(it->m_category);
				}
				else if (nodeOpen == false && wasNodeOpen == true)
				{
					guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_openNodes.erase(nodeit);
				}

				// Generate the body of the node
				if (nodeOpen)
				{
					makeStatsNodeDragDropSource(scene, guiSettings, it);
					if (ImGui::IsItemHovered()) ImGui::SetTooltip(values[0].c_str());

					ImGui::PushID(it->m_name.c_str());
					generateStatsTreeNode(scene, guiSettings, tree, it, depth + 1, threadId);
					ImGui::PopID();
					ImGui::TreePop();
				}
				else
				{
					makeStatsNodeDragDropSource(scene, guiSettings, it);
					if (ImGui::IsItemHovered()) ImGui::SetTooltip(values[0].c_str());
				}

				// Display the values
				ImVec2 cursorPosValues = ImGui::GetCursorPos();
				for (int i = 1; i < cursorPositions.size(); ++i)
				{
					if (showColumns[i])
					{
						ImGui::SetCursorPos(cursorPositions[i]);
						ImGui::Selectable(valuesClipped[i].c_str());
						if (ImGui::IsItemHovered()) ImGui::SetTooltip(values[i].c_str());
					}					
				}
				ImGui::SetCursorPos(cursorPosValues);
			}
			else
			{
				// Create the selectable for the item
				ImGui::Bullet();
				ImGui::Selectable(valuesClipped[0].c_str());
				makeStatsNodeDragDropSource(scene, guiSettings, it);
				if (ImGui::IsItemHovered()) ImGui::SetTooltip(values[0].c_str());

				// Display the values
				for (int i = 1; i < cursorPositions.size(); ++i)
				{
					if (showColumns[i])
					{
						ImGui::SetCursorPos(cursorPositions[i]);
						ImGui::Selectable(valuesClipped[i].c_str());
						if (ImGui::IsItemHovered()) ImGui::SetTooltip(values[i].c_str());
					}
				}
			}
		};
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateStatsWindow(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		// Extract the simulation settings object
		Scene::Object* simulationSettings = Scene::findFirstObject(scene, Scene::OBJECT_TYPE_SIMULATION_SETTINGS);

		// Skip generating the stats window in the first couple of frames; this avoids a number of weird behaviours
		if (simulationSettings->component<SimulationSettings::SimulationSettingsComponent>().m_frameId < 3)
			return;

		Profiler::ScopedCpuPerfCounter perfCounter(scene, "Stats Window");

		// Cursor offsets for the various columns
		static constexpr std::array<const char*, 6> s_columnNames =
		{
			"Name",
			"Current",
			"Min",
			"Avg",
			"Max",
			"Count",
		};

		static constexpr std::array<const char*, s_columnNames.size()> s_columnLengthTexts =
		{
			"##################",
			"##########",
			"##########",
			"##########",
			"##########",
			"##########",
		};

		// Create the window
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
		ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
		if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_lockLayout) flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

		if (ImGui::Begin("Statistics", nullptr, flags))
		{
			// Generate the settings editor
			if (ImGui::TreeNode("Settings"))
			{
				ImGui::Checkbox("Current", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_showCurrent);
				ImGui::SameLine();
				ImGui::Checkbox("Avg.", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_showAvg);
				ImGui::SameLine();
				ImGui::Checkbox("Min.", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_showMin);
				ImGui::SameLine();
				ImGui::Checkbox("Max.", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_showMax);
				ImGui::SameLine();
				ImGui::Checkbox("Cnt.", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_showCount);
				
				ImGui::SliderFloat("Indentation Scaling", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_indentScale, 0.0f, 2.0f);

				ImGui::TreePop();
			}

			// Generate the content area
			if (ImGui::BeginChild("Tree", ImVec2(0.0f, 0.0f), true))
			{
				// Cursor offsets
				std::array<float, s_columnNames.size()> cursorOffsets;
				std::array<bool, s_columnNames.size()> showColumns =
				{
					true,
					guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_showCurrent,
					guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_showMin,
					guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_showAvg,
					guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_showMax,
					guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_showCount
				};

				float padding = ImGui::GetStyle().FramePadding.x;
				for (int i = 0; i < s_columnNames.size(); ++i)
				{
					if (i == 0) cursorOffsets[i] = ImGui::GetFrameWidth();
					else        cursorOffsets[i] = cursorOffsets[i - 1] + (showColumns[i - 1] ? ImGui::CalcTextSize(s_columnLengthTexts[i - 1], NULL, false).x + padding : 0.0f);
				}

				// Cursor X coordinates for the header columns
				std::array<ImVec2, s_columnNames.size()> cursorPositions;
				for (int i = 0; i < s_columnNames.size(); ++i)
				{
					cursorPositions[i].x = ImGui::GetCursorPosX() + cursorOffsets[i];
					cursorPositions[i].y = ImGui::GetCursorPosY();
				};

				// Print the column headers
				for (int i = 0; i < s_columnNames.size(); ++i)
				{
					if (showColumns[i])
					{
						ImGui::SetCursorPos(cursorPositions[i]);
						ImGui::Text(s_columnNames[i]);
					}
				}

				// Draw a separator
				ImGui::Separator();

				// Traverse the tree by starting at the root node
				generateStatsTreeNode(scene, guiSettings, scene.m_profilerTree[scene.m_profilerBufferReadId][0], scene.m_profilerTree[scene.m_profilerBufferReadId][0].begin(), 0, 0);
			}

			// Handle the scroll bar persistence
			if (auto synced = EditorSettings::consumeEditorProperty<float>(scene, guiSettings, "StatsWindow_ScrollY#Synced"); synced.has_value())
			{
				ImGui::SetScrollY(synced.value());
			}
			EditorSettings::editorProperty<float>(scene, guiSettings, "StatsWindow_ScrollY") = ImGui::GetScrollY();

			ImGui::EndChild();
		}
		ImGui::End();
	}

	////////////////////////////////////////////////////////////////////////////////
	struct ProfilerNodeGeneratorPayload
	{
		// Necessary scene information
		Scene::Object* m_simulationSettings;
		Scene::Object* m_guiSettings;

		// Iterator to the node
		Profiler::ProfilerThreadTreeIterator m_iterator;

		// Data of the node
		std::reference_wrapper<Profiler::ProfilerDataEntry> m_entry;

		ProfilerNodeGeneratorPayload(Scene::Scene& scene, Profiler::ProfilerThreadTreeIterator iterator) :
			m_iterator(iterator),
			m_entry(Profiler::dataEntry(scene, *iterator)),
			m_simulationSettings(Scene::findFirstObject(scene, Scene::OBJECT_TYPE_SIMULATION_SETTINGS)),
			m_guiSettings(Scene::findFirstObject(scene, Scene::OBJECT_TYPE_GUI_SETTINGS))
		{}
	};

	////////////////////////////////////////////////////////////////////////////////
	std::string profilerNodeFormatValue(Profiler::ProfilerDataEntry const& entry, float val)
	{
		if (Profiler::isEntryType<Profiler::ProfilerDataEntry::EntryDataFieldTime>(entry.m_min) == false)
		{
			return std::to_string(val);
		}
		return Units::secondsToString(Units::millisecondsToSeconds(val));
	}

	////////////////////////////////////////////////////////////////////////////////
	float profilerNodeDataGeneratorCurrent(const void* pPayload, int index, float x)
	{
		// Extract the payload
		ProfilerNodeGeneratorPayload const& payload = *(const ProfilerNodeGeneratorPayload*)pPayload;

		// Id of the current frame
		int frameId = int(x);

		// Look the the specified frame's data
		auto it = payload.m_entry.get().m_previousValues.find(frameId);

		// Fall back to zero if it's not stored
		if (it == payload.m_entry.get().m_previousValues.end())
			return 0.0f;

		// Return the actual value otherwise
		return Profiler::convertEntryData<float>(it->second);
	}

	////////////////////////////////////////////////////////////////////////////////
	float profilerNodeDataGeneratorAvgSimple(const void* pPayload, int index, float x)
	{
		// Extract the payload
		ProfilerNodeGeneratorPayload const& payload = *(const ProfilerNodeGeneratorPayload*)pPayload;

		// Return the min value
		return Profiler::convertEntryData<float>(payload.m_entry.get().m_avg);
	}

	////////////////////////////////////////////////////////////////////////////////
	float profilerNodeDataGeneratorAvgSliding(const void* pPayload, int index, float x)
	{
		// Extract the payload
		ProfilerNodeGeneratorPayload const& payload = *(const ProfilerNodeGeneratorPayload*)pPayload;

		// Id of the current frame
		int frameId = int(x);

		// Return the min value
		return Profiler::slidingAverageWindowed<float>(payload.m_entry.get(), frameId, payload.m_guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_avgWindowSize, false);
	}

	////////////////////////////////////////////////////////////////////////////////
	void profilerNodeTooltipGenerator(const void* pPayload, int v_idx, float x, float y)
	{
		// Extract the payload
		ProfilerNodeGeneratorPayload const& payload = *(const ProfilerNodeGeneratorPayload*)pPayload;

		// Id of the current frame
		int frameId = int(x);

		// Look the the specified frame's data
		auto it = payload.m_entry.get().m_previousValues.find(frameId);

		// Float values
		float cur = it == payload.m_entry.get().m_previousValues.end() ? 0.0f : Profiler::convertEntryData<float>(it->second);
		float min = Profiler::slidingMinWindowed<float>(payload.m_entry, frameId, payload.m_guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_avgWindowSize);
		float avg = Profiler::slidingAverageWindowed<float>(payload.m_entry, frameId, payload.m_guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_avgWindowSize, false);
		float max = Profiler::slidingMaxWindowed<float>(payload.m_entry, frameId, payload.m_guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_avgWindowSize);

		// String values
		std::string frameStartTime = Units::minutesToString(Units::secondsToMinutes(payload.m_simulationSettings->component<SimulationSettings::SimulationSettingsComponent>().m_frameStartTimepoint[frameId]));
		std::string curStr = profilerNodeFormatValue(payload.m_entry.get(), cur);
		std::string minStr = profilerNodeFormatValue(payload.m_entry.get(), min);
		std::string avgStr = profilerNodeFormatValue(payload.m_entry.get(), avg);
		std::string maxStr = profilerNodeFormatValue(payload.m_entry.get(), max);
		std::string countStr = std::to_string(payload.m_entry.get().m_currentCount);
		std::string totalCountStr = std::to_string(payload.m_entry.get().m_totalEntryCount);

		// Generate the tooltip
		ImGui::BeginTooltip();
		ImGui::Text("Time: %s (Frame #%d)", frameStartTime.c_str(), frameId);
		ImGui::Separator();
		ImGui::Columns(2, "ProfilerEntryTooltip", ImGuiColumnsFlags_NoBorder | ImGuiColumnsFlags_NoResize);
		ImGui::Text("Current:"); ImGui::NextColumn(); ImGui::Text(curStr.c_str()); ImGui::NextColumn();
		ImGui::Text("Min:"); ImGui::NextColumn(); ImGui::Text(minStr.c_str()); ImGui::NextColumn();
		ImGui::Text("Average:"); ImGui::NextColumn(); ImGui::Text(avgStr.c_str()); ImGui::NextColumn();
		ImGui::Text("Max:"); ImGui::NextColumn(); ImGui::Text(maxStr.c_str()); ImGui::NextColumn();
		ImGui::Text("Entries:"); ImGui::NextColumn(); ImGui::Text("%s (%s)", countStr.c_str(), totalCountStr.c_str()); ImGui::NextColumn();
		ImGui::EndTooltip();
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateProfilerNodeXTickLabel(const void* pPayload, char* buffer, int buf_size, float val)
	{
		std::string label = std::to_string(int(val));
		std::strcpy(buffer, label.c_str());
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateProfilerNodeYTickLabel(const void* pPayload, char* buffer, int buf_size, float val)
	{
		// Extract the payload
		ProfilerNodeGeneratorPayload const& payload = *(const ProfilerNodeGeneratorPayload*)pPayload;

		std::string label = profilerNodeFormatValue(payload.m_entry.get(), val);
		std::strcpy(buffer, label.c_str());
	}

	////////////////////////////////////////////////////////////////////////////////
	std::string getProfilerNodeLabel(Scene::Scene& scene, Scene::Object* guiSettings, Profiler::ProfilerThreadTreeIterator node)
	{
		switch (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_nodeLabelMode)
		{
		case ProfilerChartsSettings::CurrentCategory:
			return node->m_name;

		case ProfilerChartsSettings::ObjectCategory:
		{
			std::string parentName;
			int parentOffset = 0;
			for (auto const& object : scene.m_objects)
			{
				int offset = node->m_category.find(object.first);
				if (offset != std::string::npos && offset > parentOffset)
				{
					parentName = object.first;
					parentOffset = offset;
				}
			}
			return parentName.empty() ? node->m_name : node->m_category.substr(parentOffset);
		}

		case ProfilerChartsSettings::FullCategory:
			return node->m_category;
		}

		return node->m_name;
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateProfilerNodeLabel(Scene::Scene& scene, Scene::Object* guiSettings, std::string label)
	{
		// Replace the separators with a single one
		std::string_replace_all(label, "::", ":");

		// Go through each category
		std::istringstream iss(label);
		bool first = true;
		for (std::string category; std::getline(iss, category, ':'); )
		{
			if (first == false)
			{
				ImGui::SameLine();
				ImGui::Arrow(1.0f, ImGuiDir_Right);
				ImGui::SameLine();
			}
			ImGui::Text(category.c_str());
			first = false;
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateProfilerNodeChart(Scene::Scene& scene, Scene::Object* guiSettings, Profiler::ProfilerThreadTreeIterator node, size_t depth, size_t threadId, bool& treeOpen)
	{
		Profiler::ScopedCpuPerfCounter perfCounter(scene, node->m_category);

		// Node generator payload
		ProfilerNodeGeneratorPayload payload(scene, node);

		// Store the cursor pos to remember the start of the line
		ImVec2 cursorPos = ImGui::GetCursorPos();
		
		// Plot the chart
		if (ImGui::Button("X"))
		{
			guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_nodesToShow.erase(node->m_category);
		}

		ImGui::SameLine();

		// Treenode flags for the chart
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;

		// Handle the tab open state persistence
		std::string openStateKey = "ProfilerChartOpen_" + node->m_category;
		if (EditorSettings::editorProperty<bool>(scene, guiSettings, openStateKey))
		{
			flags |= ImGuiTreeNodeFlags_DefaultOpen;
		}

		// Generate the tree node header
		treeOpen = ImGui::TreeNodeEx("##label", flags);
		EditorSettings::editorProperty<bool>(scene, guiSettings, openStateKey) = treeOpen;

		// Whether we need to show the tooltip or not
		bool showTooltip = ImGui::IsItemHovered();

		// Generate the tree node label
		ImGui::SameLine();
		generateProfilerNodeLabel(scene, guiSettings, getProfilerNodeLabel(scene, guiSettings, node));

		// Start frame id and number of frames
		int startFrameId = guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_startFrameId;
		int endFrameId = guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_endFrameId;
		int numFrames = guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_numFramesToShow;

		// Extract the current frame time
		int currFrameId = glm::min(payload.m_simulationSettings->component<SimulationSettings::SimulationSettingsComponent>().m_frameId - 1, startFrameId + numFrames - 1);
		auto currIt = payload.m_entry.get().m_previousValues.find(currFrameId);

		// Min, avg and max values (sliding)
		float cur = (currIt == payload.m_entry.get().m_previousValues.end()) ? 0.0f : Profiler::convertEntryData<float>(currIt->second);
		float min = Profiler::slidingMin<float>(payload.m_entry, startFrameId, endFrameId);
		float avg = Profiler::slidingAverage<float>(payload.m_entry, startFrameId, endFrameId, false);
		float max = Profiler::slidingMax<float>(payload.m_entry, startFrameId, endFrameId);

		// Construct the overlay string
		std::stringstream overlaystream;
		overlaystream
			<< std::setw(9) << profilerNodeFormatValue(Profiler::dataEntry(scene, *node), cur) << " ("
			<< "Min: " << std::setw(9) << profilerNodeFormatValue(Profiler::dataEntry(scene, *node), min) << "," << std::string(4, ' ')
			<< "Avg: " << std::setw(9) << profilerNodeFormatValue(Profiler::dataEntry(scene, *node), avg) << "," << std::string(4, ' ')
			<< "Max: " << std::setw(9) << profilerNodeFormatValue(Profiler::dataEntry(scene, *node), max) << ")" << std::endl;
		std::string overlay = overlaystream.str();

		// Generate the chart itself
		if (treeOpen)
		{
			ImGui::PlotConfig::value_generator_fn generators[] =
			{
				&profilerNodeDataGeneratorAvgSliding, &profilerNodeDataGeneratorCurrent,
			};

			ImU32 chartColors[] =
			{
				ImGui::Color32FromGlmVector(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_avgPlotColor),
				ImGui::Color32FromGlmVector(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_plotColor),
			};

			ImGui::PlotConfig plotConfig;
			plotConfig.values.ys_count = guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_showLimits ? 2 : 1;
			plotConfig.values.ysg_list = generators;
			plotConfig.values.colors = chartColors;
			plotConfig.values.user_data = &payload;
			plotConfig.values.count = numFrames;
			plotConfig.values.offset = startFrameId;
			plotConfig.grid_x.show = true;
			plotConfig.grid_x.tick_major.color = ImGui::Color32FromGlmVector(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_xGridColor);
			plotConfig.grid_x.tick_major.thickness = 2.0f;
			plotConfig.grid_x.tick_major.label_fn = &generateProfilerNodeXTickLabel;
			plotConfig.grid_x.tick_major.label_size = guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_showTickLabels ? 0.75f : 0.0f;
			plotConfig.grid_x.tick_major.label_color = ImGui::Color32FromGlmVector(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_xGridLabelColor);
			plotConfig.grid_x.ticks = guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_scaleTicks.x;
			plotConfig.grid_x.subticks = 1;
			plotConfig.grid_y.show = true;
			plotConfig.grid_y.tick_major.color = ImGui::Color32FromGlmVector(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_yGridColor);
			plotConfig.grid_y.tick_major.thickness = 2.0f;
			plotConfig.grid_y.tick_major.label_fn = &generateProfilerNodeYTickLabel;
			plotConfig.grid_y.tick_major.label_size = guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_showTickLabels ? 0.75f : 0.0f;
			plotConfig.grid_y.tick_major.label_color = ImGui::Color32FromGlmVector(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_yGridLabelColor);
			plotConfig.grid_y.ticks = guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_scaleTicks.y;
			plotConfig.grid_y.subticks = 1;
			plotConfig.tooltip.show = true;
			plotConfig.tooltip.generator = &profilerNodeTooltipGenerator;
			plotConfig.highlight.show = true;
			plotConfig.highlight.only_first = true;
			plotConfig.scale.min = (min == max) ? min - 0.5f: min;
			plotConfig.scale.max = (min == max) ? min + 0.5f : max;
			plotConfig.overlay.show = true;
			plotConfig.overlay.text = overlay.c_str();
			plotConfig.overlay.position = ImVec2(0.5f, -1.0f);
			plotConfig.frame_size = ImVec2(0.0f, guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_graphHeight);
			plotConfig.line_thickness = guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_lineThickness;
			plotConfig.skip_small_lines = true;

			ImGui::SetCursorPosX(cursorPos.x);
			ImGui::PushItemWidth(-1);
			ImGui::PlotEx("##label", plotConfig);
			ImGui::PopItemWidth();

			if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_nest == false)
				ImGui::TreePop();
		}

		// Otherwise just give a small peak
		else if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_showPeakWhenCollapsed)
		{
			float childHeight = ImGui::CalcTextSize(overlay.c_str()).y + 2.0f * ImGui::GetStyle().WindowPadding.y;
			if (ImGui::BeginChild(node->m_category.c_str(), ImVec2(0.0f, childHeight), true))
			{
				ImGui::Text(overlay.c_str());
				showTooltip = showTooltip || ImGui::IsItemHovered();
			}
			ImGui::EndChild();
		}

		// Tooltip
		if (showTooltip)
		{
			ImGui::BeginTooltip();
			generateProfilerNodeLabel(scene, guiSettings, node->m_category);
			ImGui::EndTooltip();
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateProfilerNode(Scene::Scene& scene, Scene::Object* guiSettings, Profiler::ProfilerThreadTree const& tree, Profiler::ProfilerThreadTreeIterator root, size_t depth, size_t threadId)
	{
		//Profiler::ScopedCpuPerfCounter perfCounter(scene, "Profiler Node", true);

		// Iterate over the children
		for (auto it = tree.begin(root); it != tree.end(root); ++it)
		{
			// Whether this chart should be drawn or not
			bool drawChart = guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_nodesToShow.count(it->m_category);

			// Is the chart shown or not
			bool open = false;

			// Generate the corresponding chart
			if (drawChart)
			{
				ImGui::PushID(it->m_category.c_str());
				generateProfilerNodeChart(scene, guiSettings, it, depth, threadId, open);
				ImGui::PopID();
			}

			// Keep traversing
			if (drawChart == false || guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_nest == false || open)
			{
				generateProfilerNode(scene, guiSettings, tree, it, depth + 1, threadId);
			}

			// End the tree if we are nesting
			if (open && guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_nest)
				ImGui::TreePop();
		};
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateProfilerWindow(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		Profiler::ScopedCpuPerfCounter perfCounter(scene, "Profiler Window");

		// Find the simulation settings object
		Scene::Object* simulationSettings = Scene::findFirstObject(scene, Scene::OBJECT_TYPE_SIMULATION_SETTINGS);

		// Skip generating the stats window in the first couple of frames; this avoids a number of weird behaviours
		if (simulationSettings->component<SimulationSettings::SimulationSettingsComponent>().m_frameId < 3)
			return;
		
		// Flags for the window
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
		ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
		if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_lockLayout) flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

		// Create the window
		if (ImGui::Begin("Profiler", nullptr, flags))
		{
			// Generate the settings editor
			if (ImGui::TreeNode("Settings"))
			{
				ImGui::SliderFloat("Graph Height", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_graphHeight, 100.0f, 320.0f);
				ImGui::SliderInt2("Scale Ticks", glm::value_ptr(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_scaleTicks), 2, 10);
				ImGui::SliderFloat("Line Thickness", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_lineThickness, 1.0f, 4.0f);
				ImGui::Combo("Node Label", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_nodeLabelMode, ProfilerChartsSettings::NodeLabelMode_meta);
				ImGui::SliderInt("Number of Frames", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_numFramesToShow, 1, 256);
				ImGui::SliderInt("Update Ratio", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_updateRatio, 1, 256);
				ImGui::SliderInt("Average Window Size", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_avgWindowSize, 1, 128);
				ImGui::Checkbox("Visualize Limits", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_showLimits);
				ImGui::SameLine();
				ImGui::Checkbox("Tick Labels", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_showTickLabels);

				ImGui::Checkbox("Freeze Output", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_freeze);
				ImGui::SameLine();
				ImGui::Checkbox("Nest", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_nest);
				ImGui::SameLine();
				ImGui::Checkbox("Peak When Collapsed", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_showPeakWhenCollapsed);

				if (ImGui::TreeNodeEx("Colors"))
				{
					ImGui::ColorEdit4("Plot Color", glm::value_ptr(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_plotColor));
					ImGui::ColorEdit4("Avg Color", glm::value_ptr(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_avgPlotColor));
					//ImGui::ColorEdit4("Min Plot Color", glm::value_ptr(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_minPlotColor));
					//ImGui::ColorEdit4("Max Plot Color", glm::value_ptr(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_maxPlotColor));
					ImGui::ColorEdit4("X Grid Color", glm::value_ptr(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_xGridColor));
					ImGui::ColorEdit4("X Grid Label Color", glm::value_ptr(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_xGridLabelColor));
					ImGui::ColorEdit4("Y Grid Color", glm::value_ptr(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_yGridColor));
					ImGui::ColorEdit4("Y Grid Label Color", glm::value_ptr(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_yGridLabelColor));
					ImGui::TreePop();
				}

				ImGui::TreePop();
			}

			// Compute the ID of the new start frame
			int newFrameStartId = glm::max(0, simulationSettings->component<SimulationSettings::SimulationSettingsComponent>().m_frameId - guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_numFramesToShow);
			if (newFrameStartId >= guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_lastFrameDrawn + guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_updateRatio && 
				guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_freeze == false)
			{
				guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_lastFrameDrawn = newFrameStartId;
				guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_startFrameId = newFrameStartId;
			}
			guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_endFrameId = glm::min(
				guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_startFrameId + guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_numFramesToShow - 1, 
				simulationSettings->component<SimulationSettings::SimulationSettingsComponent>().m_frameId - 2);

			// Begin the content area
			if (ImGui::BeginChild("Charts", ImVec2(0.0f, 0.0f), true))
			{
				if (guiSettings->component<GuiSettingsComponent>().m_profilerChartsSettings.m_nodesToShow.size())
				{
					// Traverse the tree by starting at the root node
					generateProfilerNode(scene, guiSettings, scene.m_profilerTree[scene.m_profilerBufferReadId][0], scene.m_profilerTree[scene.m_profilerBufferReadId][0].begin(), 0, 0);
				}

				// Handle the scroll bar persistence
				std::string scrollBarKey = "ProfilerWindow_ScrollY";
				if (auto synced = EditorSettings::consumeEditorProperty<float>(scene, guiSettings, scrollBarKey + "#Synced"); synced.has_value())
				{
					ImGui::SetScrollY(synced.value());
				}
				EditorSettings::editorProperty<float>(scene, guiSettings, scrollBarKey) = ImGui::GetScrollY();
			}
			ImGui::EndChild();

			// Drag and drop support
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload * payload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_TYPE_PROFILER_CATEGORY))
				{
					std::vector<char> category(payload->DataSize);
					memcpy(category.data(), payload->Data, payload->DataSize);
					guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_nodesToShow.insert(std::string(category.data()));
				}

				ImGui::EndDragDropTarget();
			}
		}
		ImGui::End();
	}

	////////////////////////////////////////////////////////////////////////////////
	ImVec4 getLogEntryColor(Scene::Scene& scene, Scene::Object* guiSettings, Debug::InMemoryLogEntry const& message)
	{
		glm::vec3 color = glm::vec3(1.0f);

		if (message.m_sourceLog == Debug::log_debug().m_ref.get().m_name)   color = guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_debugColor;
		if (message.m_sourceLog == Debug::log_trace().m_ref.get().m_name)   color = guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_traceColor;
		if (message.m_sourceLog == Debug::log_info().m_ref.get().m_name)    color = guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_infoColor;
		if (message.m_sourceLog == Debug::log_warning().m_ref.get().m_name) color = guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_warningColor;
		if (message.m_sourceLog == Debug::log_error().m_ref.get().m_name)   color = guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_errorColor;

		return ImGui::ColorFromGlmVector(color);
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateLogEntry(Scene::Scene& scene, Scene::Object* guiSettings, Debug::InMemoryLogEntry const& message)
	{
		// Header text size
		ImVec2 severityTextSize = ImGui::CalcTextSize("#WARNING#");
		ImVec2 dateTextSize = ImGui::CalcTextSize("2019. SEPTEMBER 12., 12:24:36");

		// Whether the tooltip should be shown or not
		bool showTooltip = false;

		// Push the message color
		ImGui::PushStyleColor(ImGuiCol_Text, getLogEntryColor(scene, guiSettings, message));

		// Add some vertical space between the entries
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + severityTextSize.y * guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_messageSpacing);

		// Generate the severity
		if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showRegion)
		{
			float colWidth = 0.0f;
			if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showDate)
			{
				colWidth = glm::max(colWidth, dateTextSize.x);
				ImGui::Text(message.m_date.c_str());
				showTooltip = showTooltip || ImGui::IsItemHovered();
			}
			if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showSeverity)
			{
				colWidth = glm::max(colWidth, severityTextSize.x);
				ImGui::Text(message.m_sourceLog.c_str());
				showTooltip = showTooltip || ImGui::IsItemHovered();
			}
			ImGui::SetColumnWidth(0, colWidth);
			ImGui::NextColumn();
		}
		else
		{
			// Id of the current column
			int colId = 0;

			if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showDate)
			{
				ImGui::SetColumnWidth(colId++, dateTextSize.x);
				ImGui::Text(message.m_date.c_str());
				showTooltip = showTooltip || ImGui::IsItemHovered();
				ImGui::NextColumn();
			}
			if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showSeverity)
			{
				ImGui::SetColumnWidth(colId++, severityTextSize.x);
				ImGui::Text(message.m_sourceLog.c_str());
				showTooltip = showTooltip || ImGui::IsItemHovered();
				ImGui::NextColumn();
			}
		}

		// Add some vertical space between the entries
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + severityTextSize.y * guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_messageSpacing);
		if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showRegion)
		{
			ImGui::Text(message.m_region.c_str());
			showTooltip = showTooltip || ImGui::IsItemHovered();
		}

		if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_longMessages)
		{
			ImGui::TextWrapped(message.m_message.c_str());
		}
		else
		{
			std::string clippedMessage = message.m_message;
			auto it = clippedMessage.find_first_of('\n');
			if (it != std::string::npos) clippedMessage = clippedMessage.substr(0, it);

			ImGui::Text(clippedMessage.c_str());
		}
		showTooltip = showTooltip || ImGui::IsItemHovered();
		ImGui::NextColumn();
		ImGui::PopStyleColor();

		// Generate the tooltip
		if (showTooltip)
		{
			std::filesystem::path sourceFile = message.m_sourceFile;
			std::string sourceFileRelative = std::filesystem::relative(sourceFile).string();
			std::string regionString = message.m_region;
			std::string_replace_all(regionString, "][", "::");
			std::string_replace_all(regionString, "[", "");
			std::string_replace_all(regionString, "]", "");

			ImGui::SetNextWindowSize(ImVec2(800.0f, -1.0f));
			ImGui::BeginTooltip();
			ImGui::Columns(2, "LogEntryTooltip", ImGuiColumnsFlags_NoBorder);
			ImGui::SetColumnWidth(0, ImGui::CalcTextSize("Severity:##").x);
			ImGui::Text("Date:"); ImGui::NextColumn(); ImGui::Text("%s", message.m_date.c_str()); ImGui::NextColumn();
			ImGui::Text("Severity:"); ImGui::NextColumn(); ImGui::Text("%s", message.m_sourceLog.c_str()); ImGui::NextColumn();
			ImGui::Text("Region:"); ImGui::NextColumn(); ImGui::Text("%s", regionString.c_str()); ImGui::NextColumn();
			ImGui::Text("Thread:"); ImGui::NextColumn(); ImGui::Text("%s", message.m_threadId.c_str()); ImGui::NextColumn();
			ImGui::Text("Source:"); ImGui::NextColumn(); ImGui::Text("%s", message.m_sourceFunction.c_str()); ImGui::NextColumn();
			ImGui::Text(""); ImGui::NextColumn(); ImGui::TextWrapped("[%s (%s)]", sourceFileRelative.c_str(), message.m_sourceLine.c_str()); ImGui::NextColumn();
			ImGui::Columns(1);
			ImGui::Separator();
			ImGui::TextWrapped(message.m_message.c_str());
			ImGui::EndTooltip();
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	bool filterLogEntry(Scene::Scene& scene, Scene::Object* guiSettings, Debug::InMemoryLogBuffer const& buffer, int messageId, Debug::InMemoryLogEntry const& message)
	{
		Profiler::ScopedCpuPerfCounter perfCounter(scene, "Filtering", true);

		// Load the cached state
		auto& msgState = guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_cachedMessageState[messageId];

		// Use the cached data, if able
		if (msgState.m_source == &buffer && msgState.m_date == message.m_dateEpoch && msgState.m_filterTime == guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_lastFilterUpdate)
			return msgState.m_visible;

		bool result = true;
		if (message.m_sourceLog == Debug::log_debug().m_ref.get().m_name)   result &= guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showDebug;
		if (message.m_sourceLog == Debug::log_trace().m_ref.get().m_name)   result &= guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showTrace;
		if (message.m_sourceLog == Debug::log_info().m_ref.get().m_name)    result &= guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showInfo;
		if (message.m_sourceLog == Debug::log_warning().m_ref.get().m_name) result &= guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showWarning;
		if (message.m_sourceLog == Debug::log_error().m_ref.get().m_name)   result &= guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showError;

		if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_includeFilter.m_empty == false)
		{
			result = result && (
				guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_includeFilter.test("Source", message.m_sourceLog) || 
				guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_includeFilter.test("Region", message.m_region) ||
				guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_includeFilter.test("Date", message.m_date) || 
				guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_includeFilter.test("Function", message.m_sourceFunction) ||
				guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_includeFilter.test("File", message.m_sourceFile) || 
				guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_includeFilter.test("Line", message.m_sourceLine) ||
				guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_includeFilter.test("Message", message.m_message)
			);
		}

		if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_discardFilter.m_empty == false)
		{
			result = result && (
				!guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_discardFilter.test("Source", message.m_sourceLog) && 
				!guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_discardFilter.test("Region", message.m_region) &&
				!guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_discardFilter.test("Date", message.m_date) && 
				!guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_discardFilter.test("Function", message.m_sourceFunction) &&
				!guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_discardFilter.test("File", message.m_sourceFile) && 
				!guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_discardFilter.test("Line", message.m_sourceLine) &&
				!guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_discardFilter.test("Message", message.m_message)
			);
		}

		// Update the cached data
		msgState.m_source = &buffer;
		msgState.m_date = message.m_dateEpoch;
		msgState.m_visible = result;
		msgState.m_filterTime = guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_lastFilterUpdate;

		return result;
	}

	////////////////////////////////////////////////////////////////////////////////
	Debug::InMemoryLogBuffer const& getLogBuffer(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showDebug)   return Debug::logger_impl::debug_full_inmemory_buffer;
		if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showTrace)   return Debug::logger_impl::trace_full_inmemory_buffer;
		if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showInfo)    return Debug::logger_impl::info_full_inmemory_buffer;
		if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showWarning) return Debug::logger_impl::warning_full_inmemory_buffer;
		return Debug::logger_impl::error_full_inmemory_buffer;
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateLogMessagesClipped(Scene::Scene& scene, Scene::Object* guiSettings, Debug::InMemoryLogBuffer const& buffer)
	{
		// Extract the visible message ids
		std::vector<size_t> visibleMessages;
		visibleMessages.reserve(buffer.m_numMessages);
		for (size_t i = 0; i < buffer.m_numMessages; ++i)
		{
			// Get the Id of the message
			int messageId = (buffer.m_startMessageId + i) % buffer.m_bufferLength;

			// Extract the corresponding message
			Debug::InMemoryLogEntry const& message = buffer.m_messages[messageId];

			// Apply a filter to the message
			if (filterLogEntry(scene, guiSettings, buffer, messageId, message))
			{
				visibleMessages.push_back(i);
			}
		}

		// Clip the messages
		ImGuiListClipper clipper(visibleMessages.size());

		// Go through each message
		while (clipper.Step())
		{
			for (size_t i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
			{
				generateLogEntry(scene, guiSettings, buffer.m_messages[visibleMessages[i]]);
			}
		}

		// Finish clipping
		clipper.End();
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateLogMessagesSlow(Scene::Scene& scene, Scene::Object* guiSettings, Debug::InMemoryLogBuffer const& buffer)
	{
		// Go through each message
		for (size_t i = 0; i < buffer.m_numMessages; ++i)
		{
			// Get the Id of the message
			int messageId = (buffer.m_startMessageId + i) % buffer.m_bufferLength;

			// Extract the corresponding message
			Debug::InMemoryLogEntry const& message = buffer.m_messages[messageId];

			// Apply a filter to the message
			if (filterLogEntry(scene, guiSettings, buffer, messageId, message))
			{
				// Generate an entry for the message
				generateLogEntry(scene, guiSettings, message);
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateLogWindow(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		Profiler::ScopedCpuPerfCounter perfCounter(scene, "Log Window");

		// Window attributes
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
		ImGuiWindowFlags flags = 0;
		if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_lockLayout) flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

		// Create the window
		if (ImGui::Begin("Log", nullptr, flags))
		{
			// Generate the settings editor
			bool settingsShown = false;
			if (settingsShown = ImGui::TreeNode("Settings"))
			{
				// Log Settings
				ImGui::Checkbox("Debug", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showDebug);
				ImGui::SameLine();
				ImGui::PushID(glm::value_ptr(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_debugColor));
				ImGui::ColorEdit3("Color", glm::value_ptr(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_debugColor));
				ImGui::PopID();

				ImGui::Checkbox("Trace", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showTrace);
				ImGui::SameLine();
				ImGui::PushID(glm::value_ptr(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_traceColor));
				ImGui::ColorEdit3("Color", glm::value_ptr(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_traceColor));
				ImGui::PopID();

				ImGui::Checkbox("Info", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showInfo);
				ImGui::SameLine();
				ImGui::PushID(glm::value_ptr(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_infoColor));
				ImGui::ColorEdit3("Color", glm::value_ptr(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_infoColor));
				ImGui::PopID();

				ImGui::Checkbox("Warning", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showWarning);
				ImGui::SameLine();
				ImGui::PushID(glm::value_ptr(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_warningColor));
				ImGui::ColorEdit3("Color", glm::value_ptr(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_warningColor));
				ImGui::PopID();

				ImGui::Checkbox("Error", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showError);
				ImGui::SameLine();
				ImGui::PushID(glm::value_ptr(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_errorColor));
				ImGui::ColorEdit3("Color", glm::value_ptr(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_errorColor));
				ImGui::PopID();

				ImGui::SliderFloat("Message Spacing", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_messageSpacing, 0.0f, 2.0f);

				if (ImGui::RegexFilter("Include Filter", guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_includeFilter))
				{
					Scene::Object* simulationSettings = Scene::findFirstObject(scene, Scene::OBJECT_TYPE_SIMULATION_SETTINGS);
					guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_lastFilterUpdate = simulationSettings->component<SimulationSettings::SimulationSettingsComponent>().m_frameId;
				}

				if (ImGui::RegexFilter("Exclude Filter", guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_discardFilter))
				{
					Scene::Object* simulationSettings = Scene::findFirstObject(scene, Scene::OBJECT_TYPE_SIMULATION_SETTINGS);
					guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_lastFilterUpdate = simulationSettings->component<SimulationSettings::SimulationSettingsComponent>().m_frameId;
				}
				
				ImGui::Checkbox("Show Date", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showDate);
				ImGui::SameLine();
				ImGui::Checkbox("Show Severity", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showSeverity);
				ImGui::SameLine();
				ImGui::Checkbox("Show Region", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showRegion);
				ImGui::SameLine();
				ImGui::Checkbox("Long Messages", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_longMessages);
				ImGui::SameLine();
				ImGui::Checkbox("Auto Scroll", &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_autoScroll);
				ImGui::SameLine();
				if (ImGui::Button("Clear"))
				{
					Debug::logger_impl::debug_only_inmemory_buffer.clear();
					Debug::logger_impl::debug_full_inmemory_buffer.clear();
					Debug::logger_impl::trace_only_inmemory_buffer.clear();
					Debug::logger_impl::trace_full_inmemory_buffer.clear();
					Debug::logger_impl::info_only_inmemory_buffer.clear();
					Debug::logger_impl::info_full_inmemory_buffer.clear();
					Debug::logger_impl::warning_only_inmemory_buffer.clear();
					Debug::logger_impl::warning_full_inmemory_buffer.clear();
					Debug::logger_impl::error_only_inmemory_buffer.clear();
					Debug::logger_impl::error_full_inmemory_buffer.clear();
				}

				ImGui::Separator();
				ImGui::TreePop();
			}

			// Extract the corresponding buffer
			Debug::InMemoryLogBuffer const& buffer = getLogBuffer(scene, guiSettings);

			// Leave some space for bottom filter and clear line, if needed
			float bottomLineSpace = 0.0f;
			if (!settingsShown)
			{
				// Height of the bar
				float barHeight = ImGui::CalcTextSize("XX").y + ImGui::GetStyle().FramePadding.y * 2.0f + ImGui::GetStyle().ItemSpacing.y * 2.0f;

				// This is the space necessary
				bottomLineSpace += barHeight;
			}

			// Begin the content area
			if (ImGui::BeginChild("Messages", ImVec2(0.0f, 0.0f - bottomLineSpace), true))
			{
				int numCols = 1;
				if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showRegion)
				{
					if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showDate ||
						guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showSeverity)
					{
						++numCols;
					}
				}
				else
				{
					if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showDate) ++numCols;
					if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showSeverity) ++numCols;
				}

				// Set it to two column mode
				ImGui::Columns(numCols, 0, ImGuiColumnsFlags_NoBorder | ImGuiColumnsFlags_NoResize);

				// Generate the log messages
				if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_longMessages)
					generateLogMessagesSlow(scene, guiSettings, buffer);
				else
					generateLogMessagesClipped(scene, guiSettings, buffer);

				// Implement auto scrolling
				float prevMaxScroll = guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_maxScrollY;
				float currMaxScroll = ImGui::GetScrollMaxY();

				guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_maxScrollY = currMaxScroll;

				if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_autoScroll && prevMaxScroll + 8.0f < currMaxScroll && ImGui::GetScrollY() <= currMaxScroll)
					ImGui::SetScrollHereY(1.0f);
			}
			ImGui::EndChild();

			// Show the quick action bar at the bottom
			if (!settingsShown)
			{
				if (ImGui::RegexFilter("Filter", guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_includeFilter))
				{
					Scene::Object* simulationSettings = Scene::findFirstObject(scene, Scene::OBJECT_TYPE_SIMULATION_SETTINGS);
					guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_lastFilterUpdate = simulationSettings->component<SimulationSettings::SimulationSettingsComponent>().m_frameId;
				}

				ImGui::SameLine();
				if (ImGui::Button("Clear"))
				{
					Debug::logger_impl::debug_only_inmemory_buffer.clear();
					Debug::logger_impl::debug_full_inmemory_buffer.clear();
					Debug::logger_impl::trace_only_inmemory_buffer.clear();
					Debug::logger_impl::trace_full_inmemory_buffer.clear();
					Debug::logger_impl::info_only_inmemory_buffer.clear();
					Debug::logger_impl::info_full_inmemory_buffer.clear();
					Debug::logger_impl::warning_only_inmemory_buffer.clear();
					Debug::logger_impl::warning_full_inmemory_buffer.clear();
					Debug::logger_impl::error_only_inmemory_buffer.clear();
					Debug::logger_impl::error_full_inmemory_buffer.clear();
				}
			}
		}

		ImGui::End();
	}

	////////////////////////////////////////////////////////////////////////////////
	std::string getVariableName(Scene::Scene& scene, Scene::Object* guiSettings, std::string name)
	{
		switch (guiSettings->component<GuiSettingsComponent>().m_shaderInspectorSettings.m_variableNames)
		{
		case ShaderInspectorSettings::Full:
			return name;
		case ShaderInspectorSettings::NameOnly:
		{
			size_t start = name.find_last_of('.');
			start = start == std::string::npos ? 0 : start + 1;
			size_t end = name.back() == ']' ? name.find_last_of('[') : std::string::npos;
			size_t count = end == std::string::npos ? std::string::npos : end - start;
			return name.substr(start, count);
		}
		}

		return name;
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateShaderInspectorUniforms(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		// Extract the program id
		GLuint program = scene.m_shaders[guiSettings->component<GuiSettingsComponent>().m_shaderInspectorSettings.m_currentProgram].m_program;

		// Get the list of uniforms
		auto uniforms = GPU::inspectProgramUniforms(program);

		// Table contents
		std::vector<std::vector<std::string>> contents;

		// Go through each row to and compute the text lengths
		for (auto const& uniform : uniforms)
		{
			// Stringify the values
			std::vector<std::string> values =
			{
				std::to_string(uniform.m_location),
				getVariableName(scene, guiSettings, uniform.m_name),
				GPU::dataTypeName(uniform.m_type, uniform.m_arraySize),
				uniform.m_isArray ? std::to_string(uniform.m_arrayStride) :  ""
			};
			contents.emplace_back(values);
		}

		// Generate the actual table
		ImGui::TableConfig conf;
		conf.cols.count = 4;
		conf.cols.headers = { "Loc", "Name", "Type", "Stride" };
		conf.cols.alignments = { ImGui::TableConfig::Column::Right, ImGui::TableConfig::Column::Left, ImGui::TableConfig::Column::Left, ImGui::TableConfig::Column::Left };
		conf.cols.data_types = { ImGui::TableConfig::Column::INT, ImGui::TableConfig::Column::STRING, ImGui::TableConfig::Column::STRING, ImGui::TableConfig::Column::INT };
		conf.rows.values = contents;
		conf.sort.p_sort = &EditorSettings::editorProperty<bool>(scene, guiSettings, "ShaderInspector_Uniforms_Sort", true);
		conf.sort.p_col_id = (uint32_t*) &EditorSettings::editorProperty<int>(scene, guiSettings, "ShaderInspector_Uniforms_ColId", 0);
		conf.sort.p_direction = (ImGui::TableConfig::Sort::Direction*) &EditorSettings::editorProperty<int>(scene, guiSettings, "ShaderInspector_Uniforms_Direction", ImGui::TableConfig::Sort::Ascending);

		ImGui::Table("Uniforms", conf);
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateShaderInspectorUniformBlocks(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		// Extract the program id
		GLuint program = scene.m_shaders[guiSettings->component<GuiSettingsComponent>().m_shaderInspectorSettings.m_currentProgram].m_program;

		// Get the list of uniforms
		auto blocks = GPU::inspectProgramUniformBlocks(program);

		// Table contents
		std::vector<std::vector<std::string>> blockListContents;

		// Go through each row to and compute the text lengths
		for (auto const& block : blocks)
		{
			// Stringify the values
			std::vector<std::string> values =
			{
				std::to_string(block.m_binding),
				block.m_name,
				std::to_string(block.m_variables.size()),
				std::to_string(block.m_dataSize)
			};
			blockListContents.emplace_back(values);
		}

		// Generate the actual table
		ImGui::TableConfig blockConf;
		blockConf.cols.count = 4;
		blockConf.cols.headers = { "Bind", "Name", "Variables", "Size" };
		blockConf.cols.alignments = { ImGui::TableConfig::Column::Right, ImGui::TableConfig::Column::Left, ImGui::TableConfig::Column::Left, ImGui::TableConfig::Column::Left };
		blockConf.cols.data_types = { ImGui::TableConfig::Column::INT, ImGui::TableConfig::Column::STRING, ImGui::TableConfig::Column::INT, ImGui::TableConfig::Column::INT };
		blockConf.rows.values = blockListContents;
		blockConf.rows.display_min = guiSettings->component<GuiSettingsComponent>().m_shaderInspectorSettings.m_numBlockRows.x;
		blockConf.rows.display_max = guiSettings->component<GuiSettingsComponent>().m_shaderInspectorSettings.m_numBlockRows.y;
		blockConf.sort.p_sort = &EditorSettings::editorProperty<bool>(scene, guiSettings, "ShaderInspector_UniformBlocks_Sort", true);
		blockConf.sort.p_col_id = (uint32_t*)&EditorSettings::editorProperty<int>(scene, guiSettings, "ShaderInspector_UniformBlocks_ColId", 0);
		blockConf.sort.p_direction = (ImGui::TableConfig::Sort::Direction*) & EditorSettings::editorProperty<int>(scene, guiSettings, "ShaderInspector_UniformBlocks_Direction", ImGui::TableConfig::Sort::Ascending);
		blockConf.selection.enabled = true;
		blockConf.selection.row_id = &EditorSettings::editorProperty<int>(scene, guiSettings, "ShaderInspector_UniformBlockId");

		ImGui::Table("Blocks", blockConf);

		// Which block to inspect
		std::vector<std::string> blockNames;
		for (auto const& block : blocks)
		{
			blockNames.push_back(block.m_name);
		}

		// Table contents
		std::vector<std::vector<std::string>> variableContents;

		// Go through each row to and compute the text lengths
		if (!blocks.empty())
		{
			for (auto const& variable : blocks[EditorSettings::editorProperty<int>(scene, guiSettings, "ShaderInspector_UniformBlockId")].m_variables)
			{
				// Stringify the values
				std::vector<std::string> values =
				{
					std::to_string(variable.m_byteOffset),
					getVariableName(scene, guiSettings, variable.m_name),
					GPU::dataTypeName(variable.m_type, variable.m_arraySize),
					std::to_string(variable.m_padding),
					variable.m_isArray ? std::to_string(variable.m_arrayStride) : ""
				};
				variableContents.emplace_back(values);
			}
		}

		// Generate the actual table
		ImGui::TableConfig variableConf;
		variableConf.cols.count = 5;
		variableConf.cols.headers = { "Offset", "Name", "Type", "Padding", "Stride" };
		variableConf.cols.alignments = { ImGui::TableConfig::Column::Right, ImGui::TableConfig::Column::Left, ImGui::TableConfig::Column::Left, ImGui::TableConfig::Column::Left, ImGui::TableConfig::Column::Left, ImGui::TableConfig::Column::Left };
		variableConf.cols.data_types = { ImGui::TableConfig::Column::INT, ImGui::TableConfig::Column::STRING, ImGui::TableConfig::Column::STRING, ImGui::TableConfig::Column::INT, ImGui::TableConfig::Column::INT };
		variableConf.rows.values = variableContents;
		variableConf.rows.display_min = guiSettings->component<GuiSettingsComponent>().m_shaderInspectorSettings.m_numVariablesRows.x;
		variableConf.rows.display_max = guiSettings->component<GuiSettingsComponent>().m_shaderInspectorSettings.m_numVariablesRows.y;
		variableConf.sort.p_sort = &EditorSettings::editorProperty<bool>(scene, guiSettings, "ShaderInspector_UniformBlockVars_Sort", true);
		variableConf.sort.p_col_id = (uint32_t*)&EditorSettings::editorProperty<int>(scene, guiSettings, "ShaderInspector_UniformBlockVars_ColId", 0);
		variableConf.sort.p_direction = (ImGui::TableConfig::Sort::Direction*) & EditorSettings::editorProperty<int>(scene, guiSettings, "ShaderInspector_UniformBlockVars_Direction", ImGui::TableConfig::Sort::Ascending);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ImGui::GetStyle().WindowPadding.x, 0.0f));
		ImGui::Table("Variables", variableConf);
		ImGui::PopStyleVar();
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateShaderInspectorShaderStorageBlocks(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		// Extract the program id
		GLuint program = scene.m_shaders[guiSettings->component<GuiSettingsComponent>().m_shaderInspectorSettings.m_currentProgram].m_program;

		// Get the list of uniforms
		auto blocks = GPU::inspectProgramShaderStorageBlocks(program);

		// Table contents
		std::vector<std::vector<std::string>> blockListContents;

		// Go through each row to and compute the text lengths
		for (auto const& block : blocks)
		{
			// Stringify the values
			std::vector<std::string> values =
			{
				std::to_string(block.m_binding),
				block.m_name,
				std::to_string(block.m_variables.size()),
				std::to_string(block.m_dataSize)
			};
			blockListContents.emplace_back(values);
		}

		// Generate the actual table
		ImGui::TableConfig blockConf;
		blockConf.cols.count = 4;
		blockConf.cols.headers = { "Bind", "Name", "Variables", "Size" };
		blockConf.cols.alignments = { ImGui::TableConfig::Column::Right, ImGui::TableConfig::Column::Left, ImGui::TableConfig::Column::Left, ImGui::TableConfig::Column::Left };
		blockConf.cols.data_types = { ImGui::TableConfig::Column::INT, ImGui::TableConfig::Column::STRING, ImGui::TableConfig::Column::INT, ImGui::TableConfig::Column::INT };
		blockConf.rows.values = blockListContents;
		blockConf.rows.display_min = guiSettings->component<GuiSettingsComponent>().m_shaderInspectorSettings.m_numBlockRows.x;
		blockConf.rows.display_max = guiSettings->component<GuiSettingsComponent>().m_shaderInspectorSettings.m_numBlockRows.y;
		blockConf.sort.p_sort = &EditorSettings::editorProperty<bool>(scene, guiSettings, "ShaderInspector_ShaderStorageBlocks_Sort", true);
		blockConf.sort.p_col_id = (uint32_t*)&EditorSettings::editorProperty<int>(scene, guiSettings, "ShaderInspector_ShaderStorageBlocks_ColId", 0);
		blockConf.sort.p_direction = (ImGui::TableConfig::Sort::Direction*) & EditorSettings::editorProperty<int>(scene, guiSettings, "ShaderInspector_ShaderStorageBlocks_Direction", ImGui::TableConfig::Sort::Ascending);
		blockConf.selection.enabled = true;
		blockConf.selection.row_id = &EditorSettings::editorProperty<int>(scene, guiSettings, "ShaderInspector_ShaderStorageBlockId");

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ImGui::GetStyle().WindowPadding.x, 0.0f));
		ImGui::Table("Blocks", blockConf);
		ImGui::PopStyleVar();

		// Which block to inspect
		std::vector<std::string> blockNames;
		for (auto const& block : blocks)
		{
			blockNames.push_back(block.m_name);
		}

		// Table contents
		std::vector<std::vector<std::string>> variableContents;

		// Go through each row to and compute the text lengths
		if (!blocks.empty())
		{
			for (auto const& variable : blocks[EditorSettings::editorProperty<int>(scene, guiSettings, "ShaderInspector_ShaderStorageBlockId")].m_variables)
			{
				// Stringify the values
				std::vector<std::string> values =
				{
					std::to_string(variable.m_byteOffset),
					getVariableName(scene, guiSettings, variable.m_name),
					GPU::dataTypeName(variable.m_type, variable.m_arraySize),
					std::to_string(variable.m_padding),
					variable.m_isArray ? std::to_string(variable.m_arrayStride) : "",
					std::to_string(variable.m_topLevelArrayStride)
				};
				variableContents.emplace_back(values);
			}
		}

		// Generate the actual table
		ImGui::TableConfig variableConf;
		variableConf.cols.count = 6;
		variableConf.cols.headers = { "Offset", "Name", "Type", "Padding", "Stride", "Owner Stride" };
		variableConf.cols.alignments = { ImGui::TableConfig::Column::Right, ImGui::TableConfig::Column::Left, ImGui::TableConfig::Column::Left, ImGui::TableConfig::Column::Left, ImGui::TableConfig::Column::Left, ImGui::TableConfig::Column::Left, ImGui::TableConfig::Column::Left };
		variableConf.cols.data_types = { ImGui::TableConfig::Column::INT, ImGui::TableConfig::Column::STRING, ImGui::TableConfig::Column::STRING, ImGui::TableConfig::Column::INT, ImGui::TableConfig::Column::INT, ImGui::TableConfig::Column::INT, ImGui::TableConfig::Column::INT };
		variableConf.rows.values = variableContents;
		variableConf.rows.display_min = guiSettings->component<GuiSettingsComponent>().m_shaderInspectorSettings.m_numVariablesRows.x;
		variableConf.rows.display_max = guiSettings->component<GuiSettingsComponent>().m_shaderInspectorSettings.m_numVariablesRows.y;
		variableConf.sort.p_sort = &EditorSettings::editorProperty<bool>(scene, guiSettings, "ShaderInspector_ShaderStorageBlockVars_Sort", true);
		variableConf.sort.p_col_id = (uint32_t*)&EditorSettings::editorProperty<int>(scene, guiSettings, "ShaderInspector_ShaderStorageBlockVars_ColId", 0);
		variableConf.sort.p_direction = (ImGui::TableConfig::Sort::Direction*) & EditorSettings::editorProperty<int>(scene, guiSettings, "ShaderInspector_ShaderStorageBlockVars_Direction", ImGui::TableConfig::Sort::Ascending);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ImGui::GetStyle().WindowPadding.x, 0.0f));
		ImGui::Table("Variables", variableConf);
		ImGui::PopStyleVar();
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateShaderInspectorInputs(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		// Extract the program id
		GLuint program = scene.m_shaders[guiSettings->component<GuiSettingsComponent>().m_shaderInspectorSettings.m_currentProgram].m_program;

		// Get the list of inputs
		auto inputs = GPU::inspectProgramInputs(program);

		// Table contents
		std::vector<std::vector<std::string>> contents;

		// Go through each row to and compute the text lengths
		for (auto const& input : inputs)
		{
			// Stringify the values
			std::vector<std::string> values =
			{
				input.m_location < 0 ? "" : std::to_string(input.m_location),
				getVariableName(scene, guiSettings, input.m_name),
				GPU::dataTypeName(input.m_type, input.m_arraySize),
			};
			contents.emplace_back(values);
		}

		// Generate the actual table
		ImGui::TableConfig conf;
		conf.cols.headers = { "Loc", "Name", "Type" };
		conf.cols.count = 3;
		conf.cols.alignments = { ImGui::TableConfig::Column::Right, ImGui::TableConfig::Column::Left, ImGui::TableConfig::Column::Left };
		conf.cols.data_types = { ImGui::TableConfig::Column::INT, ImGui::TableConfig::Column::STRING, ImGui::TableConfig::Column::INT };
		conf.rows.values = contents;
		conf.sort.p_sort = &EditorSettings::editorProperty<bool>(scene, guiSettings, "ShaderInspector_Inputs_Sort", true);
		conf.sort.p_col_id = (uint32_t*)&EditorSettings::editorProperty<int>(scene, guiSettings, "ShaderInspector_Inputs_ColId", 0);
		conf.sort.p_direction = (ImGui::TableConfig::Sort::Direction*) & EditorSettings::editorProperty<int>(scene, guiSettings, "ShaderInspector_Inputs_Direction", ImGui::TableConfig::Sort::Ascending);

		ImGui::Table("Inputs", conf);
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateShaderInspectorOutputs(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		// Extract the program id
		GLuint program = scene.m_shaders[guiSettings->component<GuiSettingsComponent>().m_shaderInspectorSettings.m_currentProgram].m_program;

		// Get the list of outputs
		auto outputs = GPU::inspectProgramOutputs(program);

		// Table contents
		std::vector<std::vector<std::string>> contents;

		// Go through each row to and compute the text lengths
		for (auto const& output : outputs)
		{
			// Stringify the values
			std::vector<std::string> values =
			{
				output.m_location < 0 ? "" : std::to_string(output.m_location),
				getVariableName(scene, guiSettings, output.m_name),
				GPU::dataTypeName(output.m_type, output.m_arraySize),
			};
			contents.emplace_back(values);
		}

		// Generate the actual table
		ImGui::TableConfig conf;
		conf.cols.headers = { "Loc", "Name", "Type" };
		conf.cols.count = 3;
		conf.cols.alignments = { ImGui::TableConfig::Column::Right, ImGui::TableConfig::Column::Left, ImGui::TableConfig::Column::Left };
		conf.cols.data_types = { ImGui::TableConfig::Column::INT, ImGui::TableConfig::Column::STRING, ImGui::TableConfig::Column::INT };
		conf.rows.values = contents;
		conf.sort.p_sort = &EditorSettings::editorProperty<bool>(scene, guiSettings, "ShaderInspector_Outputs_Sort", true);
		conf.sort.p_col_id = (uint32_t*)&EditorSettings::editorProperty<int>(scene, guiSettings, "ShaderInspector_Outputs_ColId", 0);
		conf.sort.p_direction = (ImGui::TableConfig::Sort::Direction*) & EditorSettings::editorProperty<int>(scene, guiSettings, "ShaderInspector_Outputs_Direction", ImGui::TableConfig::Sort::Ascending);

		ImGui::Table("Outputs", conf);
	}

	////////////////////////////////////////////////////////////////////////////////
	bool generateProgramSelector(Scene::Scene& scene, Scene::Object* guiSettings, std::string const& label, std::string& currentProgramId, float selectorHeight, ImGui::FilterRegex const& regex)
	{
		bool valueChanged = false;

		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::Text(label.c_str());
		ImGui::PopStyleColor();

		ImGuiWindowFlags selectorChildFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
		if (ImGui::BeginChild("ProgramSelector", ImVec2(0.0f, selectorHeight), true, selectorChildFlags))
		{
			for (auto const& program: scene.m_shaders)
			{
				const bool item_selected = (program.first == currentProgramId);

				if (regex.m_empty == false && !regex.test(program.first))
					continue;

				ImGui::PushID(program.first.c_str());

				if (ImGui::Selectable(program.first.c_str(), item_selected))
				{
					valueChanged |= true;
					currentProgramId = program.first;
				}

				if (item_selected)
					ImGui::SetItemDefaultFocus();

				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::Text(program.first.c_str());
					ImGui::EndTooltip();
				}

				ImGui::PopID();
			}
		}
		ImGui::EndChild();
		return valueChanged;
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateShaderInspectorWindow(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		Profiler::ScopedCpuPerfCounter perfCounter(scene, "Shader Inspector Window");

		// Window attributes
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
		ImGuiWindowFlags flags = 0;
		if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_lockLayout) flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

		// Create the window
		if (ImGui::Begin("Shader Inspector", nullptr, flags))
		{
			// Generate the settings editor
			if (ImGui::TreeNode("Settings"))
			{
				ImGui::SliderInt("Shader Selector Height", &guiSettings->component<GuiSettingsComponent>().m_shaderInspectorSettings.m_shaderSelectorHeight, 1, 600);
				ImGui::Combo("Variable Names", &guiSettings->component<GuiSettingsComponent>().m_shaderInspectorSettings.m_variableNames, ShaderInspectorSettings::VariableNameMethod_meta);
				ImGui::SliderInt2("Num Block Rows", glm::value_ptr(guiSettings->component<GuiSettingsComponent>().m_shaderInspectorSettings.m_numBlockRows), 0, 16);
				ImGui::SliderInt2("Num Variable Rows", glm::value_ptr(guiSettings->component<GuiSettingsComponent>().m_shaderInspectorSettings.m_numVariablesRows), 0, 16);
				ImGui::TreePop();
			}

			// Add some vertical space
			ImGui::Dummy(ImVec2(0.0f, 20.0f));

			// Current program id
			std::string& currentProgramId = guiSettings->component<GuiSettingsComponent>().m_shaderInspectorSettings.m_currentProgram;

			// Fall back to the GUI program if the old program no longer exists
			bool resetBlockIds = false;
			if (auto programIt = scene.m_shaders.find(currentProgramId); programIt == scene.m_shaders.end() || programIt->second.m_program == 0)
			{
				currentProgramId = "Imgui/imgui";
				resetBlockIds = true;
			}

			// Which program to debug
			resetBlockIds |= generateProgramSelector(scene, guiSettings, "Program", currentProgramId, guiSettings->component<GuiSettingsComponent>().m_shaderInspectorSettings.m_shaderSelectorHeight, guiSettings->component<GuiSettings::GuiSettingsComponent>().m_shaderInspectorSettings.m_selectorFilter);
			ImGui::RegexFilter("Filter", guiSettings->component<GuiSettings::GuiSettingsComponent>().m_shaderInspectorSettings.m_selectorFilter);

			// Reset the selected block ids when a significat change occurs
			if (resetBlockIds)
			{
				EditorSettings::editorProperty<int>(scene, guiSettings, "ShaderInspector_UniformBlockId") = 0;
				EditorSettings::editorProperty<int>(scene, guiSettings, "ShaderInspector_ShaderStorageBlockId") = 0;
			}

			// Add some vertical space
			ImGui::Dummy(ImVec2(0.0f, 20.0f));

			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			ImGui::Text(currentProgramId.c_str());
			ImGui::PopStyleColor();

			// Begin the content area
			if (ImGui::BeginChild("Properties", ImVec2(0.0f, 0.0f), true))
			{
				if (ImGui::BeginTabBar("Shader Properties Tab"))
				{
					// Restore the selected tab id
					std::string activeTab;
					if (auto activeTabSynced = EditorSettings::consumeEditorProperty<std::string>(scene, guiSettings, "ShaderInspectorPropertiesBar_SelectedTab#Synced"); activeTabSynced.has_value())
						activeTab = activeTabSynced.value();

					// Uniforms
					if (ImGui::BeginTabItem("Uniforms", activeTab.c_str()))
					{
						generateShaderInspectorUniforms(scene, guiSettings);

						EditorSettings::editorProperty<std::string>(scene, guiSettings, "ShaderInspectorPropertiesBar_SelectedTab") = ImGui::CurrentTabItemName();
						ImGui::EndTabItem();
					}

					// Uniform blocks
					if (ImGui::BeginTabItem("Uniform Blocks", activeTab.c_str()))
					{
						generateShaderInspectorUniformBlocks(scene, guiSettings);

						EditorSettings::editorProperty<std::string>(scene, guiSettings, "ShaderInspectorPropertiesBar_SelectedTab") = ImGui::CurrentTabItemName();
						ImGui::EndTabItem();
					}

					// Shader storage blocks
					if (ImGui::BeginTabItem("Shader Storage Blocks", activeTab.c_str()))
					{
						generateShaderInspectorShaderStorageBlocks(scene, guiSettings);

						EditorSettings::editorProperty<std::string>(scene, guiSettings, "ShaderInspectorPropertiesBar_SelectedTab") = ImGui::CurrentTabItemName();
						ImGui::EndTabItem();
					}

					// Program inputs
					if (ImGui::BeginTabItem("Inputs", activeTab.c_str()))
					{
						generateShaderInspectorInputs(scene, guiSettings);

						EditorSettings::editorProperty<std::string>(scene, guiSettings, "ShaderInspectorPropertiesBar_SelectedTab") = ImGui::CurrentTabItemName();
						ImGui::EndTabItem();
					}

					// Program outputs
					if (ImGui::BeginTabItem("Outputs", activeTab.c_str()))
					{
						generateShaderInspectorOutputs(scene, guiSettings);

						EditorSettings::editorProperty<std::string>(scene, guiSettings, "ShaderInspectorPropertiesBar_SelectedTab") = ImGui::CurrentTabItemName();
						ImGui::EndTabItem();
					}

					ImGui::EndTabBar();
				}
			}
			ImGui::EndChild();
		}

		ImGui::End();
	}

	////////////////////////////////////////////////////////////////////////////////
	bool generateTextureSelector(Scene::Scene& scene, Scene::Object* guiSettings, std::string const& label, std::string& currentTextureId, float selectorHeight, ImGui::FilterRegex const& regex)
	{
		bool valueChanged = false;

		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::Text(label.c_str());
		ImGui::PopStyleColor();

		ImGuiWindowFlags selectorChildFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
		if (ImGui::BeginChild("TextureSelector", ImVec2(0.0f, selectorHeight), true, selectorChildFlags))
		{
			for (auto const& texture : scene.m_textures)
			{
				const bool item_selected = (texture.first == currentTextureId);

				if (regex.m_empty == false && !regex.test(texture.first))
					continue;

				ImGui::PushID(texture.first.c_str());

				unsigned char alpha = texture.second.m_type == GL_TEXTURE_2D ? 255 : 0;
				if (ImGui::SelectableWithIcon(texture.first.c_str(), item_selected, &scene.m_textures[texture.first].m_texture, ImVec2(0, 1), ImVec2(1, 0), ImColor(255, 255, 255, alpha), ImColor(0, 0, 0, 0)))
				{
					valueChanged |= true;
					currentTextureId = texture.first;
				}

				if (item_selected)
					ImGui::SetItemDefaultFocus();

				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::Text(texture.first.c_str());
					ImGui::EndTooltip();
				}

				ImGui::PopID();
			}
		}
		ImGui::EndChild();
		return valueChanged;
	}

	////////////////////////////////////////////////////////////////////////////////
	bool generateTextureCombo(Scene::Scene& scene, std::string const& label, std::string& currentTextureId)
	{
		bool valueChanged = false;
		if (ImGui::BeginCombo(label.c_str(), currentTextureId.c_str()))
		{
			for (auto const& texture : scene.m_textures)
			{
				ImGui::PushID(texture.first.c_str());

				const bool item_selected = (texture.first == currentTextureId);

				unsigned char alpha = texture.second.m_type == GL_TEXTURE_2D ? 255 : 0;
				if (ImGui::SelectableWithIcon(texture.first.c_str(), item_selected, &scene.m_textures[texture.first].m_texture, ImVec2(0, 1), ImVec2(1, 0), ImColor(255, 255, 255, alpha), ImColor(0, 0, 0, 0)))
				{
					valueChanged |= true;
					currentTextureId = texture.first;
				}

				if (item_selected)
					ImGui::SetItemDefaultFocus();

				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::Text(texture.first.c_str());
					ImGui::EndTooltip();
				}

				ImGui::PopID();
			}
			ImGui::EndCombo();
		}
		return valueChanged;
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateMaterialEditorMaterialSelector(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		std::string& currentMaterialId = EditorSettings::editorProperty<std::string>(scene, guiSettings, "MaterialEditor_MaterialName");

		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::Text("Material");
		ImGui::PopStyleColor();

		ImGui::FilterRegex const& filter = guiSettings->component<GuiSettings::GuiSettingsComponent>().m_materialEditorSettings.m_selectorFilter;

		ImGuiWindowFlags selectorChildFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
		if (ImGui::BeginChild("TableContents", ImVec2(0.0f, guiSettings->component<GuiSettingsComponent>().m_materialEditorSettings.m_materialSelectorHeight), true, selectorChildFlags))
		{
			for (auto const& material : scene.m_materials)
			{
				const bool item_selected = (material.first == currentMaterialId);

				if (filter.m_empty == false && !filter.test(material.first))
					continue;

				ImGui::PushID(material.first.c_str());

				if (ImGui::Selectable(material.first.c_str(), item_selected))
				{
					currentMaterialId = material.first;
				}

				if (item_selected)
					ImGui::SetItemDefaultFocus();

				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::Text(material.first.c_str());
					ImGui::EndTooltip();
				}

				ImGui::PopID();
			}
		}
		ImGui::EndChild();
		ImGui::RegexFilter("Filter", guiSettings->component<GuiSettings::GuiSettingsComponent>().m_materialEditorSettings.m_selectorFilter);
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateMaterialEditorMaterialTexture(Scene::Scene& scene, Scene::Object* guiSettings, std::string const& name, std::string& texture, std::string const& defaultTexture)
	{
		ImGui::PushID(name.c_str());

		generateTextureCombo(scene, name.c_str(), texture);
		ImGui::SameLine();
		float cursorPosY = ImGui::GetCursorPosY();
		ImGui::SetCursorPosY(cursorPosY + ImGui::GetStyle().FramePadding.y);
		ImGui::SameLine();
		ImGui::SetCursorPosY(cursorPosY);
		if (ImGui::Button("Clear"))
			texture = defaultTexture;

		int previewHeight = guiSettings->component<GuiSettingsComponent>().m_materialEditorSettings.m_previewHeight;
		int tooltipHeight = guiSettings->component<GuiSettingsComponent>().m_materialEditorSettings.m_tooltipHeight;
		ImGui::Image(&scene.m_textures[texture].m_texture, ImVec2(previewHeight, previewHeight), ImVec2(0, 1), ImVec2(1, 0), ImColor(255, 255, 255, 255), ImColor(0, 0, 0, 0));
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::Text(texture.c_str());
			ImGui::Image(&scene.m_textures[texture].m_texture, ImVec2(tooltipHeight, tooltipHeight), ImVec2(0, 1), ImVec2(1, 0), ImColor(255, 255, 255, 255), ImColor(0, 0, 0, 0));
			ImGui::EndTooltip();
		}

		ImGui::PopID();
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateMaterialEditorMaterialEditor(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		std::string& currentMaterialId = EditorSettings::editorProperty<std::string>(scene, guiSettings, "MaterialEditor_MaterialName");

		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::Text(currentMaterialId.c_str());
		ImGui::PopStyleColor();

		if (ImGui::BeginChild("Properties", ImVec2(0.0f, 0.0f), true))
		{
			GPU::Material& material = scene.m_materials[currentMaterialId];

			ImGui::Combo("Blend Mode", &material.m_blendMode, GPU::Material::BlendMode_meta);

			ImGui::ColorEdit3("Diffuse", glm::value_ptr(material.m_diffuse));
			ImGui::ColorEdit3("Emissive", glm::value_ptr(material.m_emissive));

			ImGui::SliderFloat("Opacity", &material.m_opacity, 0.0f, 1.0f);
			ImGui::SliderFloat("Metallic", &material.m_metallic, 0.0f, 1.0f);
			ImGui::SliderFloat("Roughness", &material.m_roughness, 0.015f, 1.0f);
			ImGui::SliderFloat("Specular", &material.m_specular, 0.0f, 1.0f);
			ImGui::SliderFloat("Normal Map Strength", &material.m_normalMapStrength, 0.0f, 1.0f);
			ImGui::SliderFloat("Displacement Map Strength", &material.m_displacementScale, 0.0f, 1.0f);
			ImGui::Checkbox("Two-sided", &material.m_twoSided);

			generateMaterialEditorMaterialTexture(scene, guiSettings, "Diffuse Map", material.m_diffuseMap, "default_diffuse_map");
			generateMaterialEditorMaterialTexture(scene, guiSettings, "Normal Map", material.m_normalMap, "default_normal_map");
			generateMaterialEditorMaterialTexture(scene, guiSettings, "Specular Map", material.m_specularMap, "default_specular_map");
			generateMaterialEditorMaterialTexture(scene, guiSettings, "Alpha Map", material.m_alphaMap, "default_alpha_map");
			generateMaterialEditorMaterialTexture(scene, guiSettings, "Displacement Map", material.m_displacementMap, "default_displacement_map");
		}
		ImGui::EndChild();
	}

	////////////////////////////////////////////////////////////////////////////////
	void openMaterialEditorNewMaterialPopup(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		std::string& currentMaterialId = EditorSettings::editorProperty<std::string>(scene, guiSettings, "MaterialEditor_MaterialName");

		for (int i = 1;; ++i)
		{
			std::string matName = "Material " + std::to_string(i);
			if (scene.m_materials.find(matName) == scene.m_materials.end())
			{
				EditorSettings::editorProperty<std::string>(scene, guiSettings, "MaterialEditor_NewMaterialName", false) = matName;
				EditorSettings::editorProperty<std::string>(scene, guiSettings, "MaterialEditor_ReferenceMaterialName", false) = "";
				break;
			}
		}
		ImGui::OpenPopup("Mesh_NewMaterial");
	}

	////////////////////////////////////////////////////////////////////////////////
	void openMaterialEditorDuplicateMaterialPopup(Scene::Scene & scene, Scene::Object * guiSettings)
	{
		std::string& currentMaterialId = EditorSettings::editorProperty<std::string>(scene, guiSettings, "MaterialEditor_MaterialName");

		for (int i = 1;; ++i)
		{
			std::string matName = currentMaterialId + " " + std::to_string(i);
			if (scene.m_materials.find(matName) == scene.m_materials.end())
			{
				EditorSettings::editorProperty<std::string>(scene, guiSettings, "MaterialEditor_NewMaterialName", false) = matName;
				EditorSettings::editorProperty<std::string>(scene, guiSettings, "MaterialEditor_ReferenceMaterialName", false) = currentMaterialId;
				break;
			}
		}
		ImGui::OpenPopup("Mesh_NewMaterial");
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateMaterialEditorNewMaterialPopup(Scene::Scene & scene, Scene::Object * guiSettings)
	{
		std::string& currentMaterialId = EditorSettings::editorProperty<std::string>(scene, guiSettings, "MaterialEditor_MaterialName");

		ImGui::InputText("Material Name", EditorSettings::editorProperty<std::string>(scene, guiSettings, "MaterialEditor_NewMaterialName"));

		if (ImGui::ButtonEx("Ok", "|########|"))
		{
			std::string refMat = EditorSettings::editorProperty<std::string>(scene, guiSettings, "MaterialEditor_ReferenceMaterialName");
			scene.m_materials[EditorSettings::editorProperty<std::string>(scene, guiSettings, "MaterialEditor_NewMaterialName")] = refMat.empty() ? GPU::Material() : scene.m_materials[refMat];
			currentMaterialId = EditorSettings::editorProperty<std::string>(scene, guiSettings, "MaterialEditor_NewMaterialName");

			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::ButtonEx("Cancel", "|########|"))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateMaterialEditorWindow(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		Profiler::ScopedCpuPerfCounter perfCounter(scene, "Material Editor Window");

		// Window attributes
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
		ImGuiWindowFlags flags = 0;
		if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_lockLayout) flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

		// Forced editor focus
		if (EditorSettings::consumeEditorProperty<bool>(scene, guiSettings, "MaterialEditor_ForceFocus").has_value())
		{
			ImGui::SetNextWindowFocus();
		}

		// Create the window
		if (ImGui::Begin("Material Editor", nullptr, flags))
		{
			// Generate the settings editor
			if (ImGui::TreeNode("Settings"))
			{
				ImGui::SliderInt("Material Selector Height", &guiSettings->component<GuiSettingsComponent>().m_materialEditorSettings.m_materialSelectorHeight, 1, 600);
				ImGui::SliderInt("Preview Image Height", &guiSettings->component<GuiSettingsComponent>().m_materialEditorSettings.m_previewHeight, 1, 2048);
				ImGui::SliderInt("Tooltip Image Height", &guiSettings->component<GuiSettingsComponent>().m_materialEditorSettings.m_tooltipHeight, 1, 2048);
				ImGui::TreePop();
			}

			// Add some vertical space
			ImGui::Dummy(ImVec2(0.0f, 20.0f));

			// Fall back to the first shader
			std::string& currentMaterialId = EditorSettings::editorProperty<std::string>(scene, guiSettings, "MaterialEditor_MaterialName");
			auto materialIt = scene.m_materials.find(currentMaterialId);
			if (materialIt == scene.m_materials.end())
			{
				currentMaterialId = scene.m_materials.begin()->first;
			}

			// Which material to edit
			generateMaterialEditorMaterialSelector(scene, guiSettings);

			// New material button
			if (ImGui::ButtonEx("New", "|#########|"))
			{
				openMaterialEditorNewMaterialPopup(scene, guiSettings);
			}

			// Duplicate material
			ImGui::SameLine();
			if (ImGui::ButtonEx("Duplicate", "|#########|"))
			{
				openMaterialEditorDuplicateMaterialPopup(scene, guiSettings);
			}

			// Make a new or duplicate material
			if (ImGui::BeginPopup("Mesh_NewMaterial"))
			{
				generateMaterialEditorNewMaterialPopup(scene, guiSettings);
			}

			// Add some vertical space
			ImGui::Dummy(ImVec2(0.0f, 20.0f));

			// Begin the content area
			generateMaterialEditorMaterialEditor(scene, guiSettings);
		}

		ImGui::End();
	}

	////////////////////////////////////////////////////////////////////////////////
	void startEditingMaterial(Scene::Scene& scene, std::string const& materialName)
	{
		Scene::Object* guiSettings = Scene::findFirstObject(scene, Scene::OBJECT_TYPE_GUI_SETTINGS);

		EditorSettings::editorProperty<std::string>(scene, guiSettings, "MaterialEditor_MaterialName") = materialName;
		EditorSettings::editorProperty<bool>(scene, guiSettings, "MaterialEditor_ForceFocus") = true;
	}

	////////////////////////////////////////////////////////////////////////////////
	bool generateBufferSelector(Scene::Scene& scene, Scene::Object* guiSettings, std::string const& label, std::string& currentBufferId, float selectorHeight, ImGui::FilterRegex const& regex)
	{
		bool valueChanged = false;

		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::Text(label.c_str());
		ImGui::PopStyleColor();

		ImGuiWindowFlags selectorChildFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
		if (ImGui::BeginChild("BufferSelector", ImVec2(0.0f, selectorHeight), true, selectorChildFlags))
		{
			for (auto const& buffer : scene.m_genericBuffers)
			{
				const bool item_selected = (buffer.first == currentBufferId);

				if (regex.m_empty == false && !regex.test(buffer.first))
					continue;

				ImGui::PushID(buffer.first.c_str());

				if (ImGui::Selectable(buffer.first.c_str(), item_selected))
				{
					valueChanged |= true;
					currentBufferId = buffer.first;
				}

				if (item_selected)
					ImGui::SetItemDefaultFocus();

				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::Text(buffer.first.c_str());
					ImGui::EndTooltip();
				}

				ImGui::PopID();
			}
		}
		ImGui::EndChild();
		return valueChanged;
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateBufferInspectorWindow(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		Profiler::ScopedCpuPerfCounter perfCounter(scene, "Buffer Inspector Window");

		// Window attributes
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
		ImGuiWindowFlags flags = 0;
		if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_lockLayout) flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

		// Create the window
		if (ImGui::Begin("Buffer Inspector", nullptr, flags))
		{
			// Generate the settings editor
			if (ImGui::TreeNode("Settings"))
			{
				ImGui::SliderInt("Buffer Selector Height", &guiSettings->component<GuiSettingsComponent>().m_bufferInspectorSettings.m_bufferSelectorHeight, 1, 600);
				ImGui::TreePop();
			}

			// Add some vertical space
			ImGui::Dummy(ImVec2(0.0f, 20.0f));

			// Current program id
			std::string& currentBufferId = guiSettings->component<GuiSettingsComponent>().m_bufferInspectorSettings.m_currentBuffer;

			// Fall back to the GUI program if the old program no longer exists
			if (auto bufferIt = scene.m_genericBuffers.find(currentBufferId); bufferIt == scene.m_genericBuffers.end() || bufferIt->second.m_buffer == 0)
			{
				currentBufferId = scene.m_genericBuffers.begin()->first;
			}

			// Which program to debug
			bool bufferChanged = generateBufferSelector(scene, guiSettings, "Buffer", currentBufferId, guiSettings->component<GuiSettingsComponent>().m_bufferInspectorSettings.m_bufferSelectorHeight, guiSettings->component<GuiSettings::GuiSettingsComponent>().m_bufferInspectorSettings.m_selectorFilter);
			ImGui::RegexFilter("Filter", guiSettings->component<GuiSettings::GuiSettingsComponent>().m_bufferInspectorSettings.m_selectorFilter);

			// Reset the buffer flag
			if (bufferChanged)
			{
				guiSettings->component<GuiSettingsComponent>().m_bufferInspectorSettings.m_displayContents = false;
			}

			// Add some vertical space
			ImGui::Dummy(ImVec2(0.0f, 20.0f));

			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			ImGui::Text(currentBufferId.c_str());
			ImGui::PopStyleColor();

			#define ADD_LABEL(LABEL, COLUMNS) \
				ImGui::Columns(1); \
				ImGui::Dummy(ImVec2(0.0f, 15.0f)); \
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)); \
				ImGui::Text(LABEL); \
				ImGui::PopStyleColor(); \
				ImGui::Columns(COLUMNS, nullptr, ImGuiColumnsFlags_NoBorder)
			#define ADD_ROW(NAME, FORMAT, ...) { ImGui::Text(NAME); ImGui::NextColumn(); ImGui::Text(FORMAT, __VA_ARGS__); } ImGui::NextColumn()
			#define ADD_ROW_STR(NAME, FORMAT, STR) { std::string const& tmp = STR; ImGui::Text(NAME); ImGui::NextColumn(); ImGui::Text(FORMAT, tmp.c_str()); } ImGui::NextColumn()
			#define ADD_ROW_BOOL(NAME, FORMAT, B) { ImGui::Text(NAME); ImGui::NextColumn(); ImGui::Text(FORMAT, B ? "true" : "false"); } ImGui::NextColumn()
			#define ADD_ROW_FLAGS(NAME, FORMAT, FLAGS, FLAG) { ImGui::Text(NAME); ImGui::NextColumn(); ImGui::Text(FORMAT, ((FLAGS & FLAG) != 0) ? "true" : "false"); } ImGui::NextColumn()

			// Typeset the buffer properties
			if (ImGui::BeginChild("Properties", ImVec2(0.0f, 0.0f), true))
			{
				GPU::GenericBuffer& buffer = scene.m_genericBuffers[currentBufferId];

				ADD_LABEL("Common", 2);
				ADD_ROW_STR("Type", "%s", GPU::enumToString(buffer.m_bufferType));
				ADD_ROW("Binding", "%d", buffer.m_bindingId);
				ADD_ROW_STR("Size", "%s", Units::bytesToString(buffer.m_size));
				ADD_ROW_STR("Element Size", "%s", Units::bytesToString(buffer.m_elementSize));
				ADD_ROW_BOOL("Indexed", "%s", buffer.m_indexed);
				ADD_ROW_BOOL("Immutable", "%s", buffer.m_immutable);

				ADD_LABEL("Flags", 2);
				ADD_ROW_FLAGS("Dynamic Storage", "%s", buffer.m_flags, GL_DYNAMIC_STORAGE_BIT);
				ADD_ROW_FLAGS("Client Storage", "%s", buffer.m_flags, GL_CLIENT_STORAGE_BIT);
				ADD_ROW_FLAGS("Map Read", "%s", buffer.m_flags, GL_MAP_READ_BIT);
				ADD_ROW_FLAGS("Map Write", "%s", buffer.m_flags, GL_MAP_WRITE_BIT);
				ADD_ROW_FLAGS("Map Persistent", "%s", buffer.m_flags, GL_MAP_PERSISTENT_BIT);
				ADD_ROW_FLAGS("Map Coherent", "%s", buffer.m_flags, GL_MAP_COHERENT_BIT);
			}
			ImGui::EndChild();

			// Add some vertical space
			ImGui::Dummy(ImVec2(0.0f, 20.0f));

			// Typeset the buffer contents
			if (ImGui::BeginChild("BufferContents", ImVec2(0.0f, 0.0f), true))
			{
			}
			ImGui::EndChild();
		}

		ImGui::End();
	}

	////////////////////////////////////////////////////////////////////////////////
	void updateTextureInspectorTextures(Scene::Scene& scene, Scene::Object* guiSettings, std::string const& currentTextureName, std::string const& previewTextureName)
	{
		// Extract the texture objects
		auto& previewTexture = scene.m_textures[previewTextureName];
		auto& currentTexture = scene.m_textures[currentTextureName];

		// Visualize it
		const size_t arraySlice = guiSettings->component<GuiSettingsComponent>().m_textureInspectorSettings.m_arraySliceId;
		const size_t lodLevel = guiSettings->component<GuiSettingsComponent>().m_textureInspectorSettings.m_lodLevelId;
		const glm::ivec3 textureSize = GPU::mipDimensions(currentTexture, lodLevel);

		// Generate a properly sized texture, if needed
		if (previewTexture.m_width != textureSize[0] || previewTexture.m_height != textureSize[1] ||
			previewTexture.m_layout != currentTexture.m_layout || previewTexture.m_format != currentTexture.m_format)
		{
			Scene::createTexture(scene, previewTextureName, GL_TEXTURE_2D,
				textureSize[0], textureSize[1], 1,
				currentTexture.m_format, currentTexture.m_layout,
				GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE);
		}

		// Upload the preview texture if the display changed
		if (currentTexture.m_type == GL_TEXTURE_1D)
		{
			// TODO: implement
		}
		else if (currentTexture.m_type == GL_TEXTURE_1D_ARRAY)
		{
			// TODO: implement
		}
		else if (currentTexture.m_type == GL_TEXTURE_2D)
		{
			glCopyImageSubData(
				currentTexture.m_texture, currentTexture.m_type, lodLevel, 0, 0, 0,
				previewTexture.m_texture, previewTexture.m_type, 0, 0, 0, 0, previewTexture.m_width, previewTexture.m_height, 1);
		}
		else if (currentTexture.m_type == GL_TEXTURE_2D_ARRAY || currentTexture.m_type == GL_TEXTURE_3D)
		{
			glCopyImageSubData(
				currentTexture.m_texture, currentTexture.m_type, lodLevel, 0, 0, arraySlice,
				previewTexture.m_texture, previewTexture.m_type, 0, 0, 0, 0, previewTexture.m_width, previewTexture.m_height, 1);
		}
	}
	std::string getFileDialogFilters(Scene::Scene& scene)
	{
		// List of formats and the corresponding file filters
		using FormatDescription = std::string;
		using FormatFilter = std::vector<std::string>;
		using Format = std::pair<FormatDescription, FormatFilter>;
		static std::vector<Format> s_imageFormats =
		{
			{ "BMP - Windows Bitmap", { ".bmp" } },
			{ "JPG - JPG/JPEG Format", { ".jpg", ".jpeg" } },
			{ "PCX - Zsoft Painbrush", { ".pcx" } },
			{ "PNG - Portable Network Graphics", { ".png" } },
			{ "RAW - Raw Image Data", { ".raw" } },
			{ "TGA - Truevision Target", { ".tga" } },
			{ "TIF - Tagged Image File Format", { ".tif" } },
		};

		// Resulting str
		static std::string s_result;

		// Construct the result
		if (s_result.empty())
		{
			// Construct the "all image format" filter
			FormatFilter filter;
			for (auto const& format : s_imageFormats)
				filter.insert(filter.end(), format.second.begin(), format.second.end());
			s_imageFormats.insert(s_imageFormats.begin(), { "Any Image Format", filter });

			// Join the filters together for the result
			s_result = std::string_join(",", s_imageFormats.begin(), s_imageFormats.end(), [](Format const& format)
			{
				return format.first + " (" + std::string_join(" ", format.second.begin(), format.second.end()) + "){" + std::string_join(",", format.second.begin(), format.second.end()) + "}";
			});
		}

		return s_result;
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateTextureInspectorOpenFileDialog(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		// Extract the file dialog object
		auto fileDialog = igfd::ImGuiFileDialog::Instance();

		// Texture name and path
		std::string& textureName = EditorSettings::editorProperty<std::string>(scene, guiSettings, "LoadTexture_TextureName", false);
		std::string& texturePath = EditorSettings::editorProperty<std::string>(scene, guiSettings, "LoadTexture_TexturePath", false);

		// Open the file dialog
		if (ImGui::Button("Load Texture"))
		{
			ImGui::OpenPopup("LoadTextureDialog");
			textureName = texturePath = "";
		}

		if (ImGui::BeginPopup("LoadTextureDialog"))
		{
			//ImGui::Combo("Texture Type", );
			ImGui::InputText("Texture Name", textureName);
			ImGui::InputText("Texture Path", texturePath);
			ImGui::SameLine();
			if (ImGui::Button("Browse"))
			{
				std::string filters = getFileDialogFilters(scene);
				Debug::log_debug() << "Texture type filters: " << filters << Debug::end;

				fileDialog->OpenModal("LoadTextureFileSelectDialog", "Select texture...", filters.c_str(), "");

				std::string const& texturesFolder = (EnginePaths::assetsFolder() / "Textures").string();
				Debug::log_debug() << "Textures folder: " << texturesFolder << Debug::end;
				fileDialog->SetPath(texturesFolder);
			}

			// Generate the dialog itself
			if (fileDialog->FileDialog("LoadTextureFileSelectDialog"))
			{
				if (fileDialog->IsOk)
				{
					texturePath = fileDialog->GetFilePathName();
					textureName = std::filesystem::path(texturePath).filename().string();
				}
				fileDialog->CloseDialog("LoadTextureFileSelectDialog");
			}

			// Try to load the texture
			if (ImGui::ButtonEx("Ok", "|########|"))
			{
				Asset::loadTexture(scene, textureName, texturePath);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::ButtonEx("Cancel", "|########|"))
			{
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateTextureInspectorContentArea(Scene::Scene& scene, Scene::Object* guiSettings, bool displayChanged)
	{
		// Name of the current texture
		std::string& currentTextureId = EditorSettings::editorProperty<std::string>(scene, guiSettings, "TextureInspector_CurrentTexture");

		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::Text(currentTextureId.c_str());
		ImGui::PopStyleColor();

		if (!ImGui::BeginChild("Properties", ImVec2(0.0f, 0.0f), true))
		{
			ImGui::EndChild();
			return;
		}
		
		// Visualize the texture
		std::string const& previewTextureName = guiSettings->m_name + "_texture_inspector_preview";
		auto& previewTexture = scene.m_textures[previewTextureName];
		auto& currentTexture = scene.m_textures[currentTextureId];

		if (currentTexture.m_type == GL_TEXTURE_3D || currentTexture.m_type == GL_TEXTURE_2D_ARRAY)
		{
			displayChanged |= ImGui::SliderInt("Array Slice", &guiSettings->component<GuiSettingsComponent>().m_textureInspectorSettings.m_arraySliceId, 0, currentTexture.m_depth - 1);
		}
		displayChanged |= ImGui::SliderInt("LOD", &guiSettings->component<GuiSettingsComponent>().m_textureInspectorSettings.m_lodLevelId, 0, GPU::numMipLevels(currentTexture));
		displayChanged |= ImGui::SliderFloat("Zoom Factor", &guiSettings->component<GuiSettingsComponent>().m_textureInspectorSettings.m_zoomFactor, 1.0f, 32.0f);
		if (ImGui::Button("Export"))
		{
			Asset::saveTexture(scene, previewTextureName, EnginePaths::generateUniqueFilepath("TextureInspector/" + currentTextureId + "_", ".png"));
		}

		// Update the texture, if needed
		if (displayChanged) updateTextureInspectorTextures(scene, guiSettings, currentTextureId, previewTextureName);

		#define ADD_LABEL(LABEL, COLUMNS) \
					ImGui::Columns(1); \
					ImGui::Dummy(ImVec2(0.0f, 15.0f)); \
					ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)); \
					ImGui::Text(LABEL); \
					ImGui::PopStyleColor(); \
					ImGui::Columns(COLUMNS, nullptr, ImGuiColumnsFlags_NoBorder)
		#define ADD_ROW(NAME, FORMAT, ...) { ImGui::Text(NAME); ImGui::NextColumn(); ImGui::Text(FORMAT, __VA_ARGS__); } ImGui::NextColumn()
		#define ADD_ROW_STR(NAME, FORMAT, STR) { std::string const& tmp = STR; ImGui::Text(NAME); ImGui::NextColumn(); ImGui::Text(FORMAT, tmp.c_str()); } ImGui::NextColumn()

		ImGui::Columns(2, "ContentTable", ImGuiColumnsFlags_NoBorder);
		if (ImGui::BeginChild("Texture Parameters", ImVec2(0, guiSettings->component<GuiSettingsComponent>().m_textureInspectorSettings.m_previewHeight), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
		{
			const size_t texels = currentTexture.m_width * currentTexture.m_height * currentTexture.m_depth;
			const size_t channels = GPU::textureFormatChannels(currentTexture.m_format);
			const size_t texelSize = (GPU::textureFormatTexelSize(currentTexture.m_format) + 7) / 8;

			ADD_LABEL("Dimensions", 2);
			ADD_ROW("Width", "%d", currentTexture.m_width);
			ADD_ROW("Height", "%d", currentTexture.m_height);
			ADD_ROW("Depth", "%d", currentTexture.m_depth);
			ADD_ROW("Channels", "%d", channels);
			ADD_ROW("Texels", "%d", texels);
			ADD_ROW_STR("Texture Size", "%s", Units::bytesToString(texels * texelSize));

			ADD_LABEL("Type", 2);
			ADD_ROW_STR("Type", "%s", GPU::enumToString(currentTexture.m_type));
			ADD_ROW_STR("Format", "%s", GPU::enumToString(currentTexture.m_format));
			ADD_ROW_STR("Layout", "%s", GPU::enumToString(currentTexture.m_layout));
			ADD_ROW("Binding ID", "%d", (currentTexture.m_bindingId > 0 ? currentTexture.m_bindingId - GL_TEXTURE0 : 0));

			ADD_LABEL("Parameters", 2);
			ADD_ROW_STR("Min Filter", "%s", GPU::enumToString(currentTexture.m_minFilter));
			ADD_ROW_STR("Mag Filter", "%s", GPU::enumToString(currentTexture.m_magFilter));
			ADD_ROW_STR("Wrap Mode", "%s", GPU::enumToString(currentTexture.m_wrapMode));
			ADD_ROW("Binding ID", "%d", (currentTexture.m_bindingId > 0 ? currentTexture.m_bindingId - GL_TEXTURE0 : 0));
		}
		ImGui::EndChild();
		ImGui::NextColumn();

		if (ImGui::BeginChild("Texture Preview", ImVec2(0, ImGui::GetContentRegionMax().y - ImGui::GetCursorPos().y), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar))
		{
			// Display the actual image
			ADD_LABEL("Preview", 1);

			const float aspect = float(previewTexture.m_width) / float(previewTexture.m_height);
			const int imageSize = guiSettings->component<GuiSettingsComponent>().m_textureInspectorSettings.m_previewHeight;
			const float zoomUvScale = 1.0f / guiSettings->component<GuiSettingsComponent>().m_textureInspectorSettings.m_zoomFactor;
			ImGui::Image(&previewTexture.m_texture, ImVec2(imageSize * aspect, imageSize), ImVec2(0, 1) * zoomUvScale, ImVec2(1, 0) * zoomUvScale, ImColor(255, 255, 255, 255), ImColor(0, 0, 0, 0));

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				int imageSize = guiSettings->component<GuiSettingsComponent>().m_textureInspectorSettings.m_tooltipHeight;
				ImGui::Image(&previewTexture.m_texture, ImVec2(imageSize * aspect, imageSize), ImVec2(0, 1) * zoomUvScale, ImVec2(1, 0) * zoomUvScale, ImColor(255, 255, 255, 255), ImColor(0, 0, 0, 0));
				ImGui::EndTooltip();
			}
		}
		ImGui::EndChild();
		ImGui::NextColumn();
		ImGui::Columns(1);
		ImGui::EndChild();
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateTextureInspectorWindow(Scene::Scene & scene, Scene::Object* guiSettings)
	{
		Profiler::ScopedCpuPerfCounter perfCounter(scene, "Texture Inspector Window");

		// Window attributes
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
		ImGuiWindowFlags flags = 0;
		if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_lockLayout) flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

		// Name of the current texture
		std::string& currentTextureId = EditorSettings::editorProperty<std::string>(scene, guiSettings, "TextureInspector_CurrentTexture");

		// Whether the display changed or not
		bool displayChanged = guiSettings->component<GuiSettingsComponent>().m_textureInspectorSettings.m_liveUpdate;

		// Create the window
		if (ImGui::Begin("Texture Inspector", nullptr, flags))
		{
			// Generate the settings editor
			if (ImGui::TreeNode("Settings"))
			{
				ImGui::SliderInt("Texture Selector Height", &guiSettings->component<GuiSettingsComponent>().m_textureInspectorSettings.m_textureSelectorHeight, 1, 600);
				ImGui::SliderInt("Preview Image Height", &guiSettings->component<GuiSettingsComponent>().m_textureInspectorSettings.m_previewHeight, 1, 2048);
				ImGui::SliderInt("Tooltip Image Height", &guiSettings->component<GuiSettingsComponent>().m_textureInspectorSettings.m_tooltipHeight, 1, 2048);
				displayChanged |= ImGui::SliderInt("1D Block Height", &guiSettings->component<GuiSettingsComponent>().m_textureInspectorSettings.m_1dTextureBlockSize, 1, 1024);
				displayChanged |= ImGui::SliderInt("1D Array Block Height", &guiSettings->component<GuiSettingsComponent>().m_textureInspectorSettings.m_1dArrayTextureBlockSize, 1, 256);
				ImGui::Checkbox("Live Update", &guiSettings->component<GuiSettingsComponent>().m_textureInspectorSettings.m_liveUpdate);

				ImGui::TreePop();
			}
			
			// Add some vertical space
			ImGui::Dummy(ImVec2(0.0f, 20.0f));

			// Fall back to the font atlas if the old texture no longer exists
			if (auto textureIt = scene.m_textures.find(currentTextureId); textureIt == scene.m_textures.end() || textureIt->second.m_texture == 0)
			{
				currentTextureId = "ImguiFont";
			}

			// Generate the texture selector
			displayChanged |= generateTextureSelector(scene, guiSettings, "Texture", currentTextureId, guiSettings->component<GuiSettingsComponent>().m_textureInspectorSettings.m_textureSelectorHeight, guiSettings->component<GuiSettings::GuiSettingsComponent>().m_textureInspectorSettings.m_selectorFilter);
			ImGui::RegexFilter("Filter", guiSettings->component<GuiSettings::GuiSettingsComponent>().m_textureInspectorSettings.m_selectorFilter);

			// Generate the open file dialog
			generateTextureInspectorOpenFileDialog(scene, guiSettings);

			// Add some vertical space
			ImGui::Dummy(ImVec2(0.0f, 20.0f));

			// Generate the content area
			generateTextureInspectorContentArea(scene, guiSettings, displayChanged);
		}
		ImGui::End();
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateMainRenderWindow(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		// Get the render settings object
		Scene::Object* simulationSettings = Scene::findFirstObject(scene, Scene::OBJECT_TYPE_SIMULATION_SETTINGS);
		Scene::Object* renderSettings = Scene::findFirstObject(scene, Scene::OBJECT_TYPE_RENDER_SETTINGS);

		// Push some necessary style settings
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		// Create the main window
		ImGui::SetNextWindowSize(ImVec2(512, 512), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Main Render", nullptr, ImGuiWindowFlags_NoCollapse))
		{
			// Extract the necessary sizes
			glm::vec2 resolution = renderSettings->component<RenderSettings::RenderSettingsComponent>().m_resolution;
			glm::vec2 displaySize = { ImGui::GetWindowSize().x, ImGui::GetWindowSize().y };
			glm::vec2 blitImageSize = displaySize;
			float renderAspectRatio = resolution.x / resolution.y;
			float displayAspectRatio = displaySize.x / displaySize.y;

			// Limit the display aspect ratio
			if (guiSettings->component<GuiSettings::GuiSettingsComponent>().m_blitFixedAspectRatio)
			{
				glm::vec2 resolutionToDisplayRatio = resolution / displaySize;
				if (resolutionToDisplayRatio.x > resolutionToDisplayRatio.y)
				{
					//blitImageSize = glm::vec2(displaySize.y * renderAspectRatio, displaySize.y);
					blitImageSize = glm::vec2(displaySize.x, displaySize.x / renderAspectRatio);
				}
				else
				{
					//blitImageSize = glm::vec2(displaySize.x, displaySize.x / renderAspectRatio);
					blitImageSize = glm::vec2(displaySize.y * renderAspectRatio, displaySize.y);
				}
			}
			else
			{
				if (displayAspectRatio < guiSettings->component<GuiSettings::GuiSettingsComponent>().m_blitAspectConstraint.x || displayAspectRatio > guiSettings->component<GuiSettings::GuiSettingsComponent>().m_blitAspectConstraint.y)
				{
					float correctedAspectRatio = glm::min(glm::max(displayAspectRatio, guiSettings->component<GuiSettings::GuiSettingsComponent>().m_blitAspectConstraint.x), guiSettings->component<GuiSettings::GuiSettingsComponent>().m_blitAspectConstraint.y);
					float correctedWidth = glm::min(displaySize.y * correctedAspectRatio, displaySize.x);
					blitImageSize = glm::vec2(correctedWidth, correctedWidth / correctedAspectRatio);
				}
			}

			// End UV coordinates
			ImVec2 uv = ImVec2{ resolution.x / scene.m_textures["ImGui_MainRenderTarget"].m_width, resolution.y / scene.m_textures["ImGui_MainRenderTarget"].m_height };

			// Copy over the main scene texture contents to the intermediate buffer
			glDepthMask(GL_TRUE);
			glDisable(GL_BLEND);
			glDisable(GL_SCISSOR_TEST);
			glDisable(GL_DEPTH_TEST);
			RenderSettings::bindGbufferLayerOpenGL(scene, simulationSettings, renderSettings, 
				RenderSettings::GB_WriteBuffer, RenderSettings::GB_ReadBuffer, 0, GL_READ_FRAMEBUFFER);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, scene.m_textures["ImGui_MainRenderTarget"].m_framebuffer);
			glReadBuffer(GL_COLOR_ATTACHMENT0);
			glDrawBuffer(GL_COLOR_ATTACHMENT0);
			glBlitFramebuffer(0, 0, resolution.x, resolution.y, 0, 0, resolution.x, resolution.y, GL_COLOR_BUFFER_BIT, GL_LINEAR);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

			// Fill the alpha with 1s
			glBindFramebuffer(GL_FRAMEBUFFER, scene.m_textures["ImGui_MainRenderTarget"].m_framebuffer);
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			// Display the image
			ImGui::SetCursorPosX((displaySize.x - blitImageSize.x) * 0.5f);
			ImGui::SetCursorPosY((displaySize.y - blitImageSize.y) * 0.5f);
			ImGui::Image(&scene.m_textures["ImGui_MainRenderTarget"].m_texture, ImVec2(blitImageSize.x, blitImageSize.y), ImVec2(0.0f, uv.y), ImVec2(uv.x, 0.0f));
		}

		// Reset the settings
		ImGui::PopStyleVar();

		// End the window immediately
		ImGui::End();
	}

	////////////////////////////////////////////////////////////////////////////////
	struct GuiStateVars
	{
		std::unordered_map<std::string, bool*> m_bools;
		std::unordered_map<std::string, int*> m_ints;
		std::unordered_map<std::string, float*> m_floats;
		std::unordered_map<std::string, std::string*> m_strings;
	};

	////////////////////////////////////////////////////////////////////////////////
	GuiStateVars getGuiStateVars(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		GuiStateVars result;

		// Global theme
		result.m_floats["GUI_FontScale"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_fontScale;
		result.m_strings["GUI_Font"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_font;
		result.m_strings["GUI_Theme"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_guiStyle;
		result.m_bools["GUI_LockLayout"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_lockLayout;

		// Show gui elements
		for (auto& element : guiSettings->component<GuiSettings::GuiSettingsComponent>().m_guiElements)
			result.m_bools[element.m_name] = &element.m_open;

		// Log output
		result.m_strings["Log_IncludeFilter"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_includeFilter.m_text;
		result.m_strings["Log_ExcludeFilter"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_discardFilter.m_text;

		result.m_bools["Log_ShowTrace"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showTrace;
		result.m_bools["Log_ShowDebug"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showDebug;
		result.m_bools["Log_ShowInfo"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showInfo;
		result.m_bools["Log_ShowWarning"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showWarning;
		result.m_bools["Log_ShowError"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showError;
		result.m_bools["Log_ShowDate"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showDate;
		result.m_bools["Log_ShowSeverity"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showSeverity;
		result.m_bools["Log_ShowRegion"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_showRegion;
		result.m_bools["Log_AutoScroll"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_autoScroll;
		result.m_bools["Log_LongMessages"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_longMessages;
		result.m_floats["Log_MessageSpacing"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_messageSpacing;
		result.m_floats["Log_TraceColorX"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_traceColor.x;
		result.m_floats["Log_TraceColorY"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_traceColor.y;
		result.m_floats["Log_TraceColorZ"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_traceColor.z;
		result.m_floats["Log_DebugColorX"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_debugColor.x;
		result.m_floats["Log_DebugColorY"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_debugColor.y;
		result.m_floats["Log_DebugColorZ"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_debugColor.z;
		result.m_floats["Log_InfoColorX"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_infoColor.x;
		result.m_floats["Log_InfoColorY"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_infoColor.y;
		result.m_floats["Log_InfoColorZ"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_infoColor.z;
		result.m_floats["Log_WarningColorX"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_warningColor.x;
		result.m_floats["Log_WarningColorY"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_warningColor.y;
		result.m_floats["Log_WarningColorZ"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_warningColor.z;
		result.m_floats["Log_ErrorColorX"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_errorColor.x;
		result.m_floats["Log_ErrorColorY"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_errorColor.y;
		result.m_floats["Log_ErrorColorZ"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_errorColor.z;

		// Stats
		result.m_bools["Stats_ShowCurrent"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_showCurrent;
		result.m_bools["Stats_ShowAvg"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_showAvg;
		result.m_bools["Stats_ShowMin"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_showMin;
		result.m_bools["Stats_ShowMax"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_showMax;
		result.m_bools["Stats_ShowCount"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_showCount;
		result.m_floats["Stats_IndentScale"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_indentScale;

		// Shader inspector
		result.m_strings["ShaderInspector_Filter"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_shaderInspectorSettings.m_selectorFilter.m_text;
		result.m_strings["ShaderInspector_CurrentProgram"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_shaderInspectorSettings.m_currentProgram;
		result.m_ints["ShaderInspector_VariableNames"] = (int*)&guiSettings->component<GuiSettings::GuiSettingsComponent>().m_shaderInspectorSettings.m_variableNames;
		result.m_ints["ShaderInspector_NumBlockRowsX"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_shaderInspectorSettings.m_numBlockRows.x;
		result.m_ints["ShaderInspector_NumBlockRowsY"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_shaderInspectorSettings.m_numBlockRows.y;
		result.m_ints["ShaderInspector_NumVariableRowsX"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_shaderInspectorSettings.m_numVariablesRows.x;
		result.m_ints["ShaderInspector_NumVariableRowsY"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_shaderInspectorSettings.m_numVariablesRows.y;
		result.m_ints["ShaderInspector_SelectorHeight"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_shaderInspectorSettings.m_shaderSelectorHeight;

		// Buffer inspector
		result.m_strings["BufferInspector_Filter"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_bufferInspectorSettings.m_selectorFilter.m_text;
		result.m_strings["BufferInspector_CurrentBuffer"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_bufferInspectorSettings.m_currentBuffer;
		result.m_ints["BufferInspector_SelectorHeight"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_bufferInspectorSettings.m_bufferSelectorHeight;

		// Texture inspector
		result.m_strings["TextureInspector_Filter"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_textureInspectorSettings.m_selectorFilter.m_text;
		result.m_ints["TextureInspector_SelectorHeight"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_textureInspectorSettings.m_textureSelectorHeight;
		result.m_ints["TextureInspector_PreviewHeight"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_textureInspectorSettings.m_previewHeight;
		result.m_ints["TextureInspector_TooltipHeight"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_textureInspectorSettings.m_tooltipHeight;
		result.m_ints["TextureInspector_1DTextureBlockSize"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_textureInspectorSettings.m_1dTextureBlockSize;
		result.m_ints["TextureInspector_1DArrayTextureBlockSize"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_textureInspectorSettings.m_1dArrayTextureBlockSize;

		// Material inspector
		result.m_strings["MaterialEditor_Filter"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_materialEditorSettings.m_selectorFilter.m_text;
		result.m_ints["MaterialEditor_SelectorHeight"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_materialEditorSettings.m_materialSelectorHeight;
		result.m_ints["MaterialEditor_TooltipHeight"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_materialEditorSettings.m_tooltipHeight;
		result.m_ints["MaterialEditor_PreviewHeight"] = &guiSettings->component<GuiSettings::GuiSettingsComponent>().m_materialEditorSettings.m_previewHeight;

		return result;
	}

	////////////////////////////////////////////////////////////////////////////////
	void saveGuiState(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		Profiler::ScopedCpuPerfCounter perfCounter(scene, "GUI State Save");

		// Result of the saving
		std::ordered_pairs_in_blocks result;

		// Store some of the gui settings
		GuiStateVars guiState = getGuiStateVars(scene, guiSettings);
		for (auto const& var: guiState.m_bools) result["GuiSettings"].push_back({ var.first, std::to_string(*var.second) });
		for (auto const& var: guiState.m_ints) result["GuiSettings"].push_back({ var.first, std::to_string(*var.second) });
		for (auto const& var: guiState.m_floats) result["GuiSettings"].push_back({ var.first, std::to_string(*var.second) });
		for (auto const& var: guiState.m_strings) result["GuiSettings"].push_back({ var.first, *var.second });

		// Store the current header open state for the objects
		for (auto const& object : scene.m_objects)
		{
			if (object.second.component<EditorSettings::EditorSettingsComponent>().m_headerOpen)
			{
				result["ObjectsWindow#OpenHeaders"].push_back({ object.first, "1" });
			}
		}

		// Store the keyed editor properties
		for (auto& object : scene.m_objects)
		{
			std::string header = "Object#EditorProperties#" + object.first;
			for (auto const& property : object.second.component<EditorSettings::EditorSettingsComponent>().m_properties)
			{
				static std::string s_syncedSuffix = "#Synced";
				if (property.second.m_persistent && std::equal(s_syncedSuffix.rbegin(), s_syncedSuffix.rend(), property.first.rbegin()) == false)
				{
					result[header].push_back({ property.first, EditorSettings::storeEditorProperty(scene, &(object.second), property.first) });
				}
			}
		}

		// Store the profiler open category names
		for (auto const& categoryName : guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_openNodes)
		{
			result["StatsWindow#OpenNodes"].push_back({ categoryName, "1" });
		}

		// Store the profiler chart category names
		for (auto const& categoryName : guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_nodesToShow)
		{
			result["ProfilerChart#ShownCharts"].push_back({ categoryName, "1" });
		}

		// Save the state
		Asset::savePairsInBlocks(scene, guiSettings->component<GuiSettings::GuiSettingsComponent>().m_guiStateFileName, result, false);
	}

	////////////////////////////////////////////////////////////////////////////////
	void restoreGuiState(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		Profiler::ScopedCpuPerfCounter perfCounter(scene, "GUI State Restore");

		// Load the previous state
		auto stateOpt = Asset::loadPairsInBlocks(scene, guiSettings->component<GuiSettings::GuiSettingsComponent>().m_guiStateFileName);
		if (!stateOpt.has_value()) return;

		// Extract the state contents
		std::ordered_pairs_in_blocks& state = stateOpt.value();

		// Restore the GUI state
		GuiStateVars guiState = getGuiStateVars(scene, guiSettings);
		for (auto const& guiStateVar : state["GuiSettings"])
		{
			if (guiState.m_bools.find(guiStateVar.first) != guiState.m_bools.end()) *guiState.m_bools[guiStateVar.first] = std::from_string<bool>(guiStateVar.second);
			if (guiState.m_ints.find(guiStateVar.first) != guiState.m_ints.end()) *guiState.m_ints[guiStateVar.first] = std::from_string<int>(guiStateVar.second);
			if (guiState.m_floats.find(guiStateVar.first) != guiState.m_floats.end()) *guiState.m_floats[guiStateVar.first] = std::from_string<float>(guiStateVar.second);
			if (guiState.m_strings.find(guiStateVar.first) != guiState.m_strings.end()) *guiState.m_strings[guiStateVar.first] = guiStateVar.second;
		}

		// Restore the current header open state for the objects
		for (auto const& node : state["ObjectsWindow#OpenHeaders"])
		{
			if (scene.m_objects.find(node.first) != scene.m_objects.end())
			{
				scene.m_objects[node.first].component<EditorSettings::EditorSettingsComponent>().m_headerOpen = true;
			}
		}

		// Restore the keyed editor properties
		for (auto& object : scene.m_objects)
		{
			std::string header = "Object#EditorProperties#" + object.first;
			for (auto const& property : state[header])
			{
				if (EditorSettings::parseEditorProperty(scene, &(object.second), property.first, property.second))
				{
					object.second.component<EditorSettings::EditorSettingsComponent>().m_properties[property.first + "#Synced"] = 
						object.second.component<EditorSettings::EditorSettingsComponent>().m_properties[property.first];
				}
			}
		}

		// Restore the stat window state
		for (auto const& node : state["StatsWindow#OpenNodes"])
		{
			guiSettings->component<GuiSettings::GuiSettingsComponent>().m_statWindowSettings.m_openNodes.push_back(node.first);
		}

		// Restore the profilert chart state
		for (auto const& node : state["ProfilerChart#ShownCharts"])
		{
			guiSettings->component<GuiSettings::GuiSettingsComponent>().m_profilerChartsSettings.m_nodesToShow.insert(node.first);
		}

		// Update the regex filters
		guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_includeFilter.update();
		guiSettings->component<GuiSettings::GuiSettingsComponent>().m_logOutputSettings.m_discardFilter.update();
		guiSettings->component<GuiSettings::GuiSettingsComponent>().m_shaderInspectorSettings.m_selectorFilter.update();
		guiSettings->component<GuiSettings::GuiSettingsComponent>().m_materialEditorSettings.m_selectorFilter.update();
		guiSettings->component<GuiSettings::GuiSettingsComponent>().m_bufferInspectorSettings.m_selectorFilter.update();
		guiSettings->component<GuiSettings::GuiSettingsComponent>().m_textureInspectorSettings.m_selectorFilter.update();
	}

	////////////////////////////////////////////////////////////////////////////////
	bool guiVisible(Scene::Scene& scene, Scene::Object* guiSettings)
	{
		// Extract the necessary components
		Scene::Object* input = Scene::findFirstObject(scene, Scene::OBJECT_TYPE_INPUT);
		Scene::Object* simulationSettings = Scene::findFirstObject(scene, Scene::OBJECT_TYPE_SIMULATION_SETTINGS);

		// Evaluate the state
		return
		(
			guiSettings->component<GuiSettings::GuiSettingsComponent>().m_showGui == true &&
			simulationSettings->component<SimulationSettings::SimulationSettingsComponent>().m_focused == true &&
			(guiSettings->component<GuiSettings::GuiSettingsComponent>().m_showGuiNoInput == true || input->component<InputSettings::InputComponent>().m_input == true)
		);
	}

	////////////////////////////////////////////////////////////////////////////////
	void updateObject(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* object)
	{
		// Don't do anything if the gui needs to be hidden
		if (guiVisible(scene, object) == false)
			return;

		// Extract the imgui singleton
		ImGuiIO& io{ ImGui::GetIO() };

		// Begin an imgui frame
		ImGui::NewFrame();

		// Set the font globally here
		ImGui::PushFont(scene.m_fonts[object->component<GuiSettings::GuiSettingsComponent>().m_font]);

		// Generate the main dockspace
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_MenuBar |
			ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoNavFocus;
		ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode;
		if (/*autoHideTabs*/ false) dockFlags |= ImGuiDockNodeFlags_AutoHideTabBar;
		if (object->component<GuiSettings::GuiSettingsComponent>().m_lockLayout) dockFlags |= ImGuiDockNodeFlags_NoSplit | ImGuiDockNodeFlags_NoResize | ImGuiDockNodeFlags_NoDockingInCentralNode;

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Main Dockspace", nullptr, window_flags);
		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockFlags);
		ImGui::PopStyleVar(3);

		// Generate the available gui elements
		for (auto const& element : object->component<GuiSettings::GuiSettingsComponent>().m_guiElements)
			if (element.m_open) element.m_generator(scene, object);

		// Demo window
		if (EditorSettings::editorProperty<bool>(scene, object, "GuiSettings_ShowImguiDemo"))
		{
			ImGui::ShowDemoWindow(&EditorSettings::editorProperty<bool>(scene, object, "GuiSettings_ShowImguiDemo"));
		}

		// End the dock space
		ImGui::End();

		// Pop the global font
		ImGui::PopFont();

		// Save the gui state after each frame
		if (object->component<GuiSettings::GuiSettingsComponent>().m_saveGuiState)
		{
			float time = simulationSettings->component<SimulationSettings::SimulationSettingsComponent>().m_globalTime;
			float elapsed = time - object->component<GuiSettings::GuiSettingsComponent>().m_guiStateLastSaved;

			if (elapsed >= object->component<GuiSettings::GuiSettingsComponent>().m_saveGuiStateInterval)
			{
				saveGuiState(scene, object);
				object->component<GuiSettings::GuiSettingsComponent>().m_guiStateLastSaved = time;
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	void renderObjectOpenGL(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* renderSettings, Scene::Object* camera, std::string const& functionName, Scene::Object* object)
	{
		// Don't do anything if the gui needs to be hidden
		if (guiVisible(scene, object) == false)
			return;

		// Extract the imgui singleton
		ImGuiIO& io{ ImGui::GetIO() };

		// Render the GUI
		ImGui::Render();
		ImDrawData* drawData = ImGui::GetDrawData();

		Profiler::ScopedGpuPerfCounter perfCounter(scene, "GUI");

		int fb_width{ (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x) };
		int fb_height{ (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y) };

		drawData->ScaleClipRects(io.DisplayFramebufferScale);

		// Backup GL state
		GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
		GLint last_texture; glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
		GLint last_active_texture; glGetIntegerv(GL_ACTIVE_TEXTURE, &last_active_texture);
		GLint last_array_buffer; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
		GLint last_element_array_buffer; glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
		GLint last_vertex_array; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
		GLint last_blend_src; glGetIntegerv(GL_BLEND_SRC, &last_blend_src);
		GLint last_blend_dst; glGetIntegerv(GL_BLEND_DST, &last_blend_dst);
		GLint last_blend_equation_rgb; glGetIntegerv(GL_BLEND_EQUATION_RGB, &last_blend_equation_rgb);
		GLint last_blend_equation_alpha; glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &last_blend_equation_alpha);
		GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
		GLint last_scissor_box[4]; glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
		GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
		GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
		GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
		GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);

		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_SCISSOR_TEST);
		glActiveTexture(GL_TEXTURE0);

		// Bind the imgui shader
		Scene::bindShader(scene, "Imgui", "imgui");

		// Setup viewport, orthographic projection matrix
		glViewport(0, 0, fb_width, fb_height);
		auto proj = glm::ortho(0.0f, io.DisplaySize.x, io.DisplaySize.y, 0.0f, -1.0f, +1.0f);
		glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(proj));

		// Generate the vao and buffers
		GLuint vao, vbo, ibo;
		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);
		glGenBuffers(1, &ibo);

		// Configure the vao
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glEnableVertexAttribArray(GPU::VertexAttribIndices::VERTEX_ATTRIB_POSITION);
		glVertexAttribPointer(GPU::VertexAttribIndices::VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)offsetof(ImDrawVert, pos));
		glEnableVertexAttribArray(GPU::VertexAttribIndices::VERTEX_ATTRIB_UV);
		glVertexAttribPointer(GPU::VertexAttribIndices::VERTEX_ATTRIB_UV, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)offsetof(ImDrawVert, uv));
		glEnableVertexAttribArray(GPU::VertexAttribIndices::VERTEX_ATTRIB_COLOR);
		glVertexAttribPointer(GPU::VertexAttribIndices::VERTEX_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)offsetof(ImDrawVert, col));
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

		// Render command lists
		for (int n = 0; n < drawData->CmdListsCount; n++)
		{
			const ImDrawList* cmd_list = drawData->CmdLists[n];
			const ImDrawIdx* idx_buffer_offset = 0;

			glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), (GLvoid*)cmd_list->VtxBuffer.Data, GL_STREAM_DRAW);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), (GLvoid*)cmd_list->IdxBuffer.Data, GL_STREAM_DRAW);

			for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
			{
				const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
				if (pcmd->UserCallback)
				{
					pcmd->UserCallback(cmd_list, pcmd);
				}
				else
				{
					glBindTexture(GL_TEXTURE_2D, *(GLuint*)pcmd->TextureId);
					glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
					glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer_offset);
				}
				idx_buffer_offset += pcmd->ElemCount;
			}
		}

		// Unbind the vao and buffers
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		// Delete the buffers and vao
		glDeleteVertexArrays(1, &vao);
		glDeleteBuffers(1, &vbo);
		glDeleteBuffers(1, &ibo);

		// Restore modified GL state
		glUseProgram(last_program);
		glActiveTexture(last_active_texture);
		glBindTexture(GL_TEXTURE_2D, last_texture);
		glBindVertexArray(last_vertex_array);
		glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
		glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
		glBlendFunc(last_blend_src, last_blend_dst);
		if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
		if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
		if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
		if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
		glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
		glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei)last_scissor_box[2], (GLsizei)last_scissor_box[3]);
	}
}