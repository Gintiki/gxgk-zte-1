/*
 * =====================================================================================
 *
 *       Filename:  main.c
 *
 *    Description:  
 *
 *        Version:  $Revision: 111 $
 *        Created:  07/06/2009 08:19:17 PM
 *       Revision:  none
 *       Compiler:  gcc
 *       File Last Modify: $Date: 2009-10-28 21:51:12 +0800 (Wed, 28 Oct 2009) $
 *         Author:  BOYPT (PT), pentie@gmail.com
 *        Company:  http://apt-blog.co.cc
 *
 * =====================================================================================
 */


#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include "commondef.h"
#include "eap_protocol.h"

#define LOCKFILE "/var/run/zte-client.pid"

#define LOCKMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)



extern pcap_t      *handle;
extern int          exit_flag;

int                 lockfile;                  /* 锁文件的描述字 */
static uint8_t      exit_counter = 10;

static void
flock_reg ()
{
    char buf[16];
    struct flock fl;
    fl.l_start = 0;
    fl.l_whence = SEEK_SET;
    fl.l_len = 0;
    fl.l_type = F_WRLCK;
    fl.l_pid = getpid();
 
    //阻塞式的加锁
    if (fcntl (lockfile, F_SETLKW, &fl) < 0){
        perror ("fcntl_reg");
        exit(EXIT_FAILURE);
    }
 
    //把pid写入锁文件
    ftruncate (lockfile, 0);    
    sprintf (buf, "%ld", (long)getpid());
    write (lockfile, buf, strlen(buf) + 1);
}


void
daemon_init(void)
{
	pid_t	pid;
    int     fd0;

	if ( (pid = fork()) < 0)
	    perror ("Fork");
	else if (pid != 0) {
        fprintf(stdout, "&&Info: Forked background with PID: [%d]\n\n", pid);
		exit(EXIT_SUCCESS);
    }
	setsid();		/* become session leader */
	chdir("/tmp");		/* change working directory */
	umask(0);		/* clear our file mode creation mask */
    flock_reg ();

    fd0 = open ("/dev/null", O_RDWR);
    dup2 (fd0, STDIN_FILENO);
    dup2 (fd0, STDERR_FILENO);
    dup2 (fd0, STDOUT_FILENO);
    close (fd0);
}


static int 
is_running()
{
    struct flock fl;
    fl.l_start = 0;
    fl.l_whence = SEEK_SET;
    fl.l_len = 0;
    fl.l_type = F_WRLCK;
 
    //尝试获得文件锁
    if (fcntl (lockfile, F_GETLK, &fl) < 0){
        perror ("fcntl_get");
        exit(EXIT_FAILURE);
    }

    if (exit_flag) {
        if (fl.l_type != F_UNLCK) {
            if ( kill (fl.l_pid, SIGINT) == -1 )
                perror("kill");
            fprintf (stdout, "&&Info: Kill Signal Sent to PID %d.\n", fl.l_pid);
        }
        else 
            fprintf (stderr, "&&Info: Program not running.\n");
        exit (EXIT_FAILURE);
    }


    //没有锁，则给文件加锁，否则返回锁着文件的进程pid
    if (fl.l_type == F_UNLCK) {
        flock_reg ();
        return 0;
    }

    return fl.l_pid;
}

static void
show_usage()
{
    printf( "\n"
            "zte-client for gdst %s \n"
            "\t  -- Client for ZTE Authentication in GDST campus.\n"
            "\n"
            "  Usage:\n"
            "\tRun under root privilege, usually by `sudo', with your \n"
            "\taccount info in arguments:\n\n"
            "\t-u, --username        Your username.\n"
            "\t-p, --password        Your password.\n\n"
            "\n"
            "  Optional Arguments:\n\n"

            "\t--device              Specify which device to use.\n"
            "\t                      Default is usually eth0.\n\n"

            "\t-b, --background      Program fork as daemon after authentication.\n\n"

            "\t-l                    Tell the process to Logoff.\n\n"

            "\t-h, --help            Show this help.\n\n"
            "\n"
            "  About zte-client:\n\n"
            "\tzte-client is a program developed individually and release under MIT\n"
            "\tlicense as free software, with NO any relaiontship with ZTE company.\n\n\n"
            "\t\t\t\t\t\t\t\t2012.09.20\n",
            ZTE_VER);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  init_arguments
 *  Description:  初始化和解释命令行的字符串。getopt_long
 * =====================================================================================
 */
static void 
init_arguments(int *argc, char ***argv)
{
    extern int         dhcp_on;               /* DHCP 模式标记 */
    extern int         background;            /* 后台运行标记  */
    extern int         exit_flag;
    extern char        *dev;               /* 连接的设备名 */
    extern char        *username;          
    extern char        *password;

    static char        pass_static_buf[64];
    /* Option struct for progrm run arguments */
    static struct option long_options[] =
        {
        {"help",        no_argument,        0,              'h'},
        {"background",  no_argument,        &background,    1},
        {"device",      required_argument,  0,              2},
        {"username",    required_argument,  0,              'u'},
        {"password",    required_argument,  0,              'p'},
        {0, 0, 0, 0}
        };
    int c;
    while (1) {

        /* getopt_long stores the option index here. */
        int option_index = 0;
        c = getopt_long ((*argc), (*argv), "u:p:d:hbl",
                        long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
            case 0:
               break;
            case 'b':
                background = 1;
                break;
            case 'd':
                dev = optarg;
                break;
            case 'u':
                username = optarg;
                break;
            case 'p':
                password = optarg;
                break;
            case 'l':
                exit_flag = 1;
                break;
            case 'h':
                show_usage();
                exit(EXIT_SUCCESS);
                break;
            case '?':
                if (optopt == 'u' || optopt == 'p'|| optopt == 'd')
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                exit(EXIT_FAILURE);
                break;
            default:
                fprintf (stderr,"Unknown option character `\\x%x'.\n", c);
                exit(EXIT_FAILURE);
        }
    }
    //通过stdin读入密码
    if (username != NULL && password == NULL && !exit_flag) {
        fprintf (stdout, "Password:");
        fgets (pass_static_buf, sizeof(pass_static_buf), stdin);
        fprintf (stdout, "\n");
        
        char *line;
        if ((line = strrchr (pass_static_buf, '\n')))
            *line = '\0';
        password = pass_static_buf;
    }
}


static void
signal_interrupted (int signo)
{
    extern int exit_flag;
    if (exit_flag)
        exit (EXIT_SUCCESS);
    exit_flag = 1;
    fprintf(stdout,"\n&&Info: Interrupted. \n");
    send_eap_packet(EAPOL_LOGOFF);
}

static void
signal_alarm (int signo)
{
    extern enum STATE state;
    extern char dev_if_name[];

    switch (state) {
        case ONLINE:
            break;

        case STARTED:
            fprintf(stderr, "@@ERROR: Packet sent but no reply. "
                            "Please check adapter %s link status.\n", dev_if_name);
            pcap_breakloop (handle);
            break;

        case STATUS_ERROR:
            fprintf(stdout, "\n&&Info: Program Exit.         \n");
            pcap_breakloop (handle);
            break;

        default:
            break;
    }
}


int main(int argc, char **argv)
{
    int ins_pid;

    init_arguments (&argc, &argv);

    //打开锁文件
    lockfile = open (LOCKFILE, O_RDWR | O_CREAT , LOCKMODE);
    if (lockfile < 0){
        perror ("Lockfile");
        exit(EXIT_FAILURE);
    }

    if ( (ins_pid = is_running()) ) {
        fprintf(stderr,"@@ERROR: Program already "
                            "running with PID %d\n", ins_pid);
        exit(EXIT_SUCCESS);
    }
    init_info();
    init_device();
    init_frames ();

    signal (SIGINT, signal_interrupted);
    signal (SIGTERM, signal_interrupted);
    signal (SIGALRM, signal_alarm);

    show_local_info();

    alarm(10);
    send_eap_packet (EAPOL_START);

    pcap_loop (handle, -1, get_packet, NULL);   /* main loop */
    pcap_close (handle);
    return 0;
}



