#include "os/os_windows_nt6.h"

#include "main/main.h"

#include <shellapi.h>

#ifdef WIN_LEGACY_WINDOWED_ENABLED
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#else
int main()
#endif // WIN_LEGACY_WINDOWED_ENABLED
{
	int argc = 0;
	wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (!argv || argc == 0) {
		LocalFree(argv);
		return EXIT_FAILURE;
	}

	OS_WindowsNT6 os;
	Vector<CharString> utf8_args;
	Vector<char *> args;
	utf8_args.resize(argc);
	args.resize(argc);
	for (int i = 0; i < argc; ++i) {
		utf8_args.write[i] = String(argv[i]).utf8();
		args.write[i] = utf8_args.write[i].ptrw();
	}
	LocalFree(argv);

	const Error error = Main::setup(args[0], argc - 1, args.ptrw() + 1);
	if (error != OK) {
		return error == ERR_HELP ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	// A false start also handles successful CLI tools; preserve their exit code.
	if (Main::start()) {
		os.run();
	}
	Main::cleanup();
	return os.get_exit_code();
}
