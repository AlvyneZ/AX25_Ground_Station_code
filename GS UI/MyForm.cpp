#include "MyForm.h"

#include "Logger.h"
#include "fileOperations.h"
#include "XBeeCommunication.h"
#include "bitManipulation.h"
#include "CommunicationsHandler.h"

using namespace System;
using namespace System::Windows::Forms;

[STAThreadAttribute]
int main(array < String^ > ^ args) {
	Application::EnableVisualStyles();
	Application::SetCompatibleTextRenderingDefault(false);
	GSUI::MyForm form;

	Application::Run(%form);
	return 0;
}