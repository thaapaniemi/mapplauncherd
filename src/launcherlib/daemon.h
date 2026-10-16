/***************************************************************************
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Copyright (c) 2013 - 2021 Jolla Ltd.
** Copyright (c) 2021 Open Mobile Platform LLC.
** All rights reserved.
** Contact: Nokia Corporation (directui@nokia.com)
**
** This file is part of applauncherd
**
** If you have questions regarding the use of this file, please contact
** Nokia at directui@nokia.com.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file LICENSE.LGPL included in the packaging
** of this file.
**
****************************************************************************/

#ifndef DAEMON_H
#define DAEMON_H

#include "launcherlib.h"

#include <string>

using std::string;

#include <sys/types.h>
#include <tr1/memory>

using std::tr1::shared_ptr;

#include <vector>

using std::vector;

#include <map>

using std::map;

#include <signal.h>
#include <sys/socket.h>

class Booster;
class SocketManager;
class SingleInstance;

/*!
 * \class Daemon.
 * \brief Daemon wraps up the daemonizing functionality.
 *
 * Daemon wraps up the daemonizing functionality and is the
 * main object of the launcher program. It runs the main loop of the
 * application, listens connections from the invoker and forks Booster
 * processes.
 */
class DECL_EXPORT Daemon
{
public:

    /*!
     * \brief Constructor
     * \param argc Argument count delivered to main()
     * \param argv Argument array delivered to main()
     *
     * Supported arguments:
     * --daemon == daemonize
     * --help   == print usage
     */
    Daemon(int & argc, char * argv[]);

    //! Destructor
    ~Daemon();

    /*!
     * \brief Run main loop and fork Boosters.
     */
    void run(Booster *booster);

    /*! \brief Return the one-and-only Daemon instance.
     * \return Pointer to the Daemon instance.
     */
    static Daemon * instance();

    //! \brief Reapes children processes gone zombies (finished Boosters).
    void reapZombies();

    /*!
     * Get fd to which signal handler writes the number
     * of an arriving Unix signal.
     */
    int sigPipeFd() const;

    /*!
     * Record an arriving Unix signal for the main loop and say whether the
     * caller must write the wakeup byte into the signal pipe.
     *
     * Called from a signal handler, so it only touches sig_atomic_t state.
     * Returns false when a wakeup byte is already pending - the pipe may be
     * full, and one byte per drain is enough because the loop handles every
     * recorded signal.
     */
    bool noteSignal(int signum);

    /*!
     * Take the next recorded signal number and clear it, or 0 when none is
     * left. Called from the main loop once it drained the signal pipe.
     */
    int takePendingSignal();

    /*!
     * Clear the wakeup byte flag once the main loop read that byte.
     */
    void clearSigWakeup();

    /*!
     * Set unix signal handler and save its original value.
     */
    void setUnixSignalHandler(int signum, sighandler_t handler);

    /*!
     * Restore unix signal handlers to their saved values.
     */
    void restoreUnixSignalHandlers();

private:

    //! Disable copy-constructor
    Daemon(const Daemon & r);

    //! Disable assignment operator
    Daemon & operator= (const Daemon & r);

    //! Parse arguments
    void parseArgs(int argc, char **argv);

    //! Fork to a daemon
    void daemonize();

    //! Fork process that kills boosters if needed
    void forkKiller();

    //! Forks and initializes a new Booster
    void forkBooster(int sleepTime = 0);

    //! Kill given pid with SIGKILL by default
    void killProcess(pid_t pid, int signal = SIGKILL) const;

    //! Load single-instance plugin
    void loadSingleInstancePlugin();

    //! Read and process data from a booster pipe
    void readFromBoosterSocket(int fd);

    //! Enter normal mode (restart boosters with cache enabled)
    void enterNormalMode();

    //! Enter boot mode (restart boosters with cache disabled)
    void enterBootMode();

    //! Kill all active boosters with -9
    void killBoosters();

    //! Prints the usage and exits with given status
    void usage(const char *name, int status);

    //! Daemonize flag (--fork). Daemon forks if true.
    bool m_daemon;

    //! Debug mode flag (--debug). 
    bool m_debugMode;

    /*! Flag indicating boot mode (--boot-mode). If true, then:
     *  - Caches won't be initialized.
     *  - Booster respwan delay is 0.
     *
     *  Normal mode is activated by firing SIGUSR1.
     */
    bool m_bootMode;

    //! Vector of current child PID's
    typedef vector<pid_t> PidVect;
    PidVect m_children;

    //! Storage of booster <-> invoker pid pairs
    typedef map<pid_t, pid_t> PidMap;
    PidMap m_boosterPidToInvokerPid;

    //! Storage of booster <-> invoker socket file descriptor pairs
    typedef map<pid_t, pid_t> FdMap;
    FdMap m_boosterPidToInvokerFd;

    //! Current booster pid
    pid_t m_boosterPid;

    //! Socket pair used to tell the parent that a new booster is needed +
    //! some parameters.
    int m_boosterLauncherSocket[2];

    //! Pipe used to safely catch Unix signals. It only carries wakeup bytes,
    //! one per drain, because the handler must never block on a full pipe.
    int m_sigPipeFd[2];

    //! Signals the handler recorded, as a bit per signal number. The pipe
    //! byte is only a wakeup, so a full pipe cannot lose a signal.
    volatile sig_atomic_t m_sigPending;

    //! True while a wakeup byte is in the signal pipe (or being written)
    volatile sig_atomic_t m_sigWakeupSent;

    //! Argument vector initially given to the launcher process
    int m_initialArgc;

    //! Argument count initially given to the launcher process
    char** m_initialArgv;

    //! Singleton Daemon instance
    static Daemon * m_instance;

    //! Time to sleep before forking a new booster
    static const int m_boosterSleepTime;

    //! Manager for invoker <-> booster sockets
    SocketManager * m_socketManager;

    //! Single instance plugin handle
    SingleInstance * m_singleInstance;

    //! Original unix signal handlers are saved here
    typedef map<int, sighandler_t> SigHandlerMap;
    SigHandlerMap m_originalSigHandlers;

    //! True if systemd needs to be notified
    bool m_notifySystemd;
    string m_boostedApplication;

    //! Drop capabilities needed for initialization
    static void dropCapabilities();

    //! Booster instance
    Booster * m_booster;

#ifdef UNIT_TEST
    friend class Ut_Daemon;
#endif
};

#endif // DAEMON_H
