#pragma once

// uncomment and modify defines under for customize ImGuiFileDialog

//#define MAX_FILE_DIALOG_NAME_BUFFER 1024
//#define MAX_PATH_BUFFER_SIZE 1024

//#define USE_IMGUI_TABLES

//#define USE_EXPLORATION_BY_KEYS
// this mapping by default is for GLFW but you can use another
//#include <GLFW/glfw3.h> 
// Up key for explore to the top
//#define IGFD_KEY_UP GLFW_KEY_UP
// Down key for explore to the bottom
//#define IGFD_KEY_DOWN GLFW_KEY_DOWN
// Enter key for open directory
//#define IGFD_KEY_ENTER GLFW_KEY_ENTER
// BackSpace for comming back to the last directory
//#define IGFD_KEY_BACKSPACE GLFW_KEY_BACKSPACE

// widget
// filter combobox width
//#define FILTER_COMBO_WIDTH 120.0f
// button widget use for compose path
//#define IMGUI_PATH_BUTTON ImGui::Button
// standar button
//#define IMGUI_BUTTON ImGui::Button

// locales string
//#define createDirButtonString "+"
//#define okButtonString " OK"
//#define cancelButtonString " Cancel"
//#define resetButtonString "R"
//#define drivesButtonString "Drives"
//#define searchString "Search"
//#define dirEntryString "[DIR] "
//#define linkEntryString "[LINK] "
//#define fileEntryString "[FILE] "
//#define fileNameString "File Name : "
//#define buttonResetSearchString "Reset search"
//#define buttonDriveString "Drives"
//#define buttonResetPathString "Reset to current directory"
//#define buttonCreateDirString "Create Directory"

#include "imgui_plugins/CustomFont.h"

#define createDirButtonString ICON_IGFD_ADD
#define okButtonString ICON_IGFD_OK " OK"
#define cancelButtonString ICON_IGFD_CANCEL " Cancel"
#define resetButtonString ICON_IGFD_RESET
#define drivesButtonString ICON_IGFD_DRIVES
#define searchString ICON_IGFD_SEARCH
#define dirEntryString ICON_IGFD_FOLDER
#define linkEntryString ICON_IGFD_LINK
#define fileEntryString ICON_IGFD_FILE

// theses icons will appear in table headers
//#define USE_CUSTOM_SORTING_ICON
//#define AscendingIcon "A|"
//#define DescendingIcon "D|"
//#define tableHeaderFilenameString "File name"
//#define tableHeaderSizeString "Size"
//#define tableHeaderDateString "Date"

//#define USE_BOOKMARK
//#define bookmarkPaneWith 150.0f
//#define IMGUI_TOGGLE_BUTTON ToggleButton
//#define bookmarksButtonString "Bookmark"
//#define bookmarksButtonHelpString "Bookmark"
//#define addBookmarkButtonString "+"
//#define removeBookmarkButtonString "-"

#define USE_BOOKMARK
//#define bookmarkPaneWith 150.0f
//#define IMGUI_TOGGLE_BUTTON ToggleButton
#define bookmarksButtonString ICON_IGFD_BOOKMARK
//#define bookmarksButtonHelpString "bookmark"
#define addBookmarkButtonString ICON_IGFD_ADD
#define removeBookmarkButtonString ICON_IGFD_CANCEL