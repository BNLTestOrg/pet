#include <UIQt/UIWindow.hxx>              // for UIMainWindow class
#include <UIQt/UIMenu.hxx>                // for UIPulldownMenu class
#include <UIQt/UICollection.hxx>			    // for UIMessageArea class
#include <UIQt/UIList.hxx>                // for UIEnumList class
#include <UIQt/UIEditor.hxx>              // for UIEditorWindow class
#include <UIQt/UIPopups.hxx>              // for UILabelPopup class
#include <UIDevice/UICombineTree.hxx>     // for UICombineTree class
#include <UIAgs/UIAGSControls.hxx>		    // for UIDeviceList and UIDevicesFromController classes
#include <pet/UISearchDeviceList.hxx>	    // for UISearchDeviceList class
#include <UIQt/UIHelp.hxx>                // for UIHelpMenu class
#include <UIUtils/UIHistoryPopup.hxx>     // for UIHistoryPopup class

enum PET_WINDOW_TYPE {PET_ADO_WINDOW, PET_UNKNOWN_WINDOW};

class MenuTree;
class UICreateDeviceList;
class PetScrollingEnumList;
class PetWindow;

class SSMainWindow : public UIMainWindow
{
  public:

  SSMainWindow(const UIObject* parent, const char* name, const char* title = NULL);

  // handle UI events
  void HandleEvent(const UIObject* object, UIEvent event);

  // set the message in the message area
  void SetMessage(const char* message);

  // redisplay the machine tree based on a new selection
  void LoadTable(const UIWindow* window);
  void LoadTable(const StdNode* node);

  // delete a device page
  void RemoveWindow(UIWindow* window);

  // delete a device page and remove it from the list
  void DeleteListWindow(UIWindow* window);
  void DeleteAllWindows();

  // hide (make invisible) a device page - don't remove it from the list
  void HideListWindow(UIWindow* window);

  // show the window - make visible and bring it to the front
  void ShowListWindow(UIWindow* window);

  // find out if a window is already in the window list
  UIWindow* FindWindow(const char* file, PET_WINDOW_TYPE wtype);

  // find out what type of window
  PET_WINDOW_TYPE WindowType(UIWindow* window);
  PET_WINDOW_TYPE WindowType(const char* path);

  bool checkForMedmPage(const char* path);

  // add an already created device page to the list
  void AddListWindow(UIWindow* window);

  // load the device page list from the set of attached windows
  // indicate which window string you want selected
  void LoadPageList(const UIWindow* winSelection = NULL);

  protected:

  UIMenubar*			       menubar;
  UIPulldownMenu*	       pulldownMenu;
  UILabel*			         ppmLabel;
  UIMessageArea*	       messageArea;
  UIMachineTreeTable*    treeTable;
  PetScrollingEnumList*	 pageList;
  UIHelpMenu*			       helpMenu;
  UIPopupWindow*		     devicePopup;
  UIDeviceList*			     deviceList;
  UIBoolean			         isIcon;
  UIViewerWindow*		     viewer;
  UISearchDeviceList*		_searchPopup;
  UISearchDeviceList*   _modifiedPopup;
  UISearchDeviceList*   _checkedOutPopup;
  MenuTree*              pulldownMenuTree;
  UILabelPopup*          errFillExistWindowPopup;
  UICreateDeviceList*    editDeviceList;
  bool                  _creatingPageInTree;
  UIHistoryPopup*       _historyPopup;
  UIRecentHistoryPopup* _recentPopup;
  unsigned long         _totalFlashTimerId; // to timeout flashing after 4 seconds.

  // set the window position for a newly created window
  void SetWindowPos(UIWindow* newWin, UIWindow* currWin = NULL);

  // set the label for displaying PPM user name using process PPM user
  void SetPPMLabel();

  // remove the device_list ending of the file name
  void AdjustName(char* devicePath);

  // a display popup - to show that could not load a device list
  void DisplayError(char* errToDisplay);

  // pulldown menu routines
//  void SS_New();
//  void SS_Open();
  void SS_OpenFavorite();
  void SS_Default_PPM_User();
//  void SS_Create_RHIC_Page();
//  void SS_Create_PS_RHIC_Page();
  void SS_FindTextInFiles();
  void SS_FindRecentlyModifiedFiles();
  void SS_FindCheckedOutFiles();
  void SS_Quit();
  int ConfirmQuit();
//  void SP_Find();
//  void SP_New();
//  void SP_Show();
//  void SP_Hide();
//  void SP_Close();
//  void SP_Close_All();
  void SO_Pet_Page_History();
  void SO_Read_Archive_Log();
  void SO_PpmUserMonitor();
//  void SO_Flash_Pages(bool flash = true);
  void Exit();

  // helper routines
  const char*    GetSelectedPath();
  const StdNode* GetSelectedNode();
  dir_node_t*    GetSelectedDirNode();
//  PetWindow*     CreateAdoWindow();
//  void           OpenFile(const char* filePath);
  void           ExitAllWindows();
};

//////////////////////////////////////////////////////////////////////
class PetScrollingEnumList : public UIScrollingEnumList
{
public:
	PetScrollingEnumList(const UIObject* parent, const char* name,
	                     const char* title = "", const char* items[] = NULL);

	void SetMenuItems();
	void HandleEvent(const UIObject* object, UIEvent event);
	void SetItemsVisible(long num);
	long GetDesiredNumVisItems() { return _desiredNumVisItems; }

protected:
	void CreateWidgets();

private:
	void Initialize();

	long _desiredNumVisItems;
};

/// class used to close single page window.
class PetEventReceiver : public UIEventReceiver
{
  public:

  PetEventReceiver();

  /// override this to handle window close events properly
  void HandleEvent(const UIObject* object, UIEvent event);
};

// C++ helper functions
// given a path to a device list, extract the leaf name
const char* GetLeafName(const char* deviceListPath);

