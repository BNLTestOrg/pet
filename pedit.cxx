#include <stdio.h>
#include <stdlib.h>
#include <UIQt/UIApplication.hxx>
#include <UIQt/UIArgumentList.hxx>
#include <UIQt/UIPopups.hxx>
#include <UIQt/UIEventHandling.hxx>
#include <pet/PeditWindow.hxx>
#include <utils/utilities.h>			// for set_default_fault_handler()

static PeditWindow* peditWin = NULL;

class MyEventReceiver : public UIEventReceiver
{
  void HandleEvent(const UIObject* object, UIEvent event) {
    if(object == peditWin && event == UIWindowMenuClose) { // user clicked the window close box
      UILabelPopup exitPopup(peditWin, "exitPopup", NULL, "Yes", "No");
      exitPopup.SetLabel("Exit pedit?  Are you sure?");
      if(exitPopup.Wait() == 2)
        return;
      exit(0);
    }
    else if(object == peditWin && event == UIEvent1) { // user selected File->Quit
      exit(0);
    }
  }
};

////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
  //set up command line arguments
  UIArgumentList argList;
  argList.AddString("-file");		// file to open initially

  // initialize the application
  UIApplication* application  = new UIApplication(argc, argv, &argList);

  // set a fault handler to get tracebacks on program crashes
  set_default_fault_handler( (char*) application->Name() );

  // something to trap when the window has closed
  MyEventReceiver* myEventReceiver = new MyEventReceiver();

  // create the PeditWindow
  peditWin = new PeditWindow(application, "peditWin");
  peditWin->AddEventReceiver(myEventReceiver);

  // load a file?
  const char* filePath = NULL;
  if(argList.IsPresent("-file")) {
    filePath = argList.String("-file");
    peditWin->LoadFile(filePath);
  }
  // display the window
  peditWin->Show();

  // loop forever handling user events
  application->HandleEvents();
}

