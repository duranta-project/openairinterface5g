/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*****************************************************************************
Source      nas_timer.c

Version     0.1

Date        2012/10/09

Product     NAS stack

Subsystem   Utilities

Description Timer utilities

*****************************************************************************/

#include <pthread.h>
#include <assert.h>
#include <stdint.h>

#include <string.h> // memset
#include <stdlib.h> // malloc, free
#include <stdbool.h>
#include <stdio.h>
#include <sys/time.h>   // setitimer
#include "intertask_interface.h"
#include "nas_timer.h"
#include "commonDef.h"

/****************************************************************************/
/****************  E X T E R N A L    D E F I N I T I O N S  ****************/
/****************************************************************************/

/****************************************************************************/
/*******************  L O C A L    D E F I N I T I O N S  *******************/
/****************************************************************************/

/* Structure of an interval timer entry
 * ------------------------------------
 * The callback function is scheduled to be executed upon expiration of
 * the timer that has been previously setup to the initial interval time
 * value when the timer entry was allocated.
 */
typedef struct {
long timer_id;          /* Timer id returned by the timer API from ITTI */
  int id;					  /* Back-reference to NAS-level id */

  struct timeval itv;     /* Initial interval timer value         */
  struct timeval tv;      /* Interval timer value                 */

  nas_timer_callback_t cb;    /* Callback executed at timer expiration */
  void *args;                 /* Callback argument parameters          */
} nas_timer_entry_t;

/* Structure of a timer queue - list of active interval timer entries
 * ------------------------------------------------------------------
 * At any time, the first entry of the queue is always associated to the
 * first timer that will come to expire. Upon timer expiration, the first
 * entry is removed from the queue and freed.
 */
typedef struct _nas_timer_queue_t {
  int id;         /* Identifier of the current timer entry */
  nas_timer_entry_t *entry;   /* The current timer entry       */
  struct _nas_timer_queue_t *prev;/* The previous timer entry in the queue */
  struct _nas_timer_queue_t *next;/* The next timer entry in the queue     */
} timer_queue_t;

/* Structure of a timer database
 * -----------------------------
 * The timer database is managed to provide unique identifier to timer at
 * startup and to maintain an ordered queue of active timer entries.
 * Note: Index 0 is reserved. Valid timer IDs are strictly 1 .. (TIMER_DATABASE_SIZE - 1)
 */
typedef struct {
  int timer_id;   /* Identifier of the first available timer entry */
#define TIMER_DATABASE_SIZE 256
  timer_queue_t tq[TIMER_DATABASE_SIZE];
  timer_queue_t *head;/* Pointer to the first timer entry to be fired  */
} nas_timer_database_t;

/*
 * The timer database
 * The timer database and its synchronization mutex
 */
static nas_timer_database_t _nas_timer_db = {
  0,
  1,
  {},
  NULL
};

/*
 * The handler executed whenever the system timer expires
 */
static pthread_mutex_t _nas_timer_db_mutex = PTHREAD_MUTEX_INITIALIZER;
#define nas_timer_lock_db()   pthread_mutex_lock(&_nas_timer_db_mutex)  
#define nas_timer_unlock_db() pthread_mutex_unlock(&_nas_timer_db_mutex

/*
 * The handler executed whenever the system timer expires
 */


/*
 * -----------------------------------------------------------------------------
 *      Functions used to manage the timer database
 *      Internal database management functions (Caller must hold lock)
 * -----------------------------------------------------------------------------
 */
static void _nas_timer_db_init_locked(void);
static int _nas_timer_db_get_id_locked(void);
static bool _nas_timer_db_is_active_locked(int id);
static nas_timer_entry_t *_nas_timer_db_create_entry(long sec,
    nas_timer_callback_t cb, void *args);
static void _nas_timer_db_delete_entry_locked(int id);

static void _nas_timer_db_insert_entry_locked(int id, nas_timer_entry_t *te);
static int _nas_timer_db_insert_locked(timer_queue_t *entry);

static nas_timer_entry_t *_nas_timer_db_remove_entry_locked(int id);
static bool _nas_timer_db_remove_locked(timer_queue_t *entry);

/*
 * -----------------------------------------------------------------------------
 *      Operator functions for timeval structures
 * -----------------------------------------------------------------------------
 */
static int _nas_timer_cmp(const struct timeval *a, const struct timeval *b);
static void _nas_timer_add(const struct timeval *a, const struct timeval *b,
                           struct timeval *result);
static int _nas_timer_sub(const struct timeval *a, const struct timeval *b,
                          struct timeval *result);

/****************************************************************************/
/******************  E X P O R T E D    F U N C T I O N S  ******************/
/****************************************************************************/

/****************************************************************************
 **                                                                        **
 ** Name:    timer_init()                                              **
 **                                                                        **
 ** Description: Initializes internal data used to manage timers           **
 **                                                                        **
 ** Inputs:  None                                                      **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    _nas_timer_db                              **
 **                                                                        **
 ***************************************************************************/
int nas_timer_init(void)
{
  /* Initialize the timer database */
  nas_timer_lock_db();
  _nas_timer_db_init_locked();
  nas_timer_unlock_db();

  return (RETURNok);
}

/****************************************************************************
 **                                                                        **
 ** Name:    timer_start()                                             **
 **                                                                        **
 ** Description: Schedules the execution of the given callback function    **
 **      upon expiration of the specified time interval            **
 **                                                                        **
 ** Inputs:  sec:       The value of the time interval in seconds  **
 **      cb:        Function executed upon timer expiration    **
 **      args:      Callback argument parameters               **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    The timer identifier when successfully     **
 **             started; NAS_TIMER_INACTIVE_ID otherwise.  **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int nas_timer_start(long sec, nas_timer_callback_t cb, void *args)
{
  int id;
  nas_timer_entry_t *te;
  int ret;
  long timer_id;

  /* Do not start null timer */
  if (sec == 0) {
    return (NAS_TIMER_INACTIVE_ID);
  }

  /* Get an identifier for the new timer entry */
  te = _nas_timer_db_create_entry(sec, cb, args);
  if (te == NULL) {	  
    return (NAS_TIMER_INACTIVE_ID);
  }
  
  nas_timer_lock_db();
  id = _nas_timer_db_get_id_locked();
  if (id < 1) {	  
    nas_timer_unlock_db();
    free(te);
    return (NAS_TIMER_INACTIVE_ID);
  }
  
  te->id = id;
  _nas_timer_db_insert_entry_locked(id, te);
  nas_timer_unlock_db();
  
  ret = timer_setup(sec, 0, TASK_NAS_UE, INSTANCE_DEFAULT, TIMER_PERIODIC, te, &timer_id);
  if (ret == -1) {	  
    nas_timer_lock_db();
    _nas_timer_db_remove_entry_locked(id);
    _nas_timer_db_delete_entry_locked(id);
    nas_timer_unlock_db();
    return NAS_TIMER_INACTIVE_ID;
  }
  
  nas_timer_lock_db();
  if (_nas_timer_db_is_active_locked(id) && _nas_timer_db.tq[id].entry == te) {
    te->timer_id = timer_id;
  }
  nas_timer_unlock_db();
  
  return (id);
}


/****************************************************************************
 **                                                                        **
 ** Name:    nas_timer_start_ext()                                             **
 **                                                                        **
 ** Description: Schedules the execution of the given callback function    **
 **      upon expiration of the specified time interval specific for 5G NR **
 **                                                                        **
 ** Inputs:  sec:       The value of the time interval in seconds  **
 **      cb:        Function executed upon timer expiration    **
 **      args:      Callback argument parameters               **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    The timer identifier when successfully     **
 **             started; NAS_TIMER_INACTIVE_ID otherwise.  **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int nas_timer_start_ext(instance_t ue_instance_id, long sec, nas_timer_callback_t cb, void *args)
{
  int id;
  nas_timer_entry_t *te;
  int ret;
  long timer_id;

  if (sec == 0) {
     return (NAS_TIMER_INACTIVE_ID);
  }

   /* Create a new timer entry */
   te = _nas_timer_db_create_entry(sec, cb, args);
  if (te == NULL) {
    return (NAS_TIMER_INACTIVE_ID);
  }
 
  nas_timer_lock_db();
  id = _nas_timer_db_get_id_locked();
  if (id < 1) {
    nas_timer_unlock_db();
    free(te);
     return (NAS_TIMER_INACTIVE_ID);
   }

  te->id = id;
  _nas_timer_db_insert_entry_locked(id, te);
  nas_timer_unlock_db();

  ret = timer_setup(sec, 0, TASK_NAS_NRUE, ue_instance_id, TIMER_ONE_SHOT, te, &timer_id);
  if (ret == -1) {
    nas_timer_lock_db();
    _nas_timer_db_remove_entry_locked(id);
    _nas_timer_db_delete_entry_locked(id);
    nas_timer_unlock_db();
     return NAS_TIMER_INACTIVE_ID;
   }

  nas_timer_lock_db();
  if (_nas_timer_db_is_active_locked(id) && _nas_timer_db.tq[id].entry == te) {
    te->timer_id = timer_id;
  }
  nas_timer_unlock_db();

  return (id);
}
 
/****************************************************************************
 ** Name:    nas_timer_fire()                                              **
 ***************************************************************************/
void nas_timer_fire(void *timer_arg)  
{  
  nas_timer_entry_t *te = (nas_timer_entry_t *)timer_arg;  
  nas_timer_callback_t cb = NULL;
  void *cb_args = NULL;

  nas_timer_lock_db();  
  if (te != NULL) {
    int id = te->id;
    if (id >= 1 && id < TIMER_DATABASE_SIZE 
        && _nas_timer_db.tq[id].id == id 
        && _nas_timer_db.tq[id].entry == te) {
      
      cb = te->cb;
      cb_args = te->args;
      
      _nas_timer_db_remove_entry_locked(id);
      _nas_timer_db_delete_entry_locked(id);
    }
  }
  nas_timer_unlock_db();  
  
  if (cb != NULL) {  
    cb(cb_args);  
  }
}

/****************************************************************************
 **                                                                        **
 ** Name:    timer_stop()                                              **
 **                                                                        **
 ** Description: Stop the timer with the specified identifier              **
 **                                                                        **
 ** Inputs:  id:        The identifier of the timer to be stopped  **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    NAS_TIMER_INACTIVE_ID when successfully stop-  **
 **             ped; The timer identifier otherwise.       **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int nas_timer_stop(int id)	
{
  long itti_timer_id = -1;
  bool is_active = false;

  /* IDs <= 0 (including 0 and -1) are ignored safely */
  if (id <= 0 || id >= TIMER_DATABASE_SIZE) {
	 return (NAS_TIMER_INACTIVE_ID);
   }
 
  nas_timer_lock_db();
  if (_nas_timer_db_is_active_locked(id)) {
	nas_timer_entry_t *entry = _nas_timer_db_remove_entry_locked(id);
	if (entry != NULL) {
	  itti_timer_id = entry->timer_id;
	}
	_nas_timer_db_delete_entry_locked(id);
	is_active = true;
  }
  nas_timer_unlock_db();

  if (is_active) {
	if (itti_timer_id != -1 && timer_remove(itti_timer_id) != 0) {	
	  LOG_W(NAS, "nas_timer_stop: ITTI timer for NAS id %d (itti timer_id %ld) was already gone\n", id, itti_timer_id);  
	}
  }

  return (NAS_TIMER_INACTIVE_ID);	
}

/****************************************************************************
 **                                                                        **
 ** Name:    timer_restart()                                           **
 **                                                                        **
 ** Description: Restart the timer with the specified identifier. The ti-  **
 **      mer is scheduled to expire after the same period of time  **
 **      and will execute the callback function that has been set  **
 **      when it was started.                                      **
 **                                                                        **
 ** Inputs:  id:        The identifier of the timer to be started  **
 **             again                                      **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    The timer identifier when successfully     **
 **             re-started; NAS_TIMER_INACTIVE_ID otherwise.   **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
int nas_timer_restart(int id)
{
 if (id <= 0 || id >= TIMER_DATABASE_SIZE) {
   return (NAS_TIMER_INACTIVE_ID);
 }

 nas_timer_lock_db();
 if (_nas_timer_db_is_active_locked(id)) {
   nas_timer_entry_t *te = _nas_timer_db_remove_entry_locked(id);
   if (te != NULL) {
	 te->tv = te->itv;
	 _nas_timer_db_insert_entry_locked(id, te);
   }
   nas_timer_unlock_db();
	return (id);
  }
 nas_timer_unlock_db();

  return (NAS_TIMER_INACTIVE_ID);
}


/****************************************************************************
 ** Name:    nas_timer_get_remaining_sec()                                 **
 ***************************************************************************/
long nas_timer_get_remaining_sec(int id)
{
  struct timespec ts;
  struct timeval current_time, remaining;
  long rem_sec = 0;

  if (id <= 0 || id >= TIMER_DATABASE_SIZE) {
    return (NAS_TIMER_INACTIVE_ID);
  }

  clock_gettime(CLOCK_MONOTONIC, &ts);  
  current_time.tv_sec  = ts.tv_sec;  
  current_time.tv_usec = ts.tv_nsec / 1000;

  nas_timer_lock_db();
  if (!_nas_timer_db_is_active_locked(id)) {
    nas_timer_unlock_db();
    return (NAS_TIMER_INACTIVE_ID);
  }  

  nas_timer_entry_t *te = _nas_timer_db.tq[id].entry;  
  if (te == NULL || _nas_timer_sub(&te->tv, &current_time, &remaining) < 0) {
    nas_timer_unlock_db();
    return 0; /* already expired */
  }
  rem_sec = remaining.tv_sec;
  nas_timer_unlock_db();

  return rem_sec;
}

/*
 * -----------------------------------------------------------------------------
 *      Functions used to manage the timer database
 * -----------------------------------------------------------------------------
 */
/****************************************************************************
 **                                                                        **
 ** Name:    _nas_timer_db_init()                                      **
 **                                                                        **
 ** Description: Initializes the timer database                            **
 **                                                                        **
 ** Inputs:  None                                                      **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    None                                       **
 **      Others:    _nas_timer_db                              **
 **                                                                        **
 ***************************************************************************/
static void _nas_timer_db_init_locked(void)
{
   int i;

  _nas_timer_db.timer_id = 1;
  _nas_timer_db.head = NULL;
   for (i = 0; i < TIMER_DATABASE_SIZE; i) {
	 _nas_timer_db.tq[i].id = NAS_TIMER_INACTIVE_ID;
	 _nas_timer_db.tq[i].entry = NULL;
	 _nas_timer_db.tq[i].prev = NULL;
	 _nas_timer_db.tq[i].next = NULL;
   }
}

/****************************************************************************
 **                                                                        **
 ** Name:    _nas_timer_db_get_id_locked()                                    **
 **                                                                        **
 ** Description: Gets the identifier of the first available timer entry in **
 **      the queue of active timer entries                         **
 **                                                                        **
 ** Inputs:  None                                                      **
 **      Others:    _nas_timer_db                              **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    The identifier of the first available      **
 **             timer entry if the queue of active timers  **
 **             is not full; -1 otherwise.                 **
 **      Others:    _nas_timer_db                              **
 **                                                                        **
 ***************************************************************************/
static int _nas_timer_db_get_id_locked(void)
{
   int i;

  if (_nas_timer_db.timer_id < 1 || _nas_timer_db.timer_id >= TIMER_DATABASE_SIZE) {
	_nas_timer_db.timer_id = 1;
  }

  /* Search from current timer entry (>= 1) to the end */
   for (i = _nas_timer_db.timer_id; i < TIMER_DATABASE_SIZE; i++) {
	if (_nas_timer_db.tq[i].id < 0) {
	  _nas_timer_db.timer_id = (i + 1 < TIMER_DATABASE_SIZE) ? i + 1 : 1;
	   return i;
	 }
   }

  /* Wrap around: search from index 1 */
  for (i = 1; i < _nas_timer_db.timer_id; i++) {
	if (_nas_timer_db.tq[i].id < 0) {
	  _nas_timer_db.timer_id = (i + 1 < TIMER_DATABASE_SIZE) ? i + 1 : 1;
	   return i;
	 }
   }

  /* No available timer entry found */
   return (-1);
}


/****************************************************************************
 **                                                                        **
 ** Name:    _nas_timer_db_is_active_locked()                                 **
 **                                                                        **
 ** Description: Checks whether the entry with the given identifier is     **
 **      active within the queue of active timer entries           **
 **                                                                        **
 ** Inputs:  id:        Identifier of the timer entry to check     **
 **      Others:    _nas_timer_db                              **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    true if the timer entry is active; false   **
 **             if it is not an active timer entry.        **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
static bool _nas_timer_db_is_active_locked(int id)
{
  if (id <= 0 || id >= TIMER_DATABASE_SIZE) {
	return false;
  }
  return (_nas_timer_db.tq[id].id == id);
}


/****************************************************************************
 **                                                                        **
 ** Name:    _nas_timer_db_create_entry()                              **
 **                                                                        **
 ** Description: Creates a new timer entry                                 **
 **                                                                        **
 ** Inputs:  sec:       Time interval value                        **
 **      cb:        Function executed upon timer expiration    **
 **      args:      Callback argument parameters               **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    A pointer to the new timer entry if        **
 **             successfully allocated; NULL otherwise     **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
static nas_timer_entry_t *_nas_timer_db_create_entry(
  long sec, nas_timer_callback_t cb, void *args)
{
  nas_timer_entry_t *te = (nas_timer_entry_t *)malloc(sizeof(nas_timer_entry_t));

  if (te != NULL) {
  	te->id = NAS_TIMER_INACTIVE_ID;
    te->timer_id = -1;
    te->itv.tv_sec = sec;
    te->itv.tv_usec = 0;
    te->tv.tv_sec  = te->itv.tv_sec;
    te->tv.tv_usec = te->itv.tv_usec;
    te->cb = cb;
    te->args = args;
  }

  return (te);
}

/****************************************************************************
 **                                                                        **
 ** Name:    _nas_timer_db_delete_entry_locked()                              **
 **                                                                        **
 ** Description: Deletes the entry with the given identifier from the ti-  **
 **      mer database.                                             **
 **                                                                        **
 ** Inputs:  id:        Identifier of the entry to be deleted      **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    None                                       **
 **      Others:    _nas_timer_db                              **
 **                                                                        **
 ***************************************************************************/
static void _nas_timer_db_delete_entry_locked(int id)
{
  if (id <= 0 || id >= TIMER_DATABASE_SIZE) {
	 return;
  }

  /* The identifier of the timer is valid within the timer queue */
  assert(_nas_timer_db.tq[id].id == id);

  /* Delete the timer entry from the queue */
  _nas_timer_db.tq[id].id = NAS_TIMER_INACTIVE_ID;
  if (_nas_timer_db.tq[id].entry != NULL) {
    free(_nas_timer_db.tq[id].entry);
    _nas_timer_db.tq[id].entry = NULL;
  }
  _nas_timer_db.tq[id].prev = NULL;
  _nas_timer_db.tq[id].next = NULL;
}

/****************************************************************************
 **                                                                        **
 ** Name:    _nas_timer_db_insert_entry()                              **
 **                                                                        **
 ** Description: Inserts the entry with the given identifier into the      **
 **      queue of active timer entries and restarts the system     **
 **      timer if the new entry is the next entry for which the    **
 **      timer should be scheduled to expire.                      **
 **                                                                        **
 ** Inputs:  id:        Identifier of the new entry                **
 **      te:        Pointer to the entry to be inserted        **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    None                                       **
 **      Others:    _nas_timer_db                              **
 **                                                                        **
 ***************************************************************************/
static void _nas_timer_db_insert_entry_locked(int id, nas_timer_entry_t *te)
{
   struct timespec	ts;
   struct timeval	current_time;
 
   /* Enqueue the new timer entry */
   _nas_timer_db.tq[id].id = id;
   _nas_timer_db.tq[id].entry = te;

   clock_gettime(CLOCK_MONOTONIC, &ts);
   current_time.tv_sec = ts.tv_sec;

  current_time.tv_usec = ts.tv_nsec / 1000;
   _nas_timer_add(&te->tv, &current_time, &te->tv);
 
  _nas_timer_db_insert_locked(&_nas_timer_db.tq[id]);
}


static int _nas_timer_db_insert_locked(timer_queue_t *entry)
{
  timer_queue_t *prev, *next;
 
  /*
   * Search the list of timer entries for the first entry with an interval
   * timer value greater than the interval timer value of the new timer entry
   */
   for (prev = NULL, next = _nas_timer_db.head; next != NULL; next = next->next) {
     if (_nas_timer_cmp(&next->entry->tv, &entry->entry->tv) > 0) {
       break;
     }

     prev = next;
   }
 
   entry->prev = prev;
   entry->next = next;
 
  /* Update the pointer from the previous entry */
   if (entry->next != NULL) {
     entry->next->prev = entry;
   }
 
  /* Update the pointer from the next entry */
   if (entry->prev != NULL) {
     entry->prev->next = entry;
   } else {
    /* The new entry is the first entry of the list */
     _nas_timer_db.head = entry;
     return true;
   }
 
  /* The new entry is NOT the first entry of the list */
   return false;
}


/****************************************************************************
 **                                                                        **
 ** Name:    _nas_timer_db_remove_entry_locked()                              **
 **                                                                        **
 ** Description: Removes the entry with the given identifier from the      **
 **      queue of active timer entries and restarts the system     **
 **      timer if the entry was the next entry for which the timer **
 **      was scheduled to expire.                                  **
 **                                                                        **
 ** Inputs:  id:        Identifier of the entry to be removed      **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    A pointer to the removed entry             **
 **      Others:    _nas_timer_db                              **
 **                                                                        **
 ***************************************************************************/
static nas_timer_entry_t *_nas_timer_db_remove_entry_locked(int id)
{
  if (id <= 0 || id >= TIMER_DATABASE_SIZE) {
	return NULL;
  }

  _nas_timer_db_remove_locked(&_nas_timer_db.tq[id]);
   return (_nas_timer_db.tq[id].entry);
}

static bool _nas_timer_db_remove_locked(timer_queue_t *entry)
{
  /* Update the pointer from the previous entry */
  /* prev ---> entry ---> next */
  /* prev <--- entry <--- next */
   if (entry->next != NULL) {
    /* prev ---> entry ---> next */
    /* prev <-------------- next */
     entry->next->prev = entry->prev;
   }
 
  /* Update the pointer from the next entry */
   if (entry->prev != NULL) {
    /* prev --------------> next */
    /* prev <-------------- next */
     entry->prev->next = entry->next;
   } else {
    /* The entry was the first entry of the list */
     _nas_timer_db.head = entry->next;

     if (_nas_timer_db.head != NULL) {
      /* Other timers are scheduled to expire */
       return true;
     }
   }
 
  /* The entry was NOT the first entry of the list */
  entry->prev = NULL;
  entry->next = NULL;

   return false;
}


/*
 * -----------------------------------------------------------------------------
 *      Operator functions for timeval structures
 * -----------------------------------------------------------------------------
 */
/****************************************************************************
 **                                                                        **
 ** Name:        _nas_timer_cmp()                                          **
 **                                                                        **
 ** Description: Performs timeval comparaison                              **
 **                                                                        **
 ** Inputs:              a:     The first timeval structure                **
 **                      b:     The second timeval structure               **
 **                  Others:    None                                       **
 **                                                                        **
 ** Outputs:     None                                                      **
 **                  Return:    -1 if a < b; 1 if a > b; 0 if a == b       **
 **                  Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
static int _nas_timer_cmp(const struct timeval *a, const struct timeval *b)
{
  if (a->tv_sec < b->tv_sec) {
    return -1;
  } else if (a->tv_sec > b->tv_sec) {
    return 1;
  } else if (a->tv_usec < b->tv_usec) {
    return -1;
  } else if (a->tv_usec > b->tv_usec) {
    return 1;
  }

  return 0;
}

/****************************************************************************
 **                                                                        **
 ** Name:    _nas_timer_add()                                          **
 **                                                                        **
 ** Description: Performs timeval addition                                 **
 **                                                                        **
 ** Inputs:  a:     The first timeval structure                **
 **      b:     The second timeval structure               **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     result:    result = timeval(a + b)                    **
 **      Return:    None                                       **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
static void _nas_timer_add(const struct timeval *a, const struct timeval *b,
                           struct timeval *result)
{
  result->tv_sec = a->tv_sec + b->tv_sec;
  result->tv_usec = a->tv_usec + b->tv_usec;

  if (result->tv_usec >= 1000000) {
    result->tv_sec++;
    result->tv_usec -= 1000000;
  }
}

/****************************************************************************
 **                                                                        **
 ** Name:    _nas_timer_sub()                                          **
 **                                                                        **
 ** Description: Performs timeval substraction                             **
 **                                                                        **
 ** Inputs:  a:     The first timeval structure                **
 **      b:     The second timeval structure               **
 **      Others:    None                                       **
 **                                                                        **
 ** Outputs:     result:    a >= b, result = timeval(a - b)            **
 **      Return:    -1 if a < b; 0 otherwise                   **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
static int _nas_timer_sub(const struct timeval *a, const struct timeval *b,
                          struct timeval *result)
{
  if (_nas_timer_cmp(a,b) >= 0 ) {
    result->tv_sec = a->tv_sec - b->tv_sec;
    result->tv_usec = a->tv_usec - b->tv_usec;

    if (result->tv_usec < 0) {
      result->tv_sec--;
      result->tv_usec += 1000000;
    }

    return 0;
  }

  return -1;
}

