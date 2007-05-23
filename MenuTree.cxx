#include <basics/TreeTools.hxx>

//Reminder: User must delete the tree when it is no longer needed.

MenuTree* CreateMenuTree()
{
	MenuTree* menuTree = new MenuTree("SSmain");
	StdNode* snode=NULL;

	snode = menuTree->InsertMenuItem("File", NULL, NULL);
	menuTree->SetNodeHelpText(snode, "This menu provides options for setting up\nthe program as a whole.");
	menuTree->SetNodeMnemonic(snode, 'F');

	snode = menuTree->InsertMenuItem("New...", "/File", NULL);
	menuTree->SetNodeHelpText(snode, "Make a new window and select a file");
	menuTree->SetNodeMnemonic(snode, 'N');
	menuTree->SetNodeAccelerator(snode, "Ctrl+N");

	snode = menuTree->InsertMenuItem("Open...", "/File", NULL);
	menuTree->SetNodeHelpText(snode, "Open a file in the current window");
	menuTree->SetNodeMnemonic(snode, 'O');
	menuTree->SetNodeAccelerator(snode, "Ctrl+O");

	snode = menuTree->InsertMenuItem("----", "/File", NULL);
	menuTree->SetNodeType(snode, MenuSeparatorType);

	snode = menuTree->InsertMenuItem("Default PPM User...", "/File", NULL);
	menuTree->SetNodeHelpText(snode, "Sets the default PPM user for this program.\nThe default PPM user is used as the PPM user\nfor any new device lists that are displayed.\nIt does not affect existing device pages.");
	menuTree->SetNodeMnemonic(snode, 'P');

	snode = menuTree->InsertMenuItem("----", "/File", NULL);
	menuTree->SetNodeType(snode, MenuSeparatorType);

	snode = menuTree->InsertMenuItem("Create Temp RHIC Page...", "/File", NULL);
	menuTree->SetNodeHelpText(snode, "Create a page from selected parameters");

	snode = menuTree->InsertMenuItem("Create Temp PS RHIC Page...", "/File", NULL);
	menuTree->SetNodeHelpText(snode, "Create a RHIC page from a list of selected Power Supplies");

	snode = menuTree->InsertMenuItem("Create Temp AGS Page...", "/File", NULL);
	menuTree->SetNodeHelpText(snode, "Create an AGS page from selected devices");

	snode = menuTree->InsertMenuItem("----", "/File", NULL);
	menuTree->SetNodeType(snode, MenuSeparatorType);

	snode = menuTree->InsertMenuItem("Quit", "/File", NULL);
	menuTree->SetNodeHelpText(snode, "Select 'Quit' to exit the program immediately.\n\t");
	menuTree->SetNodeMnemonic(snode, 'Q');
	menuTree->SetNodeAccelerator(snode, "Ctrl+Q");

	snode = menuTree->InsertMenuItem("Page", NULL, NULL);
	menuTree->SetNodeHelpText(snode, "This menu displays options which affect one\nor more device pages.  In some cases, the menu\nitem refers to the current device page selected in\nthe Device Pages list.");
	menuTree->SetNodeMnemonic(snode, 'P');

	snode = menuTree->InsertMenuItem("Find", "/Page", NULL);
	menuTree->SetNodeHelpText(snode, "Clears the machine tree keyboard selection area,\nreadying the program for a new selection either\nusing the mouse or the keyboard.");
	menuTree->SetNodeMnemonic(snode, 'F');
	menuTree->SetNodeAccelerator(snode, "Ctrl+F");

	snode = menuTree->InsertMenuItem("New", "/Page", NULL);
	menuTree->SetNodeHelpText(snode, "If a leaf node is selected on the machine tree\ntable, creates a new device page for that node.\nThis does the same thing as clicking on the\nnode with the 2nd mouse button.");
	menuTree->SetNodeMnemonic(snode, 'N');
	menuTree->SetNodeAccelerator(snode, "F1");

	snode = menuTree->InsertMenuItem("----", "/Page", NULL);
	menuTree->SetNodeType(snode, MenuSeparatorType);

	snode = menuTree->InsertMenuItem("Show", "/Page", NULL);
	menuTree->SetNodeHelpText(snode, "Brings the currently selected page to the front\nand makes it visible if not already.");
	menuTree->SetNodeMnemonic(snode, 'S');

	snode = menuTree->InsertMenuItem("Hide", "/Page", NULL);
	menuTree->SetNodeHelpText(snode, "Hide (make invisible) the currently selected\npage in the list.  This takes the page off the screen,\nbut leaves it in the list.  You can then show it\nby choosing the Show menu item.");
	menuTree->SetNodeMnemonic(snode, 'H');

	snode = menuTree->InsertMenuItem("Close", "/Page", NULL);
	menuTree->SetNodeHelpText(snode, "Closes the currently selected device page.");
	menuTree->SetNodeMnemonic(snode, 'C');
	menuTree->SetNodeAccelerator(snode, "Ctrl+C");

	snode = menuTree->InsertMenuItem("Close All...", "/Page", NULL);
	menuTree->SetNodeHelpText(snode, "Closes all devices pages after issuing\na confirmation.");
	menuTree->SetNodeMnemonic(snode, 'A');
	menuTree->SetNodeAccelerator(snode, "Ctrl+A");

	snode = menuTree->InsertMenuItem("Options", NULL, NULL);
	menuTree->SetNodeHelpText(snode, "This menu provides miscellaneous options\navailable to the program as a whole.");
	menuTree->SetNodeMnemonic(snode, 'O');

	snode = menuTree->InsertMenuItem("Search...", "/Options", NULL);
	menuTree->SetNodeHelpText(snode, "This option allows the user to search for a particular\ndevice.  A prompt will ask for a search string, which\ncould be a partial device name.  The result of the search\nwill then be displayed allowing the user to view the device\nin one or more device lists.");
	menuTree->SetNodeMnemonic(snode, 'S');

	snode = menuTree->InsertMenuItem("Read Archive Log", "/Options", NULL);
	menuTree->SetNodeHelpText(snode, "Brings up a separate window allowing the user\nto view the log of archives that have been made\nwith the archiving system.");
	menuTree->SetNodeMnemonic(snode, 'A');

	snode = menuTree->InsertMenuItem("PPM User Monitor", "/Options", NULL);
	menuTree->SetNodeHelpText(snode, "Run program that lets you see which ppm users\nand supercycle tables are active and have been\nactive recently.");

	snode = menuTree->InsertMenuItem("----", "/Options", NULL);
	menuTree->SetNodeType(snode, MenuSeparatorType);

	snode = menuTree->InsertMenuItem("SLDs via Controller...", "/Options", NULL);
	menuTree->SetNodeHelpText(snode, "Choose a controller and bring up its SLDs on a page.");

	snode = menuTree->InsertMenuItem("CLDs...", "/Options", NULL);
	menuTree->SetNodeHelpText(snode, "Brings up a 'cld window' allowing the user to set\nvalues and/or view measurements for CLDs.");
	menuTree->SetNodeMnemonic(snode, 'C');

	snode = menuTree->InsertMenuItem("CLD Events...", "/Options", NULL);
	menuTree->SetNodeHelpText(snode, "Brings up a 'cld window' allowing the user\nto set timing events for CLDs.");
	menuTree->SetNodeMnemonic(snode, 'E');

	snode = menuTree->InsertMenuItem("----", "/Options", NULL);
	menuTree->SetNodeType(snode, MenuSeparatorType);

	snode = menuTree->InsertMenuItem("Reload DDF...", "/Options", NULL);

	return menuTree;
}
