#include <UI/UIWindow.hxx>			// for UIMainWindow class
#include <UI/UIMenu.hxx>			// for UIPulldownMenu class
#include <UI/UICollection.hxx>			// for UIMessageArea class
#include <UI/UIList.hxx>			// for UIEnumList class
#include <UI/UIEditor.hxx>			// for UIEditorWindow class
#include <UI/UIPopups.hxx>			// for UILabelPopup class
#include <UIDevice/UICombineTree.hxx>           // for UICombineTree class
#include <UIAgs/UIAGSControls.hxx>		// for UIDeviceList and UIDevicesFromController classes
#include <agsPage/AgsPage.hxx>			// for AgsPage and AgsPageWindow classes
#include <agsPage/UISearchDeviceList.hxx>	// for UISearchDeviceList class
#include <agsPage/genCldLib.hxx>		// for UICldObjectWindow class
#include <UI/UIHelp.hxx>			// for UIHelpMenu class

enum PET_WINDOW_TYPE {PET_LD_WINDOW, PET_CLD_WINDOW, PET_ADO_WINDOW, PET_HYBRID_WINDOW, PET_UNKNOWN_WINDOW};

class SSPageWindow;
class MenuTree;

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

  // load a device list into a window
  int LoadDeviceList(SSPageWindow* win, const char* deviceList, short ppmUser = DEFAULT_PPM_USER);

  // delete a device page or CLD window
  void RemoveWindow(UIWindow* window);
  
  // delete a device page or CLD window and remove it from the list
  void DeleteListWindow(UIWindow* window);
  void DeleteAllWindows();

  // hide (make invisible) a device page or CLD window - don't remove it from the list
  void HideListWindow(UIWindow* window);

  // show the window - make visible and bring it to the front
  void ShowListWindow(UIWindow* window);

  // find out if a window is already in the window list
  UIWindow* FindWindow(const char* file, PET_WINDOW_TYPE wtype);

  // find out what type of window
  PET_WINDOW_TYPE WindowType(UIWindow* window);
  PET_WINDOW_TYPE WindowType(const char* path);
  
  // add an already created device page or CLD window to the list
  void AddListWindow(UIWindow* window);

  // load the device page list from the set of attached windows
  // indicate which window string you want selected
  void LoadPageList(const UIWindow* winSelection = NULL);

  // load a single device list, by tree path name (start-up option)
  void ShowSingleDeviceList(const char* deviceListPath);

  // initialize the archive lib tools
  void InitArchiveLib();

  // intialize relway server
  void InitRelwayServer();
protected:
  UIMenubar*			menubar;
  UIPulldownMenu*	       	pulldownMenu;
  UILabel*			ppmLabel;
  UIMessageArea*	       	messageArea;
  UIMachineTreeTable*           treeTable;
  UIScrollingEnumList*		pageList;
  UIHelpMenu*			helpMenu;
  UIDevicesFromControllerPopup*	ctrlPopup;
  UIPopupWindow*		devicePopup;
  UIDeviceList*			deviceList;
  UIBoolean			isIcon;
  UIViewerWindow*		viewer;
  UISearchDeviceList*		searchPopup;
//   SSPageWindow*			searchPage;
  UIWindow*			searchPage;
  MenuTree*                     pulldownMenuTree;
  
  // set the window position for a newly created window
  void SetWindowPos(UIWindow* newWin, UIWindow* currWin = NULL);

  // set the label for displaying PPM user name using process PPM user
  void SetPPMLabel();

  // reconnect to a new relway server
  void ReconnectToRelway(const char* newHost = NULL);

  // pulldown menu routines
  void SS_New();
  void SS_Open();
  void SS_Set_Host();
  void SS_Default_PPM_User();
  void SS_Create_RHIC_Page();
  void SS_Create_PS_RHIC_Page();
  void SS_Create_AGS_Page();
  void SS_Quit();
  void SP_Find();
  void SP_New();
  void SP_Show();
  void SP_Hide();
  void SP_Close();
  void SP_Close_All();
  void SO_Search();
  void SO_Read_Archive_Log();
  void SO_PpmUserMonitor();
  void SO_SLDs();
  void SO_CLDs();
  void SO_CLD_Events();
  void SO_Load_DDF();
  void Exit();

  // helper routines
  const char*    GetSelectedPath();
  const StdNode* GetSelectedNode();
  dir_node_t*    GetSelectedDirNode();
  void           ShowCldEditor(const char* devname, int ppmuser);
  SSPageWindow*  CreateLdWindow();
  PetWindow*     CreateAdoWindow();
  
};

/////////////////////////////////////////////////////////////////////
class SSPageWindow : public AgsPageWindow
{
public:
  SSPageWindow(const UIObject* parent, const char* name,
	       AGS_PAGE_MODE mode = AGS_OP_MODE, const char* title = NULL,
	       UIBoolean create = UITrue);
  ~SSPageWindow();
  const char* ClassName() const { return "SSPageWindow"; }

  // override this to always append PPM user name in title string
  int LoadFile(const char* filename, const char* pageTitle = NULL, short ppmUser = DEFAULT_PPM_USER);

  // override this to reset the string in the page list
  void SetPPMUser(short ppmUser);

  // set/get the string to display in the device page list
  // by default it creates one based on device list and PPM user
  void SetListString(const char* string = NULL);
  const char* GetListString() const;

  // set/get the node in the machine tree for this window
  void SetPageNode(const StdNode* node);
  const StdNode* GetPageNode() const;

  // set/get the path to the device list
  void SetDevListPath(const char* path);
  const char* GetDevListPath() const;

  // return the current path to the file that this window is showing
  const char* GetCurrentFileName() const;
  
  // override this to put a header in
  // default starts are 1 or first visible, default ends are last row/column or last visible
  virtual int PrintVisible(long startCol = 0, long endCol = 0);
  virtual int Print(long startRow = 0, long endRow = 0, long startCol = 0, long endCol = 0);

  // handle icon events - send other events to parent class
  void HandleEvent(const UIObject* object, UIEvent event);

  // signifies a single device list only will be displayed, for modification of the
  // pulldown menu
  void SetSingleDeviceListMode();

protected:
  char*		listString;	// string to display in device page list
  const StdNode*	pageNode;	// the node in the machine tree
  char*		devListPath;	// the path to the current device list
  
  void CreateWidgets(AGS_PAGE_MODE mode);

  // override this from the base class
  virtual void SP_Duplicate();
  virtual void SD_Show_CLD_Editor();

  // print the header on the device page printouts
  virtual void PrintSSHeader(FILE* fp);
private:
  void Initialize();
};


// C++ helper functions
// given a path to a device list, extract the leaf name
const char* GetLeafName(const char* deviceListPath);

/////////////////////////////////////////////////////////////////////
class SSCldWindow : public UICldObjectWindow
{
public:
  //after creating a UICldObjectWindow, user should always check for successful creation
  SSCldWindow(const UIObject* parent, const char* name,
		    const char* CldName, int PPMNumber, const char* title = NULL,
		    UIBoolean dialogWindow = UIFalse, UIBoolean create = UITrue);
  ~SSCldWindow();
  const char* ClassName() const { return "SSCldWindow"; }

  // set/get the string to display in the device page list
  // by default it creates one based on CLD name and PPM user
  void SetListString(const char* string = NULL);
  const char* GetListString() const;

  // handle icon events - send other events to parent class
  void HandleEvent(const UIObject* object, UIEvent event);

  // signifies a single device list only will be displayed, for modification of the
  // pulldown menu
  void SetSingleDeviceListMode();

protected:
  char*		listString;	// string to display in device page list

  void CreateWidgets(UIBoolean dialogWin);
private:
  void Initialize();
};


/// class used to close single page window.
class PetEventReceiver : public UIEventReceiver
{
public:
  /// override this to handle window close events properly
  void HandleEvent(const UIObject* object, UIEvent event);
};

// C++ helper functions
// given a path to a device list, extract the leaf name
const char* GetLeafName(const char* deviceListPath);

