/*
 *  oftc-ircservices: an extensible and flexible IRC Services package
 *  akill.c - akill related functions
 *
 *  Copyright (C) 2006 Stuart Walsh and the OFTC Coding department
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
 *  $Id: dbm.c 1260 2007-12-07 08:53:17Z swalsh $
 */

#include "stdinc.h"
#include "language.h"
#include "parse.h"
#include "dbm.h"
#include "nickname.h"
#include "interface.h"
#include "client.h"
#include "hostmask.h"
#include "nickname.h"
#include "servicemask.h"

static dlink_list akill_list_cache = { 0 };

static struct ServiceMask *
row_to_akill(row_t *row)
{
  struct ServiceMask *sban;

  sban = MyMalloc(sizeof(struct ServiceMask));
  sban->id = atoi(row->cols[0]);

  if(row->cols[1] != NULL)
    sban->setter = atoi(row->cols[1]);
  else
    sban->setter = 0;

  DupString(sban->mask, row->cols[2]);
  DupString(sban->reason, row->cols[3]);
  sban->time_set = atoi(row->cols[4]);
  sban->duration = atoi(row->cols[5]);
  sban->type = AKILL_MASK;

  return sban;
}

static int
akill_check_mask(struct Client *client, const char *mask)
{
  struct irc_ssaddr addr;
  struct split_nuh_item nuh;
  char name[NICKLEN];
  char user[USERLEN+1];
  char host[HOSTLEN+1];
  int type, bits, found = 0;

  DupString(nuh.nuhmask, mask);
  nuh.nickptr = name;
  nuh.userptr = user;
  nuh.hostptr = host;

  nuh.nicksize = sizeof(name);
  nuh.usersize = sizeof(user);
  nuh.hostsize = sizeof(host);

  split_nuh(&nuh);

  type = parse_netmask(host, &addr, &bits);

  if(match(name, client->name) && match(user, client->username))
  {
    switch(type)
    {
      case HM_HOST:
        if(match(host, client->host))
          found = 1;
        break;
      case HM_IPV4:
        if (client->aftype == AF_INET)
          if (match_ipv4(&client->ip, &addr, bits))
            found = 1;
        break;
#ifdef IPV6
      case HM_IPV6:
        if (client->aftype == AF_INET6)
          if (match_ipv6(&client->ip, &addr, bits))
            found = 1;
        break;
#endif
    }
  }
  MyFree(nuh.nuhmask);
  return found;
}

void
akill_list_free(dlink_list *list)
{
  dlink_node *ptr, *next;
  struct ServiceMask *sban;

  ilog(L_DEBUG, "Freeing akill list %p of length %lu", list, 
      dlink_list_length(list));

  DLINK_FOREACH_SAFE(ptr, next, list->head)
  {
    sban = (struct ServiceMask *)ptr->data;
    free_servicemask(sban);
    dlinkDelete(ptr, list);
    free_dlink_node(ptr);
  }
}

int
akill_list(dlink_list *list)
{
  result_set_t *results;
  int error;
  int i;

  results = db_execute(GET_AKILLS, &error, "");
  if(results == NULL && error != 0)
  {
    ilog(L_CRIT, "akill_check_client: database error %d", error);
    return FALSE;
  }
  else if(results == NULL)
    return FALSE;

  if(results->row_count == 0)
    return FALSE;

  for(i = 0; i < results->row_count; i++)
  {
    row_t *row = &results->rows[i];
    struct ServiceMask *sban = row_to_akill(row);

    dlinkAdd(sban, make_dlink_node(), list);
  }

  db_free_result(results);

  return dlink_list_length(list);
}

int
akill_get_expired(dlink_list *list)
{
  result_set_t *results;
  int error;
  int i;

  results = db_execute(GET_EXPIRED_AKILL, &error, "i", &CurrentTime);
  if(results == NULL && error != 0)
  {
    ilog(L_CRIT, "akill_get_expired: database error %d", error);
    return FALSE;
  }
  else if(results == NULL)
    return FALSE;

  if(results->row_count == 0)
    return FALSE;

  for(i = 0; i < results->row_count; i++)
  {
    row_t *row = &results->rows[i];
    struct ServiceMask *sban = row_to_akill(row);

    dlinkAdd(sban, make_dlink_node(), list);
  }

  db_free_result(results);

  return dlink_list_length(list);
}

int
akill_check_client(struct Service *service, struct Client *client)
{
  dlink_node *ptr;

  if(dlink_list_length(&akill_list_cache) == 0)
    akill_list(&akill_list_cache);

  DLINK_FOREACH(ptr, akill_list_cache.head)
  {
    struct ServiceMask *sban = (struct ServiceMask *)ptr->data;

    if(akill_check_mask(client, sban->mask))
    {
      char *setter = nickname_nick_from_id(sban->setter, TRUE);

      send_akill(service, setter, sban);
      MyFree(setter);

      return TRUE;
    }
  }

  return FALSE;
}

int
akill_add(struct ServiceMask *akill)
{
  int ret;

  akill_list_free(&akill_list_cache);

  akill->type = AKILL_MASK;

  if(akill->setter != 0)
    ret = db_execute_nonquery(INSERT_AKILL, "ssiii", akill->mask,
        akill->reason, &akill->setter, &akill->time_set, &akill->duration);
  else
    ret = db_execute_nonquery(INSERT_SERVICES_AKILL, "ssii", akill->mask,
        akill->reason, &akill->time_set, &akill->duration);

  if(ret == -1)
    return FALSE;

  return TRUE;
}

struct ServiceMask *
akill_find(const char *mask)
{
  int error;
  result_set_t *results;
  struct ServiceMask *akill;

  results = db_execute(GET_AKILL, &error, "s", mask);
  if(error || results == NULL)
    return NULL;

  if(results->row_count == 0)
  {
    db_free_result(results);
    return NULL;
  }

  akill = row_to_akill(&results->rows[0]);
  db_free_result(results);

  return akill;
}

int
akill_remove_mask(const char *mask)
{
  akill_list_free(&akill_list_cache);

  return db_execute_nonquery(DELETE_AKILL, "s", mask);
}
