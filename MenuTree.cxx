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

	snode = menuTree->InsertMenuItem("Open Favorite...", "/File", NULL);
	menuTree->SetNodeHelpText(snode, "Brings up a new window showing those pet pages that you have\ndisplayed most frequently during the current run.  You can then\nselect one from the list for display.");
	menuTree->SetNodeAccelerator(snode, "Ctrl+F");

	snode = menuTree->InsertMenuItem("----", "/File", NULL);
	menuTree->SetNodeType(snode, MenuSeparatorType);

	snode = menuTree->InsertMenuItem("Default PPM User...", "/File", NULL);
	menuTree->SetNodeHelpText(snode, "Sets the default PPM user for this program.\nThe default PPM user is used as the PPM user\nfor any new device lists that are displayed.\nIt does not affect existing device pages.");
	menuTree->SetNodeMnemonic(snode, 'P');

	snode = menuTree->InsertMenuItem("----", "/File", NULL);
	menuTree->SetNodeType(snode, MenuSeparatorType);

	snode = menuTree->InsertMenuItem("Create Temp ADO Page...", "/File", NULL);
	menuTree->SetNodeHelpText(snode, "Create a page from selected parameters");

	snode = menuTree->InsertMenuItem("Create Temp PS ADO Page...", "/File", NULL);
	menuTree->SetNodeHelpText(snode, "Create a RHIC page from a list of selected Power Supplies");

	snode = menuTree->InsertMenuItem("----", "/File", NULL);
	menuTree->SetNodeType(snode, MenuSeparatorType);

	snode = menuTree->InsertMenuItem("Search pet Tree", "/File", NULL);

	snode = menuTree->InsertMenuItem("Find Text in Files...", "/File/Search pet Tree", NULL);
	menuTree->SetNodeHelpText(snode, "Brings up a popup that will let you search for any\nstring within device lists in the pet tree.  You can\npick a starting point in the tree to narrow your search.");

	snode = menuTree->InsertMenuItem("Find Recently Modified Files...", "/File/Search pet Tree", NULL);
	menuTree->SetNodeHelpText(snode, "Brings up a popup which allows you to search all or part of the pet\ntree for files that have been modified within a certain number of\ndays that you enter.");

	snode = menuTree->InsertMenuItem("Find Checked Out Files...", "/File/Search pet Tree", NULL);
	menuTree->SetNodeHelpText(snode, "Brings up a popup allowing you to find checked out files\nin all or part of the pet tree for a given user name. \nLeave the name blank to find all checked out files.");

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

	snode = menuTree->InsertMenuItem("Pet Page History", "/Options", NULL);
	menuTree->SetNodeHelpText(snode, "Brings up a new window allowing you to look at the history\nof when pet pages were loaded by this program or any\nother one.  Searches for particular pages are possible.");

	snode = menuTree->InsertMenuItem("Read Archive Log", "/Options", NULL);
	menuTree->SetNodeHelpText(snode, "Brings up a separate window allowing the user\nto view the log of archives that have been made\nwith the archiving system.");
	menuTree->SetNodeMnemonic(snode, 'A');

	snode = menuTree->InsertMenuItem("PPM User Monitor", "/Options", NULL);
	menuTree->SetNodeHelpText(snode, "Run program that lets you see which ppm users\nand supercycle tables are active and have been\nactive recently.");

	snode = menuTree->InsertMenuItem("----", "/Options", NULL);
	menuTree->SetNodeType(snode, MenuSeparatorType);

	snode = menuTree->InsertMenuItem("Flash Pages", "/Options", NULL);
	menuTree->SetNodeHelpText(snode, "Raises all open pet pages and temporarily flashes the menu bar");

	return menuTree;
}
