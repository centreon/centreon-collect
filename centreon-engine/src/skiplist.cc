/*
** Copyright 2008      Ethan Galstad
** Copyright 2011-2013 Merethis
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <cstdlib>
#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/skiplist.hh"

skiplist* skiplist_new(
            int max_levels,
            float level_probability,
            int allow_duplicates,
            int append_duplicates,
            int (*compare_function)(void const*, void const*)) {
  skiplist* newlist = NULL;

  /* alloc memory for new list structure */
  newlist = new skiplist;

  /* initialize levels, etc. */
  newlist->current_level = 0;
  newlist->max_levels = max_levels;
  newlist->level_probability = level_probability;
  newlist->allow_duplicates = allow_duplicates;
  newlist->append_duplicates = append_duplicates;
  newlist->items = 0;
  newlist->compare_function = compare_function;

  /* initialize head node */
  newlist->head = skiplist_new_node(newlist, max_levels);
  return (newlist);
}

int skiplist_insert(skiplist* list, void* data) {
  skiplistnode** update = NULL;
  skiplistnode* thisnode = NULL;
  skiplistnode* nextnode = NULL;
  skiplistnode* newnode = NULL;
  int level = 0;
  int x = 0;

  if (list == NULL || data == NULL)
    return (SKIPLIST_ERROR_ARGS);

  /* initialize update vector */
  update = new skiplistnode*[list->max_levels];

  for (x = 0; x < list->max_levels; x++)
    update[x] = NULL;

  /* check to make sure we don't have duplicates */
  /* NOTE: this could made be more efficient */
  if (list->allow_duplicates == FALSE) {
    if (skiplist_find_first(list, data, NULL))
      return (SKIPLIST_ERROR_DUPLICATE);
  }

  /* find proper position for insert, remember pointers  with an update vector */
  thisnode = list->head;
  for (level = list->current_level; level >= 0; level--) {
    while ((nextnode = thisnode->forward[level])) {
      if (list->append_duplicates == TRUE) {
        if (list->compare_function(nextnode->data, data) > 0)
          break;
      }
      else {
        if (list->compare_function(nextnode->data, data) >= 0)
          break;
      }
      thisnode = nextnode;
    }
    update[level] = thisnode;
  }

  /* get a random level the new node should be inserted at */
  level = skiplist_random_level(list);
  /*printf("INSERTION LEVEL: %d\n",level); */

  /* we're adding a new level... */
  if (level > list->current_level) {
    /*printf("NEW LEVEL!\n"); */
    list->current_level++;
    level = list->current_level;
    update[level] = list->head;
  }

  /* create a new node */
  if ((newnode = skiplist_new_node(list, level)) == NULL) {
    /*printf("NODE ERROR\n"); */
    delete[] update;
    return (SKIPLIST_ERROR_MEMORY);
  }
  newnode->data = data;

  /* update pointers to insert node at proper location */
  do {
    thisnode = update[level];
    newnode->forward[level] = thisnode->forward[level];
    thisnode->forward[level] = newnode;

  } while (--level >= 0);

  /* update counters */
  list->items++;

  /* free memory */
  delete[] update;

  return (SKIPLIST_OK);
}

skiplistnode* skiplist_new_node(skiplist* list, int node_levels) {
  skiplistnode* newnode = NULL;
  int x = 0;

  if (list == NULL)
    return (NULL);

  if (node_levels < 0 || node_levels > list->max_levels)
    return (NULL);

  /* allocate memory for node + variable number of level pointers */
  newnode = new skiplistnode[node_levels + 1];

  /* initialize forward pointers */
  for (x = 0; x < node_levels; x++)
    newnode->forward[x] = NULL;

  /* initialize data pointer */
  newnode->data = NULL;
  return (newnode);
}

int skiplist_random_level(skiplist* list) {
  int level = 0;
  float r = 0.0;

  if (list == NULL)
    return (-1);

  for (level = 0; level < list->max_levels; level++) {
    r = ((float)rand() / (float)RAND_MAX);
    if (r > list->level_probability)
      break;
  }

  return ((level >= list->max_levels) ? list->max_levels - 1 : level);
}

int skiplist_empty(skiplist* list) {
  skiplistnode* self = NULL;
  skiplistnode* next = NULL;
  int level = 0;

  if (list == NULL)
    return (ERROR);

  /* free all list nodes (but not header) */
  for (self = list->head->forward[0]; self != NULL; self = next) {
    next = self->forward[0];
    delete self;
  }

  /* reset level pointers */
  for (level = list->current_level; level >= 0; level--)
    list->head->forward[level] = NULL;

  /* reset list level */
  list->current_level = 0;

  /* reset items */
  list->items = 0;

  return (OK);
}

int skiplist_free(skiplist** list) {
  skiplistnode* self = NULL;
  skiplistnode* next = NULL;

  if (list == NULL)
    return (ERROR);
  if (*list == NULL)
    return (OK);

  /* free header and all list nodes */
  for (self = (*list)->head; self != NULL; self = next) {
    next = self->forward[0];
    delete[] self;
  }

  /* free list structure */
  delete *list;
  *list = NULL;

  return (OK);
}

/* get first item in list */
void* skiplist_peek(skiplist* list) {
  if (list == NULL)
    return (NULL);
  /* return first item */
  return (list->head->forward[0]->data);
}

/* get/remove first item in list */
void* skiplist_pop(skiplist* list) {
  skiplistnode* thisnode = NULL;
  void* data = NULL;
  int level = 0;

  if (list == NULL)
    return (NULL);

  /* get first item */
  thisnode = list->head->forward[0];
  if (thisnode == NULL)
    return (NULL);

  /* get data for first item */
  data = thisnode->data;

  /* remove first item from queue - update forward links from head to first node */
  for (level = 0; level <= list->current_level; level++) {
    if (list->head->forward[level] == thisnode)
      list->head->forward[level] = thisnode->forward[level];
  }

  /* free deleted node */
  delete thisnode;

  /* adjust items */
  list->items--;
  return (data);
}

/* get first item in list */
void* skiplist_get_first(skiplist* list, void** node_ptr) {
  skiplistnode* thisnode = NULL;

  if (list == NULL)
    return (NULL);

  /* get first node */
  thisnode = list->head->forward[0];

  /* return pointer to node */
  if (node_ptr)
    *node_ptr = (void*)thisnode;

  if (thisnode)
    return (thisnode->data);
  return (NULL);
}

/* get next item in list */
void* skiplist_get_next(void** node_ptr) {
  skiplistnode* thisnode = NULL;
  skiplistnode* nextnode = NULL;

  if (node_ptr == NULL || *node_ptr == NULL)
    return (NULL);

  thisnode = (skiplistnode*)(*node_ptr);
  nextnode = thisnode->forward[0];

  *node_ptr = (void*)nextnode;

  if (nextnode)
    return (nextnode->data);
  return (NULL);
}

/* first first item in list */
void* skiplist_find_first(skiplist* list, void* data, void** node_ptr) {
  skiplistnode* thisnode = NULL;
  skiplistnode* nextnode = NULL;
  int level = 0;

  if (list == NULL || data == NULL)
    return (NULL);

  thisnode = list->head;
  for (level = list->current_level; level >= 0; level--) {
    while ((nextnode = thisnode->forward[level])) {
      if (list->compare_function(nextnode->data, data) >= 0)
        break;
      thisnode = nextnode;
    }
  }

  /* we found it! */
  if (nextnode && list->compare_function(nextnode->data, data) == 0) {
    if (node_ptr)
      *node_ptr = (void*)nextnode;
    return (nextnode->data);
  }

  if (node_ptr)
    *node_ptr = NULL;
  return (NULL);
}

/* find next match */
void* skiplist_find_next(skiplist* list, void* data, void** node_ptr) {
  skiplistnode* thisnode = NULL;
  skiplistnode* nextnode = NULL;

  if (list == NULL || data == NULL || node_ptr == NULL)
    return (NULL);
  if (*node_ptr == NULL)
    return (NULL);

  thisnode = (skiplistnode*)(*node_ptr);
  nextnode = thisnode->forward[0];

  if (nextnode) {
    if (list->compare_function(nextnode->data, data) == 0) {
      *node_ptr = (void*)nextnode;
      return (nextnode->data);
    }
  }
  *node_ptr = NULL;
  return (NULL);
}

/* delete (all) matching item(s) from list */
int skiplist_delete(skiplist* list, void const* data) {
  return (skiplist_delete_all(list, data));
}

/* delete first matching item from list */
int skiplist_delete_first(skiplist* list, void const* data) {
  skiplistnode** update = NULL;
  skiplistnode* thisnode = NULL;
  skiplistnode* nextnode = NULL;
  int level = 0;
  int top_level = 0;
  int deleted = FALSE;
  int x = 0;

  if (list == NULL || data == NULL)
    return (ERROR);

  /* initialize update vector */
  update = new skiplistnode*[list->max_levels];

  for (x = 0; x < list->max_levels; x++)
    update[x] = NULL;

  /* find location in list */
  thisnode = list->head;
  for (top_level = level = list->current_level; level >= 0; level--) {
    while ((nextnode = thisnode->forward[level])) {
      if (list->compare_function(nextnode->data, data) >= 0)
        break;
      thisnode = nextnode;
    }
    update[level] = thisnode;
  }

  /* we found a match! */
  if (nextnode != NULL
      && list->compare_function(nextnode->data, data) == 0) {

    /* adjust level pointers to bypass (soon to be) removed node */
    for (level = 0; level <= top_level; level++) {

      thisnode = update[level];
      if (thisnode->forward[level] != nextnode)
        break;

      thisnode->forward[level] = nextnode->forward[level];
    }

    /* free node memory */
    delete[] nextnode;

    /* adjust top/current level of list is necessary */
    while (list->head->forward[top_level] == NULL && top_level > 0)
      top_level--;
    list->current_level = top_level;

    /* adjust items */
    list->items--;

    deleted = TRUE;
  }

  /* free memory */
  delete[] update;

  return (deleted);
}

/* delete all matching items from list */
int skiplist_delete_all(skiplist* list, void const* data) {
  int total_deleted(0);

  /* NOTE: there is a more efficient way to do this... */
  while (skiplist_delete_first(list, data) == 1)
    ++total_deleted;
  return (total_deleted);
}

/* delete specific node from list */
int skiplist_delete_node(skiplist* list, void const* node_ptr) {
  void* data = NULL;
  skiplistnode** update = NULL;
  skiplistnode* thenode = NULL;
  skiplistnode* thisnode = NULL;
  skiplistnode* nextnode = NULL;
  int level = 0;
  int top_level = 0;
  int deleted = FALSE;
  int x = 0;

  if (list == NULL || node_ptr == NULL)
    return (ERROR);

  /* we'll need the data from the node to first find the node */
  thenode = (skiplistnode*) node_ptr;
  data = thenode->data;

  /* initialize update vector */
  update = new skiplistnode*[list->max_levels];

  for (x = 0; x < list->max_levels; x++)
    update[x] = NULL;

  /* find location in list */
  thisnode = list->head;
  for (top_level = level = list->current_level; level >= 0; level--) {
    while ((nextnode = thisnode->forward[level])) {
      /* next node would be too far */
      if (list->compare_function(nextnode->data, data) > 0)
        break;
      /* this is the exact node we want */
      if (list->compare_function(nextnode->data, data) == 0
          && nextnode == thenode)
        break;

      thisnode = nextnode;
    }
    update[level] = thisnode;
  }

  /* we found a match! (value + pointers match) */
  if (nextnode
      && list->compare_function(nextnode->data, data) == 0
      && nextnode == thenode) {

    /* adjust level pointers to bypass (soon to be) removed node */
    for (level = 0; level <= top_level; level++) {
      thisnode = update[level];
      if (thisnode->forward[level] != nextnode)
        break;

      thisnode->forward[level] = nextnode->forward[level];
    }

    /* free node memory */
    delete nextnode;

    /* adjust top/current level of list is necessary */
    while (list->head->forward[top_level] == NULL && top_level > 0)
      top_level--;
    list->current_level = top_level;

    /* adjust items */
    list->items--;

    deleted = TRUE;
  }

  /* free memory */
  delete[] update;
  return (deleted);
}
