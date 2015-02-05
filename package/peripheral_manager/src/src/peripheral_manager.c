#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <stdlib.h>
#include <syslog.h>
#include <config.h>
#include <getopt.h>
#include "log.h"
#include "ucix.h"

#include <libubox/uloop.h>
#include <libubus.h>

int debug_level = 0;

static char *config_path = "/lib/db/config";
static char *config_file = "hw";

static char *ubus_socket;

void print_usage(char *prg_name) {
        printf("Usage: %s -h -f\n", prg_name);
        printf("  Options: \n");
        printf("      -f, --foreground\tDon't fork off as a daemon.\n");
        printf("      -d, --debug=NUM\tSet debug level. Higher = more output\n");
        printf("      -c, --config=FILE\tConfig file to use. default = %s/%s\n", config_path, config_file);
        printf("      -s, --socket=FILE\tSet the unix domain socket to connect to for ubus\n");
        printf("      -h\t\tShow this help screen.\n");
        printf("\n");
}

int main(int argc, char **argv)
{
	int ch;
	int daemon = 1;
	pid_t pid, sid;
	struct uci_context *uci_ctx = NULL;
	static struct ubus_context *ubus_ctx = NULL;

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
                        {"foreground",  no_argument, 0, 'f'},
                        {"verbose",     no_argument, 0, 'v'},
                        {"debug", required_argument, 0, 'd'},
                        {"config",required_argument, 0, 'c'},
                        {"socket",required_argument, 0, 's'},
                        {0, 0, 0, 0}
                };

                ch = getopt_long(argc, argv, "vfhd:c:s:",
                                long_options, &option_index);

		if (ch == -1)
                        break;

                switch (ch) {
                case 'f':
                        daemon = 0;
                        break;
                case 'd':
                        debug_level = strtol(optarg, 0, 0);
                        break;

                case 'c':
			config_file = basename(optarg);
			config_path = dirname(optarg);
			break;
                case 's':
			ubus_socket = optarg;
			break;

		case 'h':
                default:
			print_usage(argv[0]);
			exit(0);
                }
 	}

	if (optind < argc) {
		printf("Extra arguments discarded: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		printf("\n");
	}


	/* Setup logging */
	if (daemon) {
		setlogmask(LOG_UPTO(LOG_INFO));
		openlog(PACKAGE, LOG_CONS, LOG_USER);

		syslog(LOG_INFO, "%s daemon starting up", PACKAGE);
	} else {
		setlogmask(LOG_UPTO(LOG_DEBUG));
		openlog(PACKAGE, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);

		syslog(LOG_INFO, "%s program starting up", PACKAGE);
	}

	/* daemonize */
	if (daemon) {
		syslog(LOG_INFO, "starting the daemonizing process");

		/* Fork off the parent process */
		pid = fork();
		if (pid < 0) {
			exit(EXIT_FAILURE);
		}
		/* If we got a good PID, then
		   we can exit the parent process. */
		if (pid > 0) {
			exit(EXIT_SUCCESS);
		}

		/* Change the file mode mask */
		umask(0);

		/* Create a new SID for the child process */
		sid = setsid();
		if (sid < 0) {
			/* Log the failure */
			exit(EXIT_FAILURE);
		}

		/* Change the current working directory */
		if ((chdir("/")) < 0) {
			/* Log the failure */
			exit(EXIT_FAILURE);
		}
		/* Close out the standard file descriptors */
		//close(STDIN_FILENO);
		//close(STDOUT_FILENO);
		//close(STDERR_FILENO);
	}

	uci_ctx = ucix_init_path(config_path , config_file);
	if (! uci_ctx ) {
		syslog(LOG_ERR,"Failed to load config file \"%s/%s\"\n", config_path, config_file);
		exit(1);
	}

	if (uloop_init() != 0) {
		syslog(LOG_ERR,"Could not init event loop, Can't continue.\n");
		exit(EXIT_FAILURE);
	}

	ubus_ctx = ubus_connect(ubus_socket);

	if (!ubus_ctx) {
		syslog(LOG_ERR,"Failed to connect to ubus. Can't continue.\n");
		exit(EXIT_FAILURE);
	}

	ubus_add_uloop(ubus_ctx);

	ubus_free(ubus_ctx);

	DBG(1,"testing\n");
	syslog(LOG_INFO, "%s exiting", PACKAGE);

	uloop_done();
	return 0;
}