#include "systemclass.h"

int main(
	//_In_ HINSTANCE hInstance,
	//_In_opt_ HINSTANCE hPrevInstance,
	//_In_ LPWSTR lpCmdLine,
	//_In_ int nShowCmd
)
{
	SystemClass* System = new SystemClass();

	bool result = System->Initialize();
	if(result)
	{
		System->Run();
	}

	System->Shutdown();
	delete System;
	System = nullptr;

	return 0;
}