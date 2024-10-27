#include "Application.hpp"

int main(int argc, char** argv)
{
	// @TODO: config file?
	Application app(argc, argv);
	return app.Run();
}
