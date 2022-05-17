#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <UIQt/UIApplication.hxx>			// for UIApplication class
#include <UIQt/UIArgumentList.hxx>		// for UIArgumentList class
#include <pet/PetWindow.hxx>
#include <petss/UICreateDeviceList.hxx>
#include <utils/utilities.h>			// for set_default_fault_handler()
#include <archive/archive_lib.h>		// for init_archive_lib_globals() and ARCHIVE_LOG;
#include <ddf/DeviceDirectory.hxx>		// for DeviceDirectory global object
//#include <UIGenerics/UIMenubarTools.hxx>	// for mb_init()
//#include <UIAgs/generic_popups_derived.h>	// for gp_set_ppm _program() _user()
//#include <UIGenerics/GenericPopups.hxx>
#include "pet.hxx"
#include <sys/stat.h>				// for umask printing permissions
#include <sys/types.h>
#include <unistd.h>
#include <UIUtils/UIPPM.hxx>
#include <IO/IOData.hxx>
#include <IO/IORequest.hxx>
#include <cdevCns/cdevCns.hxx>
#include <cns/cnsRequest.hxx>                   // to tutn on cache flushing
#include <setHist/SetStorage.hxx>               // to turn on storage of ADO sets
#include <MsgLog/MessageLogger.hxx>
#include <utils/AppContext.hxx>
#include <dbtools/SelectionHistory.hxx>
#include "MenuTree.cxx"

using namespace std;

#define SS_PRINT_FILE		"/tmp/SSPagePrintFile"

static UIApplication*	  application;
static UIArgumentList	  argList;
static SSMainWindow*	  mainWindow = NULL;
static PetWindow*       activeAdoWin = NULL;
static PetWindow*       singlePetWin = NULL;
static PetEventReceiver petEventReceiver;
static unsigned long    dumpElogAndExitTimerId = 0;
static const char* wname;

static void clean_up(int st)
{
  exit(st);
}

// prevent cdev errors from being printed to std out
void errorHandler(int severity, char* text, cdevRequestObject* obj)
{
}

int main(int argc, char *argv[])
{
  //set up command line arguments
  argList.AddString("-db_server");
  argList.AddString("-root");
  argList.AddString("-ddf");
  argList.AddString("-initial_list");   // like /FECs/xyz, loads tree, then shows pet window
  argList.AddString("-device_list");    // Ted - not sure what this does
  argList.AddSwitch("-single");         // single window mode
  argList.AddSwitch("-file");		// file to open initially
  argList.AddString("-path");		// default path for file open
  argList.AddSwitch("-printToElog", "Deprecated - use -dumpToElog or -dumpToDefaultElog instead");    // used with -single or -file to print pet page to elog after data acquisition, then exit
  argList.AddString("-elog", "", "", "Deprecated - use -dumpToElog or -dumpToDefaultElog instead");           // when used with -printToElog, the name of the elog, else the default
  argList.AddString("-printTogif");     // used with -single or -file to print pet page to a gif file after data acquisition, then exit
  argList.AddString("-ppm");            // start pet with the specified user
  argList.AddString("-displayName");    // makes the specified name visible when page is opened
  // the following are redundant but -elog and -printToElog have been kept for backwards compatibility
  argList.AddString("-dumpToElog", "", "", "dump window image to the specified elog - must use with -single or -file");
  argList.AddString("-dumpToDefaultElog", "dump window image to the current default elog - must use with -single or -file");
  argList.AddString("-elogEntryTitle", "", "", "a title to attach to this elog entry - use with -dumpToElog");
  argList.AddString("-elogAttachToTitle", "", "", "attach image to the entry with this title - use with -dumpToElog");

  // initialize the application
  application  = new UIApplication(argc, argv, &argList);
  // set a fault handler to get tracebacks on program crashes
  set_app_history( (char*) application->Name() );
  set_default_fault_handler( (char*) application->Name() );
  // clean-up handler
  signal(SIGQUIT,clean_up);
  signal(SIGTERM,clean_up);
  //signal(SIGSTOP,clean_up); // valgrind says this signal is uncacheable

  // create a message logger
  FileMsgLogger* myLogger = new FileMsgLogger("pet");
  myLogger->setSingleLineOutput( true );

  // set up the CNS as the source for names
  //cdevCnsInit();

  // refresh cns cache regularly
  CnsRequest::cacheTimeLimitSet();

  cdevSystem& defSystem = cdevSystem::defaultSystem();
  defSystem.autoErrorOff();
  defSystem.setErrorHandler(errorHandler);

  // turn on storage of ADO sets
  globalSetStorage()->storageOn();

  wname = "pet";
  if (argList.IsPresent("-ppm")) {
    IORequest* ioreq = new IORequest();

    char* system = "injSpec.super";
    int ppmValue =0;
    if(!strcmp(argList.String("-ppm"), "BOOSTER_USER_FOR_NSRL")) {
      int id = ioreq->addEntry(system, "boosterPpmUserForNsrlM");
      IOData d;
      ioreq->get(id, &d);
      ppmValue = atoi(d.stringVal());
    } else if(!strcmp(argList.String("-ppm"), "BOOSTER_USER_FOR_AGS")) {
      int id = ioreq->addEntry(system, "boosterPpmUserForAgsM");
      IOData d;
      ioreq->get(id, &d);
      ppmValue = atoi(d.stringVal());
    } else if(!strcmp(argList.String("-ppm"), "TANDEM_USER_FOR_NSRL")) {
      int id = ioreq->addEntry(system, "tandemPpmUserForNsrlM");
      IOData d;
      ioreq->get(id, &d);
      ppmValue = atoi(d.stringVal());
    } else if(!strcmp(argList.String("-ppm"), "TANDEM_USER_FOR_AGS")) {
      int id = ioreq->addEntry(system,"tandemPpmUserForAgsM");
      IOData d;
      ioreq->get(id, &d);
      ppmValue = atoi(d.stringVal());
    } else if(!strcmp(argList.String("-ppm"), "LINAC_USER_FOR_NSRL")) {
      int id = ioreq->addEntry(system,"linacPpmUserForNsrlM");
      IOData d;
      ioreq->get(id, &d);
      ppmValue = atoi(d.stringVal());
    } else if(!strcmp(argList.String("-ppm"), "LINAC_USER_FOR_AGS")) {
      int id = ioreq->addEntry(system,"linacPpmUserForAgsM");
      IOData d;
      ioreq->get(id, &d);
      ppmValue = atoi(d.stringVal());
    } else if(!strcmp(argList.String("-ppm"), "EBIS_USER_FOR_NSRL")) {
      int id = ioreq->addEntry(system,"ebisPpmUserForNsrlM");
      IOData d;
      ioreq->get(id, &d);
      ppmValue = atoi(d.stringVal());
    } else if(!strcmp(argList.String("-ppm"), "EBIS_USER_FOR_AGS")) {
      int id = ioreq->addEntry(system,"ebisPpmUserForAgsM");
      IOData d;
      ioreq->get(id, &d);
      ppmValue = atoi(d.stringVal());
    } else if(!strcmp(argList.String("-ppm"), "EBIS_USER_FOR_BOOSTER")) {
      int id = ioreq->addEntry(system,"ebisPpmUserForBoosterM");
      IOData d;
      ioreq->get(id, &d);
      ppmValue = atoi(d.stringVal());
    } else if(!strcmp(argList.String("-ppm"), "AGS_USER_FOR_RHIC")) {
      int id = ioreq->addEntry(system, "agsPpmUserForRhicS");
      IOData d;
      ioreq->get(id, &d);
      ppmValue = atoi(d.stringVal());
    } else {
      ppmValue = atoi(argList.String("-ppm"));
    }
    set_ppm_user(ppmValue);
    ioreq = NULL;
  }

  // create the mainWindow
  if (mainWindow == NULL) {
    mainWindow = new SSMainWindow(application, "mainWindow", wname);
    application->AddEventReceiver(mainWindow);
  }

  // check values of some command line arguments
  if( strlen( argList.String("-db_server") ) )
    set_data_db_name( (char*) argList.String("-db_server") );

  // check for single window switch
  bool singleWindowMode = false;
  PET_WINDOW_TYPE type = PET_UNKNOWN_WINDOW;
  if(argList.IsPresent("-file") || argList.IsPresent("-single")) {

    const char* fname = argList.UntaggedItem(0);
    if (fname == NULL) {
      char* arg = "-single";
      if (argList.IsPresent("-file"))
          arg = "-file";
      fprintf(stderr, "pet file name required with %s arg\n", arg);
      exit(0);
    }
    type = mainWindow->WindowType(fname);
    switch (type) {
    case PET_ADO_WINDOW:
    default:
      singlePetWin = new PetWindow(application, "petWindow");
      singlePetWin->SetLocalPetWindowCreating(true);
      singlePetWin->AddEventReceiver(&petEventReceiver);
      singlePetWin->GetPetPage()->AddEventReceiver(&petEventReceiver);
      singleWindowMode = true;
      break;
    }
  }
  else{
    // create the main window and its user interface and display it
    if (argList.IsPresent("-printToElog") && !argList.IsPresent("-device_list"))
      cout << "-printToElog option must be used with -single (ado page) option" << endl;
    else if (argList.IsPresent("-printTogif") && !argList.IsPresent("-device_list"))
      cout << "-printTogif option must be used with -single (ado page) option" << endl;
    else if ((argList.IsPresent("-dumpToElog") || argList.IsPresent("dumpToDefaultElog")) &&
             (!argList.IsPresent("-device_list") || !argList.IsPresent("-file") || !argList.IsPresent("-single")))
      cout << "-printToElog option must be used with -single, -file, or -device_list option" << endl;
  }

  const char* initList = argList.String("-initial_list");
  if(initList && strlen(initList))
    mainWindow->OpenTreePath(initList);

  const char *path = argList.String("-path");
  if(path && strlen(path)) {
    char *str = new char[strlen(path) + 10];
    sprintf(str, "%s/*.pet", path);
    if(singleWindowMode)
      singlePetWin->ChangePath(str);
    if(str)
      delete [] str;
  }

  // load files that are untagged on the command line
  int fileNum;
  for(fileNum=0; fileNum<argList.NumUntaggedItems(); fileNum++) {
    const char* file = argList.UntaggedItem(fileNum);
    if(file && strlen(file)) {
      char *tmpFile, *path, *ptr;

      type = mainWindow->WindowType(file);
      if (type == PET_ADO_WINDOW && (mainWindow->FindWindow(file, PET_ADO_WINDOW)) == NULL) {
        tmpFile = strdup(file);
        ptr = strrchr(tmpFile, '/');
        if(ptr) {
          *ptr = '\0';
          ptr++;
          path = new char[strlen(tmpFile) + 10];
          sprintf(path, "%s/*.ado", tmpFile);
        }
        else {		// no path given
          path = strdup("*.ado");
        }

        if (singlePetWin == NULL){
          singlePetWin = new PetWindow(application, "petWindow");
          if (mainWindow) {
            mainWindow->AddListWindow(singlePetWin);
            singlePetWin->GetPetPage()->AddEventReceiver(mainWindow);
          }
        }

        // file is *supposed* to be a file but if it is not attempt to take a guess at what
        // the file might be by looking in the directory that was passed for device_list.ado
        if (UIIsFile(file) == UIFalse) {
          if (UIIsDirectory(file) == UITrue) {
            // just append device_list.ado and try that
            std::string fname = file;
            fname += "/device_list.ado";
            if (UIIsFile(fname.c_str()) == UITrue)
              singlePetWin->LoadFile(fname.c_str());
            else {
              // throw an exception
              fprintf(stderr, "Argument passed as a pet file name (%s) is a directory\n", file);
              exit(1);
            }
          } else {
            // throw an exception
            fprintf(stderr, "pet file name provided (%s) is invalid\n", file);
            exit(1);
          }
        }
        else
          singlePetWin->LoadFile(file);
        singlePetWin->ChangePath(path);
        if (!argList.IsPresent("-file") && !argList.IsPresent("-single"))
          singlePetWin->SetLocalPetWindowCreating(false);
        if(path)
          free(path);
        if(tmpFile)
          free(tmpFile);

        // LTH - kludge to check for orphaned pet pages
#ifdef USE_PET_MERELY_TO_ENUMERATE_ADOS_IN_MACHINE_TREE

        if(mainWindow) { cerr << "Use -file or -single option when listing ADO pages" << endl; exit(-1); }

        PetPage *pp = singlePetWin->GetPetPage();
        int rowsUsed = pp->NumRows();
        int colsUsed = pp->NumColumns();

        const char *adoName;
        for(int i = 1; i <= rowsUsed; i++)
          for(int j = 1; j <= colsUsed; j++)

            if(adoName = pp->CellGetAdo(i,j))
              cout << adoName << endl;

        exit(0);
#endif

        if (argList.IsPresent("-displayName"))
          singlePetWin->ShowSubString(argList.String("-displayName"));
        singlePetWin->Show();
        break;
      }
    } // if file good
  } // for

//  if ( (argList.IsPresent("-printToElog") || argList.IsPresent("-printTogif")) && (singleWindowMode || singleDeviceListOnly)){
//    UIPrintTool* pt = GetPrintTool();
//    if (argList.IsPresent("-printToElog"))
//    {
//      const char* elogName = NULL;
//      if(argList.IsPresent("-elog") )
//        elogName = argList.String("-elog");
//      pt->SetPrintOutputType(UIPrintToElog);
//      if(elogName != NULL && elogName[0] != 0)
//        pt->SetElogName(elogName);
//    }
//    else if (argList.IsPresent("-printTogif"))
//    {
//      const char* gifFileName = argList.String("-printTogif");
//      pt->SetPrintOutputType(UIPrintToFile);
//      pt->SetPrintFileType(UIgifFile);
//      pt->SetPrintFileName(gifFileName);
//    }
//    // dump snap shot of the AGS page to the elog and exit
//    if (singleDeviceListOnly)
//      dumpElogAndExitTimerId = application->EnableTimerEvent(1000);
//    else
//      singlePetWin->ElogDumpAndExit();
//  } else if ((argList.IsPresent("-dumpToElog") || argList.IsPresent("-dumpToDefaultElog")) &&
//      (singleWindowMode || singleDeviceListOnly)) {
//    UIPrintTool* printTool = GetPrintTool();
//    const char* elogName = NULL;
//    if(argList.IsPresent("-dumpToElog") )
//      elogName = argList.String("-dumpToElog");
//
//    printTool->SetPrintOutputType(UIPrintToElog);
//    printTool->SetElogAutoEntry();
//    if(elogName && elogName[0])
//      printTool->SetElogName(elogName);
//    if(argList.IsPresent("-elogEntryTitle") )
//      printTool->SetElogEntryTitle(argList.String("-elogEntryTitle") );
//    else if(argList.IsPresent("-elogAttachToTitle") )
//      printTool->SetElogAttachToTitle(argList.String("-elogAttachToTitle") );
//    // dump snap shot of the page to the elog and exit
//    if (singleDeviceListOnly)
//      dumpElogAndExitTimerId = application->EnableTimerEvent(1000);
//    else
//      singlePetWin->ElogDumpAndExit();
//  }

  // loop forever handling user events
  application->HandleEvents();
}

///////////////////////// PetEventReceiver class ///////////////////////
PetEventReceiver::PetEventReceiver()
{
}

void PetEventReceiver::HandleEvent(const UIObject* object, UIEvent event)
{
  if(object == singlePetWin && event == UIWindowMenuClose)
    exit(0);
}

////////////// SSMainWindow Class Methods ////////////////////////
SSMainWindow::SSMainWindow(const UIObject* parent, const char* name, const char* title)
  : UIMainWindow(parent, name, title)
{
  // intialize some variables
  devicePopup = NULL;
  viewer = NULL;
  _searchPopup = _modifiedPopup = _checkedOutPopup = NULL;
  errFillExistWindowPopup = NULL;
  editDeviceList = NULL;
  _creatingPageInTree = false;
  _historyPopup = NULL;
  _recentPopup = NULL;
  _totalFlashTimerId = 0L;
  selectionHistory = new SelectionHistory("pet");

  // resources
//  static const char* defaults[] = {
//    "*foreground: navy",
//    "*background: gray82",
//    "*pageWindow*page*background: gray86",
//    "*pageWindow*page*uilabelBackground: gray82",
//    "*mainWindow*treeTable*foreground: black",
//    "*mainWindow*pageList*list*foreground: black",
//    "*mainWindow.allowShellResize: true",
//    "*mainWindow*treeTable*showHeaders: false",
//    "*mainWindow*treeTable*visibleRows: 30",
//    "*mainWindow*treeTable*uinameWidth: 30",
//    "*mainWindow*treeTable*table.width: 293",
//    "*mainWindow*treeTable*table*showHScroll: true",
//    "*mainWindow*pageList*prompt*alignment: alignment_center",
//    "*mainWindow*pageList*traversalOn: false",
//    "*pageWindow*defaultPosition: true",
//    "*pageWindow*page*tableHelp*background: white",
//    "*sscldWindow*defaultPosition: true",
//    "*deviceInfoPopup*defaultPosition: false",
//    "*deviceInfoPopup.allowShellResize: false",
//    "*metaEditor*defaultPosition: false",
//    "*loadArchivePopup*defaultPosition: false",
//    "*allUserPopup*defaultPosition: false",
//	  "*treeTable*multiClick: 0", // no need to delay when double clicking on a leaf node in the tree - it should behave the same as a single click
//    "*loadArchivePopup*arclist.uihelpText: This list contains all archives for the current device page.",
//    "*loadArchivePopup*archeader.uihelpText: Displays information pertaining to the selected archive.",
//    "*loadArchivePopup*arctoggles*Show timed archives in archive list.uihelpText: "
//        "When this is selected (it looks pushed in), timed archives are also\n"
//        "shown in the list, otherwise they are not.",
//    "*loadArchivePopup*arctoggles*Sort archive list alphabetically.uihelpText: "
//        "When this is selected (it looks pushed in), the archive list is sorted\n"
//        "alphabetically, otherwise it is sorted chronologically.",
//    "*loadArchivePopup*OK.uihelpText: Loads the selected archive and closes this window.",
//    "*loadArchivePopup*Apply.uihelpText: Loads the selected archive.  Does not close this window.",
//    "*loadArchivePopup*Close.uihelpText: Closes this window without loading an archive.",
//    "*loadArchivePopup*Help.uihelpText: "
//        "This window allows you to load one or more archives into a device page.\n"
//        "The list displays all archives for the device page shown.\n"
//        "\nTo load an archive, select the archive, then click the Apply button.\n"
//        "To load an archive and close this window, then click the OK button.\n"
//        "To close this window without loading and archive, then click the Close button.\n"
//        "\nTo get information on individual items in the window, move the cursor over\n"
//        "the item of interest, then press and hold the 3rd mouse button.",
//    NULL};
//  UISetDefaultResources(defaults);

  // set up event handling for window
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
  {
    printf("Could not load menu tree.  Aborting.\n");
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
  pageList = new PetScrollingEnumList(this, "pageList");
  pageList->SetMonoFont();
  pageList->SetTitle("ADO/CDEV Pages");
  pageList->AttachTo(NULL, this, messageArea, this);
  pageList->SetItemsVisible(1);
  pageList->AddEventReceiver(this);

  // add the table which displays the machine tree
  // put this in last so that it is the one that grows when the window is resized
  treeTable = new UIMachineTreeTable(this, "treeTable");
  //treeTable->GetTable()->ColumnAutoResize(2);
  treeTable->AttachTo(ppmLabel, this, pageList, this);
  treeTable->EnableEvent(UITableBtn2Down);
  treeTable->EnableEvent(UIAccept);
  treeTable->EnableEvent(UIDoubleClick);

  treeTable->AddEventReceiver(this);

  MachineTree* machTree = treeTable->GetMachineTree();
  // check to see if users wants to use a new root to the machine tree
  if( strlen( argList.String("-root") ) ) {
    machTree->SetRootDirPath( argList.String("-root") );
  }
  // Load the machine tree
  if( treeTable->Load()){
    fprintf(stderr, "Could not get machine tree.  Aborting\n");
    clean_up(0);
  }
  // set the default min sizes
  treeTable->SetVisibleRows(36);
  //treeTable->SetNameColumnWidth(45);
  SetMinWidth(300);

  // for compatibility, initialize menubar tools and generic popups
  //mb_init(this, messageArea);

  // if there is a passed argument with device_list, don't show the SSMainWindow
  if( strlen( argList.String("-device_list") ) == 0 &&
      !argList.IsPresent("-single") && !argList.IsPresent("-file"))
  {
    // display the main window on the top left of the screen
    // the goal here is to get spawned windows to display about at top-center
    int width = 1600, height;
    application->GetScreenSize(&width, &height);
    int xpos = width/2 - 750;
    int ypos = height/2 - 650;
    SetPosition(xpos, ypos);
    Show();
  }
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
  PetWindow* petWin;
  PET_WINDOW_TYPE type;
  for(long i=0; i<numWins; i++)
  {
    type = WindowType((UIWindow*) wins[i]);
    switch (type){
      case PET_ADO_WINDOW:
        petWin = (PetWindow*) wins[i];
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
  if(numWins > 0) {
    if (pageList->GetDesiredNumVisItems() < 5)
      pageList->SetItemsVisible(numWins);

    if(numWins >= pageList->GetDesiredNumVisItems()) {
      pageList->SetItemsVisible(pageList->GetDesiredNumVisItems());
      pageList->ShowSelection();
    }
    else {
      if (pageList->GetDesiredNumVisItems() != 5) // default
        pageList->SetItemsVisible(pageList->GetDesiredNumVisItems());
      else
        pageList->SetItemsVisible( (short) numWins);
    }
  }
  else {
    if (pageList->GetDesiredNumVisItems() == 5) // default
      pageList->SetItemsVisible(1);
  }
  delete [] items;
}

void SSMainWindow::HandleEvent(const UIObject* object, UIEvent event)
{
  // main window events
  if(object == this && event == UIWindowMenuClose)
    Exit();

  // user closing the viewer window used to display archive summary log
  else if(object == viewer && event == UIWindowMenuClose)
    viewer->Hide();

  // user wants to load a pet page via one of the search popups
  else if( (object == _searchPopup || object == _modifiedPopup || object == _checkedOutPopup) && event == UISelect)
  {
    UISearchDeviceList* popup = (UISearchDeviceList*) object;
    // get the selected node
    MachineTree* mtree = treeTable->GetMachineTree();
    StdNode* rootNode = mtree->GetRootNode();
    const char* rootPath = mtree->GenerateNodePathname(rootNode);
    const char* selectString = popup->GetDeviceListSelection();
    char* nodeName = new char[strlen(rootPath) + strlen(selectString) + 2];
    strcpy(nodeName, rootPath);
    strcat(nodeName, "/");
    strcat(nodeName, selectString);
    nodeName[strlen(nodeName)-1] = 0;   // get rid of newline at the end
    StdNode* selectNode = mtree->FindNode(nodeName);
    delete [] nodeName;
    if(selectNode == NULL) {
      UILabelPopup popup(this, "searchError", "Can't find selected node in the\npet Tree.");
      popup.Wait();
      return;
    }
    // if this is the first selection, create a new window
    // otherwise, reload the save search device page window
    // simulate a button 1 or button 2 down event
    popup->SetWorkingCursor();
    treeTable->SetNodeSelected(selectNode);
    treeTable->LoadTreeTable();
    HandleEvent(treeTable, UITableBtn2Down);


    if(object == _searchPopup) { // highlight the first device name with the search string
      // get the current pet window
      PetWindow* petWin = NULL;
      long selection = pageList->GetSelection();
      if(selection > 0)
        petWin = (PetWindow*) this->GetWindow(selection);

      const char* searchStr = _searchPopup->GetSearchString();
      if(searchStr != NULL && searchStr[0] != 0 && petWin != NULL) {
        petWin->ShowSubString(searchStr);
      }
    }
    popup->SetStandardCursor();
  }

  // a device page window was made active
  else if(event == UIWindowActive)
  {
    // check to see if sent from one of the device pages
    if( IsWindowInList( (UIWindow*) object) == 0)
      return;

    // redisplay the tree table and list
    LoadTable( (UIWindow*) object);
    if (!strcmp( object->ClassName(), "PetWindow"))
      activeAdoWin = (PetWindow*) object;
    LoadPageList( (UIWindow*) object);
  }

  // user chose the Close menu item from the window menu of a device page
  else if(event == UIWindowMenuClose)
  {
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
  // user wants to view a selected pet page from the history popup
  else if(object == _historyPopup && event == UISelect)
  {
    const char* path = _historyPopup->GetSelectedFile();
    unsigned long time = _historyPopup->GetSelectedTime();
    if(path == NULL || time == 0)
      return;
    long selection = _historyPopup->GetSelection();
    if(selection == 1)
      OpenFile(path);
  }
  // user made a leaf selection from the machine tree table
  else if(object == treeTable)
  {
    SetMessage("");
    PetWindow* adoWin = NULL;
    bool creatAdoWin = false;
    long numWins = GetNumWindows();
    int petPagePPM = 0; // in case we are loading an already created page and fail and reload it to its initial list
    if(event == UISelect || event == UIDoubleClick || event == UIAccept || event == UITableBtn2Down) {
      // tree table does not automatically select the node on a button 2down
      if(event == UITableBtn2Down) {
        const StdNode* node = treeTable->GetLastChanged();
        treeTable->SelectRow(node);
      }
      char s[512];
      DirTree* tree = (DirTree*)treeTable->GetTree();
      const StdNode* selectedNode = treeTable->GetNodeSelected();
      tree->GenerateFullNodePathname(selectedNode, s);
      strcat(s, "/");
      PET_WINDOW_TYPE type = WindowType(s);

      // independently look for medm screens (.adl extension) and launch these if found.
      bool foundmedm = checkForMedmPage(s);

      if (type == PET_UNKNOWN_WINDOW)
      {
        if (foundmedm == false) {
          SetMessage("No ado device list found.");
          return;
        }
      }
      SetWorkingCursor();
      // in case we have an error - loading a window - let's remember the window we are replacing!
      char* adoWindowPath = NULL;
      if(event == UISelect || event == UIDoubleClick || event == UIAccept) {	// load current window
        if(type == PET_ADO_WINDOW)
        {
          if(numWins > 0 && activeAdoWin && IsWindowInList( (UIWindow*) activeAdoWin)) {
            adoWin = activeAdoWin;
            const char* adoPath = adoWin->GetCurrentFileName();
            petPagePPM = adoWin->GetPPMUser();
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
        if(type == PET_ADO_WINDOW)
          adoWin = (PetWindow*) FindWindow(s, PET_ADO_WINDOW);
        if(adoWin) {
          UILabelPopup popup(this, "popup", "One or more windows are already showing this file.\nDo you really want to create a duplicate?", "No", "Yes");
          int retval = popup.Wait();
          if(retval == 1) { // No
            if(adoWin) {
              adoWin->Show();
              if(type == PET_ADO_WINDOW)
                type = PET_UNKNOWN_WINDOW;
            }
            if(type == PET_UNKNOWN_WINDOW) { // nothing more to create
              SetStandardCursor();
              delete [] adoWindowPath;
              return;
            }
          }
          else { // Yes - make sure we create new windows
            adoWin = NULL;
          }
        }
        if(type == PET_ADO_WINDOW)
        {
          creatAdoWin = true;
          adoWin = CreateAdoWindow();
        }
      }
      // now we have one (or two) windows - load the proper device list(s)
      const char* selectPath = treeTable->GetNodePath();
      if(selectPath == NULL) {
        SetMessage("Can't determine path to device list.");
        SetStandardCursor();

        // if created a window - delete it - remove it from the list!
        if (creatAdoWin) {
          if (adoWin)
            DeleteWindow(adoWin);
        }
        delete [] adoWindowPath;
        return;
      }
      SetMessage("Loading device page(s)...");
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
        bool anError = false;
        if (adoWin->CreateOK())
        {
          adoWin->SetPageNode( treeTable->GetNodeSelected() );
          adoWin->Show();
          LoadPageList(adoWin);
          activeAdoWin = adoWin;
        }
        else
        {
          if (creatAdoWin)
          {
            delete [] adoWindowPath;
            adoWindowPath = NULL;
            SetStandardCursor();
            SetMessage("Could Not Load Device List");
            DeleteWindow(adoWin);
            anError = true;
          }
          else
          {
            // we were loading a device page into an existing window
            // restore to its original path
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
                                 petPagePPM);
              else
                adoWin->LoadFile(adoWindowPath, NULL, petPagePPM);
              if (adoWin->CreateOK())
              {
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
                SetMessage("Could Not Load Device List");
                DisplayError("Problem restoring original device list.");
                anError = true;
              }
            }
            else
            {
              SetMessage("Could Not Load Device List");
              DisplayError("Problem restoring original device list.");
              anError = true;
            }
          }
          if (anError)
          {
            delete [] adoWindowPath;
            SetStandardCursor();
            return;
          }
        }
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
  // user is launching a new pet page from within another pet page
  else if(event == UIEvent9)
  {
    PetPage* page = (PetPage*) object;
    const char* path = page->LaunchPetPagePath();
    int retval = treeTable->SelectNodePath(path);
    if (retval != 0) {
      // handle error
      SetMessage("Unable to load page");
    } else {
      treeTable->LoadTreeTable();
      HandleEvent(treeTable, UITableBtn2Down);  // simulate a select event
    }
  }
  else if(event == UIEvent10) {
    PetWindow* win = (PetWindow*) GetWindow(pageList->GetSelection());
    MachineTree* mtree = treeTable->GetMachineTree();
    StdNode* rootNode = mtree->GetRootNode();
    const char* rootPath = mtree->GenerateNodePathname(rootNode); // acop
    if (win != NULL) {
      const char* selectString = win->GetTreeRootPath(); // "/operations/acop/AGS/Instrumentation/Ipm/Ipm"
      if (selectString) {
        const char* nodeName = strstr(selectString, rootPath);
        if (nodeName) {
          StdNode* selectNode = mtree->FindNode(nodeName);
          win->SetPageNode(selectNode);
          SP_Show();
        }
      }
    }
  }
  // user canceled creating permanent page in the tree
  else if(event == UIEvent6)
  {
    _creatingPageInTree = false;
  }
  // user may be creating a permanent page in the tree
  else if(event == UIEvent5)
  {
    if (_creatingPageInTree) {
      _creatingPageInTree = false;
      string file = editDeviceList->GetPath();
      file += editDeviceList->GetDeviceName();
      if (!strcmp(editDeviceList->GetDeviceType(), "ado")) {
        activeAdoWin->SaveFile(file.c_str());
      }
    }
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
    // store the selection in the selectionHistory DB table
    const char* selectStr = pulldownMenu->GetSelectionPath2();
    selectionHistory->insert(selectStr, "menu");

    const UITreeData* data = pulldownMenu->GetTreeData();
    SetMessage("");

    if(!strcmp(data->namesSelected[0], "File")) {
      if(!strcmp(data->namesSelected[1], "New...")) {
        SS_New();
      }
      else if(!strcmp(data->namesSelected[1], "Open...")) {
        SS_Open();
      }
      if(!strcmp(data->namesSelected[1], "Open Favorite...")) {
        SS_OpenFavorite();
      }
      else if(!strcmp(data->namesSelected[1], "Default PPM User...")) {
        SS_Default_PPM_User();
      }
      else if(!strcmp(data->namesSelected[1], "Create Temp ADO Page...")) {
        SS_Create_RHIC_Page();
      }
      else if(!strcmp(data->namesSelected[1], "Create Temp PS ADO Page...")) {
        SS_Create_PS_RHIC_Page();
      }
      else if(!strcmp(data->namesSelected[1], "Search pet Tree")) {
        if(!strcmp(data->namesSelected[2], "Find Text in Files...")) {
          SS_FindTextInFiles();
        }
        else if(!strcmp(data->namesSelected[2], "Find Recently Modified Files...")) {
          SS_FindRecentlyModifiedFiles();
        }
        else if(!strcmp(data->namesSelected[2], "Find Checked Out Files...")) {
          SS_FindCheckedOutFiles();
        }
      }
      else if(!strcmp(data->namesSelected[1], "Quit")) {
        SS_Quit();
      }
    }
    else if(!strcmp(data->namesSelected[0], "Page")) {
      if(!strcmp(data->namesSelected[1], "Find")) {
        SP_Find();
      }
      else if(!strcmp(data->namesSelected[1], "New")) {
        SP_New();
      }
      else if(!strcmp(data->namesSelected[1], "Show")) {
        SP_Show();
      }
      else if(!strcmp(data->namesSelected[1], "Hide")) {
        SP_Hide();
      }
      else if(!strcmp(data->namesSelected[1], "Close")) {
        SP_Close();
      }
      else if(!strcmp(data->namesSelected[1], "Close All...")) {
        SP_Close_All();
      }
    }
    else if(!strcmp(data->namesSelected[0], "Options")) {
      if(!strcmp(data->namesSelected[1], "Pet Page History")) {
        SO_Pet_Page_History();
      }
      else if(!strcmp(data->namesSelected[1], "Read Archive Log")) {
        SO_Read_Archive_Log();
      }
      else if(!strcmp(data->namesSelected[1], "PPM User Monitor")) {
        SO_PpmUserMonitor();
      }
      else if(!strcmp(data->namesSelected[1], "Flash Pages")) {
        if(_totalFlashTimerId > 0L) {
          application->DisableTimerEvent(_totalFlashTimerId);
        }
        // start the timer to flash the windows for four seconds
        _totalFlashTimerId = application->EnableTimerEvent(4000);
        SO_Flash_Pages();
      }
    }
  }
  else if (event == UIMessage)
  {
    // we only want to display messages from objects that are not device_list windows!
    // they have their own message areas!
    // check to see if sent from one of the device pages
    const char* className = object->ClassName();
    if(strcmp( className, "PetWindow") && strcmp( className, "PetPage"))
      SetMessage(object->GetMessage());
  }
  else if (object == application && event == UITimer)
  {
    if (application->GetTimerId() == _totalFlashTimerId) {
      // stop the flashing of the pages
      SO_Flash_Pages(false);
      _totalFlashTimerId = 0L;
    }
  }
  // otherwise, pass event to base class
  else
    UIMainWindow::HandleEvent(object, event);
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

void SSMainWindow::SetWindowPos(UIWindow* newWin, UIWindow* currWin)
{
  if(newWin == NULL || mainWindow == NULL)
    return;

  // get the position and width of the main newWin
  short mainX, mainY, mainWidth;
  GetPosition(mainX, mainY);
  mainWidth = GetWidth();

  // set xpos of new newWin according to main newWin coordinates
  short xpos = mainX + mainWidth + 20;

  // set ypos
  short ypos = mainY + 30; // need to subtract out the height of the window title bar
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
  if (!strstr(fileName, "device_list")) {
    if(wtype == PET_ADO_WINDOW) {
      strcat(fileName, ADO_DEVICE_LIST);
    }
  } else {
    if (wtype == PET_ADO_WINDOW && !strstr(fileName, ".ado"))
      strcat(fileName, ".ado");
  }

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
      default:
        break;
    }
    if(type == wtype && winName && !strcmp(fileName, winName))
      return win;
  }
  return NULL;
}

void SSMainWindow::ExitAllWindows()
{
  UIWindow* win;
  int numWindows = GetNumWindows();
  PET_WINDOW_TYPE type;
  for(int i=0; i<numWindows; i++) {
    win = GetWindow(i+1);
    type = WindowType(win);
    switch (type)
    {
      case PET_ADO_WINDOW:
        ((PetWindow*)win)->TF_Exit();
        break;
      default:
        break;
    }
  }
}

PET_WINDOW_TYPE SSMainWindow::WindowType(UIWindow* window)
{
  if (window == NULL || window->ClassName() == NULL)
    return PET_UNKNOWN_WINDOW;

  if (!strcmp(window->ClassName(), "PetWindow"))
    return PET_ADO_WINDOW;
  else
    return PET_UNKNOWN_WINDOW;
}

PET_WINDOW_TYPE SSMainWindow::WindowType(const char* path)
{
  // check the simple case where the path already defines the expected type
  if (strstr(path, ".ado") || strstr(path, ".pet"))
    return PET_ADO_WINDOW;

  // determine whether a ADO_DEVICE_LIST can be found at the base of path
  char petfile[512];
  strcpy(petfile, path);
  if (!strstr(path, "device_list")) {
    strcat(petfile, "/");
    strcat(petfile, ADO_DEVICE_LIST);
  } else
    strcat(petfile, ".ado");

  UIBoolean adopage = UIFileExists(petfile);
  if (adopage)
    return PET_ADO_WINDOW;
  else {
    // The file name doesn't provide us with enough information.  Checking the contents
    // would be the best solution but let's just assume an ado window for now.
    return PET_ADO_WINDOW;
  }
  return PET_UNKNOWN_WINDOW;
}

bool SSMainWindow::checkForMedmPage(const char* path)
{
  bool exists = false;
  string cmd = "medm_cdev -x ";
  if (strstr(path, ".adl")) {
    exists = true;
    cmd += path;
    cmd += " &";
  } else {
    char fname[512];
    strcpy(fname, path);
    if (!strstr(path, "device_list")) {
      strcat(fname, "/");
      strcat(fname, "device_list.adl");
    } else
      strcat(fname, ".adl");


    exists = UIFileExists(fname);
    if (exists) {
      cmd += fname;
      cmd += " &";
      exists = true;
    }
  }
  if (exists)
    system(cmd.c_str());

  return exists;
}

void SSMainWindow::DeleteListWindow(UIWindow* window)
{
  RemoveWindow(window);

  // reload the list with the 1st item selected
  LoadPageList(NULL);
}

void SSMainWindow::DeleteAllWindows()
{
  UIMainWindow::DeleteAllWindows();
  activeAdoWin = NULL;

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

  if(window == activeAdoWin)
    activeAdoWin = NULL;

  // delete it (delayed) and remove it from the window list
  DeleteWindow(window);
}

void SSMainWindow::HideListWindow(UIWindow* window)
{
  window->Hide();
}

void SSMainWindow::ShowListWindow(UIWindow* window)
{
  window->Show();
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
  //treeTable->UpdateKeyboardSelector();
}

void SSMainWindow::SS_New()
{
  if (!treeTable->IsLeafNode(treeTable->GetNodeSelected())) {
    UILabelPopup popup(this, "popup", NULL, "OK");
    popup.SetLabel("Please Select the Leaf Node\nwhere the file is to be created/edited");
    popup.Wait();
    return;
  }
  if (editDeviceList == NULL)
    editDeviceList = new UICreateDeviceList(this, "editDeviceList");

  const char* path = GetSelectedPath();
  editDeviceList->SetDirectory(path);

  // temporarily remember the active windows while the user is preparing to edit
  // so that we don't lose track of the page to be edited in case the user makes
  // a different window active before finishing with the edit popup.
  // Note: This is dangerous because the user could exit the active window and
  // we'd be left with a pointer to deleted memory...
  PetWindow* adoWinToEdit = activeAdoWin;

  int retval = editDeviceList->Wait();
  if (retval == 3) // cancel
    return;
  else if (retval == 2) { // Graphical Interface
    // when done need to save the file (this will come back as a UIEvent5 event)
    // The file must be a new file since this option is not available to the user for exising files
    _creatingPageInTree = true;
    const char* type = editDeviceList->GetDeviceType();
    if (!strcmp(type, "ado")) {
      SS_Create_RHIC_Page();
      // add the window to the window list with it's permanent designation.
      // Do this by reloading the temp file just created and saved.
      string path = editDeviceList->GetPath();
      string file = path;
      file += editDeviceList->GetDeviceName();

      MachineTree* machTree = treeTable->GetMachineTree();
      const char* rootPath = machTree->GetRootPath();
      const StdNode* rootNode = machTree->GetRootNode();
      const char* rootName = rootNode->Name();
      int len = strlen(rootPath) + strlen(rootName) + 1;
      const char* name = file.c_str();
      char* p = (char*) path.c_str();
      // remove last '/' in path
      if (p[strlen(p)-1] == '/') p[strlen(p)-1] = '\0';
      activeAdoWin->LoadFile(name, &p[len]);
      activeAdoWin->SetListString(UIGetLeafName(p));
      // this forces the pageList to refresh itself with the new tree name rather than the temp name
      LoadPageList(activeAdoWin);
      activeAdoWin->SaveVersion(file.c_str());
    }
  } else { // default editor
    // create a new pet window and use it's edit method to modify the new file
    const char* type = editDeviceList->GetDeviceType();
    if (!strcmp(type, "ado")) {
      if (editDeviceList->IsNewType() || adoWinToEdit == NULL) {
        // creating new file
        PetWindow* petWin = new PetWindow(this, "petWindow");
        activeAdoWin = petWin;
        petWin->SetLocalPetWindowCreating(false);
        AddListWindow(petWin);
        petWin->GetPetPage()->AddEventReceiver(this);
        petWin->RemoveAllGpms(); // there are none but this will take care of the attachments
        // edit the file here
        petWin->EditBuiltInEditor(editDeviceList->GetPath());
        string name = editDeviceList->GetPath();
        petWin->SetTreeRootPath(name.c_str());
        LoadPageList(petWin);
      } else {
        // editing existing file
        adoWinToEdit->EditBuiltInEditor();
      }
    }
  }
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

void SSMainWindow::OpenFile(const char* filePath)
{
  // filePath should look like:
  // FECs/Development/Controls/simple_test/device_list

  char treePath[1024];
  strcpy(treePath, "/acop/");
  strcat(treePath, filePath);
  char* cptr = strstr(treePath, "/device_list");
  if(cptr)
    *cptr = 0;
  else
    return;  // not a pet page that can be opened
  treeTable->SelectNodePath(treePath);
  treeTable->LoadTreeTable();
  treeTable->ShowSelectedNode();
  HandleEvent(treeTable, UISelect);  // simulate a select event
}

void SSMainWindow::OpenTreePath(const char* treePath)
{
  // treePath could be:
  // 1) /acop/FECs/Development/Controls/simple_test
  // 2) /FECs/Development/Controls/simple_test
  // 3) FECs/Development/Controls/simple_test

  if(treePath == NULL) return;
  int len = strlen(treePath);
  if(len < 5) return;
  if( !strncmp(treePath, "/acop", 5)) {
    treeTable->SelectNodePath(treePath);
    return;
  }
  char* fullPath = new char[len + 8];
  strcpy(fullPath, "/acop");
  if(treePath[0] != '/')
    strcat(fullPath, "/");
  strcat(fullPath, treePath);

  treeTable->SelectNodePath(fullPath);
  treeTable->LoadTreeTable();
  treeTable->ShowSelectedNode();
  HandleEvent(treeTable, UISelect);  // simulate a select event
  delete [] fullPath;
}

void SSMainWindow::SS_OpenFavorite()
{
  if(_recentPopup == NULL) {
    _recentPopup = new UIRecentHistoryPopup(this, "recentPopup");
    const char* userName = GlobalAppContext()->getUnlockName();
    if(userName == NULL || userName[0] == 0)
      userName = GlobalAppContext()->getLoginName();
    _recentPopup->SetUserName(userName);
    _recentPopup->SetLabel("The list below shows the favorite pet pages displayed by user ID = ", userName, ".\n",
                           "To load one, select it and click the Show button.  Or double-click the item.");
    _recentPopup->SetTitle("Pet Page Favorites");
  }
  int retval = _recentPopup->LoadTable();
  if(retval < 0) {       // a problem
    RingBell();
    UILabelPopup popup(this, "errorPopup", "Could not load information from the database.\nAborting.", "OK");
    popup.Wait();
    return;
  }
  else if(retval == 0) { // nothing to load
    RingBell();
    UILabelPopup popup(this, "errorPopup", "You have no recent file usage for the current run.\nAborting.", "OK");
    popup.Wait();
    return;
  }
  retval = _recentPopup->Wait();
  if(retval == 2)  // Close
    return;

  // attempt to open the file and display its contents
  const char* filePath = _recentPopup->GetSelectedFile();
  OpenFile(filePath);
}

void SSMainWindow::AdjustName(char* devicePath)
{
  if (devicePath != NULL) {
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
  PetWindow* petWin = new PetWindow(this, "petWindow");
  petWin->SetLocalPetWindowCreating(false);
  AddListWindow(petWin);
  petWin->GetPetPage()->AddEventReceiver(this);
  activeAdoWin = petWin;
  petWin->TF_Create_Pet_Page();
  petWin->RemoveAllGpms(); // there are none but this will take care of the attachments
  petWin->Show();
  LoadPageList(petWin);
}

void SSMainWindow::SS_Create_PS_RHIC_Page()
{
  SetWorkingCursor();
  PetWindow* petWin = new PetWindow(this, "petWindow");
  petWin->GetPetPage()->AddEventReceiver(this);
  petWin->SetLocalPetWindowCreating(false);
  AddListWindow(petWin);
  petWin->TF_Create_PS_Pet_Page();
  petWin->RemoveAllGpms(); // there are none but this will take care of the attachments
  petWin->Show();
  LoadPageList(petWin);
  activeAdoWin = petWin;
  SetStandardCursor();
}

// only confirm the quit if the user is MCR and if Multiple Instances are running
// and at least one page is displayed
int SSMainWindow::ConfirmQuit()
{
  // check number of pet windows
  if (GetNumWindows() == 0)
    return 2; // quit with dialog

  // check user name
   char* loginName = getenv("LOGNAME");
   if (strcmp(loginName, "mcr"))
     return 2; // quit with dialog

  // check the number of pet processes running
   int nprocesses = 0;
   char line[1024];
   FILE* fp = popen("ps -ef | grep pet | grep -v grep", "r");
   if (fp != NULL) {
     while (fgets(line, 1024, fp) != NULL)
       nprocesses++;
     pclose(fp);
   }

   if (nprocesses == 1)
     return 2; // quit with dialog

  SO_Flash_Pages();

  // need to get back to the main event loop otherwise there is
  // a possibility that this window will not expose properly
  // in the current workspace
  UILabelPopup popup(this, "confirmPopup", NULL, "Yes", "No");
  popup.SetLabel("Exit ", application->Name(), "?\n\nAll flashing pet pages will be closed.");
  if (popup.Wait() == 1)
    // yes
    return 1; // quit without dialog

  SO_Flash_Pages(false);

  if (_totalFlashTimerId > 0L && application != NULL)
  {
    application->DisableTimerEvent(_totalFlashTimerId);
    _totalFlashTimerId = 0L;
  }

  return 0; // cancel quit
}

void SSMainWindow::SS_FindTextInFiles()
{
  // put up for getting user input and making device list selections
  if(_searchPopup == NULL)
  {
    _searchPopup = new UISearchDeviceList(this, "searchPopup", NULL, NULL, SPStringSearch);
    _searchPopup->AddEventReceiver(this);  // trap the View (UISelect) event from the popup
  }
  // display the popup
  _searchPopup->Show();
}

void SSMainWindow::SS_FindRecentlyModifiedFiles()
{
  // put up for getting user input and making device list selections
  if(_modifiedPopup == NULL)
  {
    _modifiedPopup = new UISearchDeviceList(this, "modifiedPopup", NULL, NULL, SPFilesModified);
    _modifiedPopup->AddEventReceiver(this);  // trap the View (UISelect) event from the popup
  }
  // display the popup
  _modifiedPopup->Show();
}

void SSMainWindow::SS_FindCheckedOutFiles()
{
  // put up for getting user input and making device list selections
  if(_checkedOutPopup == NULL)
  {
    _checkedOutPopup = new UISearchDeviceList(this, "checkedOutPopup", NULL, NULL, SPFilesCheckedOut);
    _checkedOutPopup->AddEventReceiver(this);  // trap the View (UISelect) event from the popup
  }
  // display the popup
  _checkedOutPopup->Show();
}

void SSMainWindow::SS_Quit()
{
  ExitAllWindows();
  clean_up(0);
}

void SSMainWindow::Exit()
{
//  int retval = ConfirmQuit();
  int retval = 1;
  if (retval == 0)
    return;
  else if (retval == 1) {
    ExitAllWindows();
    clean_up(0);
  }
  else if (retval == 2) {
    // first put up an exit confirmation
    UILabelPopup popup(this, "exitPopup", NULL, "Yes", "No");
    popup.SetLabel("Exit ", application->Name(), "? Are you sure?");
    if(popup.Wait() == 1)	// Yes selected
    {
      ExitAllWindows();
      clean_up(0);
    }
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

void SSMainWindow::SO_Pet_Page_History()
{
  if(_historyPopup == NULL) {
    _historyPopup = new UIHistoryPopup(this, "historyPopup", NULL, "Show", "Close");
    _historyPopup->SetTitle("Pet Page History");
    _historyPopup->SetLabel("Pet pages loaded from any program between the displayed start and stop dates are shown.\n",
                            "You can redisplay the table for different dates using the Reload Table button.");
    _historyPopup->AddEventReceiver(this);
    _historyPopup->SetAppFilter("pet");
  }
  int retval = _historyPopup->LoadTable();
  _historyPopup->Show();
  if(retval <= 0) {
    UILabelPopup errPopup(this, "errPopup", NULL, "OK", NULL);
    if(retval < 0)  // a DB error
      errPopup.SetLabel("Could not get pet page history data from the database.");
    else            // no data found for start/stop time
      errPopup.SetLabel("No pet pages were found for the given\n start and stop times.");
    errPopup.Wait();
  }
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

void SSMainWindow::SO_Flash_Pages(bool flash)
{
  PetWindow* adoWin = NULL;
  UIWindow* win = NULL;

  int ws = GetWorkspace();

  int numWins = GetNumWindows();
  for (int i=1; i<=numWins; i++){
    win = GetWindow(i);
    adoWin = NULL;
    if (win != NULL)
    {
      switch (WindowType(win))
      {
        case PET_ADO_WINDOW:
          adoWin = (PetWindow*) win;
          break;
        default:
          break;
      }
    }
    if (adoWin) {
      adoWin->SetWorkspace(ws); // this should not be necessary but it doesn't work properly otherwise
      adoWin->Show();
      adoWin->Flash(flash);
    }
  }
}

/////////////////// ScrollingPageList Class //////////////////////////////
PetScrollingEnumList::PetScrollingEnumList(const UIObject* parent, const char* name,
                                           const char* title, const char* items[]) :
		UIScrollingEnumList(parent, name, title, items)
{
	Initialize();
	CreateWidgets();
}

void PetScrollingEnumList::Initialize()
{
	_desiredNumVisItems = 5;
}

void PetScrollingEnumList::CreateWidgets()
{
	SetMenuItems();
}

void PetScrollingEnumList::SetMenuItems()
{
	const char* items[] = {
			"Grow",
			"Shrink",
			"----",
			"Copy All",
			"Copy Selected",
			"----",
			"Print All",
			"Print Selected",
			NULL
	};
	SetHelpItems(items);
	AddEventReceiver(this);
	EnableEvent(UIMessage);
}

void PetScrollingEnumList::HandleEvent(const UIObject* object, UIEvent event)
{
  if (object == this && event == UIHelp) {
    const char* selection = GetHelpSelectionString();
    if (!strcmp(selection, "Grow")) {
      _desiredNumVisItems += 5;
      SetItemsVisible(_desiredNumVisItems);
    } else if (!strcmp(selection, "Shrink")) {
      if (_desiredNumVisItems >= 10)
        _desiredNumVisItems -= 5;
      else {
        long num = GetNumDisplayedItems();
        if (num < _desiredNumVisItems)
          _desiredNumVisItems = num;
        if (_desiredNumVisItems == 0)
          _desiredNumVisItems = 1;
      }
      SetItemsVisible(_desiredNumVisItems);
    } else
      UIScrollingEnumList::HandleEvent(object, event);
  } else
    UIScrollingEnumList::HandleEvent(object, event);
}

void PetScrollingEnumList::SetItemsVisible(long num)
{
	_desiredNumVisItems = num;
	UIScrollingEnumList::SetItemsVisible(_desiredNumVisItems);
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
