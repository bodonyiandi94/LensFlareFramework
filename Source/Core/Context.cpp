#include "PCH.h"
#include "Context.h"
#include "Includes.h"

namespace Context
{
	////////////////////////////////////////////////////////////////////////////////
	Context g_context;

	////////////////////////////////////////////////////////////////////////////////
	glm::ivec2 glVersion()
	{
		static glm::ivec2 s_version = Config::AttribValue("gl_version").get<glm::ivec2>();
		return s_version;
	}

	////////////////////////////////////////////////////////////////////////////////
	int glContext()
	{
		static int s_context = Config::AttribValue("gl_context").get<std::string>() == "Core" ? GLFW_OPENGL_CORE_PROFILE : GLFW_OPENGL_COMPAT_PROFILE;
		return s_context;
	}

	////////////////////////////////////////////////////////////////////////////////
	std::string glInterceptBaseConfigFileName()
	{
		static std::string s_fileName = "gliConfig.ini";
		return s_fileName;
	}

	////////////////////////////////////////////////////////////////////////////////
	std::string glInterceptConfigFileName()
	{
		static std::string s_fileName = "gliConfig_" + std::string(GPU::TraceLevels_meta.members[GPU::glTraceLevel()].name) + ".ini";
		return s_fileName;
	}

	////////////////////////////////////////////////////////////////////////////////
	glm::ivec2 getMonitorResolution(Context& context)
	{
		const GLFWvidmode* displayParams = glfwGetVideoMode(glfwGetPrimaryMonitor());

		return glm::ivec2(displayParams->width, displayParams->height);
	}

	////////////////////////////////////////////////////////////////////////////////
	void keyPressed(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		ImGuiIO& io = ImGui::GetIO();

		if (action == GLFW_PRESS)
		{
			io.KeysDown[key] = true;
		}
		else if (action == GLFW_RELEASE)
		{
			io.KeysDown[key] = false;
		}

		io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
		io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
		io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
		io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
	}

	////////////////////////////////////////////////////////////////////////////////
	void mouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.MouseDown[button] = action == GLFW_PRESS ? 1 : 0;
	}

	////////////////////////////////////////////////////////////////////////////////
	void mouseMotionCallback(GLFWwindow* window, double x, double y)
	{
		ImGuiIO& io = ImGui::GetIO();
		if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED)
			io.MousePos = ImVec2((float)x, (float)y);
	}

	////////////////////////////////////////////////////////////////////////////////
	void scrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.MouseWheel += (float)yoffset;
	}

	////////////////////////////////////////////////////////////////////////////////
	void charCallback(GLFWwindow* window, unsigned int c)
	{
		ImGuiIO& io = ImGui::GetIO();
		if (c > 0 && c < 0x10000)
			io.AddInputCharacter((unsigned short)c);
	}

	////////////////////////////////////////////////////////////////////////////////
	void resizeCallback(GLFWwindow* window, int width, int height)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2(width, height);
	}

	////////////////////////////////////////////////////////////////////////////////
	void minimizeCallback(GLFWwindow* window, int minimized)
	{
		if (minimized)
		{
			// The window was minimized
		}
		else
		{
			// The window was restored
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	void glfwErrorCallback(int error, const char* description)
	{
		Debug::DebugRegion region("GLFW");

		Debug::log_error() << description << Debug::end;
	}

	////////////////////////////////////////////////////////////////////////////////
	void gslErrorCallback(const char* reason, const char* file, int line, int gsl_errno)
	{
		Debug::DebugRegion region("GSL");

		Debug::log_error() << "error #" << gsl_errno << " at " << file << " ( line " << line << "): " << reason << Debug::end;
	}

	////////////////////////////////////////////////////////////////////////////////
	int crtReportHook(int reportType, char* message, int* returnValue)
	{
		Debug::DebugRegion region("CRT");

		if (reportType == _CRT_WARN)
		{
			Debug::log_warning() << message << Debug::end;
		}
		else
		{
			Debug::log_error() << message << Debug::end;
		}

		return FALSE;
	}

	int initEnvironmentVariables()
	{
		// Set TF log level
		_putenv_s("TF_CPP_MIN_LOG_LEVEL", "1");

		return 0;
	}

	////////////////////////////////////////////////////////////////////////////////
	int initConsole()
	{
		// Set the global locale
		//std::locale::global(std::locale(""));
		//std::locale::global(std::locale("en_US"));
		//std::locale::global(std::locale("C"));

		return 0;
	}

	////////////////////////////////////////////////////////////////////////////////
	int initCrt()
	{
		// Install the report hook
		_CrtSetReportHook(crtReportHook);

		return 0;
	}

	////////////////////////////////////////////////////////////////////////////////
	int initGlIntercept()
	{
		// Init GLIntercept
		Debug::log_info() << "Initializing GLIntercept..." << Debug::end;

		// Construct the intercept file names
		std::filesystem::path baseConfigFile = EnginePaths::executableFolder() / glInterceptBaseConfigFileName();
		std::filesystem::path currentConfigFile = EnginePaths::executableFolder() / glInterceptConfigFileName();

		// Overwrite the old config file with the current one
		std::filesystem::copy_file(currentConfigFile, baseConfigFile, std::filesystem::copy_options::overwrite_existing);

		return 0;
	}

	////////////////////////////////////////////////////////////////////////////////
	int initGlfw()
	{
		Debug::log_info() << "Initializing GLFW..." << Debug::end;

		// Init GLFW.
		glfwSetErrorCallback(glfwErrorCallback);

		if (glfwInit() != GLFW_TRUE)
		{
			Debug::log_error() << "Error initializing GLFW!" << Debug::end;
			return 1;
		}

		// Create our main window.
		Debug::log_info() << "Creating window..." << Debug::end;

		const GLFWvidmode* displayParams = glfwGetVideoMode(glfwGetPrimaryMonitor());

		glfwWindowHint(GLFW_RED_BITS, displayParams->redBits);
		glfwWindowHint(GLFW_GREEN_BITS, displayParams->greenBits);
		glfwWindowHint(GLFW_BLUE_BITS, displayParams->blueBits);
		glfwWindowHint(GLFW_REFRESH_RATE, displayParams->refreshRate);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, glVersion()[0]);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, glVersion()[1]);
		glfwWindowHint(GLFW_OPENGL_PROFILE, glContext());
		if (glContext() == GLFW_OPENGL_CORE_PROFILE)
			glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GPU::glDebugConfig() > 0 ? GLFW_TRUE : GLFW_FALSE);
		glfwWindowHint(GLFW_DOUBLEBUFFER, 1);
		//glfwWindowHint(GLFW_SAMPLES, 1);

		g_context.m_window = glfwCreateWindow(displayParams->width, displayParams->height, "Lens Flare Framework", nullptr, nullptr);

		if (g_context.m_window == nullptr)
		{
			Debug::log_error() << "Error creating the display window!" << Debug::end;
			return 2;
		}

		// Hide the window in background loading mode
		if (Config::AttribValue("background_init").get<bool>())
		{
			glfwHideWindow(g_context.m_window);
		}

		// Make use of the created context.
		glfwMakeContextCurrent(g_context.m_window);

		// Setup GLFW input.
		glfwSetInputMode(g_context.m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwSetInputMode(g_context.m_window, GLFW_STICKY_MOUSE_BUTTONS, GLFW_TRUE);

		glfwSetKeyCallback(g_context.m_window, keyPressed);
		glfwSetMouseButtonCallback(g_context.m_window, mouseButtonCallback);
		glfwSetCursorPosCallback(g_context.m_window, mouseMotionCallback);
		glfwSetScrollCallback(g_context.m_window, scrollCallback);
		glfwSetCharCallback(g_context.m_window, charCallback);
		glfwSetFramebufferSizeCallback(g_context.m_window, resizeCallback);
		glfwSetWindowIconifyCallback(g_context.m_window, minimizeCallback);

		return 0;
	}

	////////////////////////////////////////////////////////////////////////////////
	int initGlew()
	{
		// Init GLEW.
		Debug::log_info() << "Initializing GLEW..." << Debug::end;

		glewExperimental = GL_TRUE;
		auto glewError = glewInit();

		if (glewError != GLEW_OK)
		{
			Debug::log_error() << "Error initializing GLEW! Cause: " << glewGetErrorString(glewError) << Debug::end;
			return 3;
		}

		// Set the GL debug callback function
		if (GPU::glDebugConfig() > 0)
		{
			glEnable(GL_DEBUG_OUTPUT);
			glDebugMessageCallback(GPU::glDebugCallback, nullptr);
			//glDebugMessageControl()
		}

		return 0;
	}

	
	////////////////////////////////////////////////////////////////////////////////
	int initImGui()
	{
		// Initialize the ImGui context
		Debug::log_info() << "Initializing ImGui..." << Debug::end;
		ImGui::SetCurrentContext(ImGui::CreateContext());

		return 0;
	}

	////////////////////////////////////////////////////////////////////////////////
	int initGsl()
	{
		// Set the GSL error handler
		Debug::log_info() << "Initializing GSL..." << Debug::end;
		gsl_set_error_handler(gslErrorCallback);

		return 0;
	}

	////////////////////////////////////////////////////////////////////////////////
	int init()
	{
		int returnCode = 0;

		returnCode = returnCode != 0 ? returnCode : initEnvironmentVariables();
		returnCode = returnCode != 0 ? returnCode : initConsole();
		returnCode = returnCode != 0 ? returnCode : initCrt();
		returnCode = returnCode != 0 ? returnCode : initGlIntercept();
		returnCode = returnCode != 0 ? returnCode : initGlfw();
		returnCode = returnCode != 0 ? returnCode : initGlew();
		returnCode = returnCode != 0 ? returnCode : initImGui();
		returnCode = returnCode != 0 ? returnCode : initGsl();

		return returnCode;
	}

	////////////////////////////////////////////////////////////////////////////////
	void cleanup()
	{
		// Release the imgui context
		ImGui::DestroyContext(ImGui::GetCurrentContext());

		// Release the window object.
		glfwDestroyWindow(g_context.m_window);
	}
}