/*
 *  oftc-ircservices: an exstensible and flexible IRC Services package
 *  dbm.c: The database manager
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
 *  $Id$
 */

#include "stdinc.h"
#include <dbi/dbi.h>
#include "conf/conf.h"

static char querybuffer[1025];

void
init_db()
{
  int num_drivers = dbi_initialize(NULL);

  memset(&Database, 0, sizeof(Database));

  if(num_drivers >= 0)
    printf("db: loaded: %d drivers available.\n", num_drivers);
  else
  {
    printf("db: Error loading db.\n");
    exit(-1);
  }

  printf("db: version: %s\n", dbi_version());
}

void
db_load_driver()
{
  Database.conn = dbi_conn_new(Database.driver);
  if(Database.conn == NULL)
  {
    printf("db: Error loading database driver %s\n", Database.driver);
    exit(-1);
  }

  printf("db: Driver %s loaded\n", Database.driver);

  dbi_conn_set_option(Database.conn, "username", Database.username);
  dbi_conn_set_option(Database.conn, "password", Database.password);
  dbi_conn_set_option(Database.conn, "dbname", Database.dbname);

  if(dbi_conn_connect(Database.conn) < 0)
  {
    const char *error;
    dbi_conn_error(Database.conn, &error);
    printf("db: Failed to connect to database %s\n", error);
  }
  else
    printf("db: Database connection succeeded.\n");

  Database.driv = dbi_conn_get_driver(Database.conn);
}

struct Nick *
db_find_nick(const char *nick)
{
  dbi_result result;
  char *escnick = NULL;
  struct Nick *nick_p;
  char *retnick, *retpass, *retcloak;
  
  assert(nick != NULL);

  if(dbi_driver_quote_string_copy(Database.driv, nick, &escnick) == 0)
  {
    printf("db: Failed to query: dbi_driver_quote_string_copy\n");
    return NULL;
  }
  
  snprintf(querybuffer, 1024, "SELECT id, nick, password, email, cloak, "
      "last_quit_time, reg_time, last_seen, last_used, status, flags, language "
      "FROM %s WHERE nick=%s", "nickname", escnick);

  MyFree(escnick);
  printf("db: query: %s\n", querybuffer);

  if((result = dbi_conn_query(Database.conn, querybuffer)) == NULL)
  {
    const char *error;
    dbi_conn_error(Database.conn, &error);
    printf("db: Failed to query: %s\n", error);
    return NULL;
  }

  if(dbi_result_get_numrows(result) == 0)
  {
    printf("db: Nick %s not found\n", nick);
    return NULL;
  }

  nick_p = MyMalloc(sizeof(struct Nick));
  dbi_result_first_row(result);
  dbi_result_get_fields(result, "id.%ui nick.%S password.%S email.%S cloak.%S "
      "last_quit_time.%l reg_time.%l last_seen.%l last_used.%l status.%ui "
      "flags.%ui language.%ui",
      &nick_p->id, &retnick, &retpass, &nick_p->email, &retcloak, 
      &nick_p->last_quit_time, &nick_p->reg_time, &nick_p->last_seen, 
      &nick_p->last_used, &nick_p->status, &nick_p->flags, &nick_p->language);

  strlcpy(nick_p->nick, retnick, sizeof(nick_p->nick));
  strlcpy(nick_p->pass, retpass, sizeof(nick_p->pass));
  strlcpy(nick_p->cloak, retcloak, sizeof(nick_p->cloak));

  MyFree(retnick);
  MyFree(retpass);
  MyFree(retcloak);

  return nick_p;
}

struct Nick *
db_register_nick(const char *nick, const char *password, const char *email)
{
  char *escnick = NULL;
  char *escemail = NULL;
  char *escpass = NULL;
  dbi_result result;
  
  assert(nick != NULL);

  if(dbi_driver_quote_string_copy(Database.driv, nick, &escnick) == 0)
  {
    printf("db: Failed to query: dbi_driver_quote_string_copy\n");
    return NULL;
  }

  if(dbi_driver_quote_string_copy(Database.driv, email, &escemail) == 0)
  {
    printf("db: Failed to query: dbi_driver_quote_string_copy\n");
    MyFree(escnick);
    return NULL;
  }

  if(dbi_driver_quote_string_copy(Database.driv, password, &escpass) == 0)
  {
    printf("db: Failed to query: dbi_driver_quote_string_copy\n");
    MyFree(escnick);
    MyFree(escemail);
    return NULL;
  }
  
  snprintf(querybuffer, 1024, "INSERT INTO %s (nick, password, email, reg_time,"
      " last_seen, last_used) VALUES(%s, %s, %s, %ld, %ld, %ld)", "nickname", 
      escnick, escpass, escemail, CurrentTime, CurrentTime, CurrentTime);

  MyFree(escnick);
  MyFree(escemail);
  MyFree(escpass);

  printf("db: query: %s\n", querybuffer);

  if((result = dbi_conn_query(Database.conn, querybuffer)) == NULL)
  {
    const char *error;
    dbi_conn_error(Database.conn, &error);
    printf("db: Failed to query: %s\n", error);
    return NULL;
  }  

  dbi_result_free(result);

  return db_find_nick(nick); 
}

int
db_delete_nick(const char *nick)
{
  dbi_result result;
  char *escnick;

  if(dbi_driver_quote_string_copy(Database.driv, nick, &escnick) == 0)
  {
    printf("db: Failed to delete nick: dbi_driver_quote_string_copy\n");
    return -1;
  }

  
  snprintf(querybuffer, 1024, "DELETE FROM %s WHERE nick=%s",
      "nickname", escnick);

  MyFree(escnick);

  printf("db: query: %s\n", querybuffer);

  if((result = dbi_conn_query(Database.conn, querybuffer)) == NULL)
  {
    const char *error;
    dbi_conn_error(Database.conn, &error);
    printf("db: Failed to query: %s\n", error);
    return 0;
  }  

  dbi_result_free(result);

  return 0;
}

int
db_nick_set_string(unsigned int id, const char *key, const char *value)
{
  dbi_result result;
  char *escvalue;

  if(dbi_driver_quote_string_copy(Database.driv, value, &escvalue) == 0)
  {
    printf("db: Failed to query: dbi_driver_quote_string_copy\n");
    return -1;
  }
  
  snprintf(querybuffer, 1024, "UPDATE %s SET %s=%s WHERE id=%d", 
      "nickname", key, escvalue, id);

  printf("db: query: %s\n", querybuffer);

  MyFree(escvalue);

  if((result = dbi_conn_query(Database.conn, querybuffer)) == NULL)
  {
    const char *error;
    dbi_conn_error(Database.conn, &error);
    printf("db: Failed to query: %s\n", error);
    return -1;
  }

  dbi_result_free(result);

  return 0;
}

int
db_nick_set_number(unsigned int id, const char *key, const unsigned long value)
{
  dbi_result result;

  snprintf(querybuffer, 1024, "UPDATE %s SET %s=%ld WHERE id=%d", 
      "nickname", key, value, id);

  printf("db: query: %s\n", querybuffer);

  if((result = dbi_conn_query(Database.conn, querybuffer)) == NULL)
  {
    const char *error;
    dbi_conn_error(Database.conn, &error);
    printf("db: Failed to query: %s\n", error);
    return -1;
  }

  dbi_result_free(result);

  return 0;
}

char *
db_nick_get_string(unsigned int id, const char *key)
{
  dbi_result result;
  char *esckey;
  char *value;

  assert(key != NULL);
  
  if(dbi_driver_quote_string_copy(Database.driv, key, &esckey) == 0)
  {
    printf("db: Failed to query: dbi_driver_quote_string_copy\n");
    return NULL;
  }
   
  snprintf(querybuffer, 1024, "SELECT %s FROM %s WHERE id=%d", 
      esckey, "nickname", id);
  
  MyFree(esckey);
  printf("db: query: %s\n", querybuffer);

  if((result = dbi_conn_query(Database.conn, querybuffer)) == NULL)
  {
    const char *error;
    dbi_conn_error(Database.conn, &error);
    printf("db: Failed to query: %s\n", error);
    return NULL;
  }

  if(dbi_result_get_numrows(result) == 0)
  {
    printf("db: %s not found\n", key);
    return NULL;
  }

  snprintf(querybuffer, 1024, "%s.%%S", key);

  dbi_result_first_row(result);
  dbi_result_get_fields(result, querybuffer, &value);

  return value;
}

struct RegChannel *
db_find_chan(const char *channel)
{
  dbi_result result;
  char *escchannel = NULL;
  char *findchannel; char *findfounder;
  struct RegChannel *channel_p;
  
  assert(channel != NULL);

  if(dbi_driver_quote_string_copy(Database.driv, channel, &escchannel) == 0)
  {
    printf("db: Failed to query: dbi_driver_quote_string_copy\n");
    return NULL;
  }
  
  snprintf(querybuffer, 1024, "SELECT %s.id, channel, nickname.nick "
      "FROM %s INNER JOIN %s ON %s.founder=%s.id WHERE channel=%s", "channel",
	  "channel", "nickname", "channel", "nickname", escchannel);

  MyFree(escchannel);
  printf("db: query: %s\n", querybuffer);

  if((result = dbi_conn_query(Database.conn, querybuffer)) == NULL)
  {
    const char *error;
    dbi_conn_error(Database.conn, &error);
    printf("db: Failed to query: %s\n", error);
    return NULL;
  }

  if(dbi_result_get_numrows(result) == 0)
  {
    printf("db: Channel %s not found\n", channel);
    return NULL;
  }

  channel_p = MyMalloc(sizeof(struct RegChannel));
  dbi_result_first_row(result);
  dbi_result_get_fields(result, "id.%ui channel.%S nick.%S",
      &channel_p->id, &findchannel, &findfounder);

  strlcpy(channel_p->channel, findchannel, sizeof(channel_p->channel));
  strlcpy(channel_p->founder, findfounder, sizeof(channel_p->founder));

  MyFree(findchannel);
  MyFree(findfounder);

  return channel_p;
}

int
db_register_chan(struct Client *client, char *channelname)
{
  struct RegChannel *channel;
  char *escchannel = NULL;
  char *escfounder = NULL;
  dbi_result result;
  
  assert(channel != NULL);

  if(dbi_driver_quote_string_copy(Database.driv, channelname, &escchannel) == 0)
  {
    printf("db: Failed to query: dbi_driver_quote_string_copy\n");
    MyFree(escchannel);
    return -1;
  }

  if(dbi_driver_quote_string_copy(Database.driv, client->name, &escfounder) == 0)
  {
    printf("db: Failed to query: dbi_driver_quote_string_copy\n");
    MyFree(escchannel);
    MyFree(escfounder);
    return -1;
  }
  
  snprintf(querybuffer, 1024, "INSERT INTO %s (channel, founder)"
      "VALUES(%s, (SELECT id FROM nickname WHERE nick=%s))", 
	  "channel", escchannel, escfounder);

  MyFree(escchannel);
  MyFree(escfounder);
  
  printf("db: query: %s\n", querybuffer);

  if((result = dbi_conn_query(Database.conn, querybuffer)) == NULL)
  {
    const char *error;
    dbi_conn_error(Database.conn, &error);
    printf("db: Failed to query: %s\n", error);
    return -1;
  }  

  dbi_result_free(result);

  return 0;
}

int
db_list_add(const char *table, unsigned int id, const char *value)
{
  char *escvalue;
  dbi_result result;
  
  if(dbi_driver_quote_string_copy(Database.driv, value, &escvalue) == 0)
  {
    printf("db: Failed to query: dbi_driver_quote_string_copy\n");
    return -1;
  }

  snprintf(querybuffer, 1024, "INSERT INTO %s (parent_id, entry)"
      " VALUES(%d, %s)", table, id, escvalue);

  MyFree(escvalue);

  printf("db: query: %s\n", querybuffer);
  
  if((result = dbi_conn_query(Database.conn, querybuffer)) == NULL)
  {
    const char *error;
    dbi_conn_error(Database.conn, &error);
    printf("db: Failed to query: %s\n", error);
    return -1;
  }  

  dbi_result_free(result);
  
  return 0;
}

void *
db_list_first(const char *table, unsigned int id, struct NickAccess *entry)
{
  dbi_result result;
    
  snprintf(querybuffer, 1024, "SELECT id, entry FROM %s WHERE parent_id=%d", 
      "nickname_access", id);
  
  printf("db: query: %s\n", querybuffer);

  if((result = dbi_conn_query(Database.conn, querybuffer)) == NULL)
  {
    const char *error;
    dbi_conn_error(Database.conn, &error);
    printf("db: Failed to query: %s\n", error);
    return NULL;
  }

  if(dbi_result_get_numrows(result) == 0)
  {
    printf("db: %d has no access list\n", id);
    return NULL;
  }

  if(dbi_result_first_row(result))
  {
    dbi_result_get_fields(result, "id.%ui entry.%S", &entry->id, &entry->value);
    return result;
  }

  return NULL;
}

void *
db_list_next(void *result, struct NickAccess *entry)
{
  if(dbi_result_next_row(result))
  {
    dbi_result_get_fields(result, "id.%ui entry.%S", &entry->id, &entry->value);
    return result;
  }
  return NULL;
}

void
db_list_done(void *result)
{
  dbi_result_free(result);
}
