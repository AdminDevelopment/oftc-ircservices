/*
 *  parse.c: The message parser.
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
 *  $Id: parse.c 534 2006-03-21 19:06:29Z michael $
 */

#include "stdinc.h"

/*
 * (based on orabidoo's parser code)
 *
 * This has always just been a trie. Look at volume III of Knuth ACP
 *
 *
 * ok, you start out with an array of pointers, each one corresponds
 * to a letter at the current position in the command being examined.
 *
 * so roughly you have this for matching 'trie' or 'tie'
 *
 * 't' points -> [MessageTree *] 'r' -> [MessageTree *] -> 'i'
 *   -> [MessageTree *] -> [MessageTree *] -> 'e' and matches
 *
 *                               'i' -> [MessageTree *] -> 'e' and matches
 *
 * BUGS (Limitations!)
 * 
 * I designed this trie to parse ircd commands. Hence it currently
 * casefolds. This is trivial to fix by increasing MAXPTRLEN.
 * This trie also "folds" '{' etc. down. This means, the input to this
 * trie must be alpha tokens only. This again, is a limitation that
 * can be overcome by increasing MAXPTRLEN to include upper/lower case
 * at the expense of more memory. At the extreme end, you could make
 * MAXPTRLEN 128.
 *
 * This is also not a patricia trie. On short ircd tokens, this is
 * not likely going to matter. 
 *
 * Diane Bruce (Dianora), June 6 2003
 */

#define MAXPTRLEN    32
                                /* Must be a power of 2, and
				 * larger than 26 [a-z]|[A-Z]
				 * its used to allocate the set
				 * of pointers at each node of the tree
				 * There are MAXPTRLEN pointers at each node.
				 * Obviously, there have to be more pointers
				 * Than ASCII letters. 32 is a nice number
				 * since there is then no need to shift
				 * 'A'/'a' to base 0 index, at the expense
				 * of a few never used pointers. For a small
				 * parser like this, this is a good compromise
				 * and does make it somewhat faster.
				 *
				 * - Dianora
				 */

struct MessageTree
{
  int links; /* Count of all pointers (including msg) at this node 
              * used as reference count for deletion of _this_ node.
              */
  message_t *msg;
  struct MessageTree *pointers[MAXPTRLEN];
};

static struct MessageTree msg_tree;

/*
 * NOTE: parse() should not be called recursively by other functions!
 */
static char *sender;
static char *para[IRCD_MAXPARA + 1];
static char buffer[1024];

static void handle_command(message_t *, client_t *, client_t *, unsigned int, char **);
static void recurse_report_messages(client_t *source_p, struct MessageTree *mtree);
static void add_msg_element(struct MessageTree *mtree_p, message_t *msg_p, const char *cmd);
static void del_msg_element(struct MessageTree *mtree_p, const char *cmd);

/* turn a string into a parc/parv pair */
static inline int
string_to_array(char *string, char *parv[])
{
  char *p;
  char *buf = string;
  int x = 1;

  parv[x] = NULL;

  while (*buf == ' ') /* skip leading spaces */
    buf++;

  if (*buf == '\0') /* ignore all-space args */
    return(x);

  do
  {
    if (*buf == ':') /* Last parameter */
    {
      buf++;
      parv[x++] = buf;
      parv[x]   = NULL;
      return(x);
    }
    else
    {
      parv[x++] = buf;
      parv[x]   = NULL;

      if ((p = strchr(buf, ' ')) != NULL)
      {
        *p++ = '\0';
        buf  = p;
      }
      else
        return(x);
    }       

    while (*buf == ' ')
      buf++;

    if (*buf == '\0')
      return(x);
  } while (x < IRCD_MAXPARA - 1);

  if (*p == ':')
    p++;

  parv[x++] = p;
  parv[x]   = NULL;
  return(x);
}

/*
 * parse a buffer.
 *
 * NOTE: parse() should not be called recusively by any other functions!
 */
void
parse(client_t *client, char *pbuffer, char *bufend)
{
  client_t *from = client;
  char *ch;
  char *s;
  char *numeric = 0;
  unsigned int i = 0;
  int paramcount;
  int mpara = 0;
  message_t *mptr = NULL;

  if (IsDefunct(client->server))
    return;

  assert(client->server->fd.flags.open);
  assert((bufend - pbuffer) < 512);

  for (ch = pbuffer; *ch == ' '; ch++) /* skip spaces */
    /* null statement */ ;

  para[0] = from->name;

  if (*ch == ':')
  {
    ch++;

    /* Copy the prefix to 'sender' assuming it terminates
     * with SPACE (or NULL, which is an error, though).
     */
    sender = ch;

    if ((s = strchr(ch, ' ')) != NULL)
    {
      *s = '\0';
      s++;
      ch = s;
    }

    if (*sender)
    {
      /*
       * XXX it could be useful to know which of these occurs most frequently.
       * the ID check should always come first, though, since it is so easy.
       */
      if ((from = find_person(client, sender)) == NULL)
      {
        from = find_server(sender);
      }
      
      /*
       * Hmm! If the client corresponding to the
       * prefix is not found--what is the correct
       * action??? Now, I will ignore the message
       * (old IRC just let it through as if the
       * prefix just wasn't there...) --msa
       */
      if (from == NULL)
      {
 //       return;
      }

      para[0] = from->name;

      if (from->from != client)
      {
   //     return;
      }
    }

    while (*ch == ' ')
      ++ch;
  }

  if (*ch == '\0')
  {
    return;
  }

  /* Extract the command code from the packet.  Point s to the end
   * of the command code and calculate the length using pointer
   * arithmetic.  Note: only need length for numerics and *all*
   * numerics must have parameters and thus a space after the command
   * code. -avalon
   */

  /* EOB is 3 chars long but is not a numeric */
  if (*(ch + 3) == ' ' && /* ok, lets see if its a possible numeric.. */
      IsDigit(*ch) && IsDigit(*(ch + 1)) && IsDigit(*(ch + 2)))
  {
    mptr = NULL;
    numeric = ch;
    paramcount = IRCD_MAXPARA;
    s = ch + 3;  /* I know this is ' ' from above if            */
    *s++ = '\0'; /* blow away the ' ', and point s to next part */
  }
  else
  { 
    int ii = 0;

    if ((s = strchr(ch, ' ')) != NULL)
      *s++ = '\0';

    printf("Message: %s %s\n", ch, s);
    if ((mptr = find_command(ch)) == NULL)
    {
      /* Note: Give error message *only* to recognized
       * persons. It's a nightmare situation to have
       * two programs sending "Unknown command"'s or
       * equivalent to each other at full blast....
       * If it has got to person state, it at least
       * seems to be well behaving. Perhaps this message
       * should never be generated, though...  --msa
       * Hm, when is the buffer empty -- if a command
       * code has been found ?? -Armin
       */

      return;
    }

    assert(mptr->cmd != NULL);

    paramcount = mptr->parameters;
    mpara      = mptr->maxpara;

    ii = bufend - ((s) ? s : ch);
    mptr->bytes += ii;
  }

  if (s != NULL)
    i = string_to_array(s, para);
  else
  {
    i = 0;
    para[1] = NULL;
  }

  if (mptr != NULL)
    handle_command(mptr, client, from, i, para);
}
/* handle_command()
 *
 * inputs	- pointer to message block
 *		- pointer to client
 *		- pointer to client message is from
 *		- count of number of args
 *		- pointer to argv[] array
 * output	- -1 if error from server
 * side effects	-
 */
static void
handle_command(message_t *mptr, client_t *client,
               client_t *from, unsigned int i, char *hpara[])
{
  MessageHandler handler = 0;

  if (IsServer(client))
    mptr->rcount++;

  mptr->count++;

  handler = mptr->handlers[client->handler];

  /* check right amount of params is passed... --is */
  if (i < mptr->parameters)
  {
    printf("Dropping server %s due to (invalid) command '%s' "
        "with only %d arguments (expecting %d).",
        client->name, mptr->cmd, i, mptr->parameters);
   // exit_client(client, client,
     //   "Not enough arguments to server command.");
  }
  else
    (*handler)(client, from, i, hpara);
}

/* clear_tree_parse()
 *
 * inputs       - NONE
 * output       - NONE
 * side effects - MUST MUST be called at startup ONCE before
 *                any other keyword routine is used.
 */
void
clear_tree_parse(void)
{
  memset(&msg_tree, 0, sizeof(msg_tree));
}

/* add_msg_element()
 *
 * inputs	- pointer to MessageTree
 *		- pointer to Message to add for given command
 *		- pointer to current portion of command being added
 * output	- NONE
 * side effects	- recursively build the Message Tree ;-)
 */
/*
 * How this works.
 *
 * The code first checks to see if its reached the end of the command
 * If so, that message_tTree has a msg pointer updated and the links
 * count incremented, since a msg pointer is a reference.
 * Then the code descends recursively, building the trie.
 * If a pointer index inside the message_tTree is NULL a new
 * child message_tTree has to be allocated.
 * The links (reference count) is incremented as they are created
 * in the parent.
 */
static void
add_msg_element(struct MessageTree *mtree_p, 
		message_t *msg_p, const char *cmd)
{
  struct MessageTree *ntree_p;

  if (*cmd == '\0')
  {
    mtree_p->msg = msg_p;
    mtree_p->links++;    /* Have msg pointer, so up ref count */
  }
  else
  {
    /* *cmd & (MAXPTRLEN-1) 
     * convert the char pointed to at *cmd from ASCII to an integer
     * between 0 and MAXPTRLEN.
     * Thus 'A' -> 0x1 'B' -> 0x2 'c' -> 0x3 etc.
     */

    if ((ntree_p = mtree_p->pointers[*cmd & (MAXPTRLEN-1)]) == NULL)
    {
      ntree_p = (struct MessageTree *)MyMalloc(sizeof(struct MessageTree));
      mtree_p->pointers[*cmd & (MAXPTRLEN-1)] = ntree_p;

      mtree_p->links++;    /* Have new pointer, so up ref count */
    }

    add_msg_element(ntree_p, msg_p, cmd+1);
  }
}

/* del_msg_element()
 *
 * inputs	- Pointer to MessageTree to delete from
 *		- pointer to command name to delete
 * output	- NONE
 * side effects	- recursively deletes a token from the Message Tree ;-)
 */
/*
 * How this works.
 *
 * Well, first off, the code recursively descends into the trie
 * until it finds the terminating letter of the command being removed.
 * Once it has done that, it marks the msg pointer as NULL then
 * reduces the reference count on that allocated message_tTree
 * since a command counts as a reference.
 *
 * Then it pops up the recurse stack. As it comes back up the recurse
 * The code checks to see if the child now has no pointers or msg
 * i.e. the links count has gone to zero. If its no longer used, the
 * child message_tTree can be deleted. The parent reference
 * to this child is then removed and the parents link count goes down.
 * Thus, we continue to go back up removing all unused MessageTree(s)
 */
static void
del_msg_element(struct MessageTree *mtree_p, const char *cmd)
{
  struct MessageTree *ntree_p;

  /*
   * In case this is called for a nonexistent command
   * check that there is a msg pointer here, else links-- goes -ve
   * -db
   */
  if ((*cmd == '\0') && (mtree_p->msg != NULL))
  {
    mtree_p->msg = NULL;
    mtree_p->links--;
  }
  else
  {
    if ((ntree_p = mtree_p->pointers[*cmd & (MAXPTRLEN-1)]) != NULL)
    {
      del_msg_element(ntree_p, cmd+1);

      if (ntree_p->links == 0)
      {
        mtree_p->pointers[*cmd & (MAXPTRLEN-1)] = NULL;
        mtree_p->links--;
        MyFree(ntree_p);
      }
    }
  }
}

/* msg_tree_parse()
 *
 * inputs	- Pointer to command to find
 *		- Pointer to MessageTree root
 * output	- Find given command returning Message * if found NULL if not
 * side effects	- none
 */
static message_t *
msg_tree_parse(const char *cmd, struct MessageTree *root)
{
  struct MessageTree *mtree = root;
  assert(cmd && *cmd);

  while (IsAlpha(*cmd) && (mtree = mtree->pointers[*cmd & (MAXPTRLEN - 1)]))
    if (*++cmd == '\0')
      return mtree->msg;

  return NULL;
}

/* mod_add_cmd()
 *
 * inputs	- pointer to message_t
 * output	- none
 * side effects - load this one command name
 *		  msg->count msg->bytes is modified in place, in
 *		  modules address space. Might not want to do that...
 */
void
mod_add_cmd(message_t *msg)
{
  message_t *found_msg = NULL;

  if (msg == NULL)
    return;

  /* someone loaded a module with a bad messagetab */
  assert(msg->cmd != NULL);

  /* command already added? */
  if ((found_msg = msg_tree_parse(msg->cmd, &msg_tree)) != NULL)
    return;

  add_msg_element(&msg_tree, msg, msg->cmd);
  msg->count = msg->rcount = msg->bytes = 0;
}

/* mod_del_cmd()
 *
 * inputs	- pointer to message_t
 * output	- none
 * side effects - unload this one command name
 */
void
mod_del_cmd(message_t *msg)
{
  assert(msg != NULL);

  if (msg == NULL)
    return;

  del_msg_element(&msg_tree, msg->cmd);
}

/* find_command()
 *
 * inputs	- command name
 * output	- pointer to message_t
 * side effects - none
 */
message_t *
find_command(const char *cmd)
{
  return msg_tree_parse(cmd, &msg_tree);
}
#if 0
/* report_messages()
 *
 * inputs	- pointer to client to report to
 * output	- NONE
 * side effects	- client is shown list of commands
 */
void
report_messages(client_t *source_p)
{
  message_tTree *mtree = &msg_tree;
  int i;

  for (i = 0; i < MAXPTRLEN; i++)
  {
    if (mtree->pointers[i] != NULL)
      recurse_report_messages(source_p, mtree->pointers[i]);
  }
}

static void
recurse_report_messages(client_t *source_p, message_tTree *mtree)
{
  int i;

  if (mtree->msg != NULL)
  {
    sendto_one(source_p, form_str(RPL_STATSCOMMANDS),
               me.name, source_p->name, mtree->msg->cmd,
               mtree->msg->count, mtree->msg->bytes,
               mtree->msg->rcount);
  }

  for (i = 0; i < MAXPTRLEN; i++)
  {
    if (mtree->pointers[i] != NULL)
      recurse_report_messages(source_p, mtree->pointers[i]);
  }
}
#endif

void
m_ignore(client_t *client, client_t *source_p,
         int parc, char *parv[])
{
  return;
}
