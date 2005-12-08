// #define __DEBUG_KNOB
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <UI/UIApplication.hxx>			// for UIApplication class
#include <UI/UIArgumentList.hxx>		// for UIArgumentList class
#include <pet/PetWindow.hxx>
#include <utils/utilities.h>			// for set_default_fault_handler()
#include <archive/archive_lib.h>		// for init_archive_lib_globals() and ARCHIVE_LOG;
#include <ddf/DeviceDirectory.hxx>		// for DeviceDirectory global object
#include <dgTools/dg_tools.h>			// for check_connection()
#include <UIGenerics/UIMenubarTools.hxx>	// for mb_init()
#include <UIAgs/generic_popups_derived.h>	// for gp_set_ppm _program() _user()
#include <UIAgs/cld_popup.hxx>			// for cld popups
#include <UIGenerics/GenericPopups.hxx>
#include "pet.hxx"
#include <sys/stat.h>				// for umask printing permissions
#include <sys/types.h>
#include <unistd.h>
#include <agsPage/KnobPanel.hxx>		// for supporting a knob panel
#include <UIUtils/UIPPM.hxx>
#include <cdevCns/cdevCns.hxx>
#include <setHist/SetStorage.hxx>               // to turn on storage of ADO/LD sets
#include "MenuTree.cxx"

using namespace std;

#define SS_PRINT_FILE		"/tmp/SSPagePrintFile"

static UIApplication*	application;
static UIArgumentList	argList;
static SSMainWindow*	mainWindow = NULL;
static UIBoolean	singleDeviceListOnly=UIFalse;	// used for loading a single device list only
							// no main window (tree table) is displayed
static KnobPanel*	knobPanel = NULL;
static UIBoolean	supportKnobPanel=UIFalse;
static unsigned long	knobPanelId = 0;
static SSPageWindow*	activeLdWin = NULL;
static PetWindow*       activeAdoWin = NULL;
static PetWindow*       singlePetWin = NULL;
static PetEventReceiver petEventReceiver;
static unsigned long    dumpElogAndExitTimerId = 0;
static const char* wname;

static void clean_up(int st)
{
  if (knobPanel != NULL) delete knobPanel;
  rpc_udp_cleanup();
  exit(st);
}

// poll the CDEV services to catch any async data from CDEV servers
static void CDEV_poll(void*, unsigned long*)
{
  cdevSystem::defaultSystem().poll();
  GlobalAsyncHandler()->RegisterTimer(100, CDEV_poll, NULL);
}

// prevent cdev errors from being printed to std out
void errorHandler(int severity, char* text, cdevRequestObject* obj)
{
}

int main(int argc, char *argv[])
{
  //set up command line arguments
  argList.AddString("-host");
  argList.AddString("-db_server");
  argList.AddString("-root");
  argList.AddString("-ddf");
  argList.AddString("-device_list");
  argList.AddSwitch("-knob");
  argList.AddSwitch("-single");         // single window mode
  argList.AddSwitch("-file");		// file to open initially
  argList.AddString("-path");		// default path for file open
  argList.AddSwitch("-printToElog");    // used with -single or -file to print pet page to elog after data acquisition, then exit
  argList.AddString("-elog");           // when used with -printToElog, the name of the elog, else the default
  argList.AddNumber("-ppm");            // start pet with the specified user

  // set up the CNS as the source for names
  cdevCnsInit();

  cdevSystem& defSystem = cdevSystem::defaultSystem();
  defSystem.autoErrorOff();
  defSystem.setErrorHandler(errorHandler);

  // turn on storage of ADO/LD sets
  globalSetStorage()->storageOn();

  // initialize the application
  application  = new UIApplication(argc, argv, &argList);

  if( argList.IsPresent("-knob") ) wname = "KnobPanel";
  else wname = "pet";

  if (argList.IsPresent("-ppm"))
    set_ppm_user(argList.Value("-ppm"));

  // 10Hz timer event for polling cdev - needed to handle asyncs from CDEV servers
  // this is commented out for now until Gpm starts getting built with shared object libraries
  GlobalAsyncHandler()->RegisterTimer(100, CDEV_poll, NULL);

  // check for single window switch
  bool singleWindowMode = false;
  if(argList.IsPresent("-file") || argList.IsPresent("-single")){
    singlePetWin = new PetWindow(application, "petWindow");
    singlePetWin->SetLocalPetWindowCreating(true);
    singlePetWin->AddEventReceiver(&petEventReceiver);
    singlePetWin->GetPetPage()->AddEventReceiver(&petEventReceiver);
    singleWindowMode = true;
  }
  else{
    // create the main window and its user interface and display it
    mainWindow = new SSMainWindow(application, "mainWindow", wname);
    application->AddEventReceiver(mainWindow);
    if (argList.IsPresent("-printToElog") && !argList.IsPresent("-device_list"))
      cout << "-printToElog option must be used with -single (ado page) or -device_list (sld/cld page) option" << endl;
  }

  const char *path = argList.String("-path");
  if(path && strlen(path)) {
    char *str = new char[strlen(path) + 10];
    sprintf(str, "%s/*.pet", path);
    if(singleWindowMode)
      singlePetWin->ChangePath(str);
    if(str)
      delete [] str;
  }

  // check values of some command line arguments
  if( strlen( argList.String("-db_server") ) )
    set_data_db_name( (char*) argList.String("-db_server") );

  // map a ddf
  if( strlen( argList.String("-ddf") ) )
    {
      if( DeviceDirectory.load( argList.String("-ddf") ) <= 0)
	{
	  fprintf(stderr, "Could not map ddf %s.  Aborting.\n", argList.String("-ddf") );
	  exit(1);
	}
    }
  else	// map the current ddf
    {
      if( DeviceDirectory.load() <= 0)
	{
	  fprintf(stderr, "Could not map current ddf.  Aborting.\n");
	  exit(1);
	}
    }

  if (!argList.IsPresent("-file") && !argList.IsPresent("-single"))
    // set up the archive lib tools
    mainWindow->InitArchiveLib();

  // load files that are untagged on the command line
  int fileNum;
  for(fileNum=0; fileNum<argList.NumUntaggedItems(); fileNum++) {

    const char* file = argList.UntaggedItem(fileNum);
    if(file && strlen(file)) {

      char *tmpFile;
      char *path;
      char *ptr;

      tmpFile = strdup(file);
      ptr = strrchr(tmpFile, '/');
      if(ptr) {
        *ptr = '\0';
        ptr++;
        path = new char[strlen(tmpFile) + 10];
        sprintf(path, "%s/*.ado", tmpFile);
      }
      else			// no path given
        path = strdup("*.ado");

      if(fileNum==0) {	// only do the first file
        if (singlePetWin == NULL){
          singlePetWin = new PetWindow(application, "petWindow");
          if (mainWindow)
            mainWindow->AddListWindow(singlePetWin);
        }
        singlePetWin->LoadFile(file);
        singlePetWin->ChangePath(path);
        if(path)
          free(path);
        if(tmpFile)
          free(tmpFile);
        singlePetWin->Show();
        break;
      }
    } // if file good
  } // for

  // if the switch know is provided, enable the sio line
  if( argList.IsPresent("-knob") )
    {
      knobPanel = new KnobPanel();
      int fd = knobPanel->InitKnobPanel();
      // add the descriptor to the UI event loop so that it may be serviced
      if (fd >= 0)
	{
	  knobPanelId = application->EnableFileDescEvent(fd);
	  supportKnobPanel = UITrue;
#ifdef __DEBUG_KNOB
	  printf("Supporting knob panel.\n");
#endif
	}
      else // failure, don't go any further
	{
	  fprintf(stderr, "Could not init KnobPanel.  Aborting.\n");
	  delete knobPanel;
	  rpc_udp_cleanup();
	  exit(1);
	}
    }

  // set a fault handler to get tracebacks on program crashes
  set_default_fault_handler( (char*) application->Name() );
  // clean-up handler
  signal(SIGQUIT,clean_up);
  signal(SIGTERM,clean_up);
  signal(SIGSTOP,clean_up);

  // if there is a passed argument with device_list, don't show the SSMainWindow
  if( strlen( argList.String("-device_list") )  )
    {
      if (mainWindow == NULL){
        mainWindow = new SSMainWindow(application, "mainWindow", wname);
        application->AddEventReceiver(mainWindow);
      }

      // now display the AgsPageWindow with the passed in device list
      mainWindow->ShowSingleDeviceList((char*) argList.String("-device_list"));
      singleDeviceListOnly=UITrue;
    }

  if ( argList.IsPresent("-printToElog") && (singleWindowMode || singleDeviceListOnly)){
    const char* elogName = NULL;
    if(argList.IsPresent("-elog") )
      elogName = argList.String("-elog");
    UIPrintTool* pt = GetPrintTool();
    pt->SetPrintOutputType(UIPrintToElog);
    if(elogName != NULL && elogName[0] != 0)
      pt->SetElogName(elogName);
    // dump snap shot of the AGS page to the elog and exit
    if (singleDeviceListOnly)
      dumpElogAndExitTimerId = application->EnableTimerEvent(1000);
    else
      singlePetWin->ElogDumpAndExit();
  }

  // loop forever handling user events
  application->HandleEvents();
}

///////////////////////// PetEventReceiver class ///////////////////////
PetEventReceiver::PetEventReceiver()
{
   for (int i=0; i<50; i++)
      cldWins[i] = NULL;
}

PetEventReceiver::~PetEventReceiver()
{
   for (int i=0; i<50; i++)
      delete cldWins[i];
}

void PetEventReceiver::HandleEvent(const UIObject* object, UIEvent event)
{
  if(object == singlePetWin && event == UIWindowMenuClose)
    exit(0);
  else if (event == UIEvent4){
     // this is essentially ShowCldEditor() but cld window pointers are stored
     // here
     PetPage* ppage = singlePetWin->GetPetPage();
     const char* devname = ppage->CellGetAdo();
     SSCldWindow* win = new SSCldWindow(singlePetWin, "sscldWindow", devname,
                                        ppage->GetPPMUser());
     if (win->SuccessfullyCreated() == UIFalse || win->DataToCollect() == UIFalse){
        UILabelPopup popup(singlePetWin, "cldProbPopup", "NULL", "OK");
        if (win->SuccessfullyCreated() == UIFalse)
        {
           int failure=win->ReasonNoCreate();
           if (failure == DEVICE_BAD_NAME)
              popup.SetLabel("CLD ", devname, " does not exist.\nCLD window creation aborted.");
           else if (failure == DC_GET_TIMEOUT)
              popup.SetLabel("CLD ", devname, " not reporting.\nCLD window creation aborted.");
           else if (failure == DEVICE_BAD_REPORTFORMAT)
              popup.SetLabel("CLD ", devname, " - bad report format.\nCLD window creation aborted.");
           else if (failure == DEVICE_NODATA)
              popup.SetLabel("CLD ", devname, " - no report received.\nCLD window creation aborted.");
           else if (failure == DEVICE_UNPACKERROR || failure == DEVICE_BAD_REPORTFORMAT || failure == DEVICE_NOBUFFERSPACE)
              popup.SetLabel("CLD ", devname, " - report format error.\nCLD window creation aborted.");
           else
              popup.SetLabel("CLD ", devname, " - unknown error.\nCLD window creation aborted.");
        }
        else
           popup.SetLabel("CLD ", devname, " - no data to collect.\nCLD window creation aborted.");
        delete win;
        popup.RingBell();
        popup.Wait();
        return;
     }
     win->EnableEvent(UIWindowMenuClose);
     win->AddEventReceiver(&petEventReceiver);
     win->Show();
     // store the window pointer
     for (int i=0; i<50; i++){
        if (cldWins[i] == NULL) {
           cldWins[i] = win;
           break;
        }
     }

  } else if (event == UIWindowMenuClose){
     for (int i=0; i<50; i++){
        if (cldWins[i] == object){
           cldWins[i]->DeleteThis();
           cldWins[i] = NULL;
           break;
        }
     }
  }
}

////////////// SSMainWindow Class Methods ////////////////////////
SSMainWindow::SSMainWindow(const UIObject* parent, const char* name, const char* title)
  : UIMainWindow(parent, name, title, UIFalse)
{
  // intialize some variables
  ctrlPopup = NULL;
  devicePopup = NULL;
  viewer = NULL;
  searchPopup = NULL;
  searchPage = NULL;
  errFillExistWindowPopup = NULL;

  // resources
  static const char* defaults[] = {
    "*foreground: navy",
    "*background: gray82",
    "*pageWindow*page*background: gray86",
    "*pageWindow*page*uilabelBackground: gray82",
    "*mainWindow*treeTable*foreground: black",
    "*mainWindow*pageList*list*foreground: black",
    "*fontList: -adobe-helvetica-bold-r-normal--14-140-75-75-p-82-iso8859-1",
    "*mainWindow*treeTable*font: 9x15bold",
    "*mainWindow*pageList*list*fontList: 9x15bold",
    "*mainWindow.geometry: +0+0",
    "*mainWindow.allowShellResize: true",
    "*mainWindow*treeTable*showHeaders: false",
    "*mainWindow*treeTable*visibleRows: 30",
    "*mainWindow*treeTable*uinameWidth: 30",
    "*mainWindow*treeTable*table.width: 293",
    "*mainWindow*treeTable*table*showHScroll: true",
    "*mainWindow*pageList*prompt*alignment: alignment_center",
    "*mainWindow*pageList*traversalOn: false",
    "*pageWindow*defaultPosition: true",
    "*pageWindow*page*tableHelp*background: white",
    "*sscldWindow*defaultPosition: true",
    "*deviceInfoPopup*defaultPosition: false",
    "*deviceInfoPopup.allowShellResize: false",
    "*metaEditor*defaultPosition: false",
    "*loadArchivePopup*defaultPosition: false",
    "*allUserPopup*defaultPosition: false",
    "*mainWindow*treeTable.uihelpText: "
        "The machine tree display.  A right facing arrow next to a node name\n"
        "indicates that this node is expandable, but is not currently expanded.\n"
        "A down facing arrow indicates that the node is currently expanded.\n"
        "No arrow next to a node name indicates that this is a leaf node of the tree.\n"
        "Clicking on the name or a right facing arrow will expand that section\n"
        "and cause and already expanded section to collapse if neccessary.\n"
        "To cause a section to collapse, click on the down facing arrow.",
    "*mainWindow*pageList.uihelpText: "
        "Displays the leaf name of all of the device pages currently being displayed.\n"
        "Clicking on a name in this list will cause the device page to come to the\n"
        "front of the screen and cause its path in the machine tree to be displayed.",
    "*loadArchivePopup*arclist.uihelpText: This list contains all archives for the current device page.",
    "*loadArchivePopup*archeader.uihelpText: Displays information pertaining to the selected archive.",
    "*loadArchivePopup*arctoggles*Show timed archives in archive list.uihelpText: "
        "When this is selected (it looks pushed in), timed archives are also\n"
        "shown in the list, otherwise they are not.",
    "*loadArchivePopup*arctoggles*Sort archive list alphabetically.uihelpText: "
        "When this is selected (it looks pushed in), the archive list is sorted\n"
        "alphabetically, otherwise it is sorted chronologically.",
    "*loadArchivePopup*OK.uihelpText: Loads the selected archive and closes this window.",
    "*loadArchivePopup*Apply.uihelpText: Loads the selected archive.  Does not close this window.",
    "*loadArchivePopup*Close.uihelpText: Closes this window without loading an archive.",
    "*loadArchivePopup*Help.uihelpText: "
        "This window allows you to load one or more archives into a device page.\n"
        "The list displays all archives for the device page shown.\n"
        "\nTo load an archive, select the archive, then click the Apply button.\n"
        "To load an archive and close this window, then click the OK button.\n"
        "To close this window without loading and archive, then click the Close button.\n"
        "\nTo get information on individual items in the window, move the cursor over\n"
        "the item of interest, then press and hold the 3rd mouse button.",
    NULL};
  UISetDefaultResources(defaults);

  // create the window
  UIMainWindow::CreateWidgets();

  // set up event handling for window
  SetIconName( application->Name() );
  EnableEvent(UIWindowMenuClose);
  AddEventReceiver(this);
  SetUserAttachments();

  // create a menubar at the top
  menubar = new UIMenubar(this, "mainMenubar");
  menubar->AttachTo(this, this, NULL, this);

  // put a pulldown menu in the menubar
  pulldownMenuTree = CreateMenuTree();

  // the name of the tree file is in the resource file
  pulldownMenu = new UIPulldownMenu(menubar, "mainPulldown", pulldownMenuTree);
  if(pulldownMenu->IsTreeLoaded() == UIFalse)
    {	printf("Could not load menu tree.  Aborting.\n");
    clean_up(1);
    }
  pulldownMenu->AddEventReceiver(this);

  // put in the AGS help menu
  helpMenu = new UIHelpMenu(menubar, "helpPulldown");
  helpMenu->AddEventReceiver(this);

  // put in a label to display the PPM user name
  ppmLabel = new UILabel(this, "ppmLabel");
  ppmLabel->AttachTo(menubar, this, NULL, this);
  SetPPMLabel();

  // put a message area at the bottom of the window
  messageArea = new UIMessageArea(this, "mainMessage");
  messageArea->AttachTo(NULL, this, this, this);

  // put in the list which shows the device pages shown
  pageList = new UIScrollingEnumList(this, "pageList", "ADO/SLD/CLD Pages");
  pageList->AttachTo(NULL, this, messageArea, this);
  pageList->SetItemsVisible(1);
  pageList->AddEventReceiver(this);

  // add the table which displays the machine tree
  // put this in last so that it is the one that grows when the window is resized
  treeTable = new UIMachineTreeTable(this, "treeTable");
  treeTable->AttachTo(ppmLabel, this, pageList, this);
  treeTable->EnableEvent(UITableBtn2Down);
  treeTable->EnableEvent(UIAccept);
  treeTable->AddEventReceiver(this);

  MachineTree* machTree = treeTable->GetMachineTree();
  // check to see if users wants to use a new root to the machine tree
  if( strlen( argList.String("-root") ) )
    {
      machTree->SetRootDirPath( argList.String("-root") );
    }

  // Load the machine tree
  if( treeTable->Load()){
    fprintf(stderr, "Could not get machine tree.  Aborting\n");
    clean_up(0);
  }

  // for compatibility, initialize menubar tools and generic popups
  mb_init(this, messageArea);

  // prevent main window from auto resizing itself
  UIForm* form = GetForm();
  if (form)
    form->ResizeOff();

  // if there is a passed argument with device_list, don't show the SSMainWindow
  if( strlen( argList.String("-device_list") ) == 0 )
    // display the tree table window to get any device list
    Show();
}

void SSMainWindow::InitArchiveLib()
{
  MachineTree* mtree = treeTable->GetMachineTree();
  const dir_node_t* root = mtree->GetDirRootNode();
  init_archive_lib_globals(GlobalDdfPointers(), false, 0, -1, table_dummy, (dir_node_t*) root);
}

void SSMainWindow::InitRelwayServer()
{
}

void SSMainWindow::SetMessage(const char* message)
{
  messageArea->SetMessage(message);
}

void SSMainWindow::SetPPMLabel()
{
  // get the ppm user number used by this process
  int ppmUser = get_ppm_user();
  if (argList.IsPresent("-ppm"))
    ppmUser = argList.Value("-ppm");

  // find the name associated with that number
  ppm_users_t	users[MAX_PPM_USERS];
  int numUsers = GetUsersPPM(users);
  if(numUsers < 1)
    return;		// can't read file
  int i;
  for(i=0; i<numUsers; i++)
    if(users[i].ppm_number == ppmUser)
      break;
  if(i == numUsers)	// did not make a match
    return;

  // create the string to append
  char* ppmString = new char[strlen(users[i].name) + 20];
  if( inquire_ppm_setup_mode() )	// setup mode on
    sprintf(ppmString, "Default: %s (Setup)", users[i].name);
  else
    sprintf(ppmString, "Default: %s", users[i].name);

  // load the string into the label
  ppmLabel->SetLabel(ppmString);
  delete [] ppmString;

  // change the color to agree with the ppm user
  ppmLabel->SetForegroundColor(ppmFgColors[ppmUser-1]);

  // change the menu bar color
  menubar->SetBackgroundColor(ppmBgColors[ppmUser-1]);
}

const char* SSMainWindow::GetSelectedPath()
{
  const StdNode* lastSelection = treeTable->GetNodeSelected();
  if(lastSelection == NULL)
    return NULL;

  // get the full path
  const StdTree* tree = treeTable->GetTree();
  const char* path = tree->GenerateNodePathname(lastSelection);

  // return path with /acop/ deleted off the front
  return( &path[6] );
}

const StdNode* SSMainWindow::GetSelectedNode()
{
  return( treeTable->GetNodeSelected() );
}

dir_node_t* SSMainWindow::GetSelectedDirNode()
{
  const StdNode* theNode = treeTable->GetNodeSelected();
  if(theNode != NULL)
    return( treeTable->GetMachineTree()->Convert(theNode) );
  else
    return NULL;
}

void SSMainWindow::LoadPageList(const UIWindow* winSelection)
{
  // get the list of strings to load
  const UIWindow** wins = GetWindows();
  long numWins = GetNumWindows();

  const char** items = (const char**) new char*[numWins+1];
  long selection = 0;
  const SSPageWindow* pageWin;
  const SSCldWindow* cldWin;
  PetWindow* petWin;
  PET_WINDOW_TYPE type;
  for(long i=0; i<numWins; i++)
    {
      type = WindowType((UIWindow*) wins[i]);
      switch (type){
      case PET_LD_WINDOW:
	pageWin = (SSPageWindow*) wins[i];
	items[i] = pageWin->GetListString();
	break;
      case PET_CLD_WINDOW:
	cldWin = (SSCldWindow*) wins[i];
	items[i] = cldWin->GetListString();
	break;
      case PET_ADO_WINDOW:
	petWin = (PetWindow*) wins[i];
	//items[i] = UIGetLeafName( petWin->GetTitle() );
	items[i] = petWin->GetListString();
	break;
      default:
	break;
      }

      if(wins[i] == winSelection)
	selection = i+1;
    }
  items[numWins] = NULL;
  if(selection == 0 && numWins > 0)
    selection = 1;

  // load them in the list
  pageList->SetItemsNoSelection(items);
  if(selection > 0)
    pageList->SetSelection(selection);

  // make sure all of the items are displayed
  if(numWins > 1)
    {
      if(numWins >= 5)
	{
	  pageList->SetItemsVisible(5);
	  pageList->ShowSelection();
	}
      else
	pageList->SetItemsVisible( (short) numWins);
    }
  else
    pageList->SetItemsVisible(1);

  UIForceResize(pageList);
  delete [] items;
}

void SSMainWindow::HandleEvent(const UIObject* object, UIEvent event)
{
  if (object == application && event == UIFileDesc)
    {
      if (application->GetInputId() == knobPanelId)
	{
	  if (supportKnobPanel && (activeLdWin != NULL))
	    //if it is an input on the knob panel sio line
	    knobPanel->ProcessKnobPanelSIO();
	  else
            knobPanel->ThrowAwayKnobInput();
	}
    }

  // main window events
  if(object == this && event == UIWindowMenuClose)
    Exit();

  // user closing the viewer window used to display archive summary log
  else if(object == viewer && event == UIWindowMenuClose)
    viewer->Hide();

  // events from the search popup
  else if(object == searchPopup)
    {
      if(event == UISelect)		// view the selected device list
	{
	  // get the selected node
	  MachineTree* mtree = treeTable->GetMachineTree();
	  StdNode* rootNode = mtree->GetRootNode();
	  const char* rootPath = mtree->GenerateNodePathname(rootNode);
	  const char* selectString = searchPopup->GetDeviceListSelection();
	  char* nodeName = new char[strlen(rootPath) + strlen(selectString) + 2];
	  strcpy(nodeName, rootPath);
	  strcat(nodeName, "/");
	  strcat(nodeName, selectString);
	  nodeName[strlen(nodeName)-1] = 0;		// get rid of newline at the end
	  StdNode* selectNode = mtree->FindNode(nodeName);
	  delete [] nodeName;
	  if(selectNode == NULL)
	    {
	      RingBell();
	      UILabelPopup popup(this, "searchError", "Can't find selected node in the\nMachine Tree.");
	      popup.Wait();
	      return;
	    }
	  // if this is the first selection, create a new window
	  // otherwise, reload the save search device page window
	  // simulate a button 1 or button 2 down event
	  searchPopup->SetWorkingCursor();
	  treeTable->SetNodeSelected(selectNode);
	  treeTable->LoadTreeTable();
          HandleEvent(treeTable, UITableBtn2Down);

          // store a ptr to window so can reload if neccessary
          long selection = pageList->GetSelection();
          if(selection > 0)
            searchPage = this->GetWindow(selection);

	  // make sure the first device name with the search string is highlighted
	  const char* searchStr = searchPopup->GetSearchString();
	  if(searchStr != NULL && searchStr[0] != 0 && searchPage != NULL)
	    switch (WindowType(searchPage))
	      {
	      case PET_LD_WINDOW:
	      case PET_CLD_WINDOW:
		((SSPageWindow*) searchPage)->ShowDeviceSubstring(searchStr);
		break;
	      case PET_ADO_WINDOW:
		((PetWindow*) searchPage)->ShowSubString(searchStr);
		break;
	      default:
		break;
	      }
	  searchPopup->SetStandardCursor();
	}
      else if(event == UIHide)	// the user closed the search popup
	searchPage = NULL;	// don't save this anymore once the popup goes down
    }

  // a device page window was made active
  else if(event == UIWindowActive)
  {
    // check to see if sent from one of the device pages
    if( IsWindowInList( (UIWindow*) object) == 0)
      return;

    if (activeLdWin != NULL)
    {
      if (supportKnobPanel && activeLdWin != object )
	((SSPageWindow*) activeLdWin)->ClearTheKnobPanel();
    }

    // redisplay the tree table and list
    LoadTable( (UIWindow*) object);
    if( !strcmp( object->ClassName(), "SSPageWindow") )
    {
      if (supportKnobPanel && activeLdWin != object)
	((SSPageWindow*) object)->EnableTheKnobPanel();
      activeLdWin = (SSPageWindow*) object;
    }
    else
    {
      activeAdoWin = (PetWindow*) object;
    }
    LoadPageList( (UIWindow*) object);
  }

  // user chose the Close menu item from the window menu of a device page
  else if(event == UIWindowMenuClose)
    {
      if (singleDeviceListOnly && !strcmp( object->ClassName(), "SSPageWindow") )
	//if is an SSPageWindow, may be leaving application
	{
	  Exit();
	  return;
	}
      // check to see if sent from one of the device pages
      if( IsWindowInList( (UIWindow*) object) == 0)
	return;
      // delete the window and remove it from the list
      UIWindow* win = (UIWindow*) object;
      win->Hide();
      DeleteListWindow(win);
    }

  // user chose the Hide menu item from the Page menu of a device page
  else if(event == UIHide)
    {
      // check to see if sent from one of the device pages
      if( IsWindowInList( (UIWindow*) object) == 0)
	return;
      // hide the window
      HideListWindow( (UIWindow*) object);
    }

  // user made a leaf selection from the machine tree table
  else if(object == treeTable)
  {
    SetMessage("");
    SSPageWindow* ldWin = NULL;
    PetWindow* adoWin = NULL;
    UIWindow* win;
    bool creatLdWin = false;
    bool creatAdoWin = false;
    long numWins = GetNumWindows();
    if(event == UISelect || event == UIAccept || event == UITableBtn2Down) {
      char s[512];
      DirTree* tree = (DirTree*)treeTable->GetTree();
      const StdNode* selectedNode = treeTable->GetNodeSelected();
      tree->GenerateFullNodePathname(selectedNode, s);
      strcat(s, "/");
      PET_WINDOW_TYPE type = WindowType(s);
      if (type == PET_UNKNOWN_WINDOW)
        {
          // give an error message - no file found!
          RingBell();
          SetMessage("No ld or ado device list found.");
          return;
        }
      SetWorkingCursor();
      // in case we have an error - loading a window - let's remember the window we are replacing!
      char* ldWindowPath = NULL;
      char* adoWindowPath = NULL;
      if(event == UISelect || event == UIAccept) {	// load current window
	// do we have a window (or windows) to reload of the right type - otherwise create one
	if(type == PET_LD_WINDOW || type == PET_HYBRID_WINDOW) {
	  if(numWins > 0 && activeLdWin && IsWindowInList( (UIWindow*) activeLdWin)) {
	    activeLdWin->CancelContinuousUpdate();
	    if (supportKnobPanel)
	      activeLdWin->ClearTheKnobPanel();
	    ldWin = activeLdWin;
            const char* ldPath = ldWin->GetCurrentFileName();
            if (ldPath != NULL)
              {
                ldWindowPath = new char[strlen(ldPath)+1];
                strcpy(ldWindowPath, ldPath);
                // now null terminate the /device_list.ado or /device_list ending away
                AdjustName(ldWindowPath);
              }
	  }
	  else {
            creatLdWin = true;
	    ldWin = CreateLdWindow();
          }
        }
	if(type == PET_ADO_WINDOW || type == PET_HYBRID_WINDOW)
          {
            if(numWins > 0 && activeAdoWin && IsWindowInList( (UIWindow*) activeAdoWin)) {
              adoWin = activeAdoWin;
              const char* adoPath = adoWin->GetCurrentFileName();
              if (adoPath != NULL)
                {
                  adoWindowPath = new char[strlen(adoPath)+20]; // need space - because will append ending back later!
                  strcpy(adoWindowPath, adoPath);
                  // now null terminate the /device_list.ado or /device_list ending away
                  AdjustName(adoWindowPath);
                }
            }
            else {
              creatAdoWin = true;
              adoWin = CreateAdoWindow();
            }
          }
      }
      else if(event == UITableBtn2Down) {  // create new window(s)
	// check to see if a window already exists
	if(type == PET_LD_WINDOW || type == PET_HYBRID_WINDOW)
	  ldWin = (SSPageWindow*) FindWindow(s, PET_LD_WINDOW);
	if(type == PET_ADO_WINDOW || type == PET_HYBRID_WINDOW)
	  adoWin = (PetWindow*) FindWindow(s, PET_ADO_WINDOW);
	if(ldWin || adoWin) {
	  UILabelPopup popup(this, "popup", "One or more windows are already showing this file.\nDo you really want to create a duplicate?", "No", "Yes");
	  int retval = popup.Wait();
	  if(retval == 1) { // No
	    if(ldWin) {
	      ldWin->Show();
	      if(type == PET_LD_WINDOW)  type = PET_UNKNOWN_WINDOW;
	      else                       type = PET_ADO_WINDOW;
	    }
	    if(adoWin) {
	      adoWin->Show();
	      if(type == PET_ADO_WINDOW) type = PET_UNKNOWN_WINDOW;
	      else                       type = PET_LD_WINDOW;
	    }
	    if(type == PET_UNKNOWN_WINDOW) { // nothing more to create
	      SetStandardCursor();
              delete [] ldWindowPath;
              delete [] adoWindowPath;
              return;
	    }
	  }
	  else { // Yes - make sure we create new windows
	    adoWin = NULL;
	    ldWin = NULL;
	  }
	}
	// create a window (or windows) of the right type
	if(type == PET_LD_WINDOW || type == PET_HYBRID_WINDOW)
          {
            creatLdWin = true;
            ldWin = CreateLdWindow();
          }
	if(type == PET_ADO_WINDOW || type == PET_HYBRID_WINDOW)
          {
            creatAdoWin = true;
            adoWin = CreateAdoWindow();
          }
      }
      // now we have one (or two) windows - load the proper device list(s)
      const char* selectPath = treeTable->GetNodePath();
      if(selectPath == NULL) {
	RingBell();
	SetMessage("Can't determine path to device list.");
	SetStandardCursor();

        // if created a window - delete it - remove it from the list!
        if (creatLdWin)
          {
            if (ldWin)
              DeleteWindow(ldWin);
          }
        if (creatAdoWin)
          {
            if (adoWin)
              DeleteWindow(adoWin);
          }
        delete [] ldWindowPath;
        delete [] adoWindowPath;
	return;
      }
      SetMessage("Loading device page(s)...");
      if(ldWin != NULL) {
	if( LoadDeviceList(ldWin, selectPath, get_ppm_user()) < 0) {
	  RingBell();
	  SetMessage("Could Not Load Device List");
          //	  if (event == UITableBtn2Down) {
          //	    //a new window was created but not loaded successfully, better delete it
          //	    delete ldWin;  ldWin = NULL;
          //	  }
          // if created a window - delete it - remove it from the list!
          if (creatLdWin)
            {
              DeleteWindow(ldWin);
              delete [] ldWindowPath;
              delete [] adoWindowPath;
              SetStandardCursor();
              return;
            }
          else
            {
              // we were loading a device page into an existing window
              // restore to its original path
              if( ldWindowPath != NULL && LoadDeviceList(ldWin, ldWindowPath) < 0)
                {
                  RingBell();
                  delete [] ldWindowPath;
                  delete [] adoWindowPath;
                  SetStandardCursor();
                  return;
                }
              else
                DisplayError("Problem loading device list.\nRestored to initial page.");
            }
	}
	else
	{
	  if (supportKnobPanel)
	    ldWin->SupportKnobPanel(knobPanel);
	}
	ldWin->SetListString();
	ldWin->SetPageNode( treeTable->GetNodeSelected() );
	// make sure the window is visible
	ldWin->Show();
	ldWin->Refresh();

	// put table in continuous acquisition mode
	ldWin->UpdateContinuous();
	LoadPageList(ldWin);
	activeLdWin = ldWin;
      }
      if(adoWin != NULL) {
	char s[512];
	DirTree* tree = (DirTree*)treeTable->GetTree();
	const StdNode* theNode = tree->GetRootNode();
	if (tree->GenerateFullNodePathname(theNode, s) == 0)
	  adoWin->SetTreeRootPath(s);
	tree->GenerateFullNodePathname(treeTable->GetNodeSelected(), s);
	strcat(s, "/");
	strcat(s, ADO_DEVICE_LIST);
	adoWin->LoadFile(s, tree->GenerateNodePathnameWithoutRoot( treeTable->GetNodeSelected() ),
                    get_ppm_user());
        if (adoWin->CreateOK())
          {
            if (type == PET_HYBRID_WINDOW && !adoWin->IsManaged() )
              SetWindowPos(adoWin, ldWin);
            adoWin->SetPageNode( treeTable->GetNodeSelected() );
            adoWin->Show();
            LoadPageList(adoWin);
            activeAdoWin = adoWin;
          }
        else
          {
            if (creatAdoWin)
              {
                RingBell();
                delete [] ldWindowPath;
                delete [] adoWindowPath;
                SetStandardCursor();
                SetMessage("Could Not Load Device List");
                DeleteWindow(adoWin);
                return;
              }
            else
              {
                // we were loading a device page into an existing window
                // restore to its original path
                bool anError = false;
                if( adoWindowPath != NULL)
                  {
                    DirTree* tree = (DirTree*)treeTable->GetTree();
                    // the adoWindowPath should not begin with /operations the root of the tree - for this call
                    StdNode* selectedNode = NULL;
                    if ( !strncmp(adoWindowPath, "/operations/", strlen("/operations/")) )
                      selectedNode = tree->FindNode(&(adoWindowPath[strlen("/operations/")-1]));
                    strcat(adoWindowPath, "/");
                    strcat(adoWindowPath, ADO_DEVICE_LIST);
                    if (selectedNode != NULL)
                      adoWin->LoadFile(adoWindowPath, tree->GenerateNodePathnameWithoutRoot(selectedNode),
                                       get_ppm_user());
                    else
                      adoWin->LoadFile(adoWindowPath, NULL, get_ppm_user());
                    if (adoWin->CreateOK())
                      {
                        if (type == PET_HYBRID_WINDOW && !adoWin->IsManaged() )
                          SetWindowPos(adoWin, ldWin);
                        adoWin->SetPageNode(selectedNode);
                        adoWin->Show();
                        LoadPageList(adoWin);
                        activeAdoWin = adoWin;
                        SetMessage("Could Not Load Device List");
                        DisplayError("Problem loading device list.\nRestored to initial page.");
                        anError = true;
                      }
                    else
                      {
                        RingBell();
                        SetMessage("Could Not Load Device List");
                        DisplayError("Problem restoring original device list.");
                        anError = true;
                      }
                  }
                else
                  {
                    SetMessage("Could Not Load Device List");
                    RingBell();
                    DisplayError("Problem restoring original device list.");
                    anError = true;
                  }
                if (anError)
                  {
                    delete [] ldWindowPath;
                    delete [] adoWindowPath;
                    SetStandardCursor();
                    return;
                  }
              }
          }
        delete [] ldWindowPath;
        delete [] adoWindowPath;
      }
      SetStandardCursor();
      SetMessage("");
    }
  }

  // user made a selection from the list of device pages
  else if(object == pageList && event == UISelect)
    {
      SetMessage("");
      SP_Show();
    }
    // user wants a cld window
  else if(event == UIEvent4)
  {
     PetWindow* win = (PetWindow*) GetWindow(pageList->GetSelection());
     PetPage* ppage = win->GetPetPage();
     const char* devName = ppage->CellGetAdo();
     ShowCldEditor(devName, ppage->GetPPMUser());
  }
  else if(event == UIEvent3)
    {
      SSPageWindow* ldWin = (SSPageWindow*) GetWindow(pageList->GetSelection());
      char name[64];
      ldWin->CreateAgsPage(name);
      ldWin->SetListString(name);
      AddWindow(ldWin);
      if (supportKnobPanel)
	ldWin->SupportKnobPanel(knobPanel);
      ldWin->Show();
      ldWin->UpdateContinuous();
      LoadPageList(ldWin);
      // An existing page is being replaced.  Remove references to old location in tree
      ldWin->SetDevListPath("");
      ldWin->SetPageNode(NULL);
      SetStandardCursor();
    }
  else if(event == UIEvent2)
    {
      // event passed up from pet library indicating a window was created or reloaded
      LoadPageList( (UIWindow*) object);
    }
  else if(event == UIEvent1)
    {
      // event passed up from pet library
      if (IsWindowInList((UIWindow*) object)){
	PetWindow* adoWin = (PetWindow*) object;
	int ppmUser = adoWin->GetPPMUser();
	adoWin = new PetWindow(this, "adoWindow");
   adoWin->GetPetPage()->AddEventReceiver(this);
	adoWin->SetLocalPetWindowCreating(false);
	// in this case, we will assume the file loaded is not from the tree
	// so don't set the tree - otherwise need to somehow be smart - skip it for now
	adoWin->LoadFile(object->GetMessage(), NULL, ppmUser);
        if (adoWin->CreateOK())
          {
            AddListWindow(adoWin);
            adoWin->Show();
            LoadPageList(adoWin);
            activeAdoWin = adoWin;
          }
        else
          {
            SetMessage("Received window creation event - problem loading");
          }
      }
    }

  // user made a selection from the pulldown menus
  else if(object == pulldownMenu && event == UISelect)
    {
      const UITreeData* data = pulldownMenu->GetTreeData();
      SetMessage("");

      if(!strcmp(data->namesSelected[0], "File"))
	{if(!strcmp(data->namesSelected[1], "New..."))
	  {     SS_New();
	  }
	else if(!strcmp(data->namesSelected[1], "Open..."))
	  {     SS_Open();
	  }
	else if(!strcmp(data->namesSelected[1], "Default PPM User..."))
	  {	SS_Default_PPM_User();
	  }
	else if(!strcmp(data->namesSelected[1], "Create RHIC Page..."))
	  {     SS_Create_RHIC_Page();
	  }
	else if(!strcmp(data->namesSelected[1], "Create PS RHIC Page..."))
	  {     SS_Create_PS_RHIC_Page();
	  }
	else if(!strcmp(data->namesSelected[1], "Create AGS Page..."))
	  {     SS_Create_AGS_Page();
	  }
	else if(!strcmp(data->namesSelected[1], "Quit"))
	  {	SS_Quit();
	  }
	}
      else if(!strcmp(data->namesSelected[0], "Page"))
	{	if(!strcmp(data->namesSelected[1], "Find"))
	  {	SP_Find();
	  }
	else if(!strcmp(data->namesSelected[1], "New"))
	  {	SP_New();
	  }
	else if(!strcmp(data->namesSelected[1], "Show"))
	  {	SP_Show();
	  }
	else if(!strcmp(data->namesSelected[1], "Hide"))
	  {	SP_Hide();
	  }
	else if(!strcmp(data->namesSelected[1], "Close"))
	  {	SP_Close();
	  }
	else if(!strcmp(data->namesSelected[1], "Close All..."))
	  {	SP_Close_All();
	  }
	}
      else if(!strcmp(data->namesSelected[0], "Options"))
	{	if(!strcmp(data->namesSelected[1], "Search..."))
	  {	SO_Search();
	  }
	else if(!strcmp(data->namesSelected[1], "Read Archive Log"))
	  {	SO_Read_Archive_Log();
	  }
	else if(!strcmp(data->namesSelected[1], "SLDs via Controller..."))
	  {	SO_SLDs();
	  }
	else if(!strcmp(data->namesSelected[1], "CLDs..."))
	  {	SO_CLDs();
	  }
	else if(!strcmp(data->namesSelected[1], "CLD Events..."))
	  {	SO_CLD_Events();
	  }
	else if(!strcmp(data->namesSelected[1], "PPM User Monitor"))
	  {	SO_PpmUserMonitor();
	  }
        else if(!strcmp(data->namesSelected[1], "Reload DDF..."))
          {     SO_Load_DDF();
          }
	}
    }
  else if (event == UIMessage)
    {
      SetMessage(object->GetMessage());
    }
  else if (object == application && event == UITimer)
    {
      if (application->GetTimerId() == dumpElogAndExitTimerId){
        SSPageWindow* ldWin = (SSPageWindow*) GetWindow(1);
        if (ldWin){
          ldWin->PrintDump();
          exit(0);
        } else
          SetMessage("Unable to find window for elog dump");
      }
    }
  // otherwise, pass event to base class
  else
    UIMainWindow::HandleEvent(object, event);
}

SSPageWindow* SSMainWindow::CreateLdWindow()
{
  SSPageWindow* ldWin;
  if (supportKnobPanel && activeLdWin != NULL)
    activeLdWin->ClearTheKnobPanel();

  ldWin = new SSPageWindow(this, "ldWindow");
  AddListWindow(ldWin);
  return ldWin;
}

PetWindow* SSMainWindow::CreateAdoWindow()
{
  PetWindow* adoWin;
  DirTree *tree = (DirTree*) treeTable->GetTree();
  if(tree) {
    char treeRootPathAndNode[512];
    const StdNode* theNode = tree->GetRootNode();
    if (tree->GenerateFullNodePathname(theNode, treeRootPathAndNode) == 0)
      adoWin = new PetWindow(this, "adoWindow", wname, treeRootPathAndNode);
    else
      adoWin = new PetWindow(this, "adoWindow");
  }
  else
    adoWin = new PetWindow(this, "adoWindow");
  adoWin->SetLocalPetWindowCreating(false);
  adoWin->GetPetPage()->AddEventReceiver(this);
  AddListWindow(adoWin);
  return adoWin;
}

void SSMainWindow::DisplayError(char* errToDisplay)
{
  if (errFillExistWindowPopup == NULL)
    {
      errFillExistWindowPopup = new UILabelPopup(this, "errFillExistWindowPopup");
    }
  if (errToDisplay != NULL)
    {
      errFillExistWindowPopup->SetLabel(errToDisplay);
      errFillExistWindowPopup->Show();
    }
}

int SSMainWindow::LoadDeviceList(SSPageWindow* win, const char* deviceList, short ppmUser)
{
  // find a title which starts with /Booster, /Ags, etc
  MachineTree* machTree = treeTable->GetMachineTree();
  const char* rootPath = machTree->GetRootPath();
  const StdNode* rootNode = machTree->GetRootNode();
  const char* rootName = rootNode->Name();
  int len = strlen(rootPath) + strlen(rootName) + 1;
  char* fullRootPath = new char[len + 1];
  strcpy(fullRootPath, rootPath);
  strcat(fullRootPath, "/");
  strcat(fullRootPath, rootName);

  // create the full name for the device list
  char* filename = new char[strlen(deviceList) + 16];
  strcpy(filename, deviceList);
  strcat(filename, "/");
  strcat(filename, LD_DEVICE_LIST);

  // load the device list
  win->SetPPMUser(ppmUser);
  int retval = win->LoadFile(filename, &deviceList[len], ppmUser);
  if(retval >= 0)	// success
    {
      // store the short path to the device list and the root node name
      win->SetDevListPath(&deviceList[len]);
      win->SetMachineTreeRootPath(fullRootPath);
    }
  delete [] fullRootPath;
  delete [] filename;
  return retval;
}

void SSMainWindow::SetWindowPos(UIWindow* newWin, UIWindow* currWin)
{
  if(newWin == NULL || mainWindow == NULL)
    return;

  // get the position and width of the main newWin
  short mainX, mainY, mainWidth;
  GetPosition(mainX, mainY);
  mainWidth = GetWidth();

  // set xpos of new newWin according to main newWin coordinates
  short xpos = mainX + mainWidth + 13;

  // set ypos
  short ypos = 0;
  if( !strcmp( newWin->ClassName(), "SSPageWindow") || !strcmp( newWin->ClassName(), "PetWindow") )
  {
    // get the position and height of the current device page
    // and set the ypos of the new newWin accordingly
    if(currWin == NULL) {
      long selection = pageList->GetSelection();
      if(selection)
	currWin = GetWindow(selection);
    }
    if(currWin != NULL) {
      short pageX, pageY, pageHeight;
      currWin->GetPosition(pageX, pageY);
      if(pageY < 150)		// current page is on the top part of the newWin
      {
	pageHeight = currWin->GetHeight();
	if(pageHeight < 600)	// user did not expand height of newWin
	  ypos = pageY + pageHeight + 7;
      }
    }
  }
  // set the position of the new newWin
  newWin->SetPosition(xpos, ypos);
}

UIWindow* SSMainWindow::FindWindow(const char* file, PET_WINDOW_TYPE wtype)
{
  if (file == NULL)
    return NULL;
  char fileName[512];
  strcpy(fileName, file);
  if(wtype == PET_ADO_WINDOW)
    strcat(fileName, ADO_DEVICE_LIST);
  else
    strcat(fileName, LD_DEVICE_LIST);

  UIWindow* win;
  int numWindows = GetNumWindows();
  const char* winName;
  PET_WINDOW_TYPE type;
  for(int i=0; i<numWindows; i++) {
    win = GetWindow(i+1);
    type = WindowType(win);
    winName = NULL;
    switch (type)
    {
      case PET_ADO_WINDOW:
        winName = ((PetWindow*)win)->GetCurrentFileName();
        break;
      case PET_LD_WINDOW:
        winName = ((SSPageWindow*)win)->GetCurrentFileName();
        break;
      case PET_CLD_WINDOW: // currently not supported
      default:
        break;
    }
    if(type == wtype && winName && !strcmp(fileName, winName))
      return win;
  }
  return NULL;
}

PET_WINDOW_TYPE SSMainWindow::WindowType(UIWindow* window)
{
  if (window == NULL || window->ClassName() == NULL)
    return PET_UNKNOWN_WINDOW;

  if (!strcmp(window->ClassName(), "pageWindow") || !strcmp(window->ClassName(), "SSPageWindow"))
    return PET_LD_WINDOW;
  else if (!strcmp(window->ClassName(), "SSCldWindow"))
    return PET_CLD_WINDOW;
  else if (!strcmp(window->ClassName(), "PetWindow"))
    return PET_ADO_WINDOW;
  else
    return PET_UNKNOWN_WINDOW;
}

PET_WINDOW_TYPE SSMainWindow::WindowType(const char* path)
{
  // determine whether a ADO_DEVICE_LIST or a LD_DEVICE_LIST can be found at the base of path
  char petfile[512];
  strcpy(petfile, path);
  strcat(petfile, "/");
  strcat(petfile, ADO_DEVICE_LIST);

  char ssfile[512];
  strcpy(ssfile, path);
  strcat(ssfile, "/");
  strcat(ssfile, LD_DEVICE_LIST);

  if (UIFileExists(petfile) && UIFileExists(ssfile)) {
    if (supportKnobPanel) return PET_LD_WINDOW;
    return PET_HYBRID_WINDOW;
  }
  else if (UIFileExists(petfile))
    return PET_ADO_WINDOW;
  else if (UIFileExists(ssfile))
    return PET_LD_WINDOW;
  else
    return PET_UNKNOWN_WINDOW;
}

void SSMainWindow::DeleteListWindow(UIWindow* window)
{
  RemoveWindow(window);

  // reload the list with the 1st item selected
  LoadPageList(NULL);

  // if there is a first window, bring it to the top and display its path
  long numWins = GetNumWindows();
  UIBoolean winFound = UIFalse;
  for(int i=1; i<=numWins; i++)
    {
      UIWindow* win = GetWindow(i);
      if( win->IsVisible() && !strcmp( win->ClassName(), "SSPageWindow") )
	{
	  SSPageWindow* pageWin = (SSPageWindow*) win;
	  LoadTable(pageWin);
	  pageWin->Show();
	  winFound = UITrue;
	  break;
	}
    }
}

void SSMainWindow::DeleteAllWindows()
{
  UIMainWindow::DeleteAllWindows();
  activeAdoWin = NULL;
  activeLdWin = NULL;

  // reload the device page list and the machine tree table
  LoadPageList(NULL);
  LoadTable( (const StdNode*) NULL);
}

void SSMainWindow::RemoveWindow(UIWindow* window)
{
  if (window == NULL)
    return;

  // make the window invisible
  window->Hide();

  // if the window was an SSPageWindow, and the window was stored as the searchPage,
  // set searchPage to NULL prior to deleting the window
  if ( !strcmp(window->ClassName(), "SSPageWindow") || !strcmp(window->ClassName(), "pageWindow"))
  {
    if ( (SSPageWindow*) searchPage == (SSPageWindow*) window )
      searchPage=NULL;

    // if knobbing is supported, be sure to clear the knob panel
    if (supportKnobPanel)
    {
      SSPageWindow* tmpWindow = (SSPageWindow*) window;
      tmpWindow->ClearTheKnobPanel();
    }
  }
  if(window == activeLdWin)
    activeLdWin = NULL;
  else if(window == activeAdoWin)
    activeAdoWin = NULL;

  // delete it (delayed) and remove it from the window list
  DeleteWindow(window);
}

void SSMainWindow::HideListWindow(UIWindow* window)
{
  // turn off reports for the window
  if( window->IsVisible() )
    if (!strcmp( window->ClassName(), "SSPageWindow") )
    {
      SSPageWindow* pageWin = (SSPageWindow*) window;
      // if knobbing is supported, be sure to clear the knob panel
      if (supportKnobPanel)
	pageWin->ClearTheKnobPanel();

      pageWin->StopContinuousUpdate();
    }
    else if(!strcmp( window->ClassName(), "SSCldWindow") )
    {
      SSCldWindow* cldWin = (SSCldWindow*) window;
      cldWin->StopContinuousUpdate();
    }
  // make the window invisible
  window->Hide();
}

void SSMainWindow::ShowListWindow(UIWindow* window)
{
  if( window->IsVisible() )
    {
      // make the window visible and bring it to the front
      window->Show();
    }
  else
    {
      window->Show();
      // if window was in continuous update mode, start up reports again
      if( !strcmp( window->ClassName(), "SSPageWindow") )
	{
	  SSPageWindow* pageWin = (SSPageWindow*) window;
	  pageWin->CheckUpdate();
	}
      else if( !strcmp( window->ClassName(), "SSCldWindow") )
	{
	  SSCldWindow* cldWin = (SSCldWindow*) window;
	  cldWin->StartContinuousUpdate();
	}
    }
}

void SSMainWindow::AddListWindow(UIWindow* window)
{
  // set up events to be received by main window
  window->EnableEvent(UIWindowMenuClose);
  window->EnableEvent(UIWindowActive);
  window->AddEventReceiver(this);

  // make sure the new window is positioned properly
  SetWindowPos(window);

  // store this new window with the main window
  AddWindow(window);
}

void SSMainWindow::LoadTable(const UIWindow* window)
{
  // get the node in the machine tree that was selected
  if(window == NULL)
    return;

  const StdNode* node = NULL;
  switch (WindowType((UIWindow*) window))
    {
    case PET_LD_WINDOW:
      node = ((SSPageWindow*)window)->GetPageNode();
      break;
    case PET_CLD_WINDOW:
      node = NULL;
      break;
    case PET_ADO_WINDOW:
      node = ((PetWindow*)window)->GetPageNode();
      break;
    default:
      break;
    }

  if (node == NULL)
    return;

  LoadTable(node);
}

void SSMainWindow::LoadTable(const StdNode* node)
{
  // make the node the selected one in the table and redisplay
  treeTable->SetNodeSelected(node);
  treeTable->LoadTreeTable();
  treeTable->UpdateKeyboardSelector();
}

void SSMainWindow::SS_New()
{
  PetWindow* petWin = new PetWindow(this, "petWindow");
  petWin->SetLocalPetWindowCreating(false);
  AddListWindow(petWin);
  petWin->TF_Open();
  petWin->Show();
  LoadPageList(petWin);
  activeAdoWin = petWin;
  petWin->GetPetPage()->AddEventReceiver(this);
}

void SSMainWindow::SS_Open()
{
  SetWorkingCursor();
  PetWindow* petWin;
  // get the selected window
  long selection = pageList->GetSelection();
  if (selection > 0){
    UIWindow* win = (UIWindow*) this->GetWindow(selection);
    if (WindowType(win) == PET_ADO_WINDOW)
      petWin = (PetWindow*) win;
    else{
      petWin = new PetWindow(this, "petWindow");
      petWin->SetLocalPetWindowCreating(false);
      AddListWindow(petWin);
    }
  }
  else{
    petWin = new PetWindow(this, "petWindow");
    petWin->SetLocalPetWindowCreating(false);
    AddListWindow(petWin);
  }

  petWin->GetPetPage()->AddEventReceiver(this);
  petWin->TF_Open();
  petWin->Show();
  LoadPageList(petWin);
  SetStandardCursor();
}

void SSMainWindow::SS_Set_Host()
{
}

void SSMainWindow::ReconnectToRelway(const char* newHost)
{
}

void SSMainWindow::AdjustName(char* devicePath)
{
  if (devicePath != NULL)
    {
      char* theSlashDevice = strstr(devicePath, "/device_list");
      if (theSlashDevice != NULL)
        theSlashDevice[0] = '\0';
    }
}

void SSMainWindow::SS_Default_PPM_User()
{
  // put up ppm popup and wait for user input
  UIPPMPopup popup(this, "ppmPopup");
  if(popup.Wait() == 2)	// Cancel
    return;

  // set the process PPM user and setup mode to what the user has selected
  popup.SetProcessUser( popup.GetPopupUserNum() );

  // change the label that displays the PPM user
  SetPPMLabel();
}

void SSMainWindow::SS_Create_RHIC_Page()
{
  SetWorkingCursor();
  PetWindow* petWin = new PetWindow(this, "petWindow");
  petWin->SetLocalPetWindowCreating(false);
  AddListWindow(petWin);
  petWin->GetPetPage()->AddEventReceiver(this);
  petWin->TF_Create_Pet_Page();
  petWin->Show();
  LoadPageList(petWin);
  activeAdoWin = petWin;
  SetStandardCursor();
}

void SSMainWindow::SS_Create_PS_RHIC_Page()
{
  SetWorkingCursor();
  PetWindow* petWin = new PetWindow(this, "petWindow");
  petWin->GetPetPage()->AddEventReceiver(this);
  petWin->SetLocalPetWindowCreating(false);
  AddListWindow(petWin);
  petWin->TF_Create_PS_Pet_Page();
  petWin->Show();
  LoadPageList(petWin);
  activeAdoWin = petWin;
  SetStandardCursor();
}

void SSMainWindow::SS_Create_AGS_Page()
{
  SetWorkingCursor();
  SSPageWindow* pageWin = new SSPageWindow(this, "pageWindow");
  char name[64];
  pageWin->CreateAgsPage(name);
  pageWin->SetListString(name);
  AddListWindow(pageWin);
  if (supportKnobPanel)
    pageWin->SupportKnobPanel(knobPanel);
  pageWin->Show();
  pageWin->UpdateContinuous();
  LoadPageList(pageWin);
  activeLdWin = pageWin;
  SetStandardCursor();
}

void SSMainWindow::SS_Quit()
{
  clean_up(0);
}

void SSMainWindow::Exit()
{
  // first put up an exit confirmation
  UILabelPopup popup(this, "exitPopup", NULL, "Yes", "No");
  popup.SetLabel("Exit ", application->Name(), "? Are you sure?");
  if(popup.Wait() == 1)	// Yes selected
    {
      clean_up(0);
    }
}

void SSMainWindow::SP_Find()
{
  // clear selection from the table and reload
  LoadTable( (const StdNode*) NULL);

  // make sure the table has focus
  treeTable->SetFocus();
}

void SSMainWindow::SP_New()
{
  if( treeTable->IsLeafNode( treeTable->GetNodeSelected() ) )
    {
      // simulate a 2nd mouse button event on the selected node
      HandleEvent(treeTable, UITableBtn2Down);
    }
  else
    {
      RingBell();
      SetMessage("Not on a node with a device list.");
    }
}

void SSMainWindow::SP_Show()
{
  // get the selected window
  long selection = pageList->GetSelection();
  if(selection < 1)
    {
      RingBell();
      SetMessage("Nothing selected");
      return;
    }
  UIWindow* win = (UIWindow*) this->GetWindow(selection);

  // show the window
  if(win != NULL)
    {
      ShowListWindow(win);
      // redisplay the tree table based on the selection
      LoadTable(win);
    }
}

void SSMainWindow::SP_Hide()
{
  // get the selected window
  long selection = pageList->GetSelection();
  if(selection < 1)
    {
      RingBell();
      SetMessage("Nothing selected");
      return;
    }
  UIWindow* win = (UIWindow*) this->GetWindow(selection);

  // hide the window
  if(win != NULL)
    HideListWindow(win);
}

void SSMainWindow::SP_Close()
{
  // get the selected window
  long selection = pageList->GetSelection();
  if(selection < 1)
    {
      RingBell();
      SetMessage("Nothing selected");
      return;
    }
  UIWindow* win = (UIWindow*) this->GetWindow(selection);

  // delete the window and remove it from the list
  if(win != NULL)
    DeleteListWindow(win);
}

void SSMainWindow::SP_Close_All()
{
  // first confirm
  UILabelPopup popup(this, "confirmPopup", NULL, "Yes", "No");
  popup.SetLabel("Close ALL device pages?  Are you sure?");
  if(popup.Wait() == 2)	// No selected
    return;

  // delete all windows from the list
  DeleteAllWindows();
}

void SSMainWindow::SO_Search()
{
  // put up for getting user input and making device list selections
  if(searchPopup == NULL)
    {
      searchPopup = new UISearchDeviceList(this, "searchPopup");
      searchPopup->AddEventReceiver(this);	// trap the View (UISelect) event from the popup

      // get the currently selected node and use it as a starting point
      const StdNode* currentNode = treeTable->GetNodeSelected();
      if(currentNode != NULL)
	{
	  DirTree* dtree = (DirTree*) treeTable->GetTree();
	  searchPopup->SetStartNode( dtree->GenerateNodePathname(currentNode) );
	}
    }
  // display the popup
  searchPopup->Show();
}

void SSMainWindow::SO_Read_Archive_Log()
{
  if(viewer == NULL)
    {
      viewer = new UIViewerWindow(this, "ssviewer");
      viewer->LockFile();	// turn off access to file system

      // trap Window Menu Close event so viewer can be hidden
      viewer->AddEventReceiver(this);
    }

  if( viewer->LoadFile(ARCHIVE_LOG) >= 0)
    viewer->Show();
  else
    SetMessage("Could not read Archive Summary Log");
}

void SSMainWindow::SO_PpmUserMonitor()
{
  system("/usr/controls/bin/PpmUserMon &");
}

static controller_record_t*  get_controller() {

    int i;
    ddf_pointers_t* ddf_pointers;
    controllers_t*  controllers;

    ddf_pointers = DeviceDirectory.ddfpointers();
	controllers  = ddf_pointers->controllers_ptr;
    char **controller_list = new char* [controllers->no_controllers];
    for (i=0;i<controllers->no_controllers;i++) controller_list[i] = controllers->controller[i].name;

    PopupMenu* controller_menu;
    controller_menu = new PopupMenu("Choose a controller.", controller_list, controllers->no_controllers);
    i = controller_menu->getchoice();
    delete controller_menu;
    delete[] controller_list;
    if (i < 1) return(NULL);
    return(&controllers->controller[i-1]);
}

void SSMainWindow::SO_SLDs()
{
  controller_record_t* controller_record = get_controller();
  if (controller_record == NULL) return;
  const char* theController = controller_record->name;
  TextString path;
  path << "/tmp/" << theController << "_pid" << getpid() << ".ld";
  unlink(path);
  TextString command;
  command << "echo " << theController << " > " << path;
  system(command);
  // create a new device page window and load list
  SSPageWindow* pageWin = new SSPageWindow(this, "pageWindow", AGS_ENG_MODE, theController);
  int retval = pageWin->LoadFile(path, theController, get_ppm_user());
  if(retval < 0)
    {
      RingBell();
      printf("Exiting... Could not load device list - %s\n", deviceList);
      //a new window was created but not loaded successfully, better delete it
      delete pageWin;
      delete [] deviceList;
      pageWin=NULL;
      SetStandardCursor();
      clean_up(0);
    }
  SetStandardCursor();

  //a new window.  Add to the list and enable its events
  AddListWindow(pageWin);

  pageWin->SetListString();
  pageWin->SetSingleDeviceListMode();

  // make sure the window is visible
  pageWin->Show();
  pageWin->Refresh();

  // put table in continuous acquisition mode
  pageWin->UpdateContinuous();

  // update main window if device list loaded successfully
  // update the device page list
  LoadPageList(pageWin);
  activeLdWin = pageWin;
  SetStandardCursor();
  SetMessage("");

  // don't unlink yet - we may use it for archive access ( but when will we ever get rid of it? )
  //unlink(path);
}

void SSMainWindow::SO_CLDs()
{
  // get a CLD name from the user
  if(ctrlPopup == NULL)
    ctrlPopup = new UIDevicesFromControllerPopup(this, "ctrlPopup");

  ctrlPopup->SetDeviceType(TYPE_CLD);
  const char* devname = ctrlPopup->SelectDevice( );
  if (devname == NULL)
    return;  // user cancelled

  // create a new SSCldWindow
  ShowCldEditor(devname, get_ppm_user() );
}

void SSMainWindow::SO_Load_DDF()
{
  // put up warning message
  UILabelPopup popup(this, "DDF Reload", "Reloading the DDF will force all LD and CLD windows to be reopened.  Continue?", "OK", "Cancel");
  if (popup.Wait() == 2){
    SetMessage("Reload DDF Operation Cancelled");
    return;
  }

  StdNode* nodes[128];
  for (int i=0; i<128; i++) nodes[i] = NULL;
  int index = 0;
  SSPageWindow* pageWin;
  UIWindow* win;

  // first remove LD windows
  int numWins = GetNumWindows();
  for (int i=0; i<numWins; i++){
    win = GetWindow(pageList->GetSelection());
    if (WindowType(win) == PET_LD_WINDOW || WindowType(win) == PET_CLD_WINDOW){
      pageWin = (SSPageWindow*) win;
      // keep track of the node
      StdNode* selectNode = (StdNode*) pageWin->GetPageNode();
      if (selectNode)
        nodes[index++] = selectNode;
      // delete the page
      pageWin->Hide();
      DeleteListWindow(pageWin);
    }
  }

  // map a ddf
  char msg[128];
  if( strlen( argList.String("-ddf") ) )
    {
      if( DeviceDirectory.load( argList.String("-ddf") ) <= 0)
	{
          sprintf(msg, "Could not map ddf %s.", argList.String("-ddf"));
	}
      else{
        sprintf(msg, "ddf %s successfully mapped.", argList.String("-ddf"));
      }
    }
  else	// map the current ddf
    {
      if( DeviceDirectory.load() <= 0)
	{
	  sprintf(msg, "Could not map current ddf.");
	}
      else
        sprintf(msg, "ddf successfully mapped.");
    }

  // now reload the ld pages
  for (int i=0; i<index; i++){
    treeTable->SetNodeSelected(nodes[i]);
    treeTable->LoadTreeTable();
    HandleEvent(treeTable, UITableBtn2Down);
  }

  SetMessage(msg);
}

void SSMainWindow::ShowCldEditor(const char* devname, int ppmuser)
{
  // create a new SSCldWindow
  Refresh();
  SetMessage("Creating new CLD page...");
  SetWorkingCursor();
  SSCldWindow* win = new SSCldWindow(this, "sscldWindow", devname, ppmuser);
  if (win->SuccessfullyCreated() == UIFalse || win->DataToCollect() == UIFalse)
    {
      SetStandardCursor();
      SetMessage("");
      UILabelPopup popup(this, "cldProbPopup", "NULL", "OK");
      if (win->SuccessfullyCreated() == UIFalse)
	{
	  int failure=win->ReasonNoCreate();
	  if (failure == DEVICE_BAD_NAME)
	    popup.SetLabel("CLD ", devname, " does not exist.\nCLD window creation aborted.");
	  else if (failure == DC_GET_TIMEOUT)
	    popup.SetLabel("CLD ", devname, " not reporting.\nCLD window creation aborted.");
	  else if (failure == DEVICE_BAD_REPORTFORMAT)
	    popup.SetLabel("CLD ", devname, " - bad report format.\nCLD window creation aborted.");
	  else if (failure == DEVICE_NODATA)
	    popup.SetLabel("CLD ", devname, " - no report received.\nCLD window creation aborted.");
	  else if (failure == DEVICE_UNPACKERROR || failure == DEVICE_BAD_REPORTFORMAT || failure == DEVICE_NOBUFFERSPACE)
	    popup.SetLabel("CLD ", devname, " - report format error.\nCLD window creation aborted.");
	  else
	    popup.SetLabel("CLD ", devname, " - unknown error.\nCLD window creation aborted.");
	}
      else
	popup.SetLabel("CLD ", devname, " - no data to collect.\nCLD window creation aborted.");
      delete win;
      popup.RingBell();
      popup.Wait();
      return;
    }

  // show it on the screen
  AddListWindow(win);
  win->SetListString();
  win->Show();

  // update main window if device list loaded successfully
  // update the device page list
  LoadPageList(win);
  SetStandardCursor();
  SetMessage("");
}

void SSMainWindow::SO_CLD_Events()
{
  // select the event line
  UIListPopup listPopup(this, "listPopup", "Set events on which line?");
  listPopup.SetListItems(	"AGS real time",
				"Booster real time",
				"Test real time");

  if( listPopup.Wait() == 2)	// Cancel
    return;
  long selection = listPopup.GetSelection();

  const char* ctlname;
  if (selection == 1)      ctlname = "CDC.RT.AGS.GEN";
  else if (selection == 2) ctlname = "CDC.RT.GENERATOR";
  else if (selection == 3) ctlname = "CDC.RT.GEN.TEST";
  else                     return;

  // create popup for getting device name if neccessary
  if(devicePopup == NULL)
    {
      devicePopup = new UIPopupWindow(this, "devicePopup");
      deviceList = new UIDeviceList(devicePopup->GetWorkArea(), "devicePopup");
    }

  // load the list of posible clds and wait for user input
  deviceList->LoadFromController(ctlname, TYPE_CLD);
  if( devicePopup->Wait() == 2)	// Cancel
    return;

  // get the selected device and load the timeline popup
  const char* devname = deviceList->GetSelectionString();
  if (devname == NULL)
    return;  // user cancelled

  // create a new SSCldWindow
  ShowCldEditor(devname, get_ppm_user() );
}

void SSMainWindow::ShowSingleDeviceList(const char* deviceListPath)
{
  // create a new device page window and load list
  SSPageWindow* pageWin = new SSPageWindow(this, "pageWindow");

  MachineTree* mtree = treeTable->GetMachineTree();
  const char* rootPath = mtree->GetRootPath();
  StdNode* rootNode = mtree->GetRootNode();
  const char* rootName = rootNode->Name();
  char* deviceList = new char[strlen(rootPath) + strlen(rootName) + strlen(deviceListPath) + 3];
  strcpy(deviceList, rootPath);
  strcat(deviceList, "/");
  strcat(deviceList, rootName);
  strcat(deviceList, "/");
  strcat(deviceList, deviceListPath);

  if( LoadDeviceList(pageWin, deviceList) < 0)
    {
      RingBell();
      printf("Exiting... Could not load device list - %s\n", deviceList);
      //a new window was created but not loaded successfully, better delete it
      delete pageWin;
      delete [] deviceList;
      pageWin=NULL;
      SetStandardCursor();
      clean_up(0);
    }
  else
    {
      if (supportKnobPanel)
	pageWin->SupportKnobPanel(knobPanel);
    }

  delete [] deviceList;
  SetStandardCursor();

  //a new window.  Add to the list and enable its events
  AddListWindow(pageWin);

  pageWin->SetListString();
  pageWin->SetSingleDeviceListMode();

  // make sure the window is visible
  pageWin->Show();
  pageWin->Refresh();

  // put table in continuous acquisition mode
  pageWin->UpdateContinuous();

  // update main window if device list loaded successfully
  // update the device page list
  LoadPageList(pageWin);
  activeLdWin = pageWin;
  SetStandardCursor();
  SetMessage("");
}

/////////////////// SSPageWindow Class ////////////////////////////////////
SSPageWindow::SSPageWindow(const UIObject* parent, const char* name,
			   AGS_PAGE_MODE mode, const char* title, UIBoolean create)
  : AgsPageWindow(parent, name, mode, title, UIFalse, UIFalse)
{
  Initialize();
  if(create)
    CreateWidgets(mode);
}

SSPageWindow::~SSPageWindow()
{
  if(listString != NULL)
    delete [] listString;
  if(devListPath != NULL)
    free(devListPath);
  parent->RemoveEventReceiver(this);
}

void SSPageWindow::Initialize()
{
  listString = NULL;
  devListPath = NULL;
  pageNode = NULL;

  // setup this window to receive events from the main window for icon events
  parent->AddEventReceiver(this);
}

void SSPageWindow::CreateWidgets(AGS_PAGE_MODE mode)
{
  // create a standard AgsPageWindow as a UIPrimaryWindow
  AgsPageWindow::CreateWidgets(mode, NULL, UIFalse);

  // set up events to trap program going into/out of an icon
  EnableEvent(UIMap);
  EnableEvent(UIUnmap);
}

int SSPageWindow::LoadFile(const char* filename, const char* pageTitle, short ppmUser)
{
  // load the file
  int retval = AgsPageWindow::LoadFile(filename, pageTitle, ppmUser);

  return retval;
}

void SSPageWindow::SetPPMUser(short user)
{
  // get user input and reset the page
  AgsPageWindow::SetPPMUser(user);

  // change the page list to reflect the new ppm user
  SetListString();
}

void SSPageWindow::SetListString(const char* string)
{
  if(listString == NULL)
    listString = new char[80];

  if(string != NULL)
    {
      strcpy(listString, string);
    }
  else	// create a default display string
    {
      const char* leaf = GetLeafName( GetDevListPath() );
      sprintf(listString, "%-22sSLD  U%d", leaf, GetPPMUser() );
    }
}

const char* SSPageWindow::GetListString() const
{
  if(listString != NULL)
    return listString;
  else
    return "";
}

const char* SSPageWindow::GetCurrentFileName() const
{
  return GetFilename();
}

void SSPageWindow::SetPageNode(const StdNode* node)
{
  pageNode = node;
}

const StdNode* SSPageWindow::GetPageNode() const
{
  return pageNode;
}

void SSPageWindow::SetDevListPath(const char* string)
{
  if(devListPath != NULL)
    free(devListPath);
  devListPath = NULL;

  if(string != NULL)
    devListPath = strdup(string);
}

const char* SSPageWindow::GetDevListPath() const
{
  if(devListPath != NULL)
    return devListPath;
  else
    return "";
}

int SSPageWindow::PrintVisible(long startCol, long endCol)
{
  // first store the old umask and set a new one so that file can be deleted
  mode_t oldMask = umask(0);	// can be deleted by anyone

  // open a print file
  FILE* fp = fopen(SS_PRINT_FILE, "w");
  if(fp == NULL)
    {
      umask(oldMask);
      return -1;
    }

  // print out a header indicating name, PPM user, date/time
  PrintSSHeader(fp);

  // print out the row/columns requested to the print file
  if(startCol == 0 && endCol == 0)
    page->PrintVisibleToFile(fp);
  else
    {
      long startRow = page->GetStartRow();
      long endRow = startRow + page->GetVisibleRows() - 1;
      if(startCol == 0)
	startCol = page->GetStartColumn();
      if(endCol == 0)
	endCol = page->GetStartColumn() + page->GetVisibleColumns() - 1;
      page->PrintToFile(fp, startRow, endRow, startCol, endCol);
    }
  fclose(fp);
  umask(oldMask);

  // now send the print file to the printer using lpr
  char command[128];
  strcpy(command, "lpr ");
  strcat(command, SS_PRINT_FILE);
  int retval=system(command);
  remove(SS_PRINT_FILE);
  return( retval );	// returns 0 on success
}

int SSPageWindow::Print(long startRow, long endRow, long startCol, long endCol)
{
  // first store the old umask and set a new one so that file can be deleted
  mode_t oldMask = umask(0);	// can be deleted by anyone

  // open a print file
  FILE* fp = fopen(SS_PRINT_FILE, "w");
  if(fp == NULL)
    {
      umask(oldMask);
      return -1;
    }

  // print out a header indicating name, PPM user, date/time
  PrintSSHeader(fp);

  // print out the row/columns requested to the print file
  page->PrintToFile(fp, startRow, endRow, startCol, endCol);
  fclose(fp);
  umask(oldMask);

  // now send the print file to the printer using lpr
  char command[128];
  strcpy(command, "lpr ");
  strcat(command, SS_PRINT_FILE);
  int retval=system(command);
  remove(SS_PRINT_FILE);
  return( retval );	// returns 0 on success
}

void SSPageWindow::PrintSSHeader(FILE* fp)
{
  char line[128];
  strcpy(line, "-------------------------------------------------------------------------------\n");
  fputs(line, fp);
  strcpy(line, " SpreadSheet     PPM User: ");
  short ppmUser = GetPPMUser();
  ppm_users_t	users[MAX_PPM_USERS];
  int numUsers = GetUsersPPM(users);
  if(numUsers >= ppmUser)
    strcat(line, users[ppmUser-1].name);
  strcat(line, "     ");
  const long intTime = time(0);
  strcat(line, ctime(&intTime) );
  fputs(line, fp);
  strcpy(line, "-------------------------------------------------------------------------------\n");
  fputs(line, fp);
  strcpy(line, " Device Page: ");
  strcat(line, GetDevListPath() );
  strcat(line, "\n");
  fputs(line, fp);
  // need to add some printable char at the end here so that the Laserjets in MCR print properly
  // when these printers are replaced, get rid of this character
  strcpy(line, "-------------------------------------------------------------------------------\n\n|");
  fputs(line, fp);
}

void SSPageWindow::HandleEvent(const UIObject* object, UIEvent event)
{
  if(object == window && event == UIUnmap)		// now in an icon state
    {
      // if knobbing is supported, be sure to clear the knob panel
      if (supportKnobPanel)
	ClearTheKnobPanel();
      StopContinuousUpdate();
    }
  else if(object == window && event == UIMap)		// now out of an icon state
    {
      if (supportKnobPanel)
	EnableTheKnobPanel();
      CheckUpdate();
    }

  // see if the Exit... of the pulldownMenu
  else if(object == pulldownMenu && event == UISelect)
    {
      const UITreeData* data = pulldownMenu->GetTreeData();
      if(!strcmp(data->namesSelected[0], "Page") && !strcmp(data->namesSelected[1], "Exit..."))
	SP_Close();
      else
	AgsPageWindow::HandleEvent(object, event);
    }

  // let base class handle it
  else
    AgsPageWindow::HandleEvent(object, event);
}

void SSPageWindow::SP_Duplicate()
{
  // ask user for PPM user for the duplicate page
  static const char* defaults[] =
  {
    "*duplicatePpmPopup*uiuseSetupToggle: false",
    NULL,
  };
  UISetDefaultResources(GetPrimaryWindow(), defaults);
  UIPPMPopup popup(this, "duplicatePpmPopup");
  if(IsDependentWindow() == UITrue)
    CenterWindow(&popup, 350, 400);
  short ppmUser = GetPPMUser();
  popup.SetPopupUser(ppmUser);
  popup.SetLabel("Select a PPM user for the new\ndevice page below.");
  if(popup.Wait() == 2)	// Cancel
    return;
  ppmUser = popup.GetPopupUserNum();

  // create a new device page window
  SetMessage("Creating duplicate device page...");
  SetWorkingCursor();
  SSMainWindow* mainWin = (SSMainWindow*) GetParent();
  SSPageWindow* newWin = new SSPageWindow(mainWin, "pageWindow");
  mainWin->AddListWindow(newWin);

  // load the same device list as current window into the new window
  int retval = mainWin->LoadDeviceList(newWin, GetFilePath(), ppmUser);
  newWin->Show();
  if(retval < 0)	// a problem
    {
      RingBell();
      SetMessage("Could Not Load Device List");
      SetStandardCursor();
      return;
    }
  else
    {
      if (supportKnobPanel)
	newWin->SupportKnobPanel(knobPanel);
    }

  // make sure the window is visible
  newWin->Refresh();

  // put table in continuous acquisition mode
  newWin->UpdateContinuous();

  // update main window if device list loaded successfully
  // update the device page list
  newWin->SetListString();
  newWin->SetPageNode( GetPageNode() );
  mainWin->LoadPageList(newWin);
  activeLdWin = newWin;
  SetStandardCursor();
  SetMessage("");
}

void SSPageWindow::SD_Show_CLD_Editor()
{
  // make sure this is a CLD
  CSObjectType type = GetDeviceType();
  if(type != TYPE_CLD && type != TYPE_CLD_EXPLICIT)
    {
      RingBell();
      SetMessage("Device ", GetDeviceName(), " is not a CLD.");
      return;
    }

  // create a new SSCldWindow
  SetMessage("Creating CLD editor window...");
  SetWorkingCursor();
  SSMainWindow* mainWin = (SSMainWindow*) GetParent();
  SSCldWindow* newWin = new SSCldWindow(mainWin, "sscldWindow", GetDeviceName(), GetPPMUser() );
  if (newWin->SuccessfullyCreated() == UIFalse || newWin->DataToCollect() == UIFalse)
    {
      SetStandardCursor();
      RingBell();
      if (newWin->SuccessfullyCreated() == UIFalse)
	{
	  int failure=newWin->ReasonNoCreate();
	  if (failure == DEVICE_BAD_NAME)
	    SetMessage("CLD ", GetDeviceName(), " does not exist. CLD window creation aborted.");
	  else if (failure == DC_GET_TIMEOUT)
	    SetMessage("CLD ", GetDeviceName(), " not reporting. CLD window creation aborted.");
	  else if (failure == DEVICE_BAD_REPORTFORMAT)
	    SetMessage("CLD ", GetDeviceName(), " - bad report format. CLD window creation aborted.");
	  else if (failure == DEVICE_NODATA)
	    SetMessage("CLD ", GetDeviceName(), " - no data in report. CLD window creation aborted.");
	  else if (failure == DEVICE_UNPACKERROR || failure == DEVICE_BAD_REPORTFORMAT || failure == DEVICE_NOBUFFERSPACE)
	    SetMessage("CLD ", GetDeviceName(), " - report error. CLD window creation aborted.");
	  else
	    SetMessage("CLD ", GetDeviceName(), " - unknown error. CLD window creation aborted.");
	}
      else
	SetMessage("Cld ", GetDeviceName(), " - no data to collect. CLD window creation aborted.");
      delete newWin;
      return;
    }

  if (singleDeviceListOnly)
    newWin->SetSingleDeviceListMode();

  // set the device list path for the cld window
  newWin->SetDeviceListForArchiveRetrieval(GetDevListPath());
  newWin->SetMachineTreeRootPath(GetMachineTreeRootPath());

  // show it on the screen
  mainWin->AddListWindow(newWin);
  newWin->SetListString();
  newWin->Show();

  // update main window
  mainWin->LoadPageList(newWin);
  SetStandardCursor();
  SetMessage("");
}

void SSPageWindow::SetSingleDeviceListMode()
{
  if (pulldownMenu!=NULL)
    {
      const MenuTree* menuTree = pulldownMenu->GetMenuTree();
      const StdNode* rootNode = menuTree->GetRootNode();
      if (rootNode!=NULL)
	{
	  const StdNode* firstChild=rootNode->FirstChild();
	  if (firstChild!=NULL)
	    {
	      const char* parentName=firstChild->Name();
	      if (parentName!=NULL)
		{
		  char* disable1 = new char[strlen(parentName)+strlen("Duplicate...")+3];
		  sprintf(disable1, "/%s/Duplicate...", parentName);
		  char* disable2 = new char[strlen(parentName)+strlen("Hide")+3];
		  sprintf(disable2, "/%s/Hide", parentName);
		  char* oldString1 = new char[strlen(parentName)+strlen("Close")+3];
		  sprintf(oldString1, "/%s/Close", parentName);

		  pulldownMenu->DeleteMenuItem(disable1);
		  pulldownMenu->DeleteMenuItem(disable2);
		  pulldownMenu->SetMenuItemString(oldString1, "Exit...");

		  delete [] disable1;
		  delete [] disable2;
		  delete [] oldString1;
		}
	    }
	}
    }
}

/////////////////// SSCldWindow Class ////////////////////////////////////
SSCldWindow::SSCldWindow(const UIObject* parent, const char* name,
		    const char* CldName, int PPMNumber, const char* title,
		    UIBoolean dialogWindow, UIBoolean create)
  : UICldObjectWindow(parent, name, CldName, PPMNumber, title, UITrue, UIFalse)
{
  Initialize();
  if(create)
    CreateWidgets(dialogWindow);
}

SSCldWindow::~SSCldWindow()
{
  if(listString != NULL)
    delete [] listString;
  parent->RemoveEventReceiver(this);
}

void SSCldWindow::Initialize()
{
  listString = NULL;

  // setup this window to receive events from the main window for icon events
  parent->AddEventReceiver(this);
}

void SSCldWindow::CreateWidgets(UIBoolean dialogWindow)
{
  // create a standard UICldObjectWindow as a UIDialogWindow
  UICldObjectWindow::CreateWidgets(dialogWindow);

  // set up events to trap program going into/out of an icon
  EnableEvent(UIMap);
  EnableEvent(UIUnmap);
}

void SSCldWindow::SetListString(const char* string)
{
  if(listString == NULL)
    listString = new char[80];

  if(string != NULL)
    {
      strcpy(listString, string);
    }
  else	// create a default display string
    {
      sprintf(listString, "%-22sCLD  U%d", GetCldName(), GetCldPPM() );
    }
}

const char* SSCldWindow::GetListString() const
{
  if(listString != NULL)
    return listString;
  else
    return "";
}

void SSCldWindow::HandleEvent(const UIObject* object, UIEvent event)
{
  if(object == window && event == UIUnmap)		// now in an icon state
    StopContinuousUpdate();
  else if(object == window && event == UIMap)		// now out of an icon state
    StartContinuousUpdate();
  else
    UICldObjectWindow::HandleEvent(object, event);
}

void SSCldWindow::SetSingleDeviceListMode()
{
  if (pulldownMenu!=NULL)
    {
      const MenuTree* menuTree = pulldownMenu->GetMenuTree();
      const StdNode* rootNode = menuTree->GetRootNode();
      if (rootNode!=NULL)
	{
	  const StdNode* firstChild=rootNode->FirstChild();
	  if (firstChild!=NULL)
	    {
	      const char* parentName=firstChild->Name();
	      if (parentName!=NULL)
		{
		  char* disable1 = new char[strlen(parentName)+strlen("Hide")+3];
		  sprintf(disable1, "/%s/Hide", parentName);

		  pulldownMenu->DeleteMenuItem(disable1);

		  delete [] disable1;
		}
	    }
	}
    }
}

/////// C++ Helper Routines ////////////////////
const char* GetLeafName(const char* deviceListPath)
{
  if(deviceListPath == NULL)
    return "";

  int len = strlen(deviceListPath);
  int i;
  for(i=len-1; i>=0; i--)
    if(deviceListPath[i] == '/')
      break;

  if(i != 0)
    return( &deviceListPath[i+1] );
  else
    return deviceListPath;
}



