#include "os/os_windows_nt4.h"

#include "core/profiling/profiling.h"
#include "main/main.h"

#include <windows.h>

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

	godot_init_profiler();
	OS_WindowsNT4 os;
	Vector<CharString> utf8_args;
	Vector<char *> args;
	utf8_args.resize(argc);
	args.resize(argc);
	for (int i = 0; i < argc; ++i) {
		utf8_args.write[i] = String::utf16(reinterpret_cast<const char16_t *>(argv[i])).utf8();
		args.write[i] = utf8_args.write[i].ptrw();
	}
	LocalFree(argv);

	TEST_MAIN_PARAM_OVERRIDE(argc, args.ptrw())

	const Error error = Main::setup(args[0], argc - 1, args.ptrw() + 1);
	if (error != OK) {
		godot_cleanup_profiler();
		return error == ERR_HELP ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (Main::start() == EXIT_SUCCESS) {
		os.run();
	} else {
		os.set_exit_code(EXIT_FAILURE);
	}
	Main::cleanup();
	godot_cleanup_profiler();
	return os.get_exit_code();
}
