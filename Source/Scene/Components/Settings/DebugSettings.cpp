#include "PCH.h"
#include "DebugSettings.h"
#include "SimulationSettings.h"

namespace DebugSettings
{
	////////////////////////////////////////////////////////////////////////////////
	// Define the component
	DEFINE_COMPONENT(DEBUG_SETTINGS);
	DEFINE_OBJECT(DEBUG_SETTINGS);
	REGISTER_OBJECT_UPDATE_CALLBACK(DEBUG_SETTINGS, AFTER, INPUT);

	////////////////////////////////////////////////////////////////////////////////
	void initObject(Scene::Scene& scene, Scene::Object& object)
	{
		// Default log levels
		object.component<DebugSettings::DebugSettingsComponent>().m_logToMemory = Debug::defaultLogChannelsMemory();
		object.component<DebugSettings::DebugSettingsComponent>().m_logConsole = Debug::defaultLogChannelsConsole();
		object.component<DebugSettings::DebugSettingsComponent>().m_logToFile = Debug::defaultLogChannelsFile();
		object.component<DebugSettings::DebugSettingsComponent>().m_profileCpu = Profiler::profilingDefault();
		object.component<DebugSettings::DebugSettingsComponent>().m_profileGpu = Profiler::profilingDefault();

		// Set the default log levels
		updateLoggerStates(scene, &object);

		// Set the error report mode
		if (object.component<DebugSettingsComponent>().m_crtSettings.m_debugMemory)
		{
			// Set debug flags if the debugger is present
			_CrtSetDbgFlag(
				_CRTDBG_ALLOC_MEM_DF | 
				_CRTDBG_LEAK_CHECK_DF | 
				_CRTDBG_CHECK_EVERY_1024_DF | 
				_CRTDBG_LEAK_CHECK_DF);
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	void releaseObject(Scene::Scene& scene, Scene::Object& object)
	{

	}

	////////////////////////////////////////////////////////////////////////////////
	bool collectCostlyTasks(Scene::Scene& scene, Scene::Object* debugSettings, Profiler::ProfilerThreadTree const& tree, Profiler::ProfilerThreadTreeIterator root, std::unordered_set<std::string>& costlyNodeNames)
	{
		// Should we include this node or not
		bool costly = false;

		// Iterate over the children
		for (auto it = tree.begin(root); it != tree.end(root); ++it)
		{
			// Compute the number of child entries
			int numChildren = std::distance(tree.begin(it), tree.end(it)) > 0;

			// Extract the current node's data
			auto& entry = Profiler::dataEntry(scene, *it);

			// Log it if it is too long
			costly = costly || (Profiler::isEntryType<Profiler::ProfilerDataEntry::EntryDataFieldTime>(entry.m_current) &&
				Profiler::convertEntryData<float>(entry.m_current) >= debugSettings->component<DebugSettings::DebugSettingsComponent>().m_costlyTaskLogThreshold);
			
			// Do this recursively, if they do
			if (std::distance(tree.begin(it), tree.end(it)) > 0)
			{
				costly = collectCostlyTasks(scene, debugSettings, tree, it, costlyNodeNames) || costly;
			}

			// Append it to the list, if needed
			if (costly)
			{
				costlyNodeNames.insert(entry.m_category);
			}
		};

		return costly;
	}

	////////////////////////////////////////////////////////////////////////////////
	void logCostlyTasks(Scene::Scene& scene, Scene::Object* debugSettings, Profiler::ProfilerThreadTree const& tree, Profiler::ProfilerThreadTreeIterator root, size_t depth, std::unordered_set<std::string>& costlyNodeNames)
	{
		// Iterate over the children
		for (auto it = tree.begin(root); it != tree.end(root); ++it)
		{
			// Compute the number of child entries
			int numChildren = std::distance(tree.begin(it), tree.end(it)) > 0;

			// Extract the current node's data
			auto& entry = Profiler::dataEntry(scene, *it);

			if (costlyNodeNames.find(entry.m_category) != costlyNodeNames.end())
			{
				std::stringstream prefix;
				for (size_t i = 0; i <= depth; ++i) prefix << "|" << std::string(debugSettings->component<DebugSettingsComponent>().m_costlyTaskLogNodeLength, i == depth ? '-' :' ');
				Debug::log_info() << prefix.str() << entry.m_category << ": " << Profiler::convertEntryData<std::string>(entry.m_current) << Debug::end;
			}

			// Do this recursively, if they do
			if (std::distance(tree.begin(it), tree.end(it)) > 0)
			{
				logCostlyTasks(scene, debugSettings, tree, it, depth + 1, costlyNodeNames);
			}
		};
	}

	////////////////////////////////////////////////////////////////////////////////
	void updateObject(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* object)
	{
		// Purge the profiler history buffer
		if (simulationSettings->component<SimulationSettings::SimulationSettingsComponent>().m_frameId >
			object->component<DebugSettingsComponent>().m_profilerHistoryLastPurgeFrame + object->component<DebugSettingsComponent>().m_profilerHistoryPurgeFrequency)
		{
			Profiler::ScopedCpuPerfCounter perfCounter(scene, "Profiler History Purge");

			// Erase unnecessary frames
			int frameId = simulationSettings->component<SimulationSettings::SimulationSettingsComponent>().m_frameId;
			int keptFrames = object->component<DebugSettingsComponent>().m_profilerHistoryNumPrevFrames;
			for (auto& node : scene.m_profilerData)
			{
				if (node.second.m_previousValues.size() > 0)
				{
					// Copy over the old values and clear the vector
					auto oldValues = node.second.m_previousValues;
					node.second.m_previousValues.clear();
					node.second.m_previousValues.reserve(keptFrames);

					for (auto const& value : oldValues)
					{
						if (value.first >= frameId - keptFrames)
						{
							node.second.m_previousValues[value.first] = value.second;
						}
					}
				}
			}

			// Mark that it has been purged
			object->component<DebugSettingsComponent>().m_profilerHistoryLastPurgeFrame = 
				simulationSettings->component<SimulationSettings::SimulationSettingsComponent>().m_frameId;
		}

		// How many frames to keep when purging the profiler history buffer
		int m_profilerHistoryNumprevFrames = 1000;

		// How frequently to purge the profiler frame history
		int m_profilerHistoryPurgeFrequency = 10000;

		// Collect the list of costly tasks
		std::unordered_set<std::string> costlyNodeNames;
		collectCostlyTasks(scene, object, scene.m_profilerTree[scene.m_profilerBufferReadId][0], scene.m_profilerTree[scene.m_profilerBufferReadId][0].begin(), costlyNodeNames);

		// Log the costly tasks
		if (costlyNodeNames.size() > 0)
		{
			Debug::log_info() << "[Frame #" << simulationSettings->component<SimulationSettings::SimulationSettingsComponent>().m_frameId << "] " << "List of costly tasks: " << Debug::end;

			logCostlyTasks(scene, object, scene.m_profilerTree[scene.m_profilerBufferReadId][0], scene.m_profilerTree[scene.m_profilerBufferReadId][0].begin(), 0, costlyNodeNames);
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	void updateLoggerStates(Scene::Scene& scene, Scene::Object* object)
	{
		Debug::setMemoryLogging(object->component<DebugSettings::DebugSettingsComponent>().m_logToMemory);
		Debug::setConsoleLogging(object->component<DebugSettings::DebugSettingsComponent>().m_logConsole);
		Debug::setFileLogging(object->component<DebugSettings::DebugSettingsComponent>().m_logToFile);
	}

	////////////////////////////////////////////////////////////////////////////////
	bool generateGuiLogChannel(Scene::Scene& scene, Scene::Object* guiSettings, Scene::Object* object, std::string const& label, Debug::LogChannels& channel)
	{
		bool logChanged = false;

		ImGui::Text(label.c_str());
		ImGui::PushID(label.c_str());
		ImGui::SameLine();
		logChanged = ImGui::Checkbox("Debug", &channel.m_debug) || logChanged;
		ImGui::SameLine();
		logChanged = ImGui::Checkbox("Trace", &channel.m_trace) || logChanged;
		ImGui::SameLine();
		logChanged = ImGui::Checkbox("Info", &channel.m_info) || logChanged;
		ImGui::SameLine();
		logChanged = ImGui::Checkbox("Warning", &channel.m_warning) || logChanged;
		ImGui::SameLine();
		logChanged = ImGui::Checkbox("Error", &channel.m_error) || logChanged;
		ImGui::SameLine();
		if (ImGui::Button("All"))
		{
			channel.m_debug = channel.m_trace = channel.m_info = channel.m_warning = channel.m_error = true;
			logChanged = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("None"))
		{
			channel.m_debug = channel.m_trace = channel.m_info = channel.m_warning = channel.m_error = false;
			logChanged = true;
		}
		ImGui::PopID();
		return logChanged;
	}

	////////////////////////////////////////////////////////////////////////////////
	void generateGui(Scene::Scene& scene, Scene::Object* guiSettings, Scene::Object* object)
	{
		bool logChanged = false;

		logChanged = generateGuiLogChannel(scene, guiSettings, object, "Log to Memory", object->component<DebugSettings::DebugSettingsComponent>().m_logToMemory) || logChanged;
		logChanged = generateGuiLogChannel(scene, guiSettings, object, "Log to Console", object->component<DebugSettings::DebugSettingsComponent>().m_logConsole) || logChanged;
		logChanged = generateGuiLogChannel(scene, guiSettings, object, "Log to File", object->component<DebugSettings::DebugSettingsComponent>().m_logToFile) || logChanged;

		ImGui::DragInt("Profiler History Buffer Purge Frequency", &object->component<DebugSettings::DebugSettingsComponent>().m_profilerHistoryPurgeFrequency);
		ImGui::DragInt("Profiler History Buffer Purge Kept Frames", &object->component<DebugSettings::DebugSettingsComponent>().m_profilerHistoryNumPrevFrames);

		ImGui::SliderFloat("Long Task Log Threshold", &object->component<DebugSettings::DebugSettingsComponent>().m_costlyTaskLogThreshold, 0.0f, 1000.0f);
		ImGui::SliderInt("Long Task Log Prefix Length", &object->component<DebugSettingsComponent>().m_costlyTaskLogNodeLength, 1, 4);

		ImGui::Checkbox("Profile CPU", &object->component<DebugSettings::DebugSettingsComponent>().m_profileCpu);
		ImGui::SameLine();
		ImGui::Checkbox("Profile GPU", &object->component<DebugSettings::DebugSettingsComponent>().m_profileGpu);
		ImGui::SameLine();
		ImGui::Checkbox("Profiler History", &object->component<DebugSettings::DebugSettingsComponent>().m_profilerStoreValues);

		if (ImGui::Button("Save Profiler Stats"))
		{
			EditorSettings::editorProperty<std::string>(scene, object, "Debug_SaveProfilerStats_FileName") = EnginePaths::generateUniqueFilename("Profile-Data-", ".csv");
			EditorSettings::editorProperty<std::string>(scene, object, "Debug_SaveProfilerStats_RootCategory") = ".*";
			ImGui::OpenPopup("SaveProfilerStats");
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear Profiler Stats"))
		{
			Profiler::clearTree(scene);
		}

		if (ImGui::BeginPopup("SaveProfilerStats"))
		{
			ImGui::InputText("File Name", EditorSettings::editorProperty<std::string>(scene, object, "Debug_SaveProfilerStats_FileName"));
			ImGui::InputText("Root Node", EditorSettings::editorProperty<std::string>(scene, object, "Debug_SaveProfilerStats_RootCategory"));

			if (ImGui::ButtonEx("Ok", "|########|"))
			{
				std::string csvSource = Profiler::exportTreeCsv(scene, EditorSettings::editorProperty<std::string>(scene, object, "Debug_SaveProfilerStats_RootCategory"), true);
				std::string fileName = std::string("Generated/Profiler/") + EditorSettings::editorProperty<std::string>(scene, object, "Debug_SaveProfilerStats_FileName");
				Asset::saveTextFile(scene, fileName, csvSource);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::ButtonEx("Cancel", "|########|"))
			{
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		if (logChanged)
		{
			updateLoggerStates(scene, object);
		}
	}
}