/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*****************************************************************************
Source      nas_timer.h

Version     0.1

Date        2012/11/22

Product     NAS stack

Subsystem   Utilities

Description Timer utilities

*****************************************************************************/
#ifndef __NAS_TIMER_H__
#define __NAS_TIMER_H__

/****************************************************************************/
/*********************  G L O B A L    C O N S T A N T S  *******************/
/****************************************************************************/

/*
 * Timer identifier returned when in inactive state (timer is stopped or has
 * failed to be started)
 */
#define NAS_TIMER_INACTIVE_ID   (-1)

/****************************************************************************/
/************************  G L O B A L    T Y P E S  ************************/
/****************************************************************************/

/* Timer structure */
struct nas_timer_t {
  int id;         /* The timer identifier                 */
  long sec;       /* The timer interval value in seconds  */
};

/* Type of the callback executed when the timer expired */
typedef void *(*nas_timer_callback_t)(void *);

/****************************************************************************/
/********************  G L O B A L    V A R I A B L E S  ********************/
/****************************************************************************/

/****************************************************************************/
/******************  E X P O R T E D    F U N C T I O N S  ******************/
/****************************************************************************/

int nas_timer_init(void);
int nas_timer_start(long sec, nas_timer_callback_t cb, void *args);
void nas_timer_fire(void *timer_arg);
int nas_timer_start_ext (instance_t ue_instance_id, long sec, nas_timer_callback_t cb, void *args);
int nas_timer_stop(int id);
int nas_timer_restart(int id);
long nas_timer_get_remaining_sec(int id);

#endif /* __NAS_TIMER_H__ */
