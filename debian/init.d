#! /bin/sh
#
# skeleton      example file to build /etc/init.d/ scripts.
#               This file should be used to construct scripts for /etc/init.d.
#
#               Written by Miquel van Smoorenburg <miquels@cistron.nl>.
#               Modified for Debian 
#               by Ian Murdock <imurdock@gnu.ai.mit.edu>.
#
# Version:      @(#)skeleton  1.9  26-Feb-2001  miquels@cistron.nl
#

PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
DAEMON=/usr/sbin/srcpd
NAME=srcpd
DESC="SRCP server"

test -x $DAEMON || exit 0

# Include srcpd defaults if available
if [ -f /etc/default/srcpd ] ; then
        . /etc/default/srcpd
fi

# Get lsb functions
. /lib/lsb/init-functions
. /etc/default/rcS

set -e

case "$1" in
  start)
        log_begin_msg "Starting $DESC..."
        start-stop-daemon --start --quiet --pidfile /var/run/$NAME.pid \
                --exec $DAEMON -- $DAEMON_OPTS
        log_end_msg $?
        ;;
  stop)
        log_begin_msg "Stopping $DESC..."
        start-stop-daemon --stop --quiet --pidfile /var/run/$NAME.pid \
                --exec $DAEMON
        log_end_msg $?
        ;;
  reload)
        #       If the daemon can reload its config files on the fly
        #       for example by sending it SIGHUP, do it here.
        #
        #       If the daemon responds to changes in its config file
        #       directly anyway, make this a do-nothing entry.

        log_begin_msg "Reloading $DESC configuration files..."
        start-stop-daemon --stop --signal 1 --quiet --pidfile \
                /var/run/$NAME.pid --exec $DAEMON
        log_end_msg $?
  ;;
  restart|force-reload)
        #
        #       If the "reload" option is implemented, move the "force-reload"
        #       option to the "reload" entry above. If not, "force-reload" is
        #       just the same as "restart".
        #
        log_begin_msg "Restarting $DESC..."
        start-stop-daemon --stop --quiet --pidfile \
                /var/run/$NAME.pid --exec $DAEMON
        sleep 2
        start-stop-daemon --start --quiet --pidfile \
                /var/run/$NAME.pid --exec $DAEMON -- $DAEMON_OPTS
        log_end_msg $?
        ;;
  *)
        N=/etc/init.d/$NAME
        # echo "Usage: $N {start|stop|restart|reload|force-reload}" >&2
        log_success_msg "Usage: $N {start|stop|restart|reload|force-reload}"
        exit 1
        ;;
esac

exit 0

