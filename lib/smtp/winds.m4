PUSHDIVERT(-1)
#
# Copyright (c) 1998,2000 Byron Stanoszek.  All rights reserved.
#
# This file should be modified to meet the requirements of your system.

POPDIVERT
##########################################
###   WindsMARE Mailer Specification   ###
##########################################

VERSIONID(`$Id: winds.m4,v 1.1 2000/04/08 23:56:47 gandalf Exp $')

Mwinds,		P=/usr/local/bin/redir, F=7SXm, S=11/31, R=61,
		A=redir winds.org 7348 /usr/local/etc/windsmare.smtp

LOCAL_RULE_0
R$+ < @ windsmare.winds.org. >	$#winds $@ windsmare.winds.org $: $1

LOCAL_CONFIG
CPwindsmare.winds.org
CRwindsmare.winds.org
