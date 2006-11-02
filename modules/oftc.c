/*
 *  oftc-ircservices: an exstensible and flexible IRC Services package
 *  oftc.c: A protocol handler for the OFTC IRC Network
 *
 *  Copyright (C) 2006 Stuart Walsh and the OFTC Coding department
 *
 *  Some parts:
 *
 *  Copyright (C) 2002 by the past and present ircd coders, and others.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id$
 */

#include "stdinc.h"

static void *irc_sendmsg_gnotice(va_list);
static void *irc_sendmsg_svsmode(va_list);
static void *irc_sendmsg_svscloak(va_list);

static dlink_node *oftc_gnotice_cb;
static dlink_node *oftc_umode_cb;
static dlink_node *oftc_svscloak_cb;

struct Message gnotice_msgtab = {
  "GNOTICE", 0, 0, 3, 0, MFLG_SLOW, 0,
  { m_ignore, m_ignore }
};

INIT_MODULE(oftc, "$Revision$")
{
  oftc_gnotice_cb = install_hook(gnotice_cb, irc_sendmsg_gnotice);
  oftc_umode_cb   = install_hook(umode_cb, irc_sendmsg_svsmode);
  oftc_svscloak_cb = install_hook(cloak_cb, irc_sendmsg_svscloak);
  mod_add_cmd(&gnotice_msgtab);
}

CLEANUP_MODULE
{
  mod_del_cmd(&gnotice_msgtab);
  uninstall_hook(gnotice_cb, irc_sendmsg_gnotice);
  uninstall_hook(umode_cb, irc_sendmsg_svsmode);
  uninstall_hook(cloak_cb, irc_sendmsg_svscloak);
}

static void *
irc_sendmsg_gnotice(va_list args)
{
  struct Client *client = va_arg(args, struct Client*);
  char          *source = va_arg(args, char *);
  char          *text   = va_arg(args, char *);
  
  // 1 is UMODE_ALL, aka UMODE_SERVERNOTICE
  sendto_server(client, ":%s GNOTICE %s 1 :%s", source, source, text);
  return NULL;
}

static void 
irc_sendmsg_svscloak(va_list args)
{
  char *target_name = va_arg(args, char *);
  char *cloakstring = va_arg(args, char *);

  sendto_server(client, ":%s SVSCLOAK %s :%s", 
    me.name, target_name, cloakstring);
  
  return NULL;
}

static void *
irc_sendmsg_svsmode(va_list args)
{
  struct Client *client = va_arg(args, struct Client*);
  char          *target = va_arg(args, char *);
  char          *mode   = va_arg(args, char *);

  sendto_server(client, ":%s SVSMODE %s :%s", me.name, target, mode);

  return NULL;
}

#if 0
XXX unused atm
static void
irc_sendmsg_svsnick(struct Client *client, struct Client *target, char *newnick)
{
  sendto_server(client, ":%s SVSNICK %s :%s", me.name, target->name, newnick);
}

#endif
