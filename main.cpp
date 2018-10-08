#include <cerrno>  
#include <csignal>
#include <iostream>
#include <cstdlib>
#include <climits>

#include <sys/inotify.h>
#include <unistd.h>
#include <fcntl.h>

// TODO: Document.
// TODO: homogenize case, brackets, style, comments.

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1)) // FIXME: this is messy, fix
char buf[BUF_LEN] __attribute__ ((aligned(8)));
static int inotify_file_descriptor;

volatile sig_atomic_t events_waiting = 0;

static void
handle_event(int signal) {
	events_waiting++;
}

void
kill(const char* message)
{
	std::cout << "Fatal error: " << message << std::endl;
	exit(1);
}

int
main(int argc, char* argv[])
{
	int *wd;

	if (argc < 2) {
		kill("Usage: ./a.out PATH [PATH ...]");
	}
	
	/* Create the file descriptor for accessing the inotify API */
	inotify_file_descriptor = inotify_init();
	if (inotify_file_descriptor == -1) {
		kill("inotify_init1");
	}

	/**********************************/
	/* Set up signal handler. - Tegan */
	// Set up the struct that will configure our handler.
	struct sigaction sa;      // Describes the action to take on a process's signals.
	sigemptyset(&sa.sa_mask); // Init the signal mask field for those we want to ignore.
	sa.sa_flags = SA_RESTART; // Restart? FIXME: why this is, can I/should I change?
	sa.sa_handler = handle_event; // Set the method we want to use as a handler for the signal.
	
	// Register the handler. Check for error.
	if (sigaction(SIGIO, &sa, NULL) == -1) {
		kill("Failed to register handler!");
	}

	// Allow process to receive I/O signals from the file descriptor.
	if (fcntl(inotify_file_descriptor, F_SETOWN, getpid()) == -1) {
		kill("Failed to set F_SETOWN!");
	}

	int flags = fcntl(inotify_file_descriptor, F_GETFL);
	if(fcntl(inotify_file_descriptor, F_SETFL, flags | O_ASYNC | O_NONBLOCK) == -1) {
		kill("Another error bleh.\n");
	}
	/************************************/

	/* Allocate memory for watch descriptors FIXME: change to c++*/
	wd = static_cast<int*>(calloc(argc, sizeof(int)));
	if (wd == NULL) {
		kill("calloc");
	}

	/* Mark directories for events */
	for (int i(1); i < argc; i++) {
		wd[i] = inotify_add_watch(inotify_file_descriptor, argv[i], IN_ALL_EVENTS);
		if (wd[i] == -1) {
			kill("Cannot watch input.");
		}
	}

	/* Wait for events and/or terminal input */
	std::cout << "Listening for events." << std::endl;
	while (1 /* nop nop nop nop */) {
		sleep(1); // Sleep call. Will be interrupted and return early when signal recvd.

		if(events_waiting) {
			events_waiting--;

			ssize_t numRead = read(inotify_file_descriptor, buf, BUF_LEN); // FIXME: move def up
			if (numRead == 0) {
				kill("********** read() from inotify fd returned 0!");
			} else if (numRead > 0) {
				for (char *p = buf; p < buf + numRead; ) {
					struct inotify_event *event = (struct inotify_event *) p; // FIXME: move def up
					std::cout << "Watch descriptor " << event->wd << " reported event "
						<< event->mask << " for file: " << event->name << std::endl;
					p += sizeof(struct inotify_event) + event->len;
				}
			}
		}
	}

	close(inotify_file_descriptor);
	free(wd);

	return 0;
}

