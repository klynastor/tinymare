divert(-1)
#
# This is a basic sendmail 8.10.1 configuration file for Linux. It
# is designed to support local SMTP relaying with TinyMare, as well
# as retain the full compatibility of sendmail for other user
# accounts. If you want to customize it, copy it to a name
# appropriate for your environment and do the modifications there.
#
# Usage: m4 ../m4/cf.m4 tinymare.mc
#

divert(0)
VERSIONID(`$Id: tinymare.mc,v 8.10.1 2000/04/09 01:09:23 gandalf Exp $')

OSTYPE(linux)
FEATURE(redirect)
FEATURE(use_cw_file)
FEATURE(always_add_domain)
FEATURE(accept_unresolvable_domains)
MAILER(smtp)
MAILER(uucp)
MAILER(procmail)
MAILER(winds)
