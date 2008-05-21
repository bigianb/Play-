#import "VfsManagerController.h"

static CVfsManagerController* g_sharedInstance = nil;

@implementation CVfsManagerController

+(CVfsManagerController*)defaultController
{
	if(g_sharedInstance == nil)
	{
		g_sharedInstance = [[self alloc] initWithWindowNibName: @"VfsManager"];
	}
	return g_sharedInstance;
}

-(void)awakeFromNib
{
	[m_bindingsTableView setDoubleAction: @selector(OnTableViewDblClick:)];
}

-(void)showManager
{
	[[self window] makeKeyAndOrderFront: nil];
	[NSApp runModalForWindow: [self window]];
}

-(void)OnOk: (id)sender
{
	[m_bindings save];
	[[self window] close];
}

-(void)OnCancel: (id)sender
{
	[[self window] close];
}

-(IBAction)OnTableViewDblClick: (id)sender
{
	int selectedIndex = [m_bindingsTableView clickedRow];
	if(selectedIndex == -1) return;
	CVfsManagerBinding* binding = [m_bindings getBindingAt: selectedIndex];
	[binding requestModification];
}

-(void)windowWillClose: (NSNotification*)notification
{
	[NSApp stopModal];
	[self release];
	g_sharedInstance = nil;
}

@end
