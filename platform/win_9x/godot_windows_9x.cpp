#include "encoding_windows_9x.h"
#include "os/os_windows_9x.h"

#include "core/profiling/profiling.h"
#include "main/main.h"

#ifdef WIN_LEGACY_WINDOWED_ENABLED
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#else
int main(int argc, char **argv)
#endif // WIN_LEGACY_WINDOWED_ENABLED
{
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	const int argc = __argc;
	char **argv = __argv;
#endif // WIN_LEGACY_WINDOWED_ENABLED
	godot_init_profiler();
	OS_Windows9x os;
	// The 9x CRT receives the native ANSI command line. Keep UTF-8 inside Godot.
	Vector<CharString> utf8_args;
	Vector<char *> args;
	utf8_args.resize(argc);
	args.resize(argc);
	for (int i = 0; i < argc; ++i) {
		utf8_args.write[i] = Windows9xEncoding::decode(argv[i]).utf8();
		args.write[i] = utf8_args.write[i].ptrw();
	}
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
